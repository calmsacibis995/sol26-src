#
# uts/ppc/readfont/Makefile
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All Rights Reserved.
#
#	This makefile drives the production of the readfont module
#
#	ppc architecture dependent
#
#ident	"@(#)Makefile	1.1	95/06/19 SMI"
#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= readfont
OBJECTS		= $(READFONT_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(READFONT_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

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
include $(UTSBASE)/ppc/Makefile.targ
