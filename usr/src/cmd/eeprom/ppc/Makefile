#
#ident	"@(#)Makefile	1.3	95/12/01 SMI"
#
# Copyright (c) 1993-1995 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/eeprom/ppc/Makefile
#
SUBDIRS= prep

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint : $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
