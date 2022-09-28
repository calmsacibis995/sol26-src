#
# common/fonts/Makefile
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.5	95/11/09 SMI"
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
FONTS = small.pcf med.pcf large.pcf
ROOTFONTS = $(FONTS:%=$(ROOT_FONT_DIR)/%)
BASE_INS_DIR=$(ROOT)

#
#	Include common rules.
#
include $(UTSBASE)/Makefile.uts

#
# Override FILEMODE definition in $(UTSBASE)/Makefile.uts
#
FILEMODE	 = 644

#
#	Default build targets.
#
.KEEP_STATE:

all:	$(FONTS)

lint:

install:  	$(ROOTFONTS)

install_h:

check:

small.pcf:	7x14.pcf
	rm -f small.pcf
	ln 7x14.pcf small.pcf

med.pcf:	10x20.pcf
	rm -f med.pcf
	ln 10x20.pcf med.pcf

large.pcf:	12x24.pcf
	rm -f large.pcf
	ln 12x24.pcf large.pcf

#
#	Include common targets.
#
include $(UTSBASE)/Makefile.targ
