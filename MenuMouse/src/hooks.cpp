#include "hooks.h"

#include "config.h"
#include "log.h"

#include <MinHook.h>
#include <atomic>
#include <intrin.h>

namespace mm {

using GetCursorPos_t = BOOL(WINAPI *)(LPPOINT);
using SetCursorPos_t = BOOL(WINAPI *)(int, int);
using ShowCursor_t = int(WINAPI *)(BOOL);
using GetKeyboardState_t = BOOL(WINAPI *)(PBYTE);

static GetCursorPos_t oGetCursorPos;
static SetCursorPos_t oSetCursorPos;
static ShowCursor_t oShowCursor;
static GetKeyboardState_t oGetKeyboardState;

static std::atomic<bool> g_menuMode{false};
static POINT g_gameCenter{};      // last position the game re-centered to
static bool g_haveGameCenter = false;
static int g_gameShowCount = 0;   // what the game believes the display count is

static HWND g_hwnd;
static WNDPROC g_origWndProc;
static std::atomic<int> g_wheelAccum{0};
static std::atomic<bool> g_leftClick{false};
static std::atomic<bool> g_rightClick{false};
static POINT g_lastPoll{-1, -1};

// The cursor display count belongs to the window's thread, but menu mode is
// switched from the game's update thread, so visibility changes are posted.
static const UINT kMsgSyncCursor = WM_APP + 0x4D4D;

static void SyncCursorVisibility(bool menu);

static BOOL CALLBACK FindWindowProc(HWND hwnd, LPARAM out) {
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd) && !GetWindow(hwnd, GW_OWNER)) {
		*reinterpret_cast<HWND *>(out) = hwnd;
		return FALSE;
	}
	return TRUE;
}

static LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	if (msg == kMsgSyncCursor) {
		SyncCursorVisibility(wp != 0);
		return 0;
	}
	if (g_menuMode) {
		switch (msg) {
		case WM_SETCURSOR:
			// The game's window class may have no cursor; supply the arrow.
			if (g_config.showCursorInMenus && LOWORD(lp) == HTCLIENT) {
				SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)));
				return TRUE;
			}
			break;
		case WM_MOUSEWHEEL:
			// Handled as menu up/down instead of the game's own wheel input.
			if (g_config.wheelScroll) {
				g_wheelAccum += GET_WHEEL_DELTA_WPARAM(wp);
				return 0;
			}
			break;
		case WM_LBUTTONDOWN:
			g_leftClick = true;
			break;
		case WM_RBUTTONDOWN:
			g_rightClick = true;
			break;
		}
	}
	return CallWindowProcW(g_origWndProc, hwnd, msg, wp, lp);
}

static void EnsureWindowHooked() {
	if (g_hwnd)
		return;
	HWND found = nullptr;
	EnumWindows(FindWindowProc, reinterpret_cast<LPARAM>(&found));
	if (!found)
		return;
	g_hwnd = found;
	g_origWndProc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MenuWndProc)));
	Log("Subclassed game window %p", g_hwnd);
}

HWND GameWindow() {
	return g_hwnd;
}

// The game reads the cursor and snaps it back to the window center every
// frame to get relative motion. In menus we report the center back so the
// game sees no motion, which stops its built-in up/down stepping.
static bool CalledFromGame(void *returnAddress) {
	HMODULE mod = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                   static_cast<LPCWSTR>(returnAddress), &mod);
	return mod == GetModuleHandleW(nullptr);
}

static BOOL WINAPI hkGetCursorPos(LPPOINT pt) {
	EnsureWindowHooked();
	if (g_menuMode && g_haveGameCenter && pt && CalledFromGame(_ReturnAddress())) {
		*pt = g_gameCenter;
		return TRUE;
	}
	return oGetCursorPos(pt);
}

static BOOL WINAPI hkSetCursorPos(int x, int y) {
	g_gameCenter = {x, y};
	g_haveGameCenter = true;
	if (g_menuMode)
		return TRUE;
	return oSetCursorPos(x, y);
}

