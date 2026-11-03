local b=getAddress('EDF41.exe')
local scene=readQword(b+0xcc8528)
local c=readQword(readQword(scene+0x1d8)+8)
local a=readQword(c+0x268)
local v=readQword(a+0xf50)
local s=readQword(v+0x328)
local lines={}
for i=0,readQword(v+0x338)-1 do
 local p=s+i*0x180
 lines[#lines+1]=string.format('seat[%d] addr=%X visibleRider=%d cameraType=%d occupant=%X world=%.3f %.3f %.3f',i,p,readBytes(p+0xd4,1),readInteger(p+0x80),readQword(p+0xd8) or 0,readFloat(p+0xf0+48),readFloat(p+0xf0+52),readFloat(p+0xf0+56))
end
return table.concat(lines,'\n')
