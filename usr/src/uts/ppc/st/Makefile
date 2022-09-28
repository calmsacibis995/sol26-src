#
# uts/ppc/st/Makefile
# Copyright (c) 1992-1995, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.3	96/05/27 SMI"
#
#	This makefile drives the production of the st driver
#	kernel module.
#
#	(not particularly) ppc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= st
OBJECTS		= $(ST_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ST_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/scsi/targets

#
#	Include common rules.
#
include $(UTSBASE)/ppc/Makefile.ppc

#
#	Overrides.
#
CLOBBERFILES	+= $(MODULE)
DEBUG_FLGS	=
DEBUG_DEFS	+= $(DEBUG_FLGS)

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

install:  	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Include common targets.
#
include $(UTSBASE)/ppc/Makefile.targ
