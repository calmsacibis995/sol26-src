#
# uts/ppc/tl/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#pragma	ident	"@(#)Makefile	1.4	96/05/27 SMI"
#
#	This makefile drives the production of the tl driver
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
MODULE		= tl
OBJECTS		= $(TL_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(TL_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

#
# lint pass one enforcement
#  
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(MODULE) $(SRC_CONFILE)

lint:		$(MODULE).lint

install:	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)


#
#	Include common targets.
#
include $(UTSBASE)/ppc/Makefile.targ
