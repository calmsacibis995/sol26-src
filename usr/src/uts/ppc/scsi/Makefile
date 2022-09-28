#
# uts/ppc/io/scsi/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#ident "@(#)Makefile 1.2	96/05/27 SMI"
#
#	This makefile drives the production of the scsi "drv"
#	kernel module.
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
MODULE		= scsi
OBJECTS		= $(SCSI_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SCSI_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

DEBUG_FLGS	=
DEBUG_DEFS	+= $(DEBUG_FLGS)
CLOBBERFILES	+= $(MODULE)

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
#	Include common targets.
#
include $(UTSBASE)/ppc/Makefile.targ
