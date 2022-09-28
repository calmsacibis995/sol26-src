#
# uts/sun4m/mic/Makefile
# Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile 1.6	96/05/22 SMI"
#
#	This makefile drives the production of the sun4m "mic" driver module.
#
#	sun4m architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= mic
OBJECTS		= $(MIC_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(MIC_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sun4m/Makefile.sun4m

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
include $(UTSBASE)/sun4m/Makefile.targ

