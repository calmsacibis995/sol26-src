#
#ident	"@(#)Makefile	1.3	94/11/04 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# uts/adb/platform/ppc/Makefile
#
SUBDIRS=	prep

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

