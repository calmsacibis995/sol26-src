#
# uts/sun4u/tod/Makefile
# Copyright (c) 1995 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)Makefile 1.3 96/05/22 SMI"
#
#	This makefile drives the production of tod driver
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
MODULE		= tod
OBJECTS		= $(TOD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(TOD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/sun4u/io

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/Makefile.sun4u

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


install:  	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/Makefile.targ

