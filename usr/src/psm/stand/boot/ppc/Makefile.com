#
#ident	"@(#)Makefile.com	1.21	96/06/26 SMI"
#
# Copyright (c) 1995, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/boot/ppc/Makefile.com

include $(TOPDIR)/psm/stand/boot/Makefile.boot

BOOTSRCDIR	= ../..

CMN_DIR		= $(BOOTSRCDIR)/common
MACH_DIR	= ../common
PLAT_DIR	= .

CONF_SRC	= tmpnfsconf.c fsconf.c hsfsconf.c

CMN_C_SRC	= boot.c heap_kmem.c readfile.c

MACH_C_SRC	= boot_plat.c bootops.c bootprop.c
MACH_C_SRC	+= get.c ppc_memlist.c ppc_standalloc.c
NL_C_SRC	= __var_arg.c

CONF_OBJS	= $(CONF_SRC:%.c=%.o)
CONF_L_OBJS	= $(CONF_OBJS:%.o=%.ln)

SRT0_OBJ	= $(SRT0_S:%.s=%.o)
SRT0_L_OBJ	= $(SRT0_OBJ:%.o=%.ln)
ISRT0_OBJ	= $(ISRT0_S:%.s=%.o)
ISRT0_L_OBJ	= $(ISRT0_OBJ:%.o=%.ln)

C_SRC		= $(CMN_C_SRC) $(MACH_C_SRC) $(ARCH_C_SRC) $(PLAT_C_SRC)
S_SRC		= $(MACH_S_SRC) $(ARCH_S_SRC) $(PLAT_S_SRC)

#
# NL_OBJS is used to exclude __var_arg.c from the lint target.
# See the ppc stuff in varargs.h and the lint guards from more info.
#
OBJS		= $(C_SRC:%.c=%.o) $(S_SRC:%.s=%.o)
NL_OBJS		= $(NL_C_SRC:%.c=%.o)
L_OBJS		= $(OBJS:%.o=%.ln)

# XXX	Gasp! The use of KARCH here is a total hack.  What needs to
#	happen is the boot source needs to be separated out to do
#	compile-time rather than run-time switching.  At present, the
#	sun4c-specific sources are compiled into all platforms, so
#	this kludge is needed.

CPPDEFS		= $(ARCHOPTS) -D$(PLATFORM) -D_BOOT -D_KERNEL -D_MACHDEP
CPPINCS		+= -I$(ROOT)/usr/platform/$(KARCH)/include
CPPINCS		+= -I$(ROOT)/usr/include/$(ARCHVERS)
CPPINCS		+= -I$(PSMSYSHDRDIR) -I$(STANDDIR)
CPPFLAGS	= $(CPPDEFS) $(CPPFLAGS.master) $(CPPINCS)
CPPFLAGS	+= $(CCYFLAG)$(STANDDIR)
ASFLAGS		+= $(CPPDEFS) -P -D_ASM $(CPPINCS)
#
# XXX	Should be globally enabled!

CFLAGS		+= -v
CFLAGS		+= ../common/ppc.il
#
# The following libraries are built in LIBNAME_DIR
#
LIBNAME_DIR	+= $(PSMNAMELIBDIR)/$(PLATFORM)
LIBNAME_LIBS	+= libnames.a
LIBNAME_L_LIBS	+= $(LIBNAME_LIBS:lib%.a=llib-l%.ln)

#
# The following libraries are built in LIBPROM_DIR
#
LIBPROM_DIR	+= $(PSMPROMLIBDIR)/$(PROMVERS)/common
LIBPROM_LIBS	+= libprom.a
LIBPROM_L_LIBS	+= $(LIBPROM_LIBS:lib%.a=llib-l%.ln)

#
# The following libraries are built in LIBSYS_DIR
#
LIBSYS_DIR	+= $(SYSLIBDIR)
LIBSYS_LIBS	+= libsa.a libufs.a libhsfs.a libnfs_inet.a libcachefs.a
LIBSYS_LIBS	+= libcompfs.a libpcfs.a
LIBSYS_L_LIBS	+= $(LIBSYS_LIBS:lib%.a=llib-l%.ln)

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(NL_OBJS) $(CONF_OBJS) $(SRT0_OBJ) $(ISRT0_OBJ)
.PARALLEL:	$(L_OBJS) $(CONF_L_OBJS) $(SRT0_L_OBJ) $(ISRT0_L_OBJ)
.PARALLEL:	$(UFSBOOT) $(HSFSBOOT) $(NFSBOOT)

