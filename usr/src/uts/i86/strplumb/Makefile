#
# uts/i86/strplumb/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident "@(#)Makefile	1.6	96/05/27 SMI"
#
#	This makefile drives the production of the strplumb
#	kernel module.
#
#	i86 implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= strplumb
OBJECTS		= $(STRPLUMB_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(STRPLUMB_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

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
