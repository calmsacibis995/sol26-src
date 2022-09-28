#
# uts/ppc/rpcmod/Makefile
# Copyright (c) 1994, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	96/04/19 SMI"
#
#	This makefile drives the production of the rpcmod streams kernel
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
MODULE		= rpcmod
OBJECTS		= $(RPCMOD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(RPCMOD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_STRMOD_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_SYS_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

#
#	Override defaults to build a unique, local modstubs.o.
#
MODSTUBS_DIR	 = $(OBJS_DIR)
$(MODSTUBS_O)	:= AS_CPPFLAGS += -DRPCMOD_MODULE
CLEANFILES	+= $(MODSTUBS_O)
CLOBBERFILES	+= $(MODULE)

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(MODULE)

lint:		$(MODULE).lint

$(ROOTLINK):	$(ROOT_SYS_DIR) $(ROOTMODULE)
		-$(RM) $@; ln $(ROOTMODULE) $@

install:  	$(MODULE) $(ROOTMODULE) $(ROOTLINK)

#
#	Include common targets.
#
include $(UTSBASE)/ppc/Makefile.targ
