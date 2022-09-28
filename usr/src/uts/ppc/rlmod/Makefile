#
# uts/ppc/rlmod/Makefile
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#pragma ident	"@(#)Makefile	1.5	96/05/27 SMI"
#
#	This makefile drives the production of the rlmod streams kernel
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
MODULE		= rlmod
OBJECTS		= $(RLMOD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(RLMOD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_STRMOD_DIR)/$(MODULE)

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
