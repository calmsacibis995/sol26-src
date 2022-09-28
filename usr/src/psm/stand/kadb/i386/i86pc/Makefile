#
#ident "@(#)Makefile	1.4     96/03/22 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
# psm/stand/kadb/i386/i86pc/Makefile
# was psm/stand/kadb/i386/Makefile up to version 1.9
#
# a kernel-architecture-specific kadb Makefile
#

TOPDIR =	../../../../..

include $(TOPDIR)/Makefile.master

# override global macros as necessary
TARGET_ARCH =	i386
ARCH =		i86pc
MACH =		i386
CSMACH =	i386
KSUN =		i86
PROMTYPE=	kadb
PROMLIB=	libprom.a
LIBSA-i386=	libsadb.a
PLATLIB=

COMDIR=  	../../common
STANDDIR=  	$(TOPDIR)/stand/$(MACH)
SYSDIR=  	$(TOPDIR)/uts
PSMSTANDLIBDIR= ../../../lib

INCLUDE=  -I$(SYSDIR)/$(KSUN) -I$(SYSDIR)/$(ARCH)

BOOTCFLAGS="BOOTCFLAGS=-DKADB -D_KADB"

ARCHOPTS=
VOBJ=		tables.o intr.o
VOBJ1 =		ttrap.o
SPECIAL=	locore.o

ARCHOBJ= $(SPECIAL) machdep.o prom.o readfile_$(MACH).o \
	support_$(MACH).o stubs_$(MACH).o fiximp.o $(VOBJ) $(VOBJ1)

MACH_CSDIRS=    $(SYSDIR)/i86/promif

# include common kadb definitions and initial targets
include $(COMDIR)/Makefile.com

$(VOBJ): $$(VOBJ:%.o=%.s)
	$(AS) -P -o $@ $<
 
$(VOBJ1): $$(VOBJ1:%.o=%.s)
	$(AS) -P -o $@ $<

$(ARCHOBJ): assym.s 

assym.s: genassym 
	./genassym > $@ 

genassym: genassym.c
	$(CC) $(CFLAGS) $(CPPINCS) -o $@ genassym.c

