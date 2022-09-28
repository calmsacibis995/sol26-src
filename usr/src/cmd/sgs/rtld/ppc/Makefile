#
#ident	"@(#)Makefile	1.33	96/03/15 SMI"
#
#	Copyright (c) 1996 by Sun Microsystems, Inc.
#	All rights reserved.

# Object lists are organized into primary (most frequently used code) and
# secondary lists (less frequently used code).

P_COMOBJS=	analyze.o	elf.o		globals.o	malloc.o \
		mutex.o		paths.o		setup.o		util.o \
		dlfcns.o	cache_elf.o	locale.o

S_COMOBJS=	debug.o		profile.o	object.o

G_MACHOBJS=	doreloc.o

P_MACHOBJS=	ppc_elf.o	_setup.o

S_MACHOBJS=

P_ASOBJS=	boot.o		boot_elf.o	caller.o	zero.o

S_ASOBJS=	note.o

include		$(SRC)/cmd/sgs/rtld/Makefile.com

# Add any machine specific flags.

ADBGENFLAGS +=	-erroff=%all

pics/_setup.o := CFLAGS += -Wc,-on,gotless_switch_table


.KEEP_STATE:

all:		$(RTLD)

install:	all $(ROOTDYNLIB) $(ETCDYNLIB)

adbmacros:	adb .WAIT $(ADBSCRIPTS)

adbinstall:	adbmacros .WAIT $(ROOTADB)

include		$(SRC)/cmd/sgs/rtld/Makefile.targ
