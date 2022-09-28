#
# ident	"@(#)Makefile	1.24	96/06/10 SMI"
#
# Copyright (c) 1994,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/lp/cmd/Makefile
#

include 	../Makefile.lp

SUBDIRS =	lpc lptest lpadmin lpsched lpstat adaptor

LOCALPROG1 =	lp lpsystem lpmove
LOCALPROG2 =	accept
LOCALPROG =	$(LOCALPROG1) $(LOCALPROG2)
LOCALLINKS =	disable enable reject

SBINPROG =	lpfilter lpforms lpshut lpusers 
LIBLINKS =	$(SBINPROG)

PROG =		$(LOCALPROG) $(SBINPROG)

COMMONOBJ=	comb.o
OTHEROBJS=	cancel.o lp.o $(SBINPROG:=.o)

OBJS=		$(COMMONOBJ) $(OTHEROBJS)

ROOTLIBLPLOCLPROG=	$(LOCALPROG:%=$(ROOTLIBLPLOCL)/%)
ROOTSBINPROG=	$(SBINPROG:%=$(ROOTUSRSBIN)/%)
ROOTSYMLINKS=	$(LIBLINKS:%=$(ROOTLIB)/%)
ROOTSYMLINKS2=	$(LOCALLINKS:%=$(ROOTLIBLPLOCL)/%)


CPPFLAGS =	-I$(LPINC) $(CPPFLAGS.master)

# conditional assignments
#
accept:=	LDLIBS += $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP)
cancel:=	LDLIBS += $(LIBREQ) $(LIBMSG) $(LIBOAM) $(LIBLP)
lp:=		LDLIBS += $(LIBPRT) $(LIBREQ) $(LIBMSG) $(LIBOAM) $(LIBLP)
lpfilter:=	LDLIBS += $(LIBFLT) $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP) \
			-lgen
lpforms:=	LDLIBS += $(LIBFRM) $(LIBMSG) $(LIBREQ) $(LIBOAM) \
		$(LIBACC) $(LIBLP)
lpmove:=	LDLIBS += $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP)
lpshut:=	LDLIBS += $(LIBMSG) $(LIBOAM) $(LIBLP)
lpsystem:=	LDLIBS += $(LIBSYS) $(LIBMSG) $(LIBOAM) $(LIBLP) -lnsl
lpusers:=	LDLIBS += $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBUSR) $(LIBLP)

# for messaging catalog
#
POFILE = cmd.po
MSGSUBDIRS = adaptor lpadmin lpc lpsched lpstat lptest
POFILES1 = $(MSGSUBDIRS:%=%/%.po)
POFILES2 = $(OBJS:%.o=%.po)
POFILES = $(POFILES1) $(POFILES2)
XGETFLAGS += -a -x ../lp.xcl

.KEEP_STATE:

all:		$(PROG) $(SUBDIRS)

# for messaging catalog
#
catalog: $(MSGSUBDIRS) $(POFILES2)
	$(RM) $(POFILE)
	cat $(POFILES) > $(POFILE)

accept:		$(COMMONOBJ)
		$(LINK.c) -o $@ $(COMMONOBJ) $(LDFLAGS) $(LDLIBS)
		$(POST_PROCESS)

install:	$(PROG) $(ROOTLIBLPLOCLPROG) $(ROOTSBINPROG) \
		$(ROOTSYMLINKS) $(ROOTSYMLINKS2) $(SUBDIRS)

clean:		$(SUBDIRS)
		$(RM) $(OBJS)

clobber:	$(SUBDIRS) local_clobber

local_clobber:
		$(RM) $(OBJS) $(PROG) $(POFILE) $(POFILES)

strip:		$(SUBDIRS)
		$(STRIP) $(PROG)

lint:
		$(LINT.c) $(SRCS)

$(LOCALPROG1) $(SBINPROG): $$@.o
		$(LINK.c) -o $@ $@.o $(LDFLAGS) $(LDLIBS)
		$(POST_PROCESS)

$(ROOTSYMLINKS):
		$(RM) $@; $(SYMLINK) ../sbin/$(@F) $@

$(ROOTSYMLINKS2):
		$(RM) $@; $(SYMLINK) ./accept $@

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
