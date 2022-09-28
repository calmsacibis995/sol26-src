#
#ident "@(#)Makefile	1.14	94/08/04 SMI"
#
# Copyright (c) 1992-1994, by Sun Microsystems, Inc.
#
# uts/adb/Makefile
#
SUBDIRS=	$(MACH) platform

# conditional assignments
all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

