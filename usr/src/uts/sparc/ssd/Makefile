#
# uts/sparc/ssd/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile 1.9	96/07/25 SMI"
#
#	This makefile drives the production of the ssd driver kernel module.
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
MODULE		= ssd
OBJECTS		= $(SSD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SSD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/sun/io/scsi/targets

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

all browser:		$(MODULE) $(SRC_CONFILE)

lint:		$(MODULE).lint

install:	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)

browser := CFLAGS += -xsb

#
#	Include common targets.
#
include	$(UTSBASE)/sparc/Makefile.targ


#
# Warlock targets

SSD_FILES   = $(MODULE).ll
SCSI_FILES = $(SCSI_OBJS:%.o=-l ../scsi/%.ll)

warlock: warlock_alone

warlock_alone: $(MODULE).ok

%.wlcmd:
	[ -f $@ ] || sccs get $@

scsi_files:
	cd ../scsi; $(MAKE) warlock;

ssd.ok: ssd.wlcmd $(SSD_FILES) scsi_files
	warlock -c ./ssd.wlcmd $(SSD_FILES) -l ddi_dki_impl \
	 -l scsi $(SCSI_FILES)
	touch $@
 
%.ll: $(UTSBASE)/sun/io/scsi/targets/%.c
	wlcc $(CFLAGS) $(CPPFLAGS) -o $@ $<
