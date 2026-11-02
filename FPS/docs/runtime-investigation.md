# Runtime head-clipping investigation — 2026-11-01

Target: the user's running `EDF41.exe`, PID 29904, module base
`0x7ff7a2250000`. Attached through the supplied headless Cheat Engine bridge in
`A:\gameassets\ReverseEngineeringPackage`. These are live observations obtained
through that bridge, not REA Evidence records. Addresses are session-specific.

## Findings

1. The loaded plugin was 0.1.1, and its log recorded suppression of Wing Diver's
   `eye`, `head-rev`, `head`, `hair`, and `met_translucent` materials. The issue was
   therefore not an unresolved material name or missing hair-bone descendant.
2. A live snapshot confirmed `kubi` index 51 and its hair descendants. Base and
   both LOD draw models shared the expected skeleton and palette.
3. Setting the plugin's live HeadScale from 0.001 to 1 left the obstruction visible,
   as reported by the user. Bone shrinking was not the cause of the surviving geometry.
4. Bounded, automatically resumed hardware breakpoints captured the local model
   component at RVA 0x322e40. The actor was rendered in passes 0, 1, 2 and 3.
   `state.active` was 1 only in pass 3. In the other passes it was 0, making the
   entire material filter scope bypassed. `runtime-filter-state.json` records this.
5. `restoreCamera` cleared the visibility flag along with the camera override;
   `prepareHook` also cleared it for every auxiliary camera preparation. This
   incorrectly tied visibility lifetime to the short camera-matrix override lifetime.

## Live correction and validation

Applied a reversible, session-only patch inside the loaded plugin: removed the
unconditional visibility resets while preserving the `overridden` reset needed
for normal simulation-camera restoration. The compiler had combined one reset
into a 16-bit write clearing both adjacent flags, so the initial patch was
incomplete. The final patch changed that write to clear only `overridden`.
Original bytes were retained in Cheat Engine's `EDF41LifetimePatch` table.

After correction, `runtime-filter-state-fixed.json` records `active=1` in all four
passes. Restored HeadScale to its original 0.001 value. The user then confirmed:
**“Yes, they are gone”**, referring to the hair and helmet obstructions in the
running mission.

The permanent 0.1.2 source fix separates the two lifetimes: restoring the camera
does not disable visibility, unrelated camera preparations preserve it, and a
failed primary-camera preparation clears it. Every body draw still independently
checks the live on-foot/vehicle/death/F6/split-screen eligibility conditions.
No game executable, archive, material resource, or save file was patched on disk.

## Checks and scope

- All three CTest suites pass, including new regressions for camera restoration,
  auxiliary preparations and primary-camera fallback.
- Five hook trampolines validate against the installed game binary.
- Material fixtures still cover 102 cases from all 12 class/LOD models.
- Wing Diver was observed and visually confirmed live. Other classes use the
  same corrected lifetime and their existing verified material sets; their
  visual appearance has not been checked live in this investigation.
- Temporary breakpoints were bounded and removed. HeadScale was restored.
  The live visibility correction remains until the running game exits; 0.1.2
  supplies the permanent behavior for future launches.

The Lua scripts in `tools/` with hard-coded 0.1.1 offsets are research probes for
this exact loaded build, not general-purpose scripts for later plugin versions.
