#
# uts/sun4m/cpr/Makefile
# Copyright (c) 1991 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.9	96/05/22 SMI"
#
#	This makefile drives the production of the bpp driver kernel module.
#
#	sun4m implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= cpr
OBJECTS		= $(CPR_OBJS:%=$(OBJS_DIR)/%) \
		  $(CPR_IMPL_OBJS:%=$(OBJS_DIR)/%) \
		  $(CPR_SPARC_OBJS:%=$(OBJS_DIR)/%)

LINTS		= $(CPR_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(CPR_IMPL_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(CPR_SPARC_OBJS:%.o=$(LINTS_DIR)/%.ln)

ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sun4m/Makefile.sun4m

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
include $(UTSBASE)/sun4m/Makefile.targ
