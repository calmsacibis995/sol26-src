#
# uts/sparc/fas/Makefile
#
#ident "@(#)Makefile	1.6	96/07/18 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#	This makefile drives the production of the fas driver kernel module.
#
#	sparc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE = ../..

#
#	Define the module and object file sets.
#
MODULE		= fas
OBJECTS		= $(FAS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(FAS_OBJS:%.o=$(LINTS_DIR)/%.ln)
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
#	lock_lint rules
#
warlock: $(MODULE).ok reset_notify.ll

%.wlcmd:
	$(TEST) -f $@ || $(SCCS) get $@

fas.ok: fas.ll reset_notify.ll fas_callbacks.ll fas.wlcmd
	$(WARLOCK) -c fas.wlcmd fas.ll fas_callbacks.ll \
	reset_notify.ll  -l ddi_dki_impl -l scsi
	$(TOUCH) $@

%.ll: $(UTSBASE)/sun/io/scsi/adapters/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<
