#
# Copyright (c) 1994 by SunSoft, Inc.
#
# "@(#)%M 1.2     96/03/04 SunSoft"
#
# cmd/print/gateway/translators/Makefile
#

SUBDIRS =	test cascade

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
strip :=	TARGET= strip
catalog :=      TARGET= catalog

# for messaging catalog
#
POFILE= lib.po
POFILES= $(SUBDIRS:%=%/%.po)
XGETFLAGS=-a -x $(LPROOT)/lp.xcl

.KEEP_STATE:

all install:	$(SUBDIRS)

clean strip cstyle lint:	$(SUBDIRS)

clobber: $(SUBDIRS) local_clobber

local_clobber:
	$(RM) $(POFILE) $(POFILES) $(CLOBBERFILES)

catalog:        $(SUBDIRS)
	 $(RM)	$(POFILE)
	 cat $(POFILES)  > $(POFILE)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
