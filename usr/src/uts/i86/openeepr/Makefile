#
# uts/i86/openeepr/Makefile
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.1	96/05/07 SMI"
#
#	This makefile drives the production of the openeepr driver
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
MODULE		= openeepr
OBJECTS		= $(OPENEEPR_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(OPENEEPR_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/i86/io

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

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
include $(UTSBASE)/i86/Makefile.targ
