#
#ident	"@(#)Makefile	1.9	96/09/12 SMI"
#
# Copyright (c) 1995 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/cpr/sparc/sun4u/Makefile
#

TOPDIR=../../../../..

# override global macros as necessary
#TARGET_ARCH =	sun4u
TARGET_ARCH =	sun4c
ARCH =		sun4u
MACH=		sparc
KSUN= 		sun
MMU=		sfmmu
ARCHVER =	v9
EXTRA_ASM_FLAG=	-xarch=v8plus
PROMTYPE=	ieee1275
ARCH_PROMDIR=	$(MACH)/$(PROMTYPE)/common
PLATLIBDIR =	$(TOPDIR)/psm/stand/lib/promif/$(MACH)/$(PROMTYPE)/$(ARCH)
PLATLIB=	$(PLATLIBDIR)/libplat.a
PLATLDLIBS=	-L$(PLATLIBDIR) -lplat
PROMLIB=	libprom.a
MAPFILE=	mapfile.cprboot

EXTRA_TARGET=	cprbooter
EXTRA_LINT=	$(EXTRA_TARGET)_lint
EXTRA_MAPFILE=	mapfile.cprbooter

#
# XXX	-DVAC is provided by machparam.h .. but clearly this
#	isn't going to work too well on a PAC sun4u ..
#
ARCHOPTS= -DMP -D__sparcv9cpu

#ARCHOBJ= cprbooter.o cpr_readfile.o hwmuldiv.o srt0.o \
#	cprboot_srt0.o cprboot.o

CPRBOOTOBJ= cprboot.o hwmuldiv.o cprboot_srt0.o stubs.o samuldiv64.o

CPRBOOTEROBJ= cprbooter.o cpr_readfile.o hwmuldiv.o srt0.o stubs.o samuldiv64.o

# include common kadb definitions and initial targets
include ../../common/Makefile.com

.KEEP_STATE:

STANDDIR= $(TOPDIR)/stand
CSDIR	= .
CSDIRS	=	$(STANDDIR)/lib $(STANDDIR)/sys ../../../../promif \
		$(TOPDIR)/uts/common/cpr/cpr_compress.c \
		$(TOPDIR)/uts/common/sys/cpr.h \
		$(TOPDIR)/uts/sun4u/sys/cpr_impl.h \
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
