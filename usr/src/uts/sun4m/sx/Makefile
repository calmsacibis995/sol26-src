#
# uts/sparc/sx/Makefile
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile 1.7    96/05/22 SMI"
#
#	This makefile drives the production of the sx driver
#
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= sx
OBJECTS		= $(SX_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SX_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

#
#	Include sun4m rules.
#
include $(UTSBASE)/sun4m/Makefile.sun4m

CLEANFILES	= $(OBJS_DIR)/$(MODULE).o

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
include $(UTSBASE)/sun4m/Makefile.targ
