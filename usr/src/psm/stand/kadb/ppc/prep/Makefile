#
#ident "@(#)Makefile	1.6     95/03/21 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
# psm/stand/kadb/ppc/prep/Makefile
# was psm/stand/kadb/i386/Makefile up to version 1.9
#
# a kernel-architecture-specific kadb Makefile
#

TOPDIR =	../../../../..
SYSDIR =	../../../../../uts
COMDIR=  	../../common
PSMSTANDLIBDIR= ../../../lib

include $(TOPDIR)/Makefile.master

# override global macros as necessary
TARGET_ARCH =	prep
KARCH =		prep
ARCH =		prep
MACH =		ppc
KSUN =		ppc
PROMTYPE=	ieee1275
PROMLIB=	libprom.a
PLATLIB=	../../../lib/promif/ppc/ieee1275/prep/libplat.a

BOOTCFLAGS="BOOTCFLAGS=-DKADB -D_KADB"

ARCHOPTS=
SPECIAL=	locore.o

ARCHOBJ= $(SPECIAL) machdep.o readfile_$(MACH).o support_$(MACH).o \
	stubs_$(MACH).o fiximp.o

INCLUDE=  -I$(SYSDIR)/$(ARCH) -I$(SYSDIR)/$(MACH)

# include common kadb definitions and initial targets
include $(COMDIR)/Makefile.com
