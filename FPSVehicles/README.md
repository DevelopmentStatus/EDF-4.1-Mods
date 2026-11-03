# EDF41FPSVehicles 0.1.4 (experimental)

An independent vehicle-enabled variant of EDF41FPS 0.1.3 for the same Steam
EDF 4.1 executable (SHA-256
`39b4abf7fe722175741a5e0a154063b27ef8c589a1aa5f86c84853571ac7bda8`).

- On foot: animated head camera, visible body, head/helmet/hair filtering, and
  the original look-down adjustment.
- Bikes: first person at the animated rider head; the bike stays opaque.
- Supported enclosed vehicles: camera follows the animated driver head. The occupied vehicle hull defaults to **25% opacity**.
- Vehicle-relative orientation: the camera rolls with the chassis through walls
  and ceilings while retaining the game's aim direction; eye offsets rotate too.
- Mechs: use the vehicle camera attachment node at cockpit/torso height instead
  of the hidden infantry pilot near the feet. Separate cockpit offsets apply.
- Grape passengers: retain the game's native first-person camera and opaque hull.
- C / F6 or hold L3 for 700 ms toggles first/third person, including while riding.
  Third person immediately restores ordinary vehicle rendering.

Supported RTTI classes include tanks, rescue tanks, rocket/striker vehicles,
helicopters, Grape/Striker, CarDefault, Vegalta/Begaruta and the two large robot classes.
Unknown types and invalid/stale seats retain the normal game camera.
This variant has its own release/tag and must be installed instead of EDF41FPS.

## Install

Close the game first. **Use either EDF41FPS or EDF41FPSVehicles.** They replace
the same camera and model routines and cannot run together. Back up/remove
`EDF41FPS.dll` from `Mods/Plugins`, then copy the following from
the release ZIP's `Mods/Plugins/` directory into that directory:

- `EDF41FPSVehicles.dll`
- `EDF41FPSVehicles.ini`

Keep the original mod's INI for switching back. The variant uses its own INI and
log, and leaves the original FPS source/build untouched. Its loader refuses to
activate if `EDF41FPS.dll` is loaded. EDFModLoader is required.

To switch back, close the game, remove `EDF41FPSVehicles.dll`, and restore the
original `EDF41FPS.dll`. Archives and saves are not modified.

## Vehicle settings

All settings use the `[FPS]` section of `EDF41FPSVehicles.ini`. Restart to reload.
The inherited on-foot settings are described in `../FPS/README.md`.

| Setting | Default | Effect |
|---|---:|---|
| VehicleOpacity | 0.25 | Enclosed hull opacity, 0 invisible through 1 opaque; bikes are unaffected |
| VehicleRelativeOrientation | 1 | Use the chassis as camera up, including walls and ceilings; 0 restores the old world horizon |
| MechCockpitCamera | 1 | Anchor supported mechs to their live model camera node; 0 uses the pilot head |
| MechCockpitUp | 0.15 | Offset above the cockpit anchor in world units, along chassis up |
| MechCockpitForward | 0.20 | Offset forward from the cockpit anchor in world units |

Ordinary driver cameras use the animated head with the inherited EyeUp / EyeForward
offsets. Mechs use their seat camera attachment node, with separate cockpit
offsets. Missing or invalid cockpit nodes fall back to the pilot head. The old VehicleEyeUp / VehicleEyeForward seat-offset keys are ignored. The camera retains the game's vehicle aim direction and controls while its
right/up axes follow the chassis. This does not remove weapon traverse limits
or the game's aim limits. This does not add independent freelook or cockpit art.

Version 0.1.4 corrects upside-down entry latching when the game abruptly turns
the old on-foot view toward the new vehicle aim. Continuous chassis rotation
and normal local pole crossings remain supported. The entry fix needs a visual
check on the first boarding of helicopters, mechs and heavy tanks.

## Validation and limits

The x64 Release build and five automated suites pass. Tests cover the inherited
camera math and head materials, live-reference/seat validation, death/split-screen
fallback, occupied hull pass routing, nested NPC isolation, and restoration.
A real D3D11 WARP render test reads pixels back to prove 25% blending, opaque
non-vehicle rendering, both indexed draw paths, and blend/depth restoration.
All five game hook trampolines validate against a mapped executable without
executing it.

Bike first person and enclosed-hull fading were visually confirmed during
development. Live inspection informed Grape driver/passenger handling, crawler
orientation and Vegalta cockpit placement. Automated orientation tests include
large entry yaw/pitch changes that reproduced the upside-down horizon bug before
the 0.1.4 correction. The latest correction and the complete vehicle/class matrix
still need broader visual testing. See `docs/vehicle-evidence.md` for observations,
implementation evidence and remaining unknowns.

The fade draws the occupied hull late, with constant-factor alpha blending and
depth writes disabled; opaque vehicle draws are moved into the translucent
stage. Layering with other transparent effects can show sorting artifacts.
No wall collision sweep or aiming/parallax correction is added. Online and
split-screen use remain unsupported. If D3D fade setup fails, enclosed vehicles
retain third person. Check `EDF41FPSVehicles.log` for setup and transition details.

## Build

Windows x64, Visual Studio 2022 C++ tools, Windows SDK and CMake are required.
MinHook source/license is reused from `../FPS/third_party/minhook/`.

```powershell
cmake -S FPSVehicles -B FPSVehicles/build -A x64
cmake --build FPSVehicles/build --config Release
ctest --test-dir FPSVehicles/build -C Release --output-on-failure
```

DLL output: `FPSVehicles/build/Release/EDF41FPSVehicles.dll`.
