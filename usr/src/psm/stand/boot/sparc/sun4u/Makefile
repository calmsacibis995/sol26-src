#
#ident	"@(#)Makefile	1.20	96/10/15 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#

TOPDIR =	../../../../..
PLATFORM =	sun4u
ARCHVERS =	v9
ARCHMMU  =	sfmmu
PROMVERS =	ieee1275
ARCHOPTS =	-D__sparcv9cpu
ASFLAGS +=	-xarch=v8plus

PLAT_C_SRC =	machdep.c
ARCH_C_SRC =	sun4u_memlist.c sun4x_standalloc.c
ARCH_S_SRC =	sparcv9_subr.s
SRT0_S =	sun4u_srt0.s
INLINES =

LDFLAGS +=	-L$(TOPDIR)/psm/stand/lib/promif/$(MACH)/$(PROMVERS)/common

#
# The following libraries are build in LIBPLAT_DIR
#
LIBPLAT_DIR =	$(TOPDIR)/psm/stand/lib/promif/$(MACH)/$(PROMVERS)/$(PLATFORM)
LIBPLAT_LIBS =  libplat.a
LIBPLAT_L_LIBS=	$(LIBPLAT_LIBS:lib%.a=llib-l%.ln)
LIBPLAT_DEP = 	$(LIBPLAT_DIR)/$(LIBPLAT_LIBS)
LIBPLAT_DEP_L =	$(LIBPLAT_DIR)/$(LIBPLAT_L_LIBS)

#
# Platform specific libraries
#
PSMLIBS +=      $(LIBPLAT_LIBS:lib%.a=-l%)
PSMLIB_DIRS +=  $(LIBPLAT_DIR)


#	MPSAS support
MPSAS_BUILD:sh=		echo \\043
$(MPSAS_BUILD)ARCHOPTS += -DMPSAS

include ../Makefile.com

# re-define ELFCONV for 4u--- sun4u can boot ELF directly
ELFCONV = /usr/bin/cp

# Don't need mapfile.inet for 4u inetboot
NFS_MAPFILE = $(MACH_DIR)/mapfile
