#
#ident	"@(#)Makefile.com	1.3	96/03/13 SMI"
#
# Copyright (c) 1991-1994, Sun Microsystems, Inc.
#
# psm/stand/boot/ppc/promif/Makefile
#
# create the appropriate type of prom library, based on $(BOOTCFLAGS)
# and put the output in $(OBJSDIR); these flags are both passed in from
# the caller
#
# NOTE that source is included from the uts/prep/promif directory
#

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib
include $(TOPDIR)/psm/stand/lib/Makefile.lib

PROMDIR =       $(TOPDIR)/uts/prep/promif
STANDSYSDIR=    $(TOPDIR)/stand/$(MACH)
SYSDIR  =       $(TOPDIR)/uts

LIBPROM =       libprom.a
LINTLIBPROM =   llib-lprom.ln

PROM_CFILES =		\
	prom_alloc.c		\
	prom_boot.c		\
	prom_devname.c		\
	prom_enter.c		\
	prom_exit.c		\
	prom_getchar.c		\
	prom_gettime.c		\
	prom_init.c		\
	prom_io.c		\
	prom_macaddr.c		\
	prom_panic.c		\
	prom_printf.c		\
	prom_putchar.c		\
	prom_reboot.c		\
	prom_string.c

PROM_SFILES=

KARCH	=	ppc

# defaults
OBJSDIR	=	objs
PROM_COBJ =	$(PROM_CFILES:%.c=$(OBJSDIR)/%.o)
PROM_SOBJ =	$(PROM_SFILES:%.s=$(OBJSDIR)/%.o)
OBJS =		$(PROM_COBJ) $(PROM_SOBJ)
L_OBJS =	$(OBJS:%.o=%.ln)

ARCHOPTS=	-D__ppc
ASFLAGS =	-P -D__STDC__ -D_BOOT -D_ASM
CPPDEFS	+=	$(ARCHOPTS) -D$(KARCH) -D_BOOT -D_KERNEL -D_MACHDEP
CPPINCS =	-I. -I$(SYSDIR)/$(KARCH) -I$(SYSDIR)/$(MMU) \
		-I$(SYSDIR)/$(MACH) -I$(STANDSYSDIR) -I$(SYSDIR)/sun \
		-I$(SYSDIR)/common
CPPFLAGS=	$(CPPDEFS) $(CPPINCS) $(CPPFLAGS.master)
#
# XXX	This should be globally enabled!
# CFLAGS +=	-v

.KEEP_STATE:

.PARALLEL:	$(PROM_OBJ) $(L_OBJS)

all install: $(LIBPROM)

lint: $(LINTLIBPROM)

clean:
	$(RM) $(OBJS) $(L_OBJS)

clobber: clean
	$(RM) $(LIBPROM) $(LINTLIBPROM) a.out core

$(LIBPROM): $(OBJSDIR) .WAIT $(OBJS)
	$(BUILD.AR) $(OBJS)

$(LINTLIBPROM): $(OBJSDIR) .WAIT $(L_OBJS)
	@-$(ECHO) "lint library construction:" $@
	@$(LINT.lib) -o prom $(L_OBJS)

$(OBJSDIR):
	-@[ -d $@ ] || mkdir $@

#
# build rules using standard library object subdirectory
#
$(OBJSDIR)/%.o: $(PROMDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.o: $(PROMDIR)/%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.ln: $(PROMDIR)/%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))
	@$(MV) $(@F) $@

$(OBJSDIR)/%.ln: $(PROMDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
	@$(MV) $(@F) $@