all: $(UFSBOOT) $(HSFSBOOT) $(NFSBOOT)

# 4.2 ufs filesystem booter

#
# Libraries used to build ufsboot
#
LIBUFS_LIBS	= libcompfs.a libpcfs.a libcachefs.a libnfs_inet.a
LIBUFS_LIBS	+= libufs.a libprom.a libnames.a libsa.a $(LIBPLAT_LIBS)
LIBUFS_L_LIBS	= $(LIBUFS_LIBS:lib%.a=llib-l%.ln)
UFS_LIBS	= $(LIBUFS_LIBS:lib%.a=-l%)
UFS_DIRS	= $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
UFS_DIRS	+= $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

LIBDEPS=	$(SYSLIBDIR)/libcompfs.a \
		$(SYSLIBDIR)/libpcfs.a \
		$(SYSLIBDIR)/libufs.a \
		$(SYSLIBDIR)/libhsfs.a \
		$(SYSLIBDIR)/libnfs_inet.a \
		$(SYSLIBDIR)/libcachefs.a \
		$(SYSLIBDIR)/libsa.a \
		$(LIBPROM_DIR)/libprom.a $(LIBPLAT_DEP) \
		$(LIBNAME_DIR)/libnames.a

L_LIBDEPS=	$(SYSLIBDIR)/llib-lcompfs.ln \
		$(SYSLIBDIR)/llib-lpcfs.ln \
		$(SYSLIBDIR)/llib-lufs.ln \
		$(SYSLIBDIR)/llib-lhsfs.ln \
		$(SYSLIBDIR)/llib-lnfs_inet.ln \
		$(SYSLIBDIR)/llib-lcachefs.ln \
		$(SYSLIBDIR)/llib-lsa.ln \
		$(LIBPROM_DIR)/llib-lprom.ln $(LIBPLAT_L_DEP) \
		$(LIBNAME_DIR)/llib-lnames.ln

#
# Loader flags used to build ufsboot
#
UFS_MAPFILE	= $(MACH_DIR)/mapfile
UFS_LDFLAGS	= -dn -M $(UFS_MAPFILE) -e _start $(UFS_DIRS)
UFS_L_LDFLAGS	= $(UFS_DIRS)

#
# Object files used to build ufsboot
#
UFS_SRT0	= $(SRT0_OBJ)
UFS_OBJS	= $(OBJS) diskette.o misc_utls.o fsconf.o
UFS_L_OBJS	= $(UFS_SRT0:%.o=%.ln) $(UFS_OBJS:%.o=%.ln)
LD_UFS_OBJS	= $(UFS_OBJS) $(NL_OBJS)

#
# Build rules to build ufsboot
#
$(UFSBOOT): $(UFS_MAPFILE) $(UFS_SRT0) $(LD_UFS_OBJS) $(LIBDEPS)
	$(LD) $(UFS_LDFLAGS) -o $@ $(UFS_SRT0) $(LD_UFS_OBJS) $(UFS_LIBS)
	$(POST_PROCESS)

$(UFSBOOT)_lint: $(L_LIBDEPS) $(UFS_L_OBJS) 
	@echo ""
	@echo ufsboot lint: global crosschecks:
	$(LINT.2) $(UFS_L_LDFLAGS) $(UFS_L_OBJS) $(UFS_LIBS)

# High-sierra filesystem booter.  Probably doesn't work.

#
# Libraries used to build hsfsboot
#
LIBHSFS_LIBS	= libhsfs.a libprom.a libnames.a libsa.a $(LIBPLAT_LIBS)
LIBHSFS_L_LIBS	= $(LIBHSFS_LIBS:lib%.a=llib-l%.ln)
HSFS_LIBS	= $(LIBHSFS_LIBS:lib%.a=-l%)
HSFS_DIRS	= $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
HSFS_DIRS	+= $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

#
# Loader flags used to build hsfsboot
#
HSFS_MAPFILE	= $(MACH_DIR)/mapfile
HSFS_LDFLAGS	= -dn -M $(HSFS_MAPFILE) -e _start $(HSFS_DIRS)
HSFS_L_LDFLAGS	= $(HSFS_DIRS)

