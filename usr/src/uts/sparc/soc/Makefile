#
# uts/sparc/soc/Makefile
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile 1.5 96/10/17 SMI"
#
#	This makefile drives the production of the soc driver kernel
#	module, which is used for the Sparc Storage Array.
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
MODULE		= soc
OBJECTS		= $(SOC_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SOC_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

CLOBBERFILES	+= $(MODULE)

# 
# lint pass one enforcement 
# 
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

all browser:	$(MODULE)
browser:=	CFLAGS += -xsb

lint:		$(MODULE).lint

install:  	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

#
#       Defines for local commands.
#
WARLOCK         = warlock
WLCC            = wlcc
SCCS            = sccs
TOUCH           = touch
TEST            = test

#
#       lock_lint rules
#
warlock:	$(MODULE).ok

%.wlcmd:
	$(TEST) -f $@  || $(SCCS) get $@

soc.ok: soc.ll	soc.wlcmd
	$(WARLOCK) -c soc.wlcmd soc.ll -l ddi_dki_impl -l scsi
	$(TOUCH) $@

%.ll: $(UTSBASE)/sun/io/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -D_PCIBUS_SOLARIS -o $@ $<

