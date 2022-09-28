#
#ident	"@(#)Makefile 1.11 96/02/12 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libc_psr/sparc/sun4u/Makefile
#
# Platform specific Makefile for libc_psr.
#
# PLATFORM is the target for the binary installation.
#
SRCDIR		= ../..
PLATFORM	= sun4u

#OBJECTS		= memcpy.o memset.o memcmp.o
OBJECTS		= memcpy.o memset.o memcmp.o hwmuldiv.o 

include		$(SRCDIR)/Makefile.com

MAPFILES=	mapfile-vers  $(MAPFILE-FLTR)
MAPOPTS=	$(MAPFILES:%=-M %)
DYNFLAGS +=	$(MAPOPTS)

ASFLAGS	+=	-xarch=v8plusa

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.
 
BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) $(LDLIBS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

$(DYNLIB):	$(MAPFILES)

.KEEP_STATE:

all:		$(LIBS)

install:	all $(USR_PSM_LIBS)

include		$(SRCDIR)/Makefile.targ
