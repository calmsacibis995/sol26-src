#
# i86pc/uppc/Makefile
# Copyright (c) 1993-1994, by Sun Microsystems, Inc.
#
#pragma ident	"@(#)Makefile	1.5	94/06/01 SMI"
#
#	This makefile drives the production of the uppc mach
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
MODULE		= uppc
OBJECTS		= $(UPPC_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(UPPC_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_MACH_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Overrides.
#
CLOBBERFILES	+= $(MODULE)
CFLAGS		+= -v
DEBUG_FLGS	=
DEBUG_DEFS	+= $(DEBUG_FLGS)

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
