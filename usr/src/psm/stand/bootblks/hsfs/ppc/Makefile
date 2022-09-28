#
#ident	"@(#)Makefile	1.5	95/06/27 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/hsfs/ppc/Makefile
#
BASEDIR = ../..

include $(BASEDIR)/hsfs/Makefile.hsfs

SUBDIRS	= unix prep

all	:=	TARGET= all
install	:=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint

.KEEP_STATE:

all install lint clean clobber: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
