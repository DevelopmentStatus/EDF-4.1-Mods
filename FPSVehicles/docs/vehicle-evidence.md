# Vehicle implementation evidence

Target hash and build guard are identical to the original FPS mod. REA
`open_binary` verified the installed executable identity during this task.
Its Ghidra provider was unavailable (`GHIDRA_INSTALL_DIR` unset); new static
observations below come from the existing matching Ghidra C export at:

`A:\gameassets\ReverseEngineeringPackage\workspace\EDF41-39b4abf7fe72\20261101T200735260858Z\source`

No native REA analysis Evidence IDs or in-game execution are claimed for that
decompilation. The inherited evidence is in `../../FPS/docs/re-notes.md`.

## Static observations

- `0x29de20`: stores VehicleBase at actor+0xf50, seat+0xf58 and weak control+0xf60.
- `0xe0200`: vehicle camera callback, uses seat+0x88 and +0x98 for camera settings.
- `0x2e6fd0`: creates the vehicle AnimationModel at vehicle+0xb00.
- `0x2e7820` and `0x2cd7a0`: seat array at +0x328, count +0x338, stride 0x180;
  first field is owner vehicle, exit attachment at seat+0x60; rider attachment at +0x70, DispCharacter
  byte at +0xd4. These records also contain camera attachments and references.
- `0x3232d0`: attachment stores SGO data pointer and node pointer. Data+8 is type,
  data+0xc is a signed relative offset to a homogeneous local translation.
  Composes the local matrix with node+0xa0. The variant needs its translation
  only and evaluates it using guarded reads without calling game code.
- `0x322e40` / `0x50c150`: model LOD selection and part submission. Pass 2 selects
  the separate +0xc8 part list and disables depth writes; other color draws use
  +0xa8. Passes 0/3 select depth/shadow shader variants via `0x50f500`.
- `0x5091b0`: commits renderer state and invokes context vtable+0x60 or +0xa0.
  `0x531830` invokes +0x118/+0x120 (OM blend/depth state). These signatures/slots
  identify ID3D11DeviceContext at renderContext+8. DrawIndexed and
  DrawIndexedInstanced detours therefore override state after the game's commit.
- RTTI names were independently read from the installed PE. VehicleBike is the
  explicit opaque open vehicle; other supported concrete Vehicle classes are
  enclosed implementation targets. Unknown types fail closed.

## Live observations (2026-11-02 local date)

The user supplied a paused solo mission seated on a bike. Started the package's
headless Cheat Engine and attached read-only to EDF41.exe PID 31944. The initial
read used `../../FPS/tools/snapshot_runtime.lua`; the seat snapshot uses the
adjacent `probe_vehicle.lua`. Raw results are `runtime-bike-initial.json` and
`runtime-bike-seat.json`. These are bridge observations, not REA Evidence records.

- Module base 0x7ff6cebc0000; callback is base+0xe0200.
- RTTI is exactly `.?AVVehicleBike@@`.
- Actor vehicle handle points back to the same vehicle; seat owner matches.
- Seat is the first element of a two-element seat array; DispCharacter=1.
- Rider `kubi` is node 45; its world position is approximately
  (419.414, 1.889, 311.100), distinct from the third-person camera position.
- Bike model is vehicle+0xb00; 12 bones at the expected node array offsets.
- The exit attachment at +0x60 is type 0; camera attachments are type 2; homogeneous
  translation and node world matrix match the recovered layout.

No game memory writes, injected hooks, breakpoints or gameplay inputs were used
for these observations. The user's current session still runs its original mod.

## Design choices and outstanding checks

Open vehicles use the animated character head and existing body/head filter.
Version 0.1.1 enclosed vehicles use the animated head as well. Keep
game aim orientation. Scope fading to the exact occupied vehicle's model;
revalidate the weak reference/seat for each draw. Defer opaque hull submission
to pass 2, skip its depth prepass, preserve pass 3 shadow submission. These
rendering choices require visual testing in the actual game.

The D3D11 WARP test executes the real detours, reads RGBA pixels back and verifies
25% red over blue (approximately 64,0,191), opaque red outside the fade scope,
indexed/instanced parity and restoration of original OM blend/depth state.
Synthetic game records test seat membership, unknown RTTI, stale references,
dead vehicles, split-screen, toggles, camera restore and nested rendering.

Outstanding: new DLL visual test on bike; enclosed vehicle eye placement,
transparency and draw order; different classes/seats; entering/exiting,
destroyed vehicles, restart; shadows, transparent effects and weapon aiming.

## 0.1.1 driver placement and native Grape passengers

The user confirmed 0.1.0 worked for bikes and enclosed vehicle fading, but the
camera was displaced from the seated character. Screenshots show a visible
passenger viewed from an offset position and a transparent Grape driver hull.
The initial interpretation of seat+0x60 as the rider position was incorrect.
`0x2e9640` uses +0x60 on exit; `0x2e8310` evaluates +0x70 into the rider world
matrix at seat+0xf0. Runtime data confirms seat+0xf0 matches the actor root.

Read-only CE snapshots from PID 10904 are `runtime-enclosed-before.json`,
`runtime-enclosed-head-before.json` and `runtime-grape-seats.json`. The current
vehicle is VehicleStriker (Grape), with five seats. Seat 0 has DispCharacter=0
and cameraType=1; seats 1..4 have DispCharacter=1 and cameraType=0. The user
reports those passengers already have native first-person view. The Grape
passenger bypass therefore checks the concrete VehicleStriker RTTI and seat
index >0 after validating membership and live references; its driver stays
eligible. The bypass disables all mod camera/body/hull overrides for passengers
without changing the global toggle, so the mod resumes on return to the driver.

