#
# uts/ppc/klmops/Makefile
# Copyright (c) 1991-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident  "@(#)Makefile 1.4     95/04/07 SMI"
#
#	This makefile drives the production of the klmops
#	kernel module.
#
#	ppc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= klmops
OBJECTS		= $(KLMOPS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(KLMOPS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

#
#	Overrides.
#
CLOBBERFILES	+= $(MODULE)
CFLAGS		+= -v

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
include $(UTSBASE)/ppc/Makefile.targ
