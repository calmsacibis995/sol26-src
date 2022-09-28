#
#ident	"@(#)Makefile	1.40	96/03/15 SMI"
#
#	Copyright (c) 1996 by Sun Microsystems, Inc.
#	All rights reserved.

# Object lists are organized into primary (most frequently used code) and
# secondary lists (less frequently used code, ie. a.out support).

P_COMOBJS=	analyze.o	elf.o		globals.o	malloc.o \
		mutex.o		paths.o		setup.o		util.o \
		dlfcns.o	cache_elf.o	locale.o

S_COMOBJS=	debug.o		profile.o	object.o	a.out.o \
		cache_a.out.o

G_MACHOBJS=	doreloc.o

P_MACHOBJS=	sparc_elf.o	_setup.o

S_MACHOBJS=	sparc_a.out.o

P_ASOBJS=	boot.o		boot_elf.o	caller.o	zero.o

S_ASOBJS=	boot_a.out.o	note.o

CRTI=		pics/crti.o
CRTN=		pics/crtn.o
CRTS=		$(CRTI)		$(CRTN)

include		$(SRC)/cmd/sgs/rtld/Makefile.com

# Add any a.out and machine specific flags.

CPPFLAGS +=	-DA_OUT
CFLAGS +=	-xregs=no%appl
ASFLAGS +=	-K pic
LINTFLAGS +=	-DA_OUT
ADBGENFLAGS +=	-erroff=%all


.KEEP_STATE:

all:		$(RTLD)

install:	all $(ROOTDYNLIB) $(ETCDYNLIB)

adbmacros:	adb .WAIT $(ADBSCRIPTS)

adbinstall:	adbmacros .WAIT $(ROOTADB)

include		$(SRC)/cmd/sgs/rtld/Makefile.targ
