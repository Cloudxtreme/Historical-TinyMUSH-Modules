# Generated automatically from Makefile.inc.in by configure.
# Generic Makefile fragment for a module with one .c file
#
# Module source files
compile_S = modules/compile/compile.c

MOD_S += $(compile_S)
MOD_O += modules/compile/compile.o
MOD_LA += modules/compile/compile.la
MOD_DL += -dlopen ../game/modules/compile.la

depend: depend-compile

depend-compile: unsplit $(compile_S) modules/compile/Makefile.inc
	for i in $(compile_S); do \
		$(CPP) $(ALLCFLAGS) $(CPP_DEP) $$i || :; \
	done | sed -e 's:/usr[^ ]* *::g' \
		   -e 's:modules/compile/../../::g' \
		   -e 's:^\([^/]*\:\):modules/compile/\1:' \
	     | ./unsplit > modules/compile/.depend~
	mv modules/compile/.depend~ modules/compile/.depend
	touch depend-compile

modules/compile/compile.o: modules/compile/compile.c
	$(LIBTOOL) --mode=compile $(CC) $(ALLCFLAGS) -o $@ -c `echo $@ | sed -e 's/o$$/c/'`

modules/compile/compile.la: modules/compile/compile.o
	$(LIBTOOL) --mode=link $(CC) -module -o $@ `echo $@ | sed -e 's/a$$/o/'` -rpath `pwd`/../game/modules

install-modules: install-compile

install-compile: modules/compile/compile.la
	-mv ../game/modules/compile.* ../game/modules/libcompile.* ../game/modules/old/
	$(LIBTOOL) --mode=install cp modules/compile/compile.la `pwd`/../game/modules/compile.la
	touch install-compile

clean: clean-compile

clean-compile:
	-rm -f modules/compile/*.o modules/compile/*.lo modules/compile/*.la
	-rm -rf modules/compile/.libs

distclean: distclean-compile

distclean-compile:
	-rm -f modules/compile/config.status modules/compile/config.log
	-rm -f depend-compile install-compile ../game/modules/compile.* ../game/modules/libcompile.* ../game/modules/old/compile.* ../game/modules/old/libcompile.*
	-rm -f modules/compile/Makefile.inc modules/compile/.depend
	touch modules/compile/.depend

include modules/compile/.depend
