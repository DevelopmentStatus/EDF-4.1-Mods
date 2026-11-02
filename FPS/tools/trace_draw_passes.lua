local b=getAddress('EDF41.exe')
local scene=readQword(b+0xcc8528)
local camera=readQword(readQword(scene+0x1d8)+8)
local actor=readQword(camera+0x268)
local plugin=getAddress('EDF41FPS.dll')
local state=plugin+0x100c0
EDF41DrawTrace={address=b+0x322e40,actor=actor,calls=0,localCalls={},done=false}
assert(debugProcess(1)~=false,'Native debugger attachment failed')
debug_setBreakpoint(EDF41DrawTrace.address,1,bptExecute,bpmDebugRegister,function()
 local t=EDF41DrawTrace
 t.calls=t.calls+1
 if RCX==t.actor+0x550 then
  t.localCalls[#t.localCalls+1]=string.format('local component=%X pass=%d return=%X active=%d overridden=%d stateActor=%X camera=%X',RCX,R8,readQword(RSP),readBytes(state+0x475,1),readBytes(state+0x474,1),readQword(state+8),readQword(state))
 end
 if #t.localCalls>=12 or t.calls>=2500 then
  debug_removeBreakpoint(t.address)
  t.done=true
 end
 debug_continueFromBreakpoint(co_run)
 return 1
end)
-- A watchdog ensures the high-frequency probe cannot remain installed.
local timer=createTimer(nil)
timer.Interval=4000
timer.OnTimer=function(t)
 pcall(debug_removeBreakpoint,EDF41DrawTrace.address)
 EDF41DrawTrace.done=true
 t.destroy()
end
return 'Bounded draw-pass probe armed; auto-removes after 12 local calls, 2500 total calls, or four seconds'
