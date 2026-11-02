# EDF 4.1 first-person implementation evidence

Target: x64 Steam `EDF41.exe`, SHA-256
`39b4abf7fe722175741a5e0a154063b27ef8c589a1aa5f86c84853571ac7bda8`.
Image base `0x140000000`; values below are RVAs. Runtime addresses add the actual
module base, accounting for ASLR. Hash validation and hook prologue checks fail closed.

## Evidence origin and scope

REA `open_binary` verified the path/architecture/hash. Native REA analysis was
unavailable because its Ghidra provider has no `GHIDRA_INSTALL_DIR` configured.
No REA analysis Evidence IDs are claimed for the observations below. They come
from the existing local Ghidra C export and independent read-only PE inspection:

`A:\gameassets\ReverseEngineeringPackage\workspace\EDF41-39b4abf7fe72\20261101T200735260858Z\source`

The source filenames correspond to static virtual addresses, e.g.
`fn_1400df230.c`. `pe-evidence.json` records observed bytes and the camera vtable
found by following the MSVC RTTI type descriptor and CompleteObjectLocator in
the installed executable. Static decompilation is evidence of code structure,
not evidence of in-game execution.

## Camera

| RVA / field | Observation |
|---|---|
| `0xa91e18` | CharacterGhostCamera vtable, recovered from RTTI |
| `0xdf230` | Character camera update; imports actor camera settings, runs state machine and shake |
| `0x56f530` | Builds rendering/culling camera data from matrix +0x170 and FOV +0x14 |
| camera `+0x170` | World camera matrix; translation +0x1a0 |
| camera `+0x14`, `+0x18` | Vertical FOV radians and near plane |
| camera `+0x260` | Weak actor control block |
| camera `+0x268` | Cached SoldierBase pointer, cast in constructor 0xddf40 |
| camera `+0xac0` | StateMachine callback; 0xdf5b0 is the normal on-foot update |
| `0xdeab0` | Camera destructor; clears cached mod state before destruction |
| `0xcc8528` | Scene/system global pointer |
| scene `+0x1d8`, `+0x430` | First and second camera weak references; stride 600, shown by 0x566b80 |

0x566b80 calls camera preparation before scene drawing and calls the camera's
update virtual at the later camera update stage. The plugin modifies the render
camera before preparation and restores the original matrix/FOV/near plane before
the next game camera update, avoiding feedback into third-person interpolation.
Repeated preparations restore before reapplying. Destructor cleanup prevents
carrying a pending restoration into a reused camera allocation.

Right-handed camera forward is the negative third matrix row. This is inferred
from the camera construction/look-at math and renderer convention and still
needs a visual pitch/yaw test. The nominal FOV constant at RVA 0xab65e4 is
0.785398185 radians; first-person FOV preserves the game's relative zoom through
the ratio of half-angle tangents.

## Vehicle gating

0x29de20 dynamically casts the entry target to VehicleBase and stores vehicle
pointer at SoldierBase+0xf50, seat at +0xf58, and weak reference at +0xf60.
0x29df40 clears all three on exit. 0xe0200 is the vehicle camera state.
The plugin requires normal on-foot state, no vehicle/seat/live vehicle reference,
a live actor reference, positive health (+0x16c), and the primary camera without
an active second camera. Any failed condition keeps the game camera.

## Skeleton and drawing

| Field / RVA | Observation |
|---|---|
| actor `+0x550` | Model component (0x29a6a0 calls 0x3225e0 on it) |
| actor `+0x5c0` | Skeleton container (weapon lookup in 0x2af900) |
| skeleton `+0x10`, `+0x20` | Node array and u64 node count |
| node stride `0x100` | Node name pointer +0; parent int +0x10 |
| node `+0x60`, `+0xa0` | Local and world matrices |
| skeleton `+0x30` | World-matrix palette pointer, actor+0x5f0 |
| `0x50b210` | Name lookup confirms array/name/stride layout |
| `0x50aed0` | Composes child local matrices with parent world matrices |
| `0x29bb80` | Copies actor render transform into root, composes descendants |
| `0x50b000` | Copies node world matrices into palette |
| `0x322e40` | Draws component; selects LOD and submits skinning/rigid meshes |
| `0x50b070` | Multiplies node+0x20 inferred inverse-bind matrix by palette |
| `0x50c150` | Rigid attachments consume the same palette |

0x320910 passes component+0x70 to both the base model and all LOD model
initializers, establishing their shared skeleton.

The draw hook only swaps the local actor's palette pointer. A scratch palette
scales the head subtree's 3x3 basis and contracts each descendant's translation
around the head pivot. Parent walks are count-bounded, handling invalid indices
and cycles. The original node matrices and animation/physics data are untouched.
An isolated SEH `__finally` restores the pointer after the synchronous original
draw. Bone matrices are world-space, so head translation is used directly.

### 0.1.1 head material filtering

The reported 0.1.0 screenshots show geometry intersecting the camera despite
successful head-bone resolution. The subtree-based palette change was therefore
insufficient. Wing Diver has a separate Head mesh; Ranger, Air Raider and Fencer
combine head/body geometry in one mesh group. The update hooks RVA `0x5091b0`
(submesh draw) and skips exact head-only materials while the local actor's
component draw is on the stack. It reads the name through dynamic-submesh `[0]`
source-submesh `+0x38` material `+0x18` name pointer. This mapping is established
by the source-submesh consumer and material constructors at 0x516920/0x517070.

The filter covers visor, helmet, face, eyes, reverse head surfaces and hair across
all 12 class/LOD models. A thread-local actor scope ends in the same SEH finally
block that restores the palette. Nested non-local component draws clear and
restore that scope, so shared NPC model resources keep their heads. No mesh,
material or model-resource visibility flags are changed. See
`head-mesh-hiding-evidence.md` and `head-mesh-name-probe.json` for the exact sets.
Synthetic render tests verify the real pointer lookup and restoration. The
subsequent live investigation found a render-pass lifetime bug and corrected it
in 0.1.2; see `runtime-investigation.md` for measured before/after traces and the
user's visual confirmation.

## Remaining runtime unknowns

- Resource inspection found `kubi` in all four classes: ArmySoldier index 45,
  Engineer index 45, HeavyArmor index 9, PaleWing index 51. `player-bone-names.json`
  records decoded RAB/MDB names and hierarchy from the game CPKs. PaleWing also
  has a non-skeletal mesh node named `Head`, so `kubi` takes priority in auto
  matching. The configurable default explicitly selects `kubi`.
- A live check must confirm head/helmet disappearance, body visibility and pitch
  sign for each class, and transitions through ragdoll/vehicles/mission restart.
- No render-context camera identity has been established. The prototype therefore
  supports only the primary viewport and conservatively disables for camera 2.
- Short first-person offsets have no separate collision sweep. Muzzle/reticle
  parallax, reflections/shadows and weapon clipping need playtesting.
- Compatibility with other plugins replacing the same camera/render routines is
  unverified. The adjacent MenuMouse plugin targets different functions.
