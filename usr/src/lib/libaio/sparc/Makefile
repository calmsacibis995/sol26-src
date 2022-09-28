#
#ident	"@(#)Makefile	1.15	96/10/08 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# libaio/sparc/Makefile

LIBRARY=	libaio.a
VERS=		.1

COMDIR=		../common

INCLIST=	-I. -Iinc -I.. -I$(COMDIR) -Iinc -I../../libc/inc

COMOBJS=	aio.o posix_aio.o close.o fork.o sig.o subr.o ma.o 

OBJECTS=	$(COMOBJS) $(STD_MACHOBJS) 

include		../../Makefile.lib

MAPFILES=	../common/mapfile-vers  mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

pics/%.o:	./%.s
		$(BUILD.s)
		$(POST_PROCESS_O)

pics/%.o:	 ../common/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)

objs/%.o:	./%.s
		$(BUILD.s)
		$(POST_PROCESS_O)

objs/%.o:	 ../common/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)

LIBS=		$(DYNLIB)
CPPFLAGS=	$(INCLIST) $(CPPFLAGS.master) -D_LARGEFILE64_SOURCE
ASFLAGS=	-K pic -P -D_SYS_SYS_S -D_LOCORE -D_ASM -DPIC -DLOCORE $(CPPFLAGS)
DYNFLAGS +=	$(MAPOPTS)
ZDEFS=
BUILD.s=	$(AS) $(ASFLAGS) $< -o $@

$(ROOTLIBDIR)/$(DYNLIB):=	FILEMODE = 755


# list of sources for lint
SRCS=		$(COMOBJS:%.o=../common/%.c)  $(MACHOBJS:.o=.c)
LINTFLAGS +=	-Dsun -DDEBUG
CLEANFILES +=	$(LINTOUT)

$(DYNLIB):	$(MAPFILES)

.KEEP_STATE:

all:		$(DYNLIB)

install:	$(ROOTLINKS) $(DYNLIB)

include ../../Makefile.targ

lint:		$(LINTOUT)

$(LINTOUT):	$(SRCS)
		$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1
