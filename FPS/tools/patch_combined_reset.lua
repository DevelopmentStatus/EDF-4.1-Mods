assert(EDF41LifetimePatch,'Initial lifetime patch is not installed')
local b=getAddress('EDF41FPS.dll')
local at=b+0x4bac
local before=readBytes(at,9,true)
assert(before[1]==0x66 and before[2]==0xc7 and before[3]==5 and before[8]==0 and before[9]==0,'Unexpected combined flag-reset instruction')
local d=readInteger(at+3)
if d>=0x80000000 then d=d-0x100000000 end
local target=at+9+d
assert(target==b+0x10534,'Combined reset does not target state.overridden/active')
-- Replace word=0 with overridden-byte=0; preserve active for visible passes.
local nd=target-(at+7)
local disp={string.byte(string.pack('<i4',nd),1,4)}
pause()
local ok,err=pcall(function()
 assert(writeBytes(at,0xc6,5,disp[1],disp[2],disp[3],disp[4],0,0x90,0x90))
end)
if not ok then writeBytes(at,table.unpack(before)) end
unpause()
assert(ok,err)
EDF41LifetimePatch[#EDF41LifetimePatch+1]={address=at,bytes=before}
return string.format('Combined flag reset corrected at %X; visibility now persists while overridden resets normally',at)
