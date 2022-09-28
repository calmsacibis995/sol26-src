#
# uts/i86pc/bootdev/Makefile
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.3	96/06/03 SMI"
#
#	This makefile drives the production of the bootdev driver 
#	kernel module.
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
MODULE		= bootdev
OBJECTS		= $(BOOTDEV_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(BOOTDEV_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

CLOBBERFILES	+= $(MODULE)

#
# lint pass one enforcement
#
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
