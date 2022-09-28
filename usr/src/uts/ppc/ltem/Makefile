#
# uts/ppc/ltem/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#	This makefile drives the production of the ltem driver
#
#	ppc architecture dependent
#
#ident	"@(#)Makefile	1.2	94/08/02 SMI"
#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= ltem
OBJECTS		= $(LTEM_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(LTEM_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io

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

all:		$(MODULE) $(SRC_CONFILE)

lint:		$(MODULE).lint

install:  	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Include common targets.
#
include $(UTSBASE)/ppc/Makefile.targ
