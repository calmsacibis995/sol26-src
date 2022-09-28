#
# uts/sun4u/su/Makefile
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
#ident   "@(#)Makefile 1.3     96/05/24 SMI"
#
#	This makefile drives the production of the sun4u "su" driver module.
#
#	sun4u implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..
#
#	Define the module and object file sets.
#
MODULE		= su
OBJECTS		= $(SU_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SU_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/Makefile.sun4u

#
#	Overrides
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

#	Include common targets.
#
include $(UTSBASE)/sun4u/Makefile.targ
