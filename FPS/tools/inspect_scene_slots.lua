local b=getAddress('EDF41.exe')
local scene=readQword(b+0xcc8528)
local lines={string.format('pid=%d base=%X scene=%s',getOpenedProcessID(),b,tostring(scene))}
if scene then
 for i=0,3 do
  local ctrl=readQword(scene+0x1d8+i*600)
  local camera=ctrl and ctrl~=0 and readQword(ctrl+8) or 0
  lines[#lines+1]=string.format('slot=%d ctrl=%s camera=%s',i,tostring(ctrl),tostring(camera))
 end
end
lines[#lines+1]='scale='..tostring(readFloat(getAddress('EDF41FPS.dll')+0x1008c))
return table.concat(lines,'\n')
