#
#pragma ident	"@(#)Makefile	1.33	96/06/03 SMI"
#
# Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=		finger rdate ruptime rwho whois
SUIDPROG=	rcp rlogin rsh
ALL=		$(PROG) $(SUIDPROG) $(STATPROG)
SRCS=		$(ALL:%=%.c)
STATPROG=	rcp

SUBDIRS=	ftp netstat rdist talk tftp telnet 
SUBDIR1=	talk 


include ../../Makefile.cmd

POFILES= 	talk/talk.po
POFILE=		usr.bin.po

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
catalog:=	TARGET= catalog

ROOTSUIDPROG=	$(SUIDPROG:%=$(ROOTBIN)/%)
$(ROOTSUIDPROG)	:=	FILEMODE=	04555
$(ROOTSUIDPROG)	:=	OWNER=		root
$(ROOTSTATPROG)	:=	FILEMODE=	04555
$(ROOTSTATPROG)	:=	OWNER=		root

CPPFLAGS +=	-DSYSV -DSTRNET -DBSD_COMP
LDLIBS +=	-lsocket -lnsl -lsec

ROOTSUNWRCP=	$(ROOT)/usr/lib/sunw,rcp
ROOTRSHSYMLINK=	$(ROOT)/usr/ucb/rsh
ROOTREMSHSYMLINK=$(ROOT)/usr/bin/remsh


finger := LDLIBS += -lcurses -lcmd
finger := CFLAGS += -v
# A list of static libraries that use libdl_stubs which fakes the real
# dynamic linker by returning static symbols from the pre-linked libraries.
#
STATICLIBS = -ldl_stubs -lnss_files -lnss_nis -lnss_nisplus \
	-lnss_compat -lnss_dns -lresolv -lsocket -lstraddr -lnsl
$(STATIC) := LDLIBS += -lsocket -lnsl -lc $(STATICLIBS)

.KEEP_STATE:

all: $(ALL) $(STATIC) $(SUBDIRS)

install: all .WAIT $(ROOTPROG) $(ROOTSUIDPROG) $(ROOTSTATPROG) \
	$(SUBDIRS) $(ROOTSUNWRCP) $(ROOTRSHSYMLINK) $(ROOTREMSHSYMLINK)

#
# messaging
#
catalog: 
	cd talk; $(MAKE) $(MFLAGS) $(TARGET)
	$(RM) usr.bin.po
	cat $(POFILES)	> usr.bin.po

$(STATIC): $(STATPROG).c
	$(LINK.c) -o $(STATIC) $(STATPROG).c $(LDLIBS)
	$(POST_PROCESS)

$(ROOTSUNWRCP):
	$(RM) $@; $(SYMLINK) ../bin/rcp $@

$(ROOTRSHSYMLINK):
	$(RM) $@; $(SYMLINK) ../bin/rsh $@

$(ROOTREMSHSYMLINK):
	$(RM) $@; $(SYMLINK) rsh $@

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:

clean: $(SUBDIRS)

clobber: $(SUBDIRS) clobber_local

clobber_local:
	echo $(CLOBBERFILES)
	$(RM) $(ALL)  $(CLOBBERFILES) 

lint:	$(SUBDIRS)
	$(LINT.c) $(SRCS)
