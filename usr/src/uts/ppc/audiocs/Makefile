#
# uts/ppc/audiocs/Makefile
# Copyright (c) 1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.4	95/08/11 SMI"
#
#	This makefile drives the production of the Crystal 4231
#	audio driver (audiocs) kernel module.
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
MODULE		= audiocs
OBJECTS		= $(AUDIOCS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(AUDIOCS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

#
#	Overrides
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

$(MODULE):

#
#	Include common targets.
#
include $(UTSBASE)/ppc/Makefile.targ
