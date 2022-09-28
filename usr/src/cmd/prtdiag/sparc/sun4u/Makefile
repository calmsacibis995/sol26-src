#
#ident	"@(#)Makefile	1.12	95/03/13 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
# cmd/prtdiag/sparc/sun4u/Makefile
#
# Platform specific Makefile for the prtdiag command.
#
# PLATFORM is the target for the binary installation.
#
# PLATLINKS is a list of platforms which are to be linked to this binary
#	at the file level.

SRCDIR		= ../..
PLATFORM	= sun4u
PLATLINKS	=
KARCHOBJS	= kstat.o pdevinfo_sun4u.o display_sun4u.o

include $(SRCDIR)/Makefile.com

LDLIBS		+= -lkstat

.KEEP_STATE:

all:	$(PROG)

include $(SRCDIR)/Makefile.targ
