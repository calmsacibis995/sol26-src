#
#ident	"@(#)Makefile	1.7	96/03/13 SMI"
#
# Copyright (c) 1995, Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/lib/promif/ppc/ieee1275/prep/Makefile
#
# create libplat.a from ieee1275 promif platform dependent source.
#
# NOTE that source is included from the /usr/src/psm/promif/ieee1275/* dirs
#

TOPDIR =	../../../../../../..

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib
include $(TOPDIR)/psm/stand/lib/Makefile.lib

PLATDIR =	$(TOPDIR)/psm/promif/ieee1275/prep
SYSDIR =	$(TOPDIR)/uts

LIBPLAT =	libplat.a
LINTLIBPLAT =	llib-lplat.ln

PLAT_PFILES = 			\
	prom_alloc.c		\
	prom_init.c		\
	prom_macaddr.c		\
	prom_map.c		\
	prom_mem.c		\
	prom_mmu.c		\
	prom_ppc.c		\
	prom_prop_ppc.c

PLAT_SFILES = 			\
	swap_int.s

PLAT_FILES =	$(PLAT_PFILES) $(PLAT_SFILES)

KARCH =		prep
MMU =		ppc

OBJSDIR =	objs

PLAT_POBJ =	$(PLAT_PFILES:%.c=$(OBJSDIR)/%.o)
PLAT_SOBJ =	$(PLAT_SFILES:%.s=$(OBJSDIR)/%.o)
OBJS =		$(PLAT_POBJ) $(PLAT_SOBJ)
L_OBJS =	$(OBJS:%.o=%.ln)
L_SRCS =	$(PLAT_FILES:%=$(PLATDIR)/%)

ARCHOPTS=	-Dprep -D__ppc
ASFLAGS =	-P -D__STDC__ -D_BOOT -D_ASM
CPPDEFS	=	$(ARCHOPTS) -D$(KARCH) -D_BOOT -D_KERNEL -D_MACHDEP
CPPINCS =	-I. -I$(SYSDIR)/$(KARCH) -I$(SYSDIR)/$(MMU) \
		-I$(SYSDIR)/ppc \
		-I$(SYSDIR)/common
CPPFLAGS=	$(CPPDEFS) $(CPPINCS) $(CPPFLAGS.master)
CFLAGS +=	-v

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(L_OBJS)

all install: $(LIBPLAT)

lint: $(LINTLIBPLAT)

clean:
	$(RM) $(OBJS) $(L_OBJS)

clobber: clean
	$(RM) $(LIBPLAT) $(LINTLIBPLAT) a.out core

$(LIBPLAT): $(OBJSDIR) .WAIT $(OBJS)
	$(BUILD.AR) $(OBJS)

$(LINTLIBPLAT): $(OBJSDIR) .WAIT $(L_OBJS)
	@$(ECHO) "\nlint library construction:" $@
	@$(LINT.lib) -o plat $(L_SRCS)

$(OBJSDIR):
	-@[ -d $@ ] || mkdir $@

#
# build rules using standard library object subdirectory
#
$(OBJSDIR)/%.o: $(PLATDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.o: $(PLATDIR)/%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.ln: $(PLATDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
	@$(MV) $(@F) $@

$(OBJSDIR)/%.ln: $(PLATDIR)/%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))
	@$(MV) $(@F) $@
