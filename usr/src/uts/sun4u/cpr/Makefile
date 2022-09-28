#
# uts/sun4u/cpr/Makefile
# Copyright (c) 1991 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.7	96/06/10 SMI"
#
#	This makefile drives the production of the cpr misc kernel module.
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
MODULE		= cpr
#
#	WARNING: CPR_JUMPBACK_OBJS must always be first in OBJECTS macro.
#	This guarantees that its constituents will be placed at the
#	front of the binary image.
#
OBJECTS		= $(CPR_JUMPBACK_OBJS:%=$(OBJS_DIR)/%) \
		  $(CPR_OBJS:%=$(OBJS_DIR)/%) \
		  $(CPR_IMPL_OBJS:%=$(OBJS_DIR)/%) \
		  $(CPR_SPARC_OBJS:%=$(OBJS_DIR)/%)

LINTS		= $(CPR_SPECIAL_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(CPR_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(CPR_IMPL_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(CPR_SPARC_OBJS:%.o=$(LINTS_DIR)/%.ln)

ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/Makefile.sun4u

CLOBBERFILES	+= $(MODULE)

CLEANFILES	+= $(CPR_SPECIAL_OBJS:%=$(OBJS_DIR)/%)
#
# lint pass one enforcement
#
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(MODULE)

lint:		$(MODULE).lint

install:  	$(MODULE) $(ROOTMODULE)

#
#	This target exists to insure that its consituent object files
#	produce a combined module which is less than 1 page long.  The
#	mapfile implements this check and will cause the link to fail
#	with an error message.
#
$(CPR_JUMPBACK_OBJS):	$(CPR_SPECIAL_OBJS)
	ld -r -M $(UTSBASE)/sun4u/cpr/mapfile.cpr -o $@ $(CPR_SPECIAL_OBJS)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/Makefile.targ
