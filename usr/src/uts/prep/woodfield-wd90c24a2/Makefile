#
# uts/prep/woodfield-wd90c24a2/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.7	96/05/30 SMI"
#
#	This makefile drives the production of the woodfield-wd90c24a2 `drv'
#	kernel module.
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
MODULE		= woodfield-wd90c24a2
OBJECTS		= $(WOODFIELD_WD90C24A2_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(WOODFIELD_WD90C24A2_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/prep/Makefile.prep

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
include $(UTSBASE)/prep/Makefile.targ
