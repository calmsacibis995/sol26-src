#
#ident	"@(#)Makefile	1.13	94/08/04 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/adb/Makefile
#

include ../Makefile.cmd

#
# One for each ISA.  No longer worried about platforms.
#
SUBDIRS= $(MACH)

TXTS= README common/MakeCommon

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

all:	$(SUBDIRS) $(TXTS)

install clean clobber lint : $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
