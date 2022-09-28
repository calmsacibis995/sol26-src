#
#ident	"@(#)Makefile	1.30	96/09/24 SMI"
#
# Copyright (c) 1993-1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libsocket/Makefile
#
LIBRARY= libsocket.a
VERS= .1

CLOBBERFILES += lint.out

INETOBJS=  \
bindresvport.o  bootparams_getbyname.o byteorder.o     ether_addr.o \
getnetent.o     getnetent_r.o   getprotoent.o    getprotoent_r.o \
getservent.o    getservent_r.o	getservbyname_r.o \
inet_lnaof.o    inet_mkaddr.o   inet_network.o 	netmasks.o \
rcmd.o          rexec.o         ruserpass.o

SOCKOBJS=     _soutil.o    socket.o      socketpair.o weaks.o

OBJECTS= $(INETOBJS) $(SOCKOBJS)

# libsocket build rules
objs/%.o profs/%.o pics/%.o: inet/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: socket/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# include library definitions
include ../Makefile.lib

MAPFILES=	common/mapfile-vers  $(MACH)/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

CPPFLAGS +=	-DSYSV -D_REENTRANT
%/rcmd.o :=	CPPFLAGS += -DNIS
LDLIBS +=	-lnsl -lc
DYNFLAGS +=	$(MAPOPTS)


SRCS=	$(INETOBJS:%.o=inet/%.c) $(SOCKOBJS:%.o=socket/%.c)

LIBS += $(DYNLIB) $(LINTLIB)

$(LINTLIB):= SRCS=common/llib-lsocket
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)

.KEEP_STATE:

all: $(LIBS)

install: $(LIBS) $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)

# include library targets

$(DYNLIB): $(MAPFILES) 

include ../Makefile.targ

# install rule for lint library target
$(ROOTLINTDIR)/%: common/%
	$(INS.file)
