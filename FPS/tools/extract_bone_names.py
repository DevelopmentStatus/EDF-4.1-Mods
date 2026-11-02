"""Extract only model naming evidence from four local player archives."""
import hashlib,json,struct
from pathlib import Path
from cpk_probe import chunk,cmpl

archive=Path(r'A:\SteamLibrary\steamapps\common\Earth Defense Force 4.1\Root.cpk')
selected={'ARMYANDENGINEER.MRAB','PALEWING.MRAB','HEAVYARMOR.MRAB','ENGINEER.RAB'}
results=[]
def wide(d,p):
    end=p
    while d[end:end+2]!=b'\0\0':end+=2
    return d[p:end].decode('utf-16le')
with archive.open('rb') as f:
    header=chunk(f,0)[0]; rows=chunk(f,header['TocOffset'])
    for row in rows:
        if row['FileName'] not in selected:continue
        f.seek(row['FileOffset']+2048); rab=f.read(row['FileSize'])
        assert rab[:4]==b'SSA\0'
        count,table=struct.unpack_from('<II',rab,20)
        for i in range(count):
            at=table+i*32
            name=wide(rab,at+struct.unpack_from('<I',rab,at)[0])
            if not name.lower().endswith('.mdb'):continue
            size=struct.unpack_from('<I',rab,at+4)[0]
            off=struct.unpack_from('<I',rab,at+24)[0]
            d=cmpl(rab[off:off+size]); assert d[:4]==b'MDB0'
            nn,nt,nb,bt=struct.unpack_from('<IIII',d,8)
            names=[wide(d,nt+j*4+struct.unpack_from('<I',d,nt+j*4)[0]) for j in range(nn)]
            records=[struct.unpack_from('<8i',d,bt+j*192) for j in range(nb)]
            bones=[{'index':j,'name':names[v[0]],'parent':v[1],
                    'nextSibling':v[2],'firstChild':v[3]} for j,v in enumerate(records)]
            results.append({'cpkMember':row['DirName']+'/'+row['FileName'],'model':name,
                'modelSha256':hashlib.sha256(d).hexdigest(),'boneTableCount':nb,'bones':bones})
out=Path(__file__).resolve().parents[1]/'docs'/'player-bone-names.json'
out.write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
for item in results:
    print(item['cpkMember'],item['model'],item['boneTableCount'])
    print([b for b in item['bones'] if b['name'] in ['kubi','Head']])
