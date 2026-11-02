"""Bounded, read-only CPK table/model name inspection for local research."""
import argparse, json, struct
from pathlib import Path


def utf(data):
    data = bytearray(data)
    if data[:4] != b'@UTF':
        key = 0x655f
        for i in range(len(data)):
            data[i] ^= key & 255
            key = key * 0x4115 & 0xffffffff
    if data[:4] != b'@UTF':
        raise ValueError('not UTF')
    _, size, version, ro, so, do, tn, nc, rw, nr = struct.unpack_from('>4sIHHIIIHHI', data)
    strings = 8 + so
    def string(off):
        start = strings + off
        return bytes(data[start:data.index(0, start)]).decode('utf-8', errors='replace')
    def val(pos, ty):
        fmts = {0:'B',1:'b',2:'H',3:'h',4:'I',5:'i',6:'Q',7:'q',8:'f',9:'d',10:'I',11:'II'}
        fmt = fmts[ty]
        result = struct.unpack_from('>'+fmt, data, pos)
        result = result[0] if len(result)==1 else list(result)
        if ty == 10: result = string(result)
        return result, pos + struct.calcsize(fmt)
    cols = []
    p = 32
    for _ in range(nc):
        flags = data[p]; name = string(struct.unpack_from('>I', data,p+1)[0]); p+=5
        value = None
        if flags & 0xf0 == 0x30: value,p=val(p,flags&15)
        cols.append((name,flags,value))
    rows=[]
    for row in range(nr):
        p=8+ro+row*rw; obj={}
        for name,flags,value in cols:
            if flags & 0xf0 == 0x50: value,p=val(p,flags&15)
            obj[name]=value
        rows.append(obj)
    return rows


def chunk(f, offset):
    f.seek(offset); header=f.read(16)
    size=struct.unpack_from('<Q',header,8)[0]
    if size > 32*1024*1024: raise ValueError('table too big')
    return utf(f.read(size))


def cmpl(data, rp=4078):
    if data[:4] != b'CMPL': return data
    target = struct.unpack_from('>I', data,4)[0]
    if target > 32*1024*1024: raise ValueError('CMPL too big')
    ring = bytearray(4096); out=bytearray(); p=8
    while len(out)<target:
        flags=data[p]; p+=1
        for bit in range(8):
            if len(out)>=target: break
            if flags & (1<<bit):
                b=data[p]; p+=1; out.append(b); ring[rp]=b; rp=(rp+1)&4095
            else:
                a,b=data[p:p+2]; p+=2
                at=(a<<4)|(b>>4); count=(b&15)+3
                for j in range(count):
                    v=ring[(at+j)&4095]; out.append(v); ring[rp]=v; rp=(rp+1)&4095
    if len(out)!=target: raise ValueError('CMPL size mismatch')
    return bytes(out)


if __name__ == '__main__':
    ap=argparse.ArgumentParser(); ap.add_argument('archive'); ap.add_argument('--filter',default=''); ap.add_argument('--output')
    args=ap.parse_args()
    with open(args.archive,'rb') as f:
        hdr=chunk(f,0)[0]
        toc=chunk(f,hdr['TocOffset'])
        rows=[r for r in toc if args.filter.lower() in (str(r.get('DirName',''))+'/'+str(r.get('FileName',''))).lower()]
        out={'archive':str(Path(args.archive).resolve()),'header':hdr,'rows':rows}
        result=json.dumps(out,ensure_ascii=False,indent=2)
        if args.output: Path(args.output).write_text(result,encoding='utf-8')
        else: print(result)
