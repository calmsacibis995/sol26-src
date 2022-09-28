#
# uts/i86/diaudio/Makefile
# Copyright (c) 1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	95/11/03 SMI"
#
#	This makefile drives the production of the i86 "diaudio" module.
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
MODULE		= diaudio
OBJECTS		= $(DIAUDIO_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(DIAUDIO_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

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

install:	$(MODULE) $(ROOTMODULE)

#	Include common targets.
#
include $(UTSBASE)/i86/Makefile.targ
