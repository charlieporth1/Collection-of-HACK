nte.EXE : nte.OBJ blowfish.OBJ md5.OBJ vms.OBJ
	$(LINK) $(LINKFLAGS) /EXECUTABLE = $@ $+

blowfish.OBJ : blowfish.c blowfish.h

md5.OBJ : md5.c md5.h

nte.OBJ : nte.c blowfish.h md5.h

vms.OBJ : vms.c

clean :
	if (f$search( "blowfish.OBJ") .nes "") then -
         delete /log blowfish.OBJ;*
	if (f$search( "md5.OBJ") .nes "") then -
         delete /log md5.OBJ;*
	if (f$search( "nte.OBJ") .nes "") then -
         delete /log nte.OBJ;*
	if (f$search( "vms.OBJ") .nes "") then -
         delete /log vms.OBJ;*
	if (f$search( "nte.EXE") .nes "") then -
         delete /log nte.EXE;*
