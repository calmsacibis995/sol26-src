#
# uts/i86pc/ttvx/Makefile
# Copyright (c) 1992-1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.4	94/06/01 SMI"
#
#	This makefile drives the production of the ttvx streams
#	kernel module.
#
#	i86pc implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= ttvx
OBJECTS		= $(TTVX_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(TTVX_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_STRMOD_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Overrides.
#
CLOBBERFILES	+= $(MODULE)
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
include $(UTSBASE)/i86pc/Makefile.targ
