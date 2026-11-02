# Head, helmet and hair draw groups

Offline observations from the installed game resources and the saved Ghidra
export used in `re-notes.md`. The reported clipping screenshots motivate this
fix; these observations do not themselves establish a successful live fix.

`tools/inspect_head_mesh_names.py` reads `Root.cpk`, decompresses the three class
archives in memory, and records all 12 main/LOD MDBs in
`head-mesh-name-probe.json`. It retains metadata, not model payloads.

| Class | Mesh grouping | Head material names |
|---|---|---|
| Ranger | One `ArmySoldier` group mixes body and head | `rikusen-face`, `met`, `met_Translucent` |
| Air Raider | One `Engineer` group mixes body and head | `rikusen-face`, `met`, `met_Translucent` |
| Fencer | One `HeavyArmor` group mixes body and head | `head3`, `eye` |
| Wing Diver | Separate `Palewing` body and `Head` group | `met_Translucent`, `head-rev`, `eye`, `hair`, `head` |

The material names agree across all three LODs. Wing Diver's LOD2 omits the
separate eye submesh. Her `Head` group is node index 2, outside the `kubi`
skeletal subtree. Therefore group-level Head visibility covers Wing Diver but
cannot cover the other classes without also removing their bodies.

## Runtime material name path

`FUN_14050c150` iterates grouped dynamic submesh records, with `plVar2` pointing
at one dynamic submesh. The underlying source submesh is `*plVar2` and its
material pointer is source-submesh `+0x38` (also read in `FUN_1405091b0`).

`FUN_140517070` allocates material records at stride `0x68` and calls
`FUN_140516920` for each raw MDB material record (stride `0x20`). That constructor
reads the raw material name index at raw-material `+4`, resolves the model name
table at model-resource `+0x28` with 8-byte pointer entries, and stores the result
in runtime-material `+0x18` (`param_1[3]`). The raw string table is UTF-16LE.

Thus the lookup path is:

```text
dynamicSubmesh -> [0] sourceSubmesh -> [+0x38] material -> [+0x18] name pointer
```

Filtering these exact head-only materials within an own-player render scope can
skip helmet, hair, eyes, face and reverse surfaces without depending on skinning
weights or the skeleton parent relationship. The scope must end immediately
after that player's draw to preserve other characters and third-person views.

Group visibility itself is also established: `FUN_140509370` initializes the
mesh-group suppression byte at `+0`; `FUN_14050c150` skips the whole group when
that byte is `1`. Group resource record is group `+0x28` and its bone/node index
is record `+4`, used for rigid transforms. Do not suppress a mixed body/head
group for Ranger, Air Raider or Fencer.
