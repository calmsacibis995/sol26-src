#
# uts/sparc/se/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident  "@(#)Makefile 1.5     96/05/24 SMI"
#
#	This makefile drives the production of the asy `drv'
#	kernel module.
#
#	Sparc implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= se
OBJECTS		= $(SE_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SE_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
#	Overrides.
#
#
#	Override defaults to build a unique, local modstubs.o.
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

install:  	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ
