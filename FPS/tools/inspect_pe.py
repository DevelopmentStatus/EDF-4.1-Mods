"""Read-only PE evidence for this EDF41 build; no third-party packages."""
import pathlib, struct, json, hashlib
p = pathlib.Path(r'A:\SteamLibrary\steamapps\common\Earth Defense Force 4.1\EDF41.exe')
b = p.read_bytes()
pe = struct.unpack_from('<I', b, 0x3c)[0]
n, = struct.unpack_from('<H', b, pe+6)
osize, = struct.unpack_from('<H', b, pe+20)
sections=[]
for i in range(n):
    o=pe+24+osize+i*40
    name=b[o:o+8].rstrip(b'\0').decode()
    vs,va,rs,ro=struct.unpack_from('<IIII',b,o+8)
    sections.append((name,va,vs,ro,rs))
def off(rva):
    for _,va,vs,ro,rs in sections:
        if va <= rva < va+rs: return ro+rva-va
    raise ValueError(hex(rva))
def rva(o):
    for _,va,vs,ro,rs in sections:
        if ro <= o < ro+rs: return va+o-ro
    raise ValueError(o)
out={'sha256':hashlib.sha256(b).hexdigest(),'sections':sections,'functions':{}}
for a in (0xdf230,0x56f530,0x322e40,0x5091b0,0xdeab0,0x50b000,0x50b070):
    out['functions'][hex(a)]=b[off(a):off(a)+32].hex(' ')
# An MSVC x64 vftable is preceded by its CompleteObjectLocator pointer.
name=b'.?AVCharacterGhostCamera@@\0'
o=b.find(name)
if o>=0:
    td=rva(o)-16
    cols=[]
    needle=struct.pack('<I',td)
    start=0
    while (j:=b.find(needle,start))>=0:
        start=j+1
        if j>=12 and struct.unpack_from('<I',b,j-12)[0]==1:
            col=rva(j-12)
            ptr=struct.pack('<Q',0x140000000+col)
            at=0
            while (k:=b.find(ptr,at))>=0:
                at=k+1
                if k%8==0:
                    vt=rva(k+8)
                    cols.append({'vtable_rva':hex(vt),'slots':[hex(x-0x140000000) for x in struct.unpack_from('<16Q',b,k+8)]})
    out['camera_vtables']=cols
print(json.dumps(out,indent=2))
