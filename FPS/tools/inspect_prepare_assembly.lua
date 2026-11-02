local b=getAddress('EDF41FPS.dll')
local lines={}
for _,rva in ipairs({0x33b0,0x4b50}) do
 local a=b+rva
 lines[#lines+1]=string.format('Function %X',a)
 for i=1,65 do
  lines[#lines+1]=disassemble(a)
  a=a+getInstructionSize(a)
 end
end
return table.concat(lines,'\n')
