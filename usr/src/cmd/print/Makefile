#
# Copyright (c) 1994,1996 by SunSoft, Inc.
# All rights reserved.
#
# "@(#)%M 1.6     96/06/10 SunSoft"
#
# cmd/print/Makefile
#

include ../Makefile.cmd


SUBDIRS  =		\
		scripts	\
		lpget	\
		lpset	\
		lp	\
		lpstat	\
		cancel	\
		lpmove	\
		conv_fix	\
		gateway	

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
strip :=	TARGET= strip

# for messaging catalog
#
_msg    :=      TARGET = catalog

POFILE= print.po
POFILES= $(SUBDIRS:%=%/%.po)
XGETFLAGS=-a -x $(LPROOT)/lp.xcl

.KEEP_STATE:

all install:	$(SUBDIRS)

_msg:   $(MSGDOMAIN) $(SUBDIRS)
	$(RM)	$(POFILE)
	cat     $(POFILES)      > $(POFILE)
	$(RM)  $(MSGDOMAIN)/$(POFILE)
	cp $(POFILE)    $(MSGDOMAIN)

clean strip cstyle lint:	$(SUBDIRS)

clobber: $(SUBDIRS) local_clobber

local_clobber:
	$(RM) $(POFILE) $(POFILES) $(CLOBBERFILES)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
