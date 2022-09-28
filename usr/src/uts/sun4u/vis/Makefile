#
# uts/sun4u/vis/Makefile
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
#ident   "@(#)Makefile 1.1     96/05/01 SMI"
#
#       This makefile drives the production of the vis kernel module
#
#       sun4u implementation architecture dependent
#

#
#       Path to the base of the uts directory tree (usually /usr/src/uts).
#
#
UTSBASE = ../..

#
#       Define the module and object file sets.
#
MODULE		= vis
OBJECTS		= $(VIS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(VIS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE      = $(ROOT_PSM_MISC_DIR)/$(MODULE)


#
#       Include common rules.
#
include $(UTSBASE)/sun4u/Makefile.sun4u

CLOBBERFILES	+= $(MODULE)

#
# lint pass one enforcement
#
CFLAGS += -v

.KEEP_STATE:

all:		$(MODULE)

lint:		$(MODULE).lint

install: 	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/Makefile.targ
