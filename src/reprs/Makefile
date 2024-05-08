# this makefile must be run from the MoarVM submodule
include Makefile

REPROBJECTS = ../../src/reprs/hamt-node.o

hamtrepr: ${REPROBJECTS}
	echo done

libmhamt.a: $(REPROBJECTS) 
	$(MSG) linking $@
	$(CMD)$(AR) $(ARFLAGS) $@ $(OBJECTS) $(NOOUT)

libhamt.dylib: $(REPROBJECTS) 
	$(MSG) linking $@
	$(CMD)$(LD) -o $@ $(LDFLAGS) -dynamiclib -install_name "/Users/cog/mine/Persistent-HAMT/3rdparty/MoarVM/install/lib/libmoar.dylib" $(OBJECTS) $(DLL_LIBS)
