# MenuMouse — point-and-click menus for EDF 4.1

Adds absolute mouse support to Earth Defense Force 4.1's menus. Hovering over a menu item selects it, left click activates it, right click goes back and the wheel scrolls lists. Keyboard and controller work unchanged. Gameplay mouse aiming is untouched.

It's a DLL plugin for [EDFModLoader](https://github.com/BlueAmulet/EDFModLoader). No game archives are modified.

## Install
1. Download the EDFModLoader release and unpack it next to `EDF41.exe`. It adds `winmm.dll` and a `Mods` folder.
2. Copy `MenuMouse.dll` into `<EDF 4.1>\Mods\Plugins\`.
3. Start the game once. `MenuMouse.ini` and `MenuMouse.log` appear beside the DLL.

## Settings (`MenuMouse.ini`)
| Key | Default | Meaning |
|---|---|---|
| Enabled | 1 | Master switch |
| HoverSelect | 1 | Moving the mouse over an item selects it |
| ClickConfirm | 1 | Left click activates the item under the cursor |
| RightClickBack | 1 | Right click = back/cancel |
| WheelScroll | 1 | Wheel moves through lists |
| ShowCursorInMenus | 1 | Show the Windows cursor while a menu is open |
| DebugLog | 0 | Log every menu's item layout and each click's UI coordinates (for calibration) |

### Debugging hover
Set `DebugLog=1`. Each time a new menu gets focus, `MenuMouse.log` lists its items with the on-screen rectangle the plugin computed (`-> screen (x0,y0)-(x1,y1)` in window pixels). Every click logs its window position and which item it hit. If a rectangle doesn't match where the item is drawn, send the log.

## Build
Requires Visual Studio 2022 and CMake.

```
cmake -B build -A x64
cmake --build build --config Release
```

Output: `build\Release\MenuMouse.dll`.

## How it works
- The game reads mouse motion by calling `GetCursorPos` and snapping the cursor back with `SetCursorPos` every frame. While a menu is open, the plugin intercepts both, so the cursor stays free and the game sees no relative motion. The game's own mouse up/down stepping is therefore off in menus.
- Every menu's selection runs through one shared function (`ui::CursorBase` move-selection). The plugin hooks it, projects each item's rectangle to the screen through the menu's own world/projection matrices (the same ones the renderer uses, so tilted 3D menus and any resolution work), hit-tests the mouse against them and steers that call onto the hovered item. The game plays its own move sound and highlight animation.
- Clicks are injected as one frame of the game's own "decide" (left) or "cancel" (right) action in the UI input state. A left click only counts when the pointer is on the selected item. The wheel becomes up/down.
- Reverse-engineering notes (addresses, signatures, struct offsets) are in [docs/re-notes.md](docs/re-notes.md).
