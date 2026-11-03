local b=getAddress('EDF41.exe'); local scene=readQword(b+0xcc8528)
local c=readQword(readQword(scene+0x1d8)+8);local a=readQword(c+0x268);local v=readQword(a+0xf50);local s=readQword(a+0xf58)
local lines={}
local nodes=readQword(v+0xb80);local count=readQword(v+0xb90)
lines[#lines+1]=string.format('nodes=%X count=%d',nodes,count)
for i=0,math.min(count-1,150) do
 local n=nodes+i*256
 lines[#lines+1]=string.format('bone[%d] %s parent=%d world=%.4f %.4f %.4f',i,readString(readQword(n),90,true) or '?',readInteger(n+0x10),readFloat(n+0xd0),readFloat(n+0xd4),readFloat(n+0xd8))
end
for _,off in ipairs({0x70,0x88,0x98}) do
 local d=readQword(s+off);local n=readQword(s+off+8);local p=d+readInteger(d+0xc)
 lines[#lines+1]=string.format('attachment[%X] type=%d node=%s local=%.4f %.4f %.4f %.4f',off,readInteger(d+8),readString(readQword(n),90,true),readFloat(p),readFloat(p+4),readFloat(p+8),readFloat(p+12))
end
return table.concat(lines,'\n')
