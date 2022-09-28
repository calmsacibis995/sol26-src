#
#ident	"@(#)Makefile	1.2	96/04/24 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/x500/common/Makefile
#

LIBRARYCCC = fn_ctx_x500.a
VERS = .1

X500_OBJECTS = x500.o X500Context.o X500XFN.o X500Trace.o
LDAP_OBJECTS = LDAPDUA.o LDAPData.o
XDS_OBJECTS = XDSDUA.o XDSXOM.o XDSInfo.o XDSExtern.o XDSDN.o

OBJECTS = $(X500_OBJECTS) $(LDAP_OBJECTS) $(XDS_OBJECTS)

objs/%.o profs/%.o pics/%.o: ../ldap/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../xds/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

TOUCH = touch

# include library definitions
include ../../../Makefile.libfn

FILES =		x500.conf

ROOTETC= 	$(ROOT)/etc
ROOTETCFN= 	$(ROOTETC)/fn
ROOTETCFNFILES= $(FILES:%=$(ROOTETCFN)/%)

$(ROOTETCFN) := DIRMODE = 775
$(ROOTETCFN) := OWNER = root
$(ROOTETCFN) := GROUP = sys

$(ROOTETCFNFILES) := OWNER = root
$(ROOTETCFNFILES) := GROUP = sys

SRCS = $(LDAP_OBJECTS:%.o=../ldap/%.cc) $(XDS_OBJECTS:%.o=../xds/%.cc)

LIBS = $(DYNLIBCCC)
LDFLAGS += -L$(FNLIBDIR)
LDLIBS += -lxfn -lfn_spf -lnsl $(LDFLAGS) -lldap -ldl -lC -lc 

FNRPATH=	/usr/lib/fn

CPPFLAGS += -I. -I../libldap -I../ldap -I../xds

.KEEP_STATE:


all: $(LIBS)

debug install tcov: all $(ROOTLIBDIR) $(ROOTETC) $(ROOTETCFN) \
			$(ROOTLIBS) $(ROOTETCFNFILES)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

$(ROOTETC) $(ROOTETCFN):
	$(INS.dir)

$(ROOTETCFN)/%: %
	$(INS.file)

# include library targets
include ../../../Makefile.targ

