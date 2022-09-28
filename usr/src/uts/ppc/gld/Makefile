#
# uts/ppc/gld/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#pragma ident	"@(#)Makefile	1.1	94/04/26 SMI"
#
#	This makefile drives the production of the gld driver 
#	kernel module.
#
#	ppc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= gld
OBJECTS		= $(GLD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(GLD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

#
#	Override defaults to build a unique, local modstubs.o.
#
CLOBBERFILES	+= $(MODULE)
CFLAGS		+= -v -DSUNDDI

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
include $(UTSBASE)/ppc/Makefile.targ
