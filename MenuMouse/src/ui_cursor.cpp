#include "ui_cursor.h"

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "sigscan.h"

#include <windows.h>
#include <MinHook.h>
#include <cstdint>

// Layouts below are from docs/re-notes.md.
namespace mm {

namespace {

// ui::CursorBase: selection model shared by every UiCursor_* widget.
struct CursorItem {
	int32_t x, y, w, h; // relative to the owner widget
	uint64_t tag;
	uint8_t enabled;
};

struct ItemRef {
	void *ctrl;
	CursorItem *item;
};

struct CursorColumn { // grid mode: one per column, 0x20 bytes
	void *unk0;
	uint32_t *indices; // item indices top to bottom
	uint64_t unk10;
	uint64_t count;
};

constexpr size_t kOwner = 0x28;       // ui::Object* the list belongs to
constexpr size_t kItems = 0x10;       // ItemRef*
constexpr size_t kItemCount = 0x20;   // uint64
constexpr size_t kMode = 0x30;        // 2 = vertical list, 3 = grid
constexpr size_t kColumn = 0x38;      // grid column
constexpr size_t kIndex = 0x3c;       // selected row / index
constexpr size_t kColumns = 0x58;     // CursorColumn*
constexpr size_t kColumnCount = 0x68; // uint64
constexpr size_t kOwnerRect = 0x48;   // int x, y, w, h on the owner widget

// UiInput: per-player action bits, plus a merged copy.
constexpr size_t kInputPlayerStride = 0xd98;
constexpr size_t kInputHeld = 0xd88;
constexpr size_t kInputPressed = 0xd8c;
constexpr size_t kInputRepeat = 0xd90;
constexpr size_t kInputReleased = 0xd94;
constexpr size_t kInputActivePlayers = 0x4400; // int[2], -1 when unused
constexpr size_t kInputMerged = 0x4408;        // held, pressed, repeat, released
constexpr uint32_t kActUp = 0x01, kActDown = 0x02, kActDecide = 0x40, kActCancel = 0x80;

constexpr uint32_t kMoveUp = 0x01;

template <typename T>
T &At(void *base, size_t off) {
	return *reinterpret_cast<T *>(static_cast<uint8_t *>(base) + off);
}

using MoveSelection_t = char(__fastcall *)(void *cursor, uint32_t dir, uint32_t flags);
using UiInputUpdate_t = void(__fastcall *)(void *input, int frames);

MoveSelection_t oMoveSelection;
UiInputUpdate_t oUiInputUpdate;

// Shared between the two hooks, both of which run on the game thread.
ULONGLONG g_lastMenuTick = 0;
void *g_lastCursor = nullptr;
MouseFrame g_mouse;
bool g_mouseFresh = false; // g_mouse not yet consumed by a move hook this frame
bool g_hoverOnSelected = false;
uint32_t g_releaseNext = 0;

constexpr ULONGLONG kMenuTimeoutMs = 250;

// Owner widgets expose their world and projection matrices as virtual
// getters (ui::Base slots 7 and 8, row-vector D3D convention).
constexpr size_t kOwnerWorldSlot = 7;
constexpr size_t kOwnerProjSlot = 8;
constexpr size_t kOwnerScroll = 0xa4;         // int, scrolled lists only
constexpr uintptr_t kUiCursorModelVtbl = 0xaa9d58; // RVA
constexpr size_t kModelScrolls = 0x78;        // bool: rows positioned by index - scroll
constexpr size_t kModelClampTop = 0x79;       // bool: first row may not go above 0

using GetMatrix_t = float *(__fastcall *)(void *self, float *out);

struct Quad {
	float x[4], y[4]; // client pixels, corner order around the rect
};

bool InImage(const void *p) {
	uintptr_t a = reinterpret_cast<uintptr_t>(p);
	return a >= ModuleBase() && a < ModuleBase() + ModuleSize();
}

bool GetOwnerMatrices(void *owner, float world[16], float proj[16]) {
	void **vtbl = At<void **>(owner, 0);
	if (!InImage(vtbl) || !InImage(vtbl[kOwnerWorldSlot]) || !InImage(vtbl[kOwnerProjSlot]))
		return false;
	reinterpret_cast<GetMatrix_t>(vtbl[kOwnerWorldSlot])(owner, world);
	reinterpret_cast<GetMatrix_t>(vtbl[kOwnerProjSlot])(owner, proj);
	return true;
}

// Local widget point -> client pixel, the same way the renderer does it.
bool Project(const float w[16], const float p[16], float x, float y, SIZE client, float &sx, float &sy) {
	float v[4] = {
		x * w[0] + y * w[4] + w[12],
		x * w[1] + y * w[5] + w[13],
		x * w[2] + y * w[6] + w[14],
		x * w[3] + y * w[7] + w[15],
	};
	float c[4];
	for (int i = 0; i < 4; ++i)
		c[i] = v[0] * p[i] + v[1] * p[4 + i] + v[2] * p[8 + i] + v[3] * p[12 + i];
	if (c[3] <= 1e-6f)
		return false;
	sx = (c[0] / c[3] * 0.5f + 0.5f) * client.cx;
	sy = (0.5f - c[1] / c[3] * 0.5f) * client.cy;
	return true;
}

bool PointInQuad(const Quad &q, float px, float py) {
	bool pos = false, neg = false;
	for (int i = 0; i < 4; ++i) {
		int j = (i + 1) % 4;
		float cross = (q.x[j] - q.x[i]) * (py - q.y[i]) - (q.y[j] - q.y[i]) * (px - q.x[i]);
		pos |= cross > 0;
		neg |= cross < 0;
	}
	return !(pos && neg);
}

// Screen quad of item i, or false if it isn't on screen.
bool ItemQuad(void *cursor, void *owner, const CursorItem *it, uint64_t i, const float world[16],
              const float proj[16], SIZE client, Quad &q) {
	float top = static_cast<float>(it->y);
	if (At<uintptr_t>(cursor, 0) == ModuleBase() + kUiCursorModelVtbl && At<uint8_t>(cursor, kModelScrolls)) {
		// Scrolling list: rows stack by index and shift by the owner's scroll.
		int y = it->h * static_cast<int>(i) - At<int32_t>(owner, kOwnerScroll);
		int lo = At<uint8_t>(cursor, kModelClampTop) ? 0 : -it->h;
		int hi = At<int32_t>(owner, kOwnerRect + 12);
		if (At<uint8_t>(cursor, kModelClampTop))
			hi = hi - it->h + 1;
		if (y < lo || y > hi)
			return false;
		top = static_cast<float>(y);
	}
	float xs[4] = {(float)it->x, (float)(it->x + it->w), (float)(it->x + it->w), (float)it->x};
	float ys[4] = {top, top, top + it->h, top + it->h};
	for (int k = 0; k < 4; ++k) {
		if (!Project(world, proj, xs[k], ys[k], client, q.x[k], q.y[k]))
			return false;
	}
	return true;
}

// Returns the index of the enabled item under the mouse, or -1.
int HitTest(void *cursor, const MouseFrame &m) {
	__try {
		void *owner = At<void *>(cursor, kOwner);
		auto items = At<ItemRef *>(cursor, kItems);
		auto count = At<uint64_t>(cursor, kItemCount);
		float world[16], proj[16];
		if (!owner || !items || count > 512 || !GetOwnerMatrices(owner, world, proj))
			return -1;
		float px = static_cast<float>(m.client.x), py = static_cast<float>(m.client.y);
		for (uint64_t i = 0; i < count; ++i) {
			const CursorItem *it = items[i].item;
			Quad q;
			if (it && it->enabled && ItemQuad(cursor, owner, it, i, world, proj, m.clientSize, q) &&
			    PointInQuad(q, px, py))
				return static_cast<int>(i);
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
	}
	return -1;
}

void LogCursorLayout(void *cursor, SIZE client) {
	__try {
		void *owner = At<void *>(cursor, kOwner);
		int mode = At<int32_t>(cursor, kMode);
		auto count = At<uint64_t>(cursor, kItemCount);
		Log("Menu %p vtbl=+%llX mode=%d items=%llu sel=%d owner=%p", cursor,
		    static_cast<unsigned long long>(At<uintptr_t>(cursor, 0) - ModuleBase()), mode, count,
		    At<int32_t>(cursor, kIndex), owner);
		if (!owner)
			return;
		Log("  owner rect x=%d y=%d w=%d h=%d client=%ldx%ld", At<int32_t>(owner, kOwnerRect),
		    At<int32_t>(owner, kOwnerRect + 4), At<int32_t>(owner, kOwnerRect + 8), At<int32_t>(owner, kOwnerRect + 12),
		    client.cx, client.cy);
		float world[16], proj[16];
		bool haveMatrices = GetOwnerMatrices(owner, world, proj);
		auto items = At<ItemRef *>(cursor, kItems);
		for (uint64_t i = 0; items && i < count && i < 64; ++i) {
			const CursorItem *it = items[i].item;
			if (!it)
				continue;
			Quad q;
			if (haveMatrices && ItemQuad(cursor, owner, it, i, world, proj, client, q))
				Log("  item %llu: x=%d y=%d w=%d h=%d enabled=%d -> screen (%.0f,%.0f)-(%.0f,%.0f)", i, it->x, it->y,
				    it->w, it->h, it->enabled, q.x[0], q.y[0], q.x[2], q.y[2]);
			else
				Log("  item %llu: x=%d y=%d w=%d h=%d enabled=%d (off screen)", i, it->x, it->y, it->w, it->h,
				    it->enabled);
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		Log("Menu %p: layout read faulted", cursor);
	}
}

// Rewrites the cursor's position so that one "up" step lands on `item`,
// letting the game's own move code play the sound and move the highlight.
bool PrepareSelect(void *cursor, int item, uint32_t &dir) {
	__try {
		int mode = At<int32_t>(cursor, kMode);
		if (mode == 2) {
			if (At<int32_t>(cursor, kIndex) == item)
				return false;
			At<int32_t>(cursor, kIndex) = item + 1;
			dir = kMoveUp;
			return true;
		}
		if (mode == 3) {
			auto cols = At<CursorColumn *>(cursor, kColumns);
			auto ncols = At<uint64_t>(cursor, kColumnCount);
			for (uint64_t c = 0; cols && c < ncols && c < 64; ++c) {
				for (uint64_t r = 0; r < cols[c].count && r < 512; ++r) {
					if (cols[c].indices[r] != static_cast<uint32_t>(item))
						continue;
					if (At<int32_t>(cursor, kColumn) == static_cast<int>(c) &&
					    At<int32_t>(cursor, kIndex) == static_cast<int>(r))
						return false;
					At<int32_t>(cursor, kColumn) = static_cast<int>(c);
					At<int32_t>(cursor, kIndex) = static_cast<int>(r) + 1;
					dir = kMoveUp;
					return true;
				}
			}
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
	}
	return false;
}

int EnabledItemCount(void *cursor) {
	__try {
		auto items = At<ItemRef *>(cursor, kItems);
		auto count = At<uint64_t>(cursor, kItemCount);
		int n = 0;
		for (uint64_t i = 0; items && i < count && i < 512; ++i)
			n += items[i].item && items[i].item->enabled;
		return n;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

bool IsSelected(void *cursor, int item) {
	__try {
		auto items = At<ItemRef *>(cursor, kItems);
		return items && At<CursorItem *>(cursor, 0x48) == items[item].item;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

char __fastcall hkMoveSelection(void *cursor, uint32_t dir, uint32_t flags) {
	g_lastMenuTick = GetTickCount64();
	if (cursor != g_lastCursor) {
		g_lastCursor = cursor;
		if (g_config.debugLog)
			LogCursorLayout(cursor, g_mouse.clientSize);
	}

	// Only the first cursor updated after a mouse poll reacts, so stacked
	// menus (a dialog over a list) don't both chase the pointer.
	if (g_mouseFresh && g_mouse.inside) {
		g_mouseFresh = false;
		int hit = HitTest(cursor, g_mouse);
		if (hit >= 0 && g_config.hoverSelect && g_mouse.moved && dir == 0)
			PrepareSelect(cursor, hit, dir);
		char result = oMoveSelection(cursor, dir, flags);
		// A single-button popup ("OK") has nothing to choose, so any click confirms it.
		g_hoverOnSelected = (hit >= 0 && IsSelected(cursor, hit)) || EnabledItemCount(cursor) == 1;
		if (g_config.debugLog && g_mouse.leftPressed)
			LogDebug("Click at client (%ld,%ld) hit=%d", g_mouse.client.x, g_mouse.client.y, hit);
		return result;
	}
	return oMoveSelection(cursor, dir, flags);
}

void InjectAction(void *input, size_t field, uint32_t bits) {
	At<uint32_t>(input, kInputMerged + (field - kInputHeld)) |= bits;
	for (int i = 0; i < 2; ++i) {
		int player = At<int32_t>(input, kInputActivePlayers + i * 4);
		if (player >= 0 && player < 4)
			At<uint32_t>(input, player * kInputPlayerStride + field) |= bits;
	}
}

void __fastcall hkUiInputUpdate(void *input, int frames) {
	oUiInputUpdate(input, frames);

	bool menu = GetTickCount64() - g_lastMenuTick < kMenuTimeoutMs;
	SetMenuMouseMode(menu);
	if (!menu) {
		g_mouseFresh = false;
		g_hoverOnSelected = false;
		g_releaseNext = 0;
		return;
	}

	// Finish last frame's synthetic press with a release edge.
	if (g_releaseNext) {
		InjectAction(input, kInputReleased, g_releaseNext);
		g_releaseNext = 0;
	}

	g_mouse = PollMouse();
	g_mouseFresh = true;
	if (!g_mouse.inside)
		return;

	uint32_t press = 0;
	if (g_mouse.leftPressed && g_config.clickConfirm && g_hoverOnSelected)
		press |= kActDecide;
	if (g_mouse.rightPressed && g_config.rightClickBack)
		press |= kActCancel;
	if (press) {
		InjectAction(input, kInputHeld, press);
		InjectAction(input, kInputPressed, press);
		InjectAction(input, kInputRepeat, press);
		g_releaseNext = press;
	}
	if (g_mouse.wheelSteps && g_config.wheelScroll) {
		uint32_t step = g_mouse.wheelSteps > 0 ? kActUp : kActDown;
		InjectAction(input, kInputRepeat, step);
	}
}

const char *kSigMoveSelection =
	"40 57 41 56 41 57 48 83 EC 40 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 60 48 89 6C 24 68 "
	"48 89 74 24 70 8B F2 48 8B D9 8B 41 70 45 33 FF 85 D2 74 04 44 89 79 70 44 09 41 70";
const char *kSigUiInputUpdate =
	"48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 81 EC A0 00 00 00 "
	"48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 90 00 00 00 45 33 C0 44 8B DA 89 54 24 34";

bool Hook(const char *name, const char *sig, void *detour, void **original) {
	uintptr_t addr = SigScan(sig);
	if (!addr) {
		Log("Signature not found: %s", name);
		return false;
	}
	Log("%s at EDF41.exe+%llX", name, static_cast<unsigned long long>(addr - ModuleBase()));
	MH_STATUS st = MH_CreateHook(reinterpret_cast<void *>(addr), detour, original);
	if (st != MH_OK) {
		Log("Failed to hook %s: %s", name, MH_StatusToString(st));
		return false;
	}
	return true;
}

} // namespace

bool InstallMenuHooks() {
	return Hook("CursorBase::MoveSelection", kSigMoveSelection, reinterpret_cast<void *>(&hkMoveSelection),
	            reinterpret_cast<void **>(&oMoveSelection)) &&
	       Hook("UiInput::Update", kSigUiInputUpdate, reinterpret_cast<void *>(&hkUiInputUpdate),
	            reinterpret_cast<void **>(&oUiInputUpdate));
}

} // namespace mm
