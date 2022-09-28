#
# uts/prep/audiocs/Makefile
# Copyright (c) 1995-1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.3	96/08/14 SMI"
#
#	This makefile only installs audiocs.conf (which is
#	platform dependent).
#
#	prep architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= audiocs
CONF_SRCDIR	= $(UTSBASE)/prep/io/audiocs
ROOT_PLAT_KERNEL_DRV_DIRS	 = $(PLATFORMS:%=$(ROOT_PLAT_DIR)/%/kernel/drv)
AUDIOCS_FILES	=  $(PLATFORMS:%=$(ROOT_PLAT_DIR)/%/kernel/drv/audiocs.conf) \
		   $(PLATFORM:%=$(ROOT_PLAT_DIR)/%/kernel/drv/audiocs.conf)

INS.audiocs.file=       $(RM) $@; $(INS) -s -m 644 -f $(@D) audiocs.conf
$(CH)INS.audiocs.file=  $(INS) -s -m 644 -u $(OWNER) -g $(GROUP) -f $(@D) audiocs.conf

#
#	Include common rules.
#
include $(UTSBASE)/prep/Makefile.prep

#
#	Default build targets.
#
.KEEP_STATE:

all clean clobber lint:

install:	$(ROOT_PLAT_KERNEL_DRV_DIRS) $(AUDIOCS_FILES)

$(ROOT_PLAT_KERNEL_DRV_DIRS):
	-$(INS.dir.root.sys)

$(ROOT_PLAT_DIR)/%/kernel/drv/audiocs.conf:	%
	 	sh install_conf $<
		if [ -f audiocs.conf ] ; \
		then	$(INS.audiocs.file) ; \
			$(RM) audiocs.conf ; \
		fi

$(PLATFORMS) $(PLATFORM):

#
#       No lints, so keep the clobber and clean targets reasonable.
#
CLEANLINTFILES	= Nothing_to_remove
CLEANFILES	= Nothing_to_remove
CLOBBERFILES	= Nothing_to_remove

#
#	Include common targets.
#
include $(UTSBASE)/prep/Makefile.targ
