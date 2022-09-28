#
# uts/sparc/ecpp/Makefile
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	96/05/24 SMI"
#
#	This Makefile drives the production of ecpp
#	kernel module.
#	
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= ecpp
OBJECTS		= $(ECPP_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ECPP_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
#	Overrides.
#
CLOBBERFILES	+= $(MODULE)

#
# lint pass one enforcement
#
CFLAGS += -v

# Turn this on once compiler understands v9 in it's backend
#INLINES		+= $(UTSBASE)/sun/io/ecpp.il

#	Default build targets.
#
.KEEP_STATE:

all:		$(MODULE)

lint:		$(MODULE).lint

install:  	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ
