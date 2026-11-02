local t=EDF41ScaleProbe
assert(t,'No head scale probe to restore')
assert(readFloat(t.address)==1,'Scale changed since the probe; refusing to overwrite it')
assert(writeFloat(t.address,t.original))
EDF41ScaleProbe=nil
return 'Original HeadScale restored; the corrected live visibility patch remains active'
