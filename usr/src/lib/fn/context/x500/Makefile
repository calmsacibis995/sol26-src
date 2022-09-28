#
#ident	"@(#)Makefile	1.4	96/03/31 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/x500/Makefile
#

SUBDIRS= libldap common


install	:= TARGET = install
all	:= TARGET = all
clean	:= TARGET = clean
clobber	:= TARGET = clobber
debug	:= TARGET = debug
tcov	:= TARGET = tcov
analyse	:= TARGET = analyse

all debug install:  $(SUBDIRS)
clean clobber tcov analyse:  $(SUBDIRS)
install_h private_h check:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

.KEEP_STATE:
