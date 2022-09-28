#
# uts/i86pc/pci/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	96/05/27 SMI"
#
#	This makefile drives the production of the PCI nexus driver
#
#	i86pc implementation architecture dependent
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
include $(UTSBASE)/i86pc/Makefile.i86pc

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

install:	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/i86pc/Makefile.targ
