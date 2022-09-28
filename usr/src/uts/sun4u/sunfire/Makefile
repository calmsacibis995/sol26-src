#
# uts/sun4u/sunfire/Makefile
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.1	96/10/15 SMI"
#
#	This makefile drives the production of the sun4u sunfire platform module.
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
OBJECTS		= $(SUNFIRE_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SUNFIRE_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_SUNFIRE_MISC_DIR)/$(MODULE)
PLAT_DIR	= .
HERE		= ../sunfire

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

all:		$(SYM_MOD)

$(PLATLIB):	$(MODULE)
	${LD} -o $(PLATLIB) -G $(MODULE)

$(SYM_MOD):	$(UNIX_O) $(PLATLIB)
	@echo "resolving symbols against unix.o"
	@(cd $(UNIX_DIR); pwd; \
	    PLAT_DIR=$(HERE) SYM_MOD=$(HERE)/$(SYM_MOD) $(MAKE) symcheck)

lint:		$(MODULE).lint

install:	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/Makefile.targ
