#
# uts/sparc/genunix/Makefile
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.8	96/09/12 SMI"
#
#	This makefile drives the production of the generic
#	unix kernel module.
#
#	sparc implementation architecture dependent
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

PLATFORM	= sun4u
ROOTMODULE	= $(ROOT_PSM_MOD_DIR)/$(MODULE)

LIBGEN		= libgenunix.so
LIBSTUBS	= $(GENSTUBS_OBJS:%=$(OBJS_DIR)/%)
SONAME		= $(MODULE)


#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

LINT_LIB_DIR	= $(UTSBASE)/$(PLATFORM)/lint-libs
LINT_LIB	= $(UTSBASE)/$(PLATFORM)/lint-libs/llib-lunix.ln
GEN_LINT_LIB	=

#
#	Include v9 files
#
include $(UTSBASE)/sparc/v9/Makefile.files

CLEANFILES	+= $(LIBSTUBS)
CLOBBERFILES	+= $(MODULE) $(LIBGEN)

# 
# lint pass one enforcement 
# 
CFLAGS += -v

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
include $(UTSBASE)/sparc/Makefile.targ

#
#	Include v9 rules.
#
include $(UTSBASE)/sparc/v9/Makefile.rules

#
#	Include sun4u workarounds.
#
include $(UTSBASE)/sun4u/Makefile.workarounds

ALL_DEFS +=	$(WORKAROUND_DEFS) -D__sparcv9cpu
