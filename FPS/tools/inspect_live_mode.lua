local b=getAddress('EDF41.exe')
local p=getAddress('EDF41FPS.dll')
local state=p+0x100c0
local camera=readQword(state)
local actor=readQword(state+8)
local out={}
local values={enabled=readBytes(p+0x10070,1),hide=readBytes(p+0x10072,1),active=readBytes(state+0x475,1),overridden=readBytes(state+0x474,1),camera=camera,actor=actor,scale=readFloat(p+0x1008c)}
if actor and actor~=0 then values.HP=readFloat(actor+0x16c);values.vehicle=readQword(actor+0xf50);values.seat=readQword(actor+0xf58) end
if camera and camera~=0 then values.cameraState=readQword(camera+0xac0) end
for k,v in pairs(values) do out[#out+1]=k..'='..tostring(v) end
return table.concat(out,' ')
