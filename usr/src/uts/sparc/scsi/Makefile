#
# uts/sparc/scsi/Makefile
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.11	96/10/04 SMI"
#
#	This makefile drives the production of the scsi kernel module.
#
#	sparc architecture dependent
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
WLCC		= wlcc
TOUCH		= touch

#
#	Warlock targets
#
WARLOCK_OBJECTS	= $(SCSI_OBJS:%.o=%.ll)

warlock: $(MODULE).ok $(WARLOCK_OBJECTS)

%.ok: $(WARLOCK_OBJECTS)
	$(TOUCH) $@

%.ll: $(UTSBASE)/common/io/scsi/impl/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

%.ll: $(UTSBASE)/common/io/scsi/conf/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<
