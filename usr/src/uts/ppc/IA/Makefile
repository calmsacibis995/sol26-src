#
# uts/ppc/IA/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#pragma ident	"@(#)Makefile	1.2	94/11/04 SMI"
#
#	This makefile drives the production of the IA driver kernel module.
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
MODULE		= IA
OBJECTS		= $(IA_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(IA_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_SCHED_DIR)/$(MODULE)

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
