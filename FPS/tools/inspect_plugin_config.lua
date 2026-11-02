local b=getAddress('EDF41FPS.dll')
local nt=b+readInteger(b+0x3c)
local n=readSmallInteger(nt+6)
local first=nt+24+readSmallInteger(nt+20)
local matches={}
local pattern=string.pack('<ffffff',0.025,0.15,0.20,20,75,12)
for i=0,n-1 do
 local h=first+i*40
 local name=readString(h,8)
 if name=='.data' then
  local start=b+readInteger(h+12)
  local length=readInteger(h+8)
  local bytes=readBytes(start,length,true)
  local pieces={}
  for j=1,#bytes do pieces[j]=string.char(bytes[j]) end
  local s=table.concat(pieces)
  local pos=1
  while true do
   local at=string.find(s,pattern,pos,true)
   if not at then break end
   local p=start+at-1
   matches[#matches+1]=string.format('floatSettings=%X flags=%d,%d,%d scaleAddress=%X scale=%g near=%g fov=%g',p,readBytes(p-4,1),readBytes(p-3,1),readBytes(p-2,1),p+24,readFloat(p+24),readFloat(p+28),readFloat(p+32))
   pos=at+1
  end
 end
end
return table.concat(matches,'\n')
