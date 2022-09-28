#
#ident "@(#)Makefile	1.2	96/04/16 SMI"
#
# Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/cpr/Makefile
#

# All the real work gets done in kernel-architecture directories,
# and that's where the targets wind up. This makefile just gets us there.

SUBDIRS = sun4u sun4m

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
clean.lint	:=	TARGET= clean.lint

.KEEP_STATE:

all install clean clobber lint clean.lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
