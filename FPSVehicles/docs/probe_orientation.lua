local b=getAddress('EDF41.exe')
local scene=readQword(b+0xcc8528)
local c=readQword(readQword(scene+0x1d8)+8)
local a=readQword(c+0x268)
local v=readQword(a+0xf50)
local s=readQword(a+0xf58)
local lines={string.format('base=%X camera=%X actor=%X vehicle=%X seat=%X yaw=%g pitch=%g',b,c,a,v,s,readFloat(a+0xcb0),readFloat(a+0xcb4))}
local function mat(p,label)
 local rows={}
 for r=0,3 do local x=p+r*16;rows[#rows+1]=string.format('%.6f %.6f %.6f %.6f',readFloat(x),readFloat(x+4),readFloat(x+8),readFloat(x+12)) end
 lines[#lines+1]=label..' '..table.concat(rows,' / ')
end
if v and v~=0 then
 local col=readQword(readQword(v)-8)
 lines[#lines+1]='RTTI='..readString(b+readInteger(col+0xc)+16,90)
 mat(v+0x60,'vehicle')
 mat(s+0xf0,'rider')
 lines[#lines+1]=string.format('cameraType=%d angleAdjust=%g',readInteger(s+0x80),readFloat(s+0xb0))
 for _,off in ipairs({0x88,0x98}) do mat(readQword(s+off+8)+0xa0,'attachmentNode'..off) end
end
mat(a+0x60,'actor');mat(c+0x170,'camera')
local nodes=readQword(a+0x5d0)
for i=0,readQword(a+0x5e0)-1 do if readString(readQword(nodes+i*256),80,true)=='kubi' then mat(nodes+i*256+0xa0,'head');break end end
return table.concat(lines,'\n')
