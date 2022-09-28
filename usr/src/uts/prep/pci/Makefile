#
# uts/prep/pci/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#pragma ident	"@(#)Makefile	1.4	95/10/18 SMI"
#
#	This makefile drives the production of the PCI nexus driver
#
#	prep implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= pci
OBJECTS		= $(PCINEXUS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PCINEXUS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/prep/Makefile.prep

MODSTUBS_DIR	 = $(OBJS_DIR)
$(MODSTUBS_O)	:= AS_CPPFLAGS += -DPCINEXUS_MODULE
CLEANFILES	+= $(MODSTUBS_O)
CLOBBERFILES	+= $(MODULE)
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(MODULE)

lint:		$(MODULE).lint

install:	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/prep/Makefile.targ
