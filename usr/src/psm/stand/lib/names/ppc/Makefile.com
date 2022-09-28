#
#ident	"@(#)Makefile.com	1.4	96/03/13 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/lib/names/ppc/Makefile
#
# ppc architecture Makefile for Standalone Library
# Platform-specific, but shared, routines.
#

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib
include $(TOPDIR)/psm/stand/lib/Makefile.lib

PSMSYSHDRDIR =	$(TOPDIR)/psm/stand

LIBNAMES =	libnames.a
LINTLIBNAMES =	llib-lnames.ln

# ARCHCMNDIR - common code for several machines of a given isa
# OBJSDIR - where the .o's go

ARCHCMNDIR =	../common
OBJSDIR =	objs

CMNSRC = 	mfgname.c
NAMESRCS =	$(PLATSRC) $(CMNSRC)
NAMEOBJS =	$(NAMESRCS:%.c=%.o)

OBJS =		$(NAMEOBJS:%=$(OBJSDIR)/%)
L_OBJS =	$(OBJS:%.o=%.ln)
L_SRCS = 	$(CMNSRC:%=$(ARCHCMNDIR)/%) $(PLATSRC)

CPPINCS +=	-I$(ROOT)/usr/platform/$(PLATFORM)/include
CPPINCS	+= 	-I$(PSMSYSHDRDIR)
CPPFLAGS =	$(CPPFLAGS.master) $(CPPINCS) $(CCYFLAG)$(PSMSYSHDRDIR)
ASFLAGS =	-P -D__STDC__ -D_ASM $(CPPFLAGS.master) $(CPPINCS)
CFLAGS +=	-v

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(L_OBJS)

all install: $(LIBNAMES)

lint: $(LINTLIBNAMES)

clean:
	$(RM) $(OBJS) $(L_OBJS)

clobber: clean
	$(RM) $(LIBNAMES) $(LINTLIBNAMES) a.out core

$(LIBNAMES): $(OBJSDIR) .WAIT $(OBJS)
	$(BUILD.AR) $(OBJS)

$(LINTLIBNAMES): $(OBJSDIR) .WAIT $(L_OBJS)
	@$(ECHO) "\nlint library construction:" $@
	@$(LINT.lib) -o names $(L_SRCS)

$(OBJSDIR):
	-@[ -d $@ ] || mkdir $@

#
# build rules using standard library object subdirectory
#
$(OBJSDIR)/%.o: $(ARCHCMNDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.o: $(ARCHCMNDIR)/%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.ln: $(ARCHCMNDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
	@$(MV) $(@F) $@

$(OBJSDIR)/%.ln: $(ARCHCMNDIR)/%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))
	@$(MV) $(@F) $@

