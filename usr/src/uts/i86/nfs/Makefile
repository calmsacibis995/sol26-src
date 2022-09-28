#
# uts/i86/nfs/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident "@(#)Makefile	1.6	94/06/01 SMI"
#
#	This makefile drives the production of the nfs file system
#	kernel module.
#
#	i86 implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= nfs
OBJECTS		= $(NFS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(NFS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_SYS_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

#
#	Overrides.
#
MODSTUBS_DIR	 = $(OBJS_DIR)
$(MODSTUBS_O)	:= AS_CPPFLAGS += -DNFS_MODULE
CLEANFILES	+= $(MODSTUBS_O)

CLOBBERFILES	+= $(MODULE)
CFLAGS		+= -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(MODULE)

lint:		$(MODULE).lint

$(ROOTLINK):	$(ROOT_SYS_DIR) $(ROOTMODULE)
	-$(RM) $@; ln $(ROOTMODULE) $@

install:  	$(MODULE) $(ROOTMODULE) $(ROOTLINK)

#
#	Include common targets.
#
include $(UTSBASE)/i86/Makefile.targ
