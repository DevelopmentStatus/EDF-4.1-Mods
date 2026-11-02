"""Read-only MDB mesh/name metadata investigation; no model payload retained."""
import json,struct
from pathlib import Path
from cpk_probe import chunk,cmpl

def wide(d,p):
    e=p
    while e+2<=len(d) and d[e:e+2]!=b'\0\0':e+=2
    return d[p:e].decode('utf-16le')

out=[]
with Path(r'A:\SteamLibrary\steamapps\common\Earth Defense Force 4.1\Root.cpk').open('rb') as f:
    h=chunk(f,0)[0]
    for row in chunk(f,h['TocOffset']):
        if row['FileName'] not in {'ARMYANDENGINEER.MRAB','PALEWING.MRAB','HEAVYARMOR.MRAB'}:continue
        f.seek(row['FileOffset']+2048);rab=f.read(row['FileSize'])
        count,table=struct.unpack_from('<II',rab,20)
        for i in range(count):
            at=table+i*32;name=wide(rab,at+struct.unpack_from('<I',rab,at)[0])
            if not name.lower().endswith('.mdb'):continue
            size=struct.unpack_from('<I',rab,at+4)[0];off=struct.unpack_from('<I',rab,at+24)[0]
            d=cmpl(rab[off:off+size]);nn,nt=struct.unpack_from('<II',d,8)
            names=[wide(d,nt+j*4+struct.unpack_from('<I',d,nt+j*4)[0]) for j in range(nn)]
            mc,mt=struct.unpack_from('<II',d,24)
            matc,matt=struct.unpack_from('<II',d,32)
            mats=[names[struct.unpack_from('<i',d,matt+j*32+4)[0]] for j in range(matc)]
            meshes=[]
            for j in range(mc):
                at=mt+j*16;mid,node,nsub,suboff=struct.unpack_from('<4i',d,at)
                subs=[]
                for k in range(nsub):
                    rec=struct.unpack_from('<10i',d,at+suboff+k*40)
                    subs.append({'index':k,'materialIndex':rec[1],'material':mats[rec[1]] if 0<=rec[1]<len(mats) else None,
                                 'vertexCount':rec[5],'indexCount':rec[8]})
                meshes.append({'meshIndex':j,'nodeIndex':node,'nodeName':names[node],'submeshes':subs})
            out.append({'model':name,'size':len(d),'materials':mats,'meshes':meshes})
path=Path(__file__).resolve().parents[1]/'docs'/'head-mesh-name-probe.json'
path.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf8')
for m in out: print(m['model'],m['meshes'])