#
# Object files used to build hsfsboot
#
HSFS_SRT0	= $(SRT0_OBJ)
HSFS_OBJS	= $(OBJS) hsfsconf.o
HSFS_L_OBJS	= $(HSFS_SRT0:%.o=%.ln) $(HSFS_OBJS:%.o=%.ln)
LD_HSFS_OBJS	= $(HSFS_OBJS) $(NL_OBJS)

$(HSFSBOOT): $(HSFS_MAPFILE) $(HSFS_SRT0) $(LD_HSFS_OBJS) $(LIBDEPS)
	$(LD) $(HSFS_LDFLAGS) -o $@ $(HSFS_SRT0) $(LD_HSFS_OBJS) $(HSFS_LIBS)
	$(POST_PROCESS)

$(HSFSBOOT)_lint: $(HSFS_L_OBJS) $(L_LIBDEPS)
	@echo ""
	@echo hsfsboot lint: global crosschecks:
	$(LINT.2) $(HSFS_L_LDFLAGS) $(HSFS_L_OBJS) $(HSFS_LIBS)

# NFS version 2 over UDP/IP booter

#
# Libraries used to build nfsboot
#
LIBNFS_LIBS	= libcompfs.a libpcfs.a
LIBNFS_LIBS	+= libnfs_inet.a libprom.a libnames.a libsa.a $(LIBPLAT_LIBS)
LIBNFS_L_LIBS	= $(LIBNFS_LIBS:lib%.a=llib-l%.ln)
NFS_LIBS	= $(LIBNFS_LIBS:lib%.a=-l%)
NFS_DIRS	= $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
NFS_DIRS	+= $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

#
# Loader flags used to build nfsboot
#
NFS_MAPFILE	= $(MACH_DIR)/mapfile
NFS_LDFLAGS	= -dn -M $(NFS_MAPFILE) -e _start $(NFS_DIRS)
NFS_L_LDFLAGS	= $(NFS_DIRS)

#
# Object files used to build hsfsboot
#
NFS_SRT0	= $(ISRT0_OBJ)
NFS_OBJS	= $(OBJS) diskette.o misc_utls.o tmpnfsconf.o
NFS_L_OBJS	= $(NFS_SRT0:%.o=%.ln) $(NFS_OBJS:%.o=%.ln)
LD_NFS_OBJS	= $(NFS_OBJS) $(NL_OBJS)

$(NFSBOOT): $(NFS_MAPFILE) $(NFS_SRT0) $(LD_NFS_OBJS) $(LIBDEPS)
	$(LD) $(NFS_LDFLAGS) -o $@.elf $(NFS_SRT0) $(LD_NFS_OBJS) $(NFS_LIBS)
	#@strip $@.elf
	@mcs -d -a "`date`" $@.elf
	$(RM) $@; cp $@.elf $@

$(NFSBOOT)_lint: $(NFS_L_OBJS) $(L_LIBDEPS)
	@echo ""
	@echo nfsboot lint: global crosschecks:
	$(LINT.2) $(NFS_L_LDFLAGS) $(NFS_L_OBJS) $(NFS_LIBS)

#
# NFS_SRT0 is derived from srt0.s with -DINETBOOT
# UFS_SRT0 and HSFS_SRT0 are built using standard rules.
#
$(NFS_SRT0): $(ISRT0_S)
	$(COMPILE.s) -DINETBOOT -o $@ $(ISRT0_S)
	$(POST_PROCESS_O)

$(NFS_SRT0:%.o=%.ln): $(ISRT0_S)
	@($(LHEAD) $(LINT.s) -DINETBOOT -C$(ISRT0_L_OBJ:%.ln=%) \
	    $(ISRT0_S) $(LTAIL))

include $(BOOTSRCDIR)/Makefile.rules

clean:
	$(RM) $(OBJS) $(CONF_OBJS) make.out lint.out
	$(RM) $(SRT0_OBJ) $(ISRT0_OBJ) $(NFSBOOT).elf
	$(RM) $(L_OBJS) $(CONF_L_OBJS)
	$(RM) $(SRT0_L_OBJ) $(ISRT0_L_OBJ)

clobber: clean
	$(RM) $(UFSBOOT) $(HSFSBOOT) $(NFSBOOT)

lint: $(UFSBOOT)_lint $(HSFSBOOT)_lint $(NFSBOOT)_lint

include $(BOOTSRCDIR)/Makefile.targ

FRC:
