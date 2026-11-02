if EDF41DrawTrace then pcall(debug_removeBreakpoint,EDF41DrawTrace.address) end
-- The on-disk DLL has been replaced; resolve the already-captured runtime
-- address rather than asking the symbol loader to inspect the new file layout.
local scale=readFloat(0x7ffb95a3008c)
local ok,result=pcall(detachIfPossible)
assert(ok,'Debugger detach failed: '..tostring(result))
return 'Temporary render breakpoints removed; native debugger detached. Last captured head-scale address now reads '..tostring(scale)..'. Confirmed session-only visibility fix retained until game exit.'
