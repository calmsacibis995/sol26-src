#
# uts/i86pc/vdi/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.4	94/07/21 SMI"
#
#	This makefile drives the production of the pseudo driver 'vdi' for
#	DDA support in VPIX.
#
#	i86pc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= vdi
OBJECTS		= $(VDI_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(VDI_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/i86pc/io

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

#
#	This module is just lurking in the kernel until a diffinative plan
#	can be made with respect to VPIX support.  Until then don't install.
#
#install:	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)
#
install:

#
#	Include common targets.
#
include $(UTSBASE)/i86pc/Makefile.targ
