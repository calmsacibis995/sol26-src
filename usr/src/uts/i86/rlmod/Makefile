#
# uts/i86/rlmod/Makefile
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#pragma ident	"@(#)Makefile	1.4	96/05/27 SMI"
#
#	This makefile drives the production of the rlmod streams kernel
#	module.
#
#	i86 architecture dependent
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
include $(UTSBASE)/i86/Makefile.i86

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
include $(UTSBASE)/i86/Makefile.targ
