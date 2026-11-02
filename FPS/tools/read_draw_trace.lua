local t=EDF41DrawTrace
assert(t,'No trace armed')
pcall(debug_removeBreakpoint,t.address)
return string.format('done=%s totalCalls=%d\n%s',tostring(t.done),t.calls,table.concat(t.localCalls,'\n'))
