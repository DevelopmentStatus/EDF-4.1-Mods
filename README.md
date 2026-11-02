# EDF 4.1 Mods

Development source for two Earth Defense Force 4.1 PC plugins. Both use
[EDFModLoader](https://github.com/BlueAmulet/EDFModLoader); neither modifies game
archives or save files.

| Mod | Features | Documentation |
|---|---|---|
| EDF41FPS | Head-position first-person camera, visible body, head/helmet/hair hiding, dynamic look-down offset, automatic third person in vehicles | [FPS](FPS/README.md) |
| MenuMouse | Absolute mouse hover, click confirmation, right-click back, and wheel scrolling in menus | [MenuMouse](MenuMouse/README.md) |

## Development status

These are development builds. EDF41FPS 0.1.3 includes C/F6 camera toggles and a
configurable controller stick-click hold (default: hold L3 for 700 ms). Its head
visibility lifetime fix was measured with Cheat Engine and visually confirmed
on Wing Diver; the other classes still need live visual checks. MenuMouse has
its implementation and reverse-engineering notes available for further testing.

Read each project's README for installation, settings, compatibility and known
limits. EDF41FPS currently accepts only the executable hash documented there.

## Build from source

Requirements: Windows, CMake, Visual Studio 2022 C++ tools, and the Windows SDK.
MinHook source and its license are included in each project.

```powershell
cmake -S FPS -B FPS/build -A x64
cmake --build FPS/build --config Release
ctest --test-dir FPS/build -C Release --output-on-failure

cmake -S MenuMouse -B MenuMouse/build -A x64
cmake --build MenuMouse/build --config Release
```

DLLs are produced in each project's `build/Release` directory. Install plugins
in the game's `Mods/Plugins` directory after installing EDFModLoader. FPS also
ships a sample `EDF41FPS.ini`; MenuMouse creates its configuration at first load.

## Repository contents

Source, configuration examples, tests, research tools and implementation notes
are included. Build outputs, release archives, backup DLLs, runtime logs and
unrelated save-editor files are excluded. Some research helpers document this
development machine's paths and require adjustment on other machines.

Third-party MinHook licensing is retained in each `third_party/minhook` folder.