static int WINAPI hkShowCursor(BOOL show) {
	g_gameShowCount += show ? 1 : -1;
	if (g_menuMode && g_config.showCursorInMenus)
		return g_gameShowCount;
	return oShowCursor(show);
}

// Mute the mouse buttons for the game while in menu mode; clicks are
// routed through the hit-tested menu code instead.
static BOOL WINAPI hkGetKeyboardState(PBYTE keys) {
	BOOL ok = oGetKeyboardState(keys);
	if (ok && g_menuMode && keys) {
		keys[VK_LBUTTON] = 0;
		keys[VK_RBUTTON] = 0;
		keys[VK_MBUTTON] = 0;
	}
	return ok;
}

static void SyncCursorVisibility(bool menu) {
	if (!g_config.showCursorInMenus)
		return;
	int count;
	if (menu) {
		// Raise the real display count until the cursor shows.
		do
			count = oShowCursor(TRUE);
		while (count < 0);
		SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)));
	} else {
		// Restore the count the game asked for.
		count = oShowCursor(TRUE);
		count = oShowCursor(FALSE);
		while (count > g_gameShowCount)
			count = oShowCursor(FALSE);
		while (count < g_gameShowCount)
			count = oShowCursor(TRUE);
	}
}

void SetMenuMouseMode(bool active) {
	if (g_menuMode == active)
		return;
	g_menuMode = active;
	if (g_hwnd)
		PostMessageW(g_hwnd, kMsgSyncCursor, active ? 1 : 0, 0);
	if (!active && g_haveGameCenter)
		oSetCursorPos(g_gameCenter.x, g_gameCenter.y);
	g_wheelAccum = 0;
	g_leftClick = false;
	g_rightClick = false;
	Log("Menu mouse mode %s", active ? "on" : "off");
}

MouseFrame PollMouse() {
	MouseFrame f;
	EnsureWindowHooked();
	POINT screen;
	if (!g_hwnd || !oGetCursorPos(&screen))
		return f;
	f.client = screen;
	ScreenToClient(g_hwnd, &f.client);
	RECT rc;
	GetClientRect(g_hwnd, &rc);
	f.clientSize = {rc.right - rc.left, rc.bottom - rc.top};
	f.inside = f.client.x >= 0 && f.client.y >= 0 && f.client.x < f.clientSize.cx &&
	           f.client.y < f.clientSize.cy && GetForegroundWindow() == g_hwnd;
	f.moved = f.client.x != g_lastPoll.x || f.client.y != g_lastPoll.y;
	g_lastPoll = f.client;
	f.leftPressed = g_leftClick.exchange(false);
	f.rightPressed = g_rightClick.exchange(false);
	int wheel = g_wheelAccum.load();
	f.wheelSteps = wheel / WHEEL_DELTA;
	g_wheelAccum -= f.wheelSteps * WHEEL_DELTA;
	return f;
}

template <typename T>
static bool HookApi(const wchar_t *module, const char *name, void *detour, T *original) {
	MH_STATUS st = MH_CreateHookApi(module, name, detour, reinterpret_cast<void **>(original));
	if (st != MH_OK) {
		Log("Failed to hook %s: %s", name, MH_StatusToString(st));
		return false;
	}
	return true;
}

bool InstallInputHooks() {
	bool ok = true;
	ok &= HookApi(L"user32", "GetCursorPos", &hkGetCursorPos, &oGetCursorPos);
	ok &= HookApi(L"user32", "SetCursorPos", &hkSetCursorPos, &oSetCursorPos);
	ok &= HookApi(L"user32", "ShowCursor", &hkShowCursor, &oShowCursor);
	ok &= HookApi(L"user32", "GetKeyboardState", &hkGetKeyboardState, &oGetKeyboardState);
	if (ok)
		ok = MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
	Log("Input hooks %s", ok ? "installed" : "FAILED");
	return ok;
}

void RemoveInputHooks() {
	if (g_hwnd && g_origWndProc)
		SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origWndProc));
}

} // namespace mm
