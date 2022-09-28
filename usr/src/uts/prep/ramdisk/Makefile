#
# uts/prep/ramdisk/Makefile
# Copyright (c) 1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.14	94/08/10 SMI"
#
#	This makefile drives the production of the ramdisk driver kernel
#	module.
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
MODULE		= ramdisk
OBJECTS		= $(RAMDISK_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(RAMDISK_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io

#
#	Include common rules.
#
include $(UTSBASE)/prep/Makefile.prep

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

#
#	Don't install. Simply a sample driver.
#
#install:	all

#
# XXXPPC TEMPORARY CHANGE TO INSTALL ramdisk by default.
#
install:  	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Include common targets.
#
include $(UTSBASE)/prep/Makefile.targ
