local p=getAddress('EDF41FPS.dll')+0x1008c
assert(math.abs(readFloat(p)-0.001)<0.00001,'Live setting no longer matches the captured plugin; refusing write')
EDF41ScaleProbe={address=p,original=readFloat(p)}
assert(writeFloat(p,1))
return string.format('Temporary headScale=1 at %X; material filtering remains enabled',p)
