# EDF41FPSVehicles v0.1.4 — experimental

A separate vehicle-enabled FPS variant. The original EDF41FPS release remains
available for players who want the on-foot camera and normal vehicle views.

- Animated head camera and visible body on foot, with head/helmet/hair hiding.
- First-person bikes with an opaque bike model.
- Enclosed first-person vehicles with configurable hull opacity (default 25%).
- Vehicle-relative rotation and eye offsets for walls, ceilings and crawler flips.
- Mech cockpit-height anchors with adjustable up/forward offsets.
- Grape passengers retain their native first-person camera and opaque hull.
- C/F6 or hold L3 for 700 ms toggles perspective in eligible seats and on foot.
- Corrects the orientation-history bug that could latch the first entry upside
  down after a large boarding camera turn.

## Install

Requires Steam EDF 4.1 PC x64 and
[EDFModLoader](https://github.com/BlueAmulet/EDFModLoader).

1. Close the game. Back up/remove `Mods/Plugins/EDF41FPS.dll` if installed.
2. Extract the ZIP into the folder containing `EDF41.exe`, merging `Mods`.
3. Verify `EDF41FPSVehicles.dll` and `EDF41FPSVehicles.ini` are in `Mods/Plugins`.

**Use either EDF41FPS or EDF41FPSVehicles, never both.** Keep the original FPS
INI if you may switch back. Future vehicle-variant updates can replace only the
DLL to retain settings. No game archives or saves are modified.

## Validation and limitations

Release x64 builds successfully; all five test suites and all five mapped game
hook trampoline checks pass. Tests include D3D11 pixel blending, camera/state
restoration, full chassis rotations, cockpit placement and entry-orientation
regressions. The newest entry correction still needs broader in-game testing.

Solo offline use only. Online and split-screen are unsupported. Hull transparency
can show sorting artifacts, cockpit placement varies by model, and close walls
or weapons may intersect the view. Native aiming/traverse limits still apply.

Supported executable SHA-256:
`39b4abf7fe722175741a5e0a154063b27ef8c589a1aa5f86c84853571ac7bda8`.

The attached Nexus description TXT is ready to paste into the separate listing.
