#
#ident	"@(#)Makefile	1.4	94/08/12 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# lib/libkvm/platform/Makefile
#
sparc_SUBDIRS=	sparc
i386_SUBDIRS=	i386
ppc_SUBDIRS=	ppc
SUBDIRS=	$($(MACH)_SUBDIRS)

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

