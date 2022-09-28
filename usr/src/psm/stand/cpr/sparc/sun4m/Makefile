#
#ident	"@(#)Makefile	1.11	96/06/12 SMI"
#
# Copyright (c) 1995 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/cpr/sparc/sun4m/Makefile
#

TOPDIR=../../../../..

# override global macros as necessary
#TARGET_ARCH =	sun4u
TARGET_ARCH =	sun4c
ARCH =		sun4m
MACH=		sparc
KSUN= 		sun
MMU=		srmmu
ARCHVER =	v7
ARCH_PROMDIR=	sparc/obp/common
PROMTYPE=	obp
PROMLIB=	libprom.a
PLATLIB=
PLATLIBDIR=
PLATLDLIBS=
EXTRA_TARGET=
EXTRA_LINT=
MAPFILE=	mapfile
MAP_FLAG=	 -e start -Bstatic

ARCHOPTS=	-DMP

CPRBOOTOBJ=	locore.o cpr.o cpr_sun4m.o module.o fixmmu_sun4m.o \
		module_asm.o sadiv.o stubs.o samuldiv64.o

# include common kadb definitions and initial targets
include ../../common/Makefile.com

.KEEP_STATE:

STANDDIR= $(TOPDIR)/stand
CSDIR	= .
CSDIRS	=	$(STANDDIR)/lib $(STANDDIR)/sys ../../../../promif \
		$(TOPDIR)/uts/common/cpr/cpr_compress.c \
		$(TOPDIR)/uts/common/sys/cpr.h \
		$(TOPDIR)/uts/sun4m/sys/cpr_impl.h \
		$(TOPDIR)/psm/stand/cpr/common \
		.
		
CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)
CSINCS	= $(CSPATHS:%=-I%)
CSCOPE	= cscope
CTAGS	= ctags

cscope.out: cscope.files FRC
	${CSCOPE} -b -f `pwd`/$@

cscope.files:   FRC
	@$(RM) -f cscope.files
	$(ECHO) $(CSINCS) > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    \( -type d -name '.del*' \) -prune -o -type f \
	    \( -name '*.[chs]' -o -name 'Makefile*' -o -name '*.il' \) \
	    -a ! -name '.del*' -print >> cscope.files
	@$(WC) -l cscope.files

#
# Create a tags data base, similar to above.
# Since assembler files now contain C fragments for lint,
# the lint fragments will allow ctags to "work" on assembler.
#
# Things to note:
#	1. We put plain files before headers, and .c before .s (because
#	   ctags doesn't understand assembly comments).
#	2. We *don't* sort the output of find, as we want functions in
#	   architecture directories to take precedence over those in
#	   sun, and those over those in common.
#
tags: cscope.files
	$(CTAGS) -wt `sed 1d cscope.files`
