#
# uts/sparc/pcs/Makefile
# Copyright (c) 1994-1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)Makefile	1.2	96/05/23 SMI"
#
#	This makefile drives the production of the DoRight driver kernel module.
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
MODULE		= pcs
OBJECTS		= $(PCS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PCS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

CLOBBERFILES    += $(MODULE)

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

install:	$(MODULE) $(ROOTMODULE)

#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ
