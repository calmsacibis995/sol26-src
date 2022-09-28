#
# Copyright (c) 1992-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.14	96/01/15 SMI"
#
# lib/scheme/Makefile
#

#SUBDIRS= unix dce test rhosts_auth dial_auth
SUBDIRS= unix rhosts_auth dial_auth sample

all:= 		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

all clean clobber lint: $(SUBDIRS)

install: all $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
