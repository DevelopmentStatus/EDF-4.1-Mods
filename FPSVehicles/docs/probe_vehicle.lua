local b=getAddress('EDF41.exe')
local scene=readQword(b+0xcc8528)
local camera=readQword(readQword(scene+0x1d8)+8)
local actor=readQword(camera+0x268)
local vehicle=readQword(actor+0xf50)
local seat=readQword(actor+0xf58)
local vt=readQword(vehicle)
local col=readQword(vt-8)
local name=readString(b+readInteger(col+0xc)+16,90)
local lines={string.format('base=%X camera=%X actor=%X vehicle=%X seat=%X RTTI=%s',b,camera,actor,vehicle,seat,name)}
local function add(s,...) lines[#lines+1]=string.format(s,...) end
add('vehicleHP=%.3f actorHP=%.3f vehicleCtrl=%X cached=%X seatOwner=%X seats=%X count=%d DispCharacter=%d',readFloat(vehicle+0x16c),readFloat(actor+0x16c),readQword(actor+0xf60),readQword(readQword(actor+0xf60)+8),readQword(seat),readQword(vehicle+0x328),readQword(vehicle+0x338),readBytes(seat+0xd4,1))
local function mat(p,label)
 local m=readBytes(p,64,true)
 add('%s[%X] pos=%.4f %.4f %.4f',label,p,readFloat(p+48),readFloat(p+52),readFloat(p+56))
end
mat(actor+0x60,'actor');mat(vehicle+0x60,'vehicle')
for _,off in ipairs({0x60,0x88,0x98}) do
 local data=readQword(seat+off)
 local node=readQword(seat+off+8)
 add('attachment[%X] data=%X node=%X type=%d',off,data,node,readInteger(data+8))
 local p=data+readInteger(data+0xc)
 add('local=%.4f %.4f %.4f %.4f',readFloat(p),readFloat(p+4),readFloat(p+8),readFloat(p+12))
 mat(node+0xa0,'attachmentWorld')
end
local nodes=readQword(vehicle+0xb80)
local count=readQword(vehicle+0xb90)
add('vehicle model=%X nodes=%X bones=%d',vehicle+0xb00,nodes,count)
for i=0,math.min(count-1,90) do
 local p=nodes+i*0x100
 local name=readString(readQword(p),80,true)
 if name and (string.find(name:lower(),'seat') or string.find(name:lower(),'ride') or string.find(name:lower(),'camera') or i<5) then add('bone[%d]=%s',i,name) end
end
return table.concat(lines,'\n')
