#
# uts/i86pc/mc/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.6	96/05/27 SMI"?
#
#	This makefile drives the production of the MCA nexus driver
#
#	i86pc implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= mc
OBJECTS		= $(MCANEXUS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(MCANEXUS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/i86pc/io

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Overrides.
#
CLOBBERFILES	+= $(MODULE)
CPPFLAGS	+= -v -DMCA_BUS

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(MODULE) $(SRC_CONFFILE)

lint:		$(MODULE).lint

install:	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Include common targets.
#
include $(UTSBASE)/i86pc/Makefile.targ
