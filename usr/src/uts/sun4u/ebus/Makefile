#
# uts/sun4u/ebus/Makefile
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident  "@(#)Makefile 1.2     95/07/11 SMI"
#
#	This makefile drives the production of the ebus driver kernel module
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
MODULE		= ebus
OBJECTS		= $(EBUS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(EBUS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/Makefile.sun4u

CLOBBERFILES	+= $(MODULE)

# Turn this on once compiler understands v9 in it's backend
#INLINES		+= $(UTSBASE)/sun4u/io/ebus.il

#
# lint pass one enforcement
#
CFLAGS += -v

#
# Turn on doubleword alignment for 64 bit registers
#
CFLAGS += -dalign

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
include $(UTSBASE)/sun4u/Makefile.targ
