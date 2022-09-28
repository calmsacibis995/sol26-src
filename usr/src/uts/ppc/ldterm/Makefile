#
# uts/ppc/ldterm/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.7	94/04/11 SMI"
#
#	This makefile drives the production of the ldterm streams kernel
#	module.
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
MODULE		= ldterm
OBJECTS		= $(LDTERM_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(LDTERM_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_STRMOD_DIR)/$(MODULE)

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
