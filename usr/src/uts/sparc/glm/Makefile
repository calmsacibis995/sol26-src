#
# uts/sparc/glm/Makefile
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)Makefile	1.8	96/10/04 SMI"
#
#	This makefile drives the production of the glm driver kernel module.
#
#	Sparc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= glm
OBJECTS		= $(GLM_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(GLM_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
#	Overrides.
#
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
include $(UTSBASE)/sparc/Makefile.targ

#
#	Defines for local commands.
#
WARLOCK		= warlock
WLCC		= wlcc
TOUCH		= touch

#
#	lock_lint rules
#
warlock: $(MODULE).ok reset_notify.ll

glm.ok: glm.ll reset_notify.ll
	$(WARLOCK) -c glm.wlcmd glm.ll reset_notify.ll \
	    -l ddi_dki_impl -l scsi
	$(TOUCH) $@

glm.ll: $(UTSBASE)/common/io/scsi/adapters/glm.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o glm.ll \
	$(UTSBASE)/common/io/scsi/adapters/glm.c

reset_notify.ll: $(UTSBASE)/sun/io/scsi/adapters/reset_notify.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o reset_notify.ll \
	$(UTSBASE)/sun/io/scsi/adapters/reset_notify.c
