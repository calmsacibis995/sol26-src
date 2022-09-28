#
# uts/ppc/tirdwr/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.2	96/05/27 SMI"
#
#	This makefile drives the production of the tirdwr streams kernel
#	module.
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
MODULE		= tirdwr
OBJECTS		= $(TIRDWR_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(TIRDWR_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_STRMOD_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

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
include $(UTSBASE)/ppc/Makefile.targ
