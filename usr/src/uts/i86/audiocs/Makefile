#
# uts/i86/audiocs/Makefile
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.3	96/01/26 SMI"
#
#	This makefile drives the production of the Crystal 4231
#	audio driver (audiocs) kernel module.
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
MODULE		= audiocs
OBJECTS		= $(AUDIOCS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(AUDIOCS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/i86/io

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

#
#	Overrides
#
CLOBBERFILES	+= $(MODULE) 
CFLAGS		+= -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(MODULE) $(SRC_CONFILE)

lint:		$(MODULE).lint

install:  	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)

$(MODULE):

#
#	Include common targets.
#
include $(UTSBASE)/i86/Makefile.targ
