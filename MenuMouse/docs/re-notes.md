# EDF41.exe reverse-engineering notes

Target: Steam `EDF41.exe`, 13,896,192 bytes, SHA-256 `39b4abf7…71ac7bda8`, x64, ImageBase `0x140000000`, MSVC 2013. Not packed.
Addresses are static VAs. The plugin finds its hooks by signature (see `src/ui_cursor.cpp`).
Ghidra export: `A:\gameassets\ReverseEngineeringPackage\workspace\EDF41-39b4abf7fe72\20261101T200735260858Z\source` (39,880 functions, gamedb-indexed).

## Platform input (`FUN_140561740`, per frame)
- Pumps messages, then calls `GetKeyboardState` into `0x140cc3040[256]`. The mouse buttons are VK_LBUTTON/RBUTTON/MBUTTON in that buffer.
- If the window is foreground: `GetWindowRect` + `GetCursorPos`. If the cursor isn't at the window center, adds the delta to `float[2] @0x140c64900`, calls `SetCursorPos(center)` and hides the cursor with `ShowCursor(0)`.
- WndProc `FUN_140560850`: `WM_MOUSEWHEEL` adds `delta/120` to `int @0x140cc303c`. `WM_KEYDOWN` queues UI key events (Up=1, Down=2, Left=3, Backspace=5, Esc=6, PgUp=7, PgDn=8, Tab=9, Enter=10, F1–F12=11–22).

## Virtual buttons (`FUN_1405630e0`)
Per player (stride `0x490`) at `0x140cc3d20`, an array of button-object pointers. Each object has value `float @+0x48` and pressed `bool @+0x4c`.
The mouse delta also feeds an accumulator (`0x140cc4190`), which emits digital "step" buttons. That's 4.1's built-in relative mouse menu navigation. The plugin cancels it by reporting zero motion to the game while a menu is active.

## UI input: `FUN_1400b0ed0(UiInput* this, int frames)`. **Hooked.**
Signature: `48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 81 EC A0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 90 00 00 00 45 33 C0 44 8B DA 89 54 24 34`

| Offset | Meaning |
|---|---|
| player block `p*0xd98 + 0xd88/0xd8c/0xd90/0xd94` | held / pressed / repeat / released action bits |
| `+0x4400` int[2] | active player indices (-1 = none) |
| `+0x4408/0x440c/0x4410/0x4414` | merged held / pressed / repeat / released |

Action bits: `0x01` up, `0x02` down, `0x04` left, `0x08` right, `0x40` decide, `0x80` cancel. When decide is held, the other bits are masked (`& 0xffff2040`).
Screens read `pressed & 0x40/0x80` for decide/cancel and `repeat & 1/2` for navigation (e.g. `UiModeSelect_Main` input `FUN_1404a6440`).

## Selection model: `ui::CursorBase` (vtable `0x140aae1a8`, ctor `FUN_1404e7b60`)
Base of every `UiCursor_*` (PanelCursor, SlidePanelCursor, SimpleBox, Model, ChangeColor).

| Offset | Meaning |
|---|---|
| `+0x10` | `{ctrl*, CursorItem*}[]` items (16 bytes each) |
| `+0x20` | item count (u64) |
| `+0x28` | owner widget (`ui::Object*`). The owner's `+0x48` is `int x, y, w, h` |
| `+0x30` | mode: 2 = vertical list, 3 = grid |
| `+0x34` | wrap mode (1 = wrap) |
| `+0x38` | grid column |
| `+0x3c` | selected index (row in grid mode) |
| `+0x40/+0x48` | shared_ptr to selected item (`+0x48` = raw `CursorItem*`) |
| `+0x58/+0x68` | grid columns `{?, u32* indices, ?, u64 count}` (0x20 each) / column count |
| `+0x70` | flags |

`CursorItem`: `int x, y, w, h` (relative to the owner), `u64 tag`, `u8 enabled @+0x18`. It's added by `FUN_1404e7c70(cursor, rect*)`, which returns an index. For example, `UiModeSelect_Main` adds rows `{0, i*60, 360, 32}`.

### `FUN_1404e7e90(CursorBase*, u32 dir, u32 flags) -> bool`. **Hooked.**
Signature: `40 57 41 56 41 57 48 83 EC 40 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 60 48 89 6C 24 68 48 89 74 24 70 8B F2 48 8B D9 8B 41 70 45 33 FF 85 D2 74 04 44 89 79 70 44 09 41 70`
- `dir`: `1` up, `2` down, `4` left, `8` right. Every active screen calls it once per frame from its input handler, even with `dir == 0`, so it also serves as the "a menu has focus" signal.
- It moves the index, skips disabled items, wraps when `+0x34 == 1` and resolves the item. If the item changed, it calls `FUN_14034d4d0` (move sound) and vfunc `+0x10` (animate the highlight to the new item's rect).
- **Plugin trick:** to select item N, set index = N+1 and pass `dir = 1`. The game's own path then runs, with sound and highlight.

## UI classes
Screens: `UiTitle_Main` 0x140aad8f8, `UiModeSelect_Main` 0x140aac2f8 (list at `+0xd0`), `UiHQ_Main` 0x140aab678, …, and about 40 callers of `FUN_1404e7e90` (see gamedb).
`UiCursor_Model` (vtable 0x140aa9d58): highlight rect float[4] `@+0xd0`, lerped toward the selected item. Its draw is `FUN_140416570`.

## Projection (item rect -> pixels)
Items are drawn in the owner widget's local space. The owner (`ui::Base`) exposes virtual getters:
- slot 7 (`+0x38`), e.g. `FUN_1404e8740`: the world matrix. It takes the 4x4 at `+0x60`, moves its translation to the owner rect origin (`+0x48` x, y), applies the transition scale from `+0xa8`, and when `+0x44 != 1` applies the 3D scale (0.01, y flipped).
- slot 8 (`+0x40`), e.g. `FUN_1404dbcd0`: the projection. When `+0x44 == 1` it's an ortho over the backbuffer (`FUN_14051b350`). Otherwise it's a right-handed perspective (`FUN_14051b230`, aspect = bbW/bbH, near 0.01, far 4000).
- The UI design space is 1920x1080 (`0x140ab6a34`/`0x140ab6a14`).
- Pixel = NDC mapped D3D-style (y down). The plugin projects all 4 corners and does a point-in-quad test.

Scrolling (`UiCursor_Model` only, `+0x78` set): row y = `item.h * index - owner[+0xa4]`, and the row is visible when it's within `[-h or 0, owner.h (- h + 1)]` (`+0x79` picks the clamp). See `FUN_140416570`.

## Still unverified (needs an in-game check)
1. Whether the draw context adds a view transform beyond slot 7 x slot 8. The `DebugLog=1` output gives each item's computed screen rect for comparison.
2. Whether other `UiCursor_*` types (PanelCursor, SlidePanel, SimpleBox) draw items at their plain rect.
