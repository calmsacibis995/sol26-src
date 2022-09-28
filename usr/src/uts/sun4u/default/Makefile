#
# uts/sun4u/default/Makefile
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.1	96/10/15 SMI"
#
#	This makefile drives the production of the sun4u default platform module.
#
#	sun4u implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..
#
#	Define the module and object file sets.
#
MODULE		= platmod
OBJECTS		= $(DEFAULT_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(DEFAULT_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_PLAT_DIR)/$(MODULE)
PLAT_DIR	= .

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/Makefile.sun4u

#
#	clean and clobber files
#
CLEANFILES	+= $(PLATLIB) $(SYM_MOD)
CLOBBERFILES	+= $(MODULE)

#
# lint pass one enforcement
#
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(PLATLIB)

$(PLATLIB):	$(MODULE)
	${LD} -o $(PLATLIB) -G $(MODULE) -h misc/platmod

lint:		$(MODULE).lint

install:	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/Makefile.targ
