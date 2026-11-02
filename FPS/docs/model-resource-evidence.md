# Player head naming from local game resources

Offline observations from the installed `Root.cpk`, not runtime verification.
The research scripts only read game files and retain naming/hierarchy metadata;
they do not retain extracted model binaries or modify archives.

| Class | CPK member / model | Head/neck bone | Bone index |
|---|---|---|---|
| Ranger | `OBJECT/ARMYANDENGINEER.MRAB` / `ArmySoldier.mdb` | `kubi` | 45 |
| Air Raider | `OBJECT/ARMYANDENGINEER.MRAB` / `Engineer.mdb` | `kubi` | 45 |
| Fencer | `OBJECT/HEAVYARMOR.MRAB` / `HeavyArmor.mdb` | `kubi` | 9 |
| Wing Diver | `OBJECT/PALEWING.MRAB` / `PaleWing.mdb` | `kubi` | 51 |

All three LODs have the same `kubi` index within their class. The separate
`OBJECT/ENGINEER.RAB` agrees with the shared archive. The JSON includes names,
parent/child/sibling indices, counts, and SHA-256 hashes of all 13 decompressed
MDBs inspected.

Wing Diver also has a node literally named `Head` at index 2. Its parent is the
`Palewing` model node, outside the skeleton rooted at `globalSRT`. Therefore
`kubi` must take precedence over a generic `Head` alias for the camera anchor.
`kubi` is the neck/head attachment point; a small upward eye offset is a design
choice and still needs visual calibration in-game.

Reproduce from the workspace root:

```powershell
python FPS/tools/extract_bone_names.py
```

Parsing details: the CPK `@UTF` tables are encrypted with initial XOR key
`0x655f`, multiplier `0x4115`; this archive's TOC member offsets use a 2048-byte
base. RAB/MRAB members have `SSA\0` headers and individual `CMPL` entries.
CMPL uses a 4096-byte ring initialized to zero, write index 4078, LSB-first
literal flags, match index `(a << 4) | (b >> 4)`, and length `(b & 15) + 3`.
MDB0 v20 string table offsets are relative to each table entry; bone records
are 192 bytes and store name index, parent, sibling, and first child in their
first four 32-bit values.

This establishes naming and hierarchy in shipped model resources. It does not
establish visual camera height, runtime animation timing, near clipping, aiming
alignment, split-screen behavior, or the final appearance of scaled geometry.
