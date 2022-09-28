#
# uts/sparc/pln/Makefile
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)Makefile	1.5	96/08/30 SMI"
#
#	This makefile drives the production of the pln driver kernel module.
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
MODULE		= pln
OBJECTS		= $(PLN_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PLN_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/sun/io/scsi/adapters


#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

CLOBBERFILES	+= $(MODULE)

# 
# lint pass one enforcement 
# 
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

all browser:		$(MODULE) $(SRC_CONFILE)

browser := CFLAGS += -xsb

lint:		$(MODULE).lint

install:  	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)


#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

#
#	Defines for local commands.
#
WARLOCK		= warlock
WLCC		= wlcc
SCCS		= sccs
TOUCH		= touch
TEST		= test

#
#	lock_lint rules
#
warlock: $(MODULE).ok

%.wlcmd:
	$(TEST) -f $@  || $(SCCS) get $@

pln.ok: pln.ll  pln.wlcmd
	$(WARLOCK) -c pln.wlcmd pln.ll -l ddi_dki_impl -l scsi
	$(TOUCH) $@

%.ll: $(UTSBASE)/sun/io/scsi/adapters/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<
