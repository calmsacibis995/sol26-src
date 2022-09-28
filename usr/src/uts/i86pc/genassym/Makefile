#
# uts/i86pc/genassym/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.6	94/07/06 SMI"
#
#	This makefile drives the production of assym.s through genassym.
#
#	i86pc implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Default build targets.
#
.KEEP_STATE:

all:	$(DSF_DIR)/assym.s

#
#	genassym build.
#
$(DSF_DIR)/genassym:    $(GENASSYM_SRC)
	$(NATIVECC) $(ALWAYS_DEFS) $(GENASSYM_DEFS) $(INCLUDE_PATH) \
	-o $(DSF_DIR)/genassym $(GENASSYM_SRC)

#
#	Execute genassym to produce assym.s (should be assym.h).
#
$(DSF_DIR)/assym.s:     $(DSF_DIR)/genassym
	$(DSF_DIR)/genassym > $(DSF_DIR)/assym.s

#
#	Nothing to install.
#
install:

clean.lint:

clobber.lint: clean.lint

clean:
	-$(RM) $(DSF_DIR)/assym.s $(DSF_DIR)/genassym

clobber: clean
