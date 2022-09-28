#
# uts/i86/devfs/Makefile
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#pragma ident	"@(#)Makefile	1.7	93/10/21 SMI"
#
#	This makefile drives the production of the devfs support files
#
#	i86 architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

#
#	Define the module and object file sets.
#
SRCDIR		= $(UTSBASE)/i86/os
DRVFILES	= name_to_major minor_perm driver_aliases name_to_sysnum driver_classes
PTIFILE		= path_to_inst
FILEMODE	= 644
OWNER		= root
GROUP		= sys
ROOTETC		= $(ROOT)/etc

SRCFILES	= $(DRVFILES:%=$(SRCDIR)/%) $(PTIFILE:%=$(SRCDIR)/%)
ROOTDRVFILES	= $(DRVFILES:%=$(ROOTETC)/%)
ROOTPTIFILES	= $(PTIFILE:%=$(ROOTETC)/%)

#
#	Default build targets.
#
.KEEP_STATE:

all: $(SRCFILES)

install: all $(ROOTDRVFILES) $(ROOTPTIFILES)

$(ROOTETC)/%: $(ROOTETC) $(SRCDIR)/%
	$(INS.file)

$(ROOT_DRV_DIR)/%: $(ROOT_DRV_DIR) $(SRCDIR)/%
	$(INS.file)

$(ROOTETC):
	$(INS.dir)

# 
#	No lints, so keep the clobber and clean targets reasonable. 
# 
CLEANLINTFILES	= Nothing_to_remove
CLEANFILES	= Nothing_to_remove
CLOBBERFILES	= Nothing_to_remove

#
#	Include common targets.
#
include $(UTSBASE)/i86/Makefile.targ
