#
# uts/i86/profile/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident  "@(#)Makefile 1.2     94/06/21 SMI"
#
#	This makefile drives the production of the profile driver kernel module.
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
MODULE		= profile
OBJECTS		= $(PROFILE_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PROFILE_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

#
#	Overrides.
#
CLOBBERFILES	+= $(MODULE)
EXTRA_FLAGS	+= -DGPROF
CFLAGS		+= -v

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
