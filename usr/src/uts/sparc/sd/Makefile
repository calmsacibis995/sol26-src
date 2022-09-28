#
# uts/sparc/sd/Makefile
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.31	96/10/04 SMI"
#
#	This makefile drives the production of the sd driver kernel module.
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
MODULE		= sd
OBJECTS		= $(SD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SD_OBJS:%.o=$(LINTS_DIR)/%.ln)
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

all:		$(MODULE) $(SRC_CONFILE)

lint:		$(MODULE).lint

install:	$(MODULE) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Include common targets.
#
include	$(UTSBASE)/sparc/Makefile.targ


#
#	Defines for local commands.
#
WARLOCK		= warlock
WLCC		= wlcc
TOUCH		= touch
SCCS		= sccs
TEST		= test
MAKE		= make

#
# Warlock targets
#
# Note that in warlock_with_{esp,isp} it is important to load sd.ll
# before {isp,esp}.ll; the reason is that both have _init/_info/_fini
# and warlock can only handle one extern function by a given name;
# any loaded after the first are ignored.

SD_FILES   = $(MODULE).ll
SCSI_FILES = $(SCSI_OBJS:%.o=-l ../scsi/%.ll)

warlock: warlock_alone warlock_with_esp warlock_with_isp warlock_with_fas \
	warlock_with_glm

warlock_alone: $(MODULE).ok

%.wlcmd:
	$(TEST) -f $@  || $(SCCS) get $@

warlock_with_esp: sd_with_esp.wlcmd $(SD_FILES) scsi_files esp_files 
	$(WARLOCK) -c ./sd_with_esp.wlcmd \
		$(SD_FILES) ../esp/esp $(SCSI_FILES) ../esp/reset_notify \
		-l ddi_dki_impl -l scsi

warlock_with_fas: sd_with_fas.wlcmd $(SD_FILES) scsi_files fas_files 
	$(WARLOCK) -c ./sd_with_fas.wlcmd \
		$(SD_FILES) ../fas/fas \
		../fas/fas_callbacks \
		../fas/reset_notify \
		$(SCSI_FILES) \
		-l ddi_dki_impl -l scsi

warlock_with_isp: sd_with_isp.wlcmd $(SD_FILES) scsi_files isp_files 
	$(WARLOCK) -c ./sd_with_isp.wlcmd \
		$(SD_FILES) ../isp/isp $(SCSI_FILES) ../isp/reset_notify \
		-l ddi_dki_impl -l scsi

warlock_with_glm: sd_with_glm.wlcmd $(SD_FILES) scsi_files glm_files
	$(WARLOCK) -c ./sd_with_glm.wlcmd \
		$(SD_FILES) ../glm/glm $(SCSI_FILES) ../glm/reset_notify \
		-l ddi_dki_impl -l scsi

scsi_files:
	@cd ../scsi; pwd; $(MAKE) warlock

esp_files:
	@cd ../esp; pwd; $(MAKE) warlock

fas_files:
	@cd ../fas; pwd; $(MAKE) warlock

isp_files:
	@cd ../isp; pwd; $(MAKE) warlock

glm_files:
	@cd ../glm; pwd; $(MAKE) warlock

sd.ok: sd.wlcmd $(SD_FILES) scsi_files
	$(WARLOCK) -c ./sd.wlcmd $(SD_FILES) -l ddi_dki_impl -l scsi $(SCSI_FILES)
	$(TOUCH) $@

%.ll: $(UTSBASE)/sun/io/scsi/targets/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<
