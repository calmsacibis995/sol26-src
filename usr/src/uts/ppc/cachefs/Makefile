#
# uts/ppc/cachefs/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#pragma ident	"@(#)Makefile	1.1	94/04/20 SMI"
#
#	This makefile drives the production of the Cache file system
#	kernel module.
#
#	ppc implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= cachefs
OBJECTS		= $(CACHEFS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(CACHEFS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

#
#	Override defaults to build a unique, local modstubs.o.
#
MODSTUBS_DIR	 = $(OBJS_DIR)
$(MODSTUBS_O)	:= AS_CPPFLAGS += -DCACHEFS_MODULE
CLEANFILES	+= $(MODSTUBS_O)
CLOBBERFILES	+= $(MODULE)

#CFLAGS		+= -DZIP

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
