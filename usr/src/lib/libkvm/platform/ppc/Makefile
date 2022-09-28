#
#ident	"@(#)Makefile	1.6	94/08/04 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# lib/libkvm/platform/ppc/Makefile
#
SUBDIRS=	prep

# conditional assignments
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
test :=		TARGET= test

.KEEP_STATE:

all install clean clobber lint test: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