The driver's actor root was (462.173,0.542,385.539) and animated kubi was
(462.246,1.987,385.600). The old exit local translation was (2.2,0,2.4), explaining
the lateral displacement. Version 0.1.1 uses the animated head and inherited
eye offsets for all eligible seats. It preserves the game's driver visibility:
the mod hides only head parts if the body is actually submitted. It does not
force an otherwise hidden driver model to appear.

Regression checks now distinguish the head from a deliberately distant exit
attachment, cover Grape driver/passenger transitions and default-camera
restoration, and retain the existing rendering/GPU pixel checks. Four suites
pass. The 0.1.1 camera correction still needs an in-game visual check.

## 0.1.2 vehicle-relative orientation

The user reported improved 0.1.1 driver placement and requested continuous
vehicle-relative rotation for the wall/ceiling crawler. Read-only Cheat Engine
snapshot `runtime-crawler-orientation-before.json` records PID 33300,
Vehicle502_GroundRobo on a steep surface. Its chassis up was approximately
(0.391,-0.146,-0.909), while the native camera up was (-0.093,0.995,-0.044).
The actor and seat rider basis matched the chassis. This confirms the
world-level camera horizon is independent from the valid chassis orientation.

The new render-only helper normalizes the complete chassis basis, expresses the
native aim direction in that frame, and constructs camera right/up in the
vehicle frame. Previous right is stored in vehicle-local coordinates, selecting
the continuous branch through local vertical aim poles instead of a 180-degree
horizon inversion. It does not extract Euler roll, use world-up sign tests, or
change native aiming/weapon controls. The original simulation matrix is saved
before changing orientation, then restored by the existing update path.
EyeUp now follows chassis up and EyeForward lies in the chassis tangent plane;
on-foot behavior and Grape native passenger bypass remain unchanged.
`VehicleRelativeOrientation=0` restores the old orientation/eye-offset behavior.

A dedicated suite sweeps chassis roll, pitch and yaw from -720 through +720
degrees, local pitch/yaw look through two complete loops, checks aim preservation,
continuous axes, handedness, finite/rotated eye offsets, malformed transforms,
and scaled bases. Hook-level tests check wall rotation, restoring the unmodified
simulation camera, and the opt-out setting. All five suites pass. This remains
synthetic validation; the 0.1.2 DLL still needs a crawler visual playtest.

## 0.1.3 mech cockpit anchor

The user reported the mech camera was below its cockpit. Read-only CE snapshots
from PID 7872, `runtime-mech-orientation-before.json` and
`runtime-mech-cockpit-before.json`, identify VehicleBegaruta/Vegalta. Its hidden
infantry head was at Y=1.936, the vehicle aim_center/camera attachment node at
Y=4.400, and the vehicle head joint at Y=4.738. Seat camera and target attachments
both reference aim_center. The third-person SGO camera translation is
(0,4,-7.4); using that translation would recreate the unwanted chase camera.

For Begaruta, FortressRobo and GroundRobo, 0.1.3 instead uses the world position
of the camera attachment's model node at seat+0x90, with independent cockpit
up/forward offsets. Node membership/alignment/count, finite position and bounded
distance from the chassis are checked. Invalid/root nodes fall back to the
known pilot head. Other vehicle classes retain their existing anchors.
Vehicle-relative orientation and eye-offset rotation remain in effect.

Hook-level tests distinguish the high camera node from the hidden pilot and
third-person attachment, test the separate forward offset, invalid-node fallback,
configuration opt-out, and isolation from ordinary tanks. All five suites pass.
The 0.1.3 cockpit anchor needs visual testing; only Vegalta node placement has
been inspected live, and other supported mech classes remain unverified.

## 0.1.4 upside-down first-entry regression

The user reports the view can be upside down on first entry into helicopters,
mechs and gigatanks, resolving on exit/re-entry. A live read was attempted in
PID 7872 but no scene was active, so no failing runtime basis was obtained.
The cause below is supported by source analysis and a synthetic reproduction,
not a captured vehicle-entry trace.

The 0.1.3 orientation continuity rule compares consecutive vehicle-local right
axes directly. Preparing a view facing behind the vehicle, then abruptly
switching to forward aim, causes that rule to invert right/up and latch the
inverted horizon. A regression test failed against 0.1.3 for this exact sequence.
Additional pitched entry tests exposed the same failure for mixed yaw/pitch.

Version 0.1.4 stores previous local aim as well as right. For continuous changes
it transports the previous right by the shortest aim rotation before selecting
the horizon branch. A discontinuity (consecutive aim cosine <=0.98, roughly
11.5 degrees per preparation) is re-seeded from chassis up, since it cannot be
assumed to represent a smooth passage through a vertical pole. Near-antipodal
pose changes use the upright yaw interpretation. Chassis roll itself does not
trigger this rule, because aim history is stored in vehicle-local coordinates.
Very abrupt local pitch input through a pole is likewise treated as a new pose;
normal continuous vertical aim tests and full chassis loops remain passing.

Regression coverage includes entry yaw changes of +/-100, +/-170, +/-180 degrees,
with initial pitches +/-45 and +/-80; the pre-fix test failure was observed.
All five suites and mapped game-hook trampoline checks pass. Visual validation
of first boarding on the reported vehicle types is still outstanding.
