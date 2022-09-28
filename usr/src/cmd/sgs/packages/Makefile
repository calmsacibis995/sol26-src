#
#ident	"@(#)Makefile	1.25	96/09/10 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/packages/Makefile

include		$(SRC)/Makefile.master

SUBDIRS= 	../libdl \
		../libelf \
		../liblddbg \
		../librtld_db \
		.WAIT \
		../librtld \
		../libld \
		.WAIT \
		../rtld \
		../rtld.4.x \
		../libldstab \
		../libldmake/$(MACH) \
		../0@0 \
		../gprof \
		../tools \
		../librtld_db/rdb_demo/$(MACH)

INTLDIRS=	../ar \
		../ld \
		../ldd \
		../mcs \
		../pvs

$(SUBDIRS) :=	TARGET = install
$(INTLDIRS) :=	TARGET = package

../%:		FRC
		@ cd $@; pwd; $(MAKE) $(TARGET)

.KEEP_STATE:
.PARALLEL:	$(SUBDIRS) $(INTLDIRS)

all clean clobber install lint: FRC
		@ cd $(MACH); pwd; $(MAKE) $@

# 
# invoke `package' to build this package from scratch.  It will 
# perform make install in the directories that contains components 
# that this package is dependent upon 
# 
package:	package_dep .WAIT install

package_dep:	package_dep_ .WAIT $(SUBDIRS) $(INTLDIRS)

package_dep_:	FRC
		@ cd ../tools; pwd; $(MAKE) proto && ./proto
		@ cd $(SRC)/head; pwd; $(MAKE) \
			$(ROOT)/usr/include/dlfcn.h \
			$(ROOT)/usr/include/libelf.h \
			$(ROOT)/usr/include/proc_service.h \
			$(ROOT)/usr/include/rtld_db.h \
			$(ROOT)/usr/include/link.h
		@ cd $(SRC)/uts/common/sys; pwd; $(MAKE) \
			$(ROOT)/usr/include/sys/elf.h \
			$(ROOT)/usr/include/sys/elf_386.h \
			$(ROOT)/usr/include/sys/elf_M32.h \
			$(ROOT)/usr/include/sys/elf_SPARC.h \
			$(ROOT)/usr/include/sys/elf_ppc.h \
			$(ROOT)/usr/include/sys/elf_notes.h \
			$(ROOT)/usr/include/sys/elftypes.h \
			$(ROOT)/usr/include/sys/auxv.h \
			$(ROOT)/usr/include/sys/link.h \
			$(ROOT)/usr/include/sys/note.h \
			$(ROOT)/usr/include/sys/systeminfo.h
		@ cd ../tools/$(MACH); pwd; $(MAKE) sgsmsg jive
		@ cd ..; pwd; $(MAKE) _msg
		@ cd ..; pwd; $(MAKE) native-add
		@ cd ../rtld/$(MACH); pwd; $(MAKE) adbinstall
		@ cd ../messages/$(MACH); pwd; $(MAKE) test

FRC:
