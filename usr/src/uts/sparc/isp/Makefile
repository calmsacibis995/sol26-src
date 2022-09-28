#
# uts/sparc/isp/Makefile
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)Makefile	1.13	96/07/18 SMI"
#
#	This makefile drives the production of the isp driver kernel module.
#
#	sparc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= isp
OBJECTS		= $(ISP_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ISP_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
#	Overrides.
#
CLOBBERFILES	+= $(MODULE)

# 
# lint pass one enforcement 
# 
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(MODULE)

lint:		$(MODULE).lint

install:  	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

#
#	Defines for local commands.
#
WARLOCK		= warlock
WLCC		= wlcc
TOUCH		= touch
SCCS		= sccs
TEST		= test

#
#	Warlock targets
#

warlock: $(MODULE).ok reset_notify.ll

%.wlcmd:
	$(TEST) -f isp.wlcmd  || $(SCCS) get isp.wlcmd

isp.ok: isp.ll reset_notify.ll isp.wlcmd
	$(WARLOCK) -c ./isp.wlcmd isp.ll reset_notify.ll -l ddi_dki_impl -l scsi
	$(TOUCH) $@

%.ll: $(UTSBASE)/sun/io/scsi/adapters/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<
