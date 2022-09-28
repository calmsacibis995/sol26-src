#
#ident	"@(#)Makefile	1.8	96/05/17 SMI"
#
# Copyright (c) 1994,1995 by Sun Microsystems, Inc.
# All rights reserved.
#

TOPDIR =	../../../../..
PLATFORM =	prep
KARCH =		prep
#ARCHVERS =	
PROMVERS =	ieee1275
ARCHOPTS =	-D__ppc -Dppc
#ASFLAGS +=	

PLAT_C_SRC =	machdep.c
ARCH_C_SRC =
ARCH_S_SRC =	
SRT0_S =	srt0.s
ISRT0_S =	inet_srt0.s
INLINES =

#
# The following libraries are build in LIBPLAT_DIR
#
LIBPLAT_DIR =	$(TOPDIR)/psm/stand/lib/promif/$(MACH)/$(PROMVERS)/$(PLATFORM)
LIBPLAT_LIBS =	libplat.a
LIBPLAT_L_LIBS=	$(LIBPLAT_LIBS:lib%.a=llib-l%.ln)
LIBPLAT_DEP =	$(LIBPLAT_DIR)/$(LIBPLAT_LIBS)
LIBPLAT_DEP_L =	$(LIBPLAT_DIR)/$(LIBPLAT_L_LIBS)

#
# Platform specific libraries
#
PSMLIBS +=	$(LIBPLAT_LIBS:lib%.a=-l%)
PSMLIB_DIRS +=	$(LIBPLAT_DIR)

include ../Makefile.com
