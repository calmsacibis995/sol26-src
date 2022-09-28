#
#ident	"@(#)Makefile.com	1.18	96/09/19 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

RTLD=		ld.so.1

BLTOBJ=		msg.o
OBJECTS=	$(BLTOBJ) \
		$(P_ASOBJS)   $(P_COMOBJS)   $(P_MACHOBJS)   $(G_MACHOBJS)  \
		$(S_ASOBJS)   $(S_COMOBJS)   $(S_MACHOBJS)

COMOBJS=	$(P_COMOBJS)  $(S_COMOBJS)
ASOBJS=		$(P_ASOBJS)   $(S_ASOBJS)
MACHOBJS=	$(P_MACHOBJS) $(S_MACHOBJS)

ADBGEN1=	/usr/lib/adb/adbgen1
ADBGEN3=	/usr/lib/adb/adbgen3
ADBGEN4=	/usr/lib/adb/adbgen4
ADBSUB=		/usr/lib/adb/adbsub.o

ADBSRCS=	fmap.adb	link_map.adb	rt_map.adb	permit.adb \
		fct.adb		pnode.adb	maps.adb	maps.next.adb \
		dyn.adb		dyn.next.adb

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers

ADBSCRIPTS=	$(ADBSRCS:%.adb=adb/%)
ROOTADB=	$(ADBSRCS:%.adb=$(SGSONLD)/lib/adb/%)

# A version of this library needs to be placed in /etc/lib to allow
# dlopen() functionality while in single-user mode.

ETCLIBDIR=	$(ROOT)/etc/lib
ETCDYNLIB=	$(RTLD:%=$(ETCLIBDIR)/%)

ROOTDYNLIB=	$(RTLD:%=$(ROOTLIBDIR)/%)

FILEMODE =	755

# Add -DPRF_RTLD to allow ld.so.1 to profile itself

CPPFLAGS=	-I. -I../common -I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/lib/libc/inc \
		-I$(SRCBASE)/uts/common/krtld \
		-I$(SRCBASE)/uts/$(ARCH)/krtld \
		$(CPPFLAGS.master)
ASFLAGS=	-P -D_ASM $(CPPFLAGS)
SONAME=		/usr/lib/$(RTLD)
DYNFLAGS +=	-e _rt_boot -Bsymbolic -M $(MAPFILE)
LDLIBS=		$(LDLIBS.lib) -L$(ROOT)/usr/lib/pics -lc_pic \
		-L ../../libld/$(MACH) -lld \
		-L ../../liblddbg/$(MACH) -llddbg \
		-L ../../librtld/$(MACH) -lrtld \
		-L ../../libconv/$(MACH) -lconv
BUILD.s=	$(AS) $(ASFLAGS) $< -o $@
LD=		$(SGSPROTO)/ld

BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/rtld

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n rtld_msg

# list of sources for lint
lint:=		LDLIBS = $(LDLIBS.lib) -lc \
		-L ../../libld/$(MACH) -lld \
		-L ../../liblddbg/$(MACH) -llddbg \
		-L ../../librtld/$(MACH) -lrtld

SRCS=		$(COMOBJS:%.o=../common/%.c)  $(MACHOBJS:%.o=%.c) $(BLTDATA) \
		$(G_MACHOBJS:%.o=$(SRCBASE)/uts/$(ARCH)/krtld/%.c) \
		$(ASOBJS:%.o=%.s)


LINTFLAGS +=	-Dsun $(LDLIBS)

CLEANFILES +=	$(LINTOUT)  $(CRTS)  $(BLTFILES)  $(ADBSCRIPTS)
CLOBBERFILES +=	$(RTLD)

.PARALLEL:	$(ADBSCRIPTS)
