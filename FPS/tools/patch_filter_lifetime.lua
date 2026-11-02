local b=getAddress('EDF41FPS.dll')
local nt=b+readInteger(b+0x3c)
local first=nt+24+readSmallInteger(nt+20)
local target=b+0x10535 -- state.active in the byte-matched 0.1.1 build/map
local patches={}
for i=0,readSmallInteger(nt+6)-1 do
 local h=first+i*40
 if readString(h,8)=='.text' then
  local start=b+readInteger(h+12)
  local bytes=readBytes(start,readInteger(h+8),true)
  for j=1,#bytes-6 do
   local length=bytes[j]==0xc6 and bytes[j+1]==5 and bytes[j+6]==0 and 7 or
                bytes[j]==0x88 and bytes[j+1]==5 and 6 or 0
   if length>0 then
    local at=start+j-1
    local displacement=readInteger(at+2)
    if displacement>=0x80000000 then displacement=displacement-0x100000000 end
    if at+length+displacement==target then
     patches[#patches+1]={address=at,bytes=readBytes(at,length,true)}
    end
   end
  end
 end
end
assert(#patches==2,'Expected exactly two transient active resets, found '..#patches..'; refusing patch')
assert(not EDF41LifetimePatch,'Patch already installed')
pause()
local ok,err=pcall(function()
 for _,p in ipairs(patches) do
  local nops={} for i=1,#p.bytes do nops[i]=0x90 end
  assert(writeBytes(p.address,table.unpack(nops)))
 end
end)
if not ok then
 for _,p in ipairs(patches) do writeBytes(p.address,table.unpack(p.bytes)) end
end
unpause()
assert(ok,err)
EDF41LifetimePatch=patches
local lines={'Filter lifetime patch installed in current process only:'}
for _,p in ipairs(patches) do lines[#lines+1]=string.format('NOP active reset at %X',p.address) end
return table.concat(lines,'\n')
