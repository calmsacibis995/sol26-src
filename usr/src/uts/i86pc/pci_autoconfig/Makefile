#
# uts/i86pc/pci_autoconfig/Makefile
# Copyright (c) 1995, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.2	95/04/03 SMI"
#
#	This makefile drives the production of the PCI autoconfiguration
#	kernel module.
#
#	i86pc platform dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= pci_autoconfig
OBJECTS		= $(PCI_AUTOCONFIG_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PCI_AUTOCONFIG_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

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
include $(UTSBASE)/i86pc/Makefile.targ
