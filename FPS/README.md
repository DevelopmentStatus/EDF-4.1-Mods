# EDF41FPS 0.1.3 — experimental first-person mod

Built for the installed Steam EDF 4.1 executable (SHA-256
`39b4abf7fe722175741a5e0a154063b27ef8c589a1aa5f86c84853571ac7bda8`).
Other executable versions are rejected before hooks are installed.

Follows the animated head's **position** while keeping the game's mouse-controlled
camera orientation. The head and its bone descendants shrink to 0.001 scale during
the player's draw call. Head, face, eye, helmet, visor and hair material draws are
also suppressed for all four classes and their LODs, covering pieces outside the
head-bone hierarchy while keeping body materials. Looking down smoothly pushes
the eye forward, exposing more of the torso.
Vehicles retain their original third-person camera. Death, special camera states,
and an active second viewport also fall back to third person.

## Install

The local game already has EDFModLoader. With the game closed, copy these two files
from `dist/EDF41FPS/` into the game's `Mods/Plugins/` folder:

- `EDF41FPS.dll`
- `EDF41FPS.ini`

Start an offline mission. **C** toggles between first and third person while on
foot; **F6** also works. On an XInput controller, **hold left-stick click (L3) for
0.7 seconds**, then release before toggling again. Inputs require game focus.
Remove those two files to uninstall. No game archives or saves are modified.
The plugin writes `EDF41FPS.log` beside the DLL.

When upgrading, replace only the DLL to retain your settings. The
`HideHeadMaterials` setting defaults to enabled even when absent from your INI.
The log lists each suppressed material once per player skeleton.

## Settings

Edit `EDF41FPS.ini` and restart the game to reload settings.

| Setting | Default | Effect |
|---|---:|---|
| Enabled | 1 | Initial state; C/F6 or a stick hold toggles for the current session |
| ControllerToggle | LeftStick | Hold LeftStick, RightStick, BothSticks, or Off |
| ControllerHoldMs | 700 | Hold duration in milliseconds, clamped to 250–3000 |
| ControllerIndex | 0 | XInput controller slot, 0–3 |
| HeadBone | kubi | Verified joint used by all four classes; blank enables automatic matching |
| DumpBones | 1 | Log bone names and parents when a new player skeleton appears |
| HideHeadMaterials | 1 | Skip local first-person head/helmet/hair materials; keep body and NPC draws |
| EyeForward / EyeUp | 0.025 / 0.15 | Eye offset from the animated head/neck joint, in world units |
| LookDownForward | 0.20 | Additional horizontal forward offset looking down |
| LookDownStartDegrees / EndDegrees | 20 / 75 | Pitch range over which the offset increases |
| LookDownSmoothing | 12 | Larger values respond faster |
| HeadScale | 0.001 | Tiny, nonzero render scale for the head subtree |
| NearClip | 0.035 | First-person near plane |
| VerticalFov | 65 | Vertical FOV in degrees; relative weapon zoom retained; 0 uses game FOV |

If the camera stays third person, inspect the log. Head matching requires exactly
one recognized bone. If it reports `Head resolution failed`, set `HeadBone` to
the exact head name in the logged bone list. An unknown or ambiguous skeleton
keeps the original camera instead of guessing a head position.

The controller shortcut observes button state without consuming game input.
Any action you have assigned to that stick click still occurs. Right-stick
click marks targets in the default controls, so LeftStick is the default choice.
Change `ControllerToggle=BothSticks` if your custom layout already uses L3.
The hold fires once and requires release; short clicks do not toggle. Steam
Input controllers must be presented as an Xbox/XInput controller.

## Validation and current limits

**The 0.1.2 fix was verified in the live Wing Diver mission with Cheat Engine and
confirmed visually by the user.** 0.1.1 cleared its visibility flag between render
passes, so only pass 3 suppressed the head. 0.1.2 keeps visibility active across
all four passes while still restoring the simulation camera. The other classes
retain their verified material filters but still need live visual checks.
Animation timing, aiming parallax, and vehicle
transitions still need visual testing. First-person offsets do not yet run a separate wall-collision sweep;
very close walls can intersect the view. No new weapon viewmodels or crosshair
ballistics correction are included. Online and split-screen use are unsupported;
the plugin disables itself for an active second camera.

Completed checks:

- Release x64 DLL builds with MSVC 2022.
- Core tests cover bounded bone hierarchy walks, subtree selection, render-only
  collapse around the head pivot, look-down blending, frame-rate independence,
  vertical-look fallback, and weapon zoom scaling.
- 102 material cases from all 12 class/LOD models verify head-part filtering and
  preservation of body materials, including the `met` helmet / `metal` body distinction.
- Synthetic renderer tests exercise the actual hooks' material-pointer lookup,
  draw-argument forwarding, nested NPC rendering, filter switch and exception restoration.
- Regression checks verify that camera restoration and auxiliary camera preparation
  preserve first-person visibility, while primary-camera fallback clears it.
- All five hooks create valid MinHook trampolines against a mapped copy of the
  installed executable. The probe never enables hooks or executes game code.
- Cheat Engine traces captured the failing visibility lifetime and confirmed all
  four passes active after the live correction. See `docs/runtime-investigation.md`.

For a useful first playtest: check all four classes; look straight down; roll,
jump/fly and reload; zoom a scoped weapon; enter/exit a tank, helicopter and mech;
die/restart; toggle F6. The head should return instantly in vehicles and third
person. If the view fails, retain `EDF41FPS.log` for the next iteration.

## Build

Requires CMake, Visual Studio 2022 with C++, and the Windows SDK. MinHook source
and its license are included.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\fps_hook_probe.exe 'A:\SteamLibrary\steamapps\common\Earth Defense Force 4.1\EDF41.exe'
```

Output: `build/Release/EDF41FPS.dll`. Implementation evidence and remaining
unknowns are in `docs/re-notes.md`.
