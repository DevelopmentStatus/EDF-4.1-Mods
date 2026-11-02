local b=getAddress('EDF41.exe')
local scene=readQword(b+0xcc8528)
local ctrl=readQword(scene+0x1d8)
local camera=readQword(ctrl+8)
local actor=readQword(camera+0x268)
local component=actor+0x550
local nodes=readQword(actor+0x5d0)
local count=readQword(actor+0x5e0)
local lines={}
local function add(s,...) lines[#lines+1]=string.format(s,...) end
add('base=%X scene=%X camera=%X actor=%X component=%X nodes=%X count=%d',b,scene,camera,actor,component,nodes,count)
add('cameraState=%X vehicle=%X seat=%X',readQword(camera+0xac0),readQword(actor+0xf50),readQword(actor+0xf58))
for _,off in ipairs({0x170,0x60}) do
 add('camera[%X]=%.4f %.4f %.4f',off,readFloat(camera+off+48),readFloat(camera+off+52),readFloat(camera+off+56))
end
for i=0,count-1 do
 local name=readString(readQword(nodes+i*256),100,true)
 if name=='kubi' or name=='Head' or string.find(name or '','hair') then
  add('node[%d] %s parent=%d pos=%.4f %.4f %.4f',i,name,readInteger(nodes+i*256+16),readFloat(nodes+i*256+208),readFloat(nodes+i*256+212),readFloat(nodes+i*256+216))
 end
end
local function drawModel(p,label)
 local sk=readQword(p+0x58)
 add('%s=%X skeleton=%X palette=%X groups=%d',label,p,sk,readQword(sk+0x30),readQword(p+0x50))
end
drawModel(component+0x130,'baseDraw')
local lod=readQword(component+0x220)
local lodCount=readQword(component+0x230)
add('LODs=%d distance=%.4f',lodCount,readFloat(component+0x1c))
for i=0,lodCount-1 do drawModel(lod+i*0xf0,'lod'..i) end
return table.concat(lines,'\n')
