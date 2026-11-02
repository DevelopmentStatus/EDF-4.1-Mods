local b=getAddress('EDF41FPS.dll')
local target=b+0x10535
local lines={}
local nt=b+readInteger(b+0x3c)
local first=nt+24+readSmallInteger(nt+20)
for i=0,readSmallInteger(nt+6)-1 do
 local h=first+i*40
 if readString(h,8)=='.text' then
  local start=b+readInteger(h+12)
  local size=readInteger(h+8)
  local bytes=readBytes(start,size,true)
  for j=1,#bytes-8 do
   if (bytes[j]==0xc6 or bytes[j]==0x66 or bytes[j]==0xc7 or bytes[j]==0x88 or bytes[j]==0x89) then
    for k=1,5 do
     local addr=start+j-1+k
     local d=readInteger(addr)
     if d and d>=0x80000000 then d=d-0x100000000 end
     if d then
      local delta=addr+4+d-target
      if delta>=-1 and delta<=0 then
       local at=start+j-1
       lines[#lines+1]=string.format('%X delta=%d %s',at,delta,disassemble(at))
      end
     end
    end
   end
  end
 end
end
return table.concat(lines,'\n')
