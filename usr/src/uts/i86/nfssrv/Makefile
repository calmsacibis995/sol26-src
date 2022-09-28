#
# uts/i86/nfssrv/Makefile
# Copyright (c) 1991-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)Makefile	1.7	95/03/17 SMI"
#
#	This makefile drives the production of the nfs server-specific
#	kernel module.
#
#	i86 implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= nfssrv
OBJECTS		= $(NFSSRV_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(NFSSRV_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

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

install:  	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/i86/Makefile.targ
