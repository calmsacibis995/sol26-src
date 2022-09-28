#
# uts/i86/ppp/Makefile
# Copyright (c) 1993-1994, by Sun Microsystems, Inc.
#
#pragma ident	"@(#)Makefile	1.3	96/05/27 SMI"
#
#	This makefile drives the production of the ppp streams kernel
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
MODULE		= ppp
OBJECTS		= $(PPP_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PPP_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_STRMOD_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

#
#	Overrides.
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
include $(UTSBASE)/i86/Makefile.targ
