#
# uts/ppc/genunix/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.6	94/11/15 SMI"
#
#	This makefile drives the production of the generic
#	unix kernel module.
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
MODULE		= genunix

OBJECTS		= $(GENUNIX_OBJS:%=$(OBJS_DIR)/%) \
		  $(NOT_YET_KMODS:%=$(OBJS_DIR)/%)

LINTS		= $(GENUNIX_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(NOT_YET_KMODS:%.o=$(LINTS_DIR)/%.ln)

ROOTMODULE	= $(ROOT_MOD_DIR)/$(MODULE)

LIBGEN		= libgenunix.so
LIBSTUBS	= $(GENSTUBS_OBJS:%=$(OBJS_DIR)/%)
SONAME		= $(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

CLEANFILES	+= $(LIBSTUBS)
CLOBBERFILES	+= $(MODULE) $(LIBGEN)

#
# lint pass one enforcement
#
CFLAGS		+= -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(LIBGEN)

$(LIBGEN):	$(MODULE) $(LIBSTUBS)
		$(LD) -o $@ -G -h $(SONAME) $(MODULE) $(LIBSTUBS)

lint:		$(MODULE).lint

install:  	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/ppc/Makefile.targ

#
# Override.
#
$(MODULE).lint := GEN_LINT_LIB =
