#
#pragma ident	"@(#)Makefile	1.4	96/09/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libresolv2/Makefile
#
LIBRARY= libresolv.a
VERS= .2

PICS=   pics/herror.o pics/res_debug.o pics/res_data.o pics/res_comp.o \
	pics/res_init.o pics/res_mkquery.o pics/res_query.o pics/res_send.o \
	pics/getnetbyaddr.o pics/getnetbyname.o pics/getnetent.o \
	pics/getnetnamadr.o pics/gethnamaddr.o pics/sethostent.o \
	pics/nsap_addr.o pics/inet_addr.o

objs/%.o pics/%.o: %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

OBJECTS=herror.o res_debug.o res_data.o res_comp.o res_init.o res_mkquery.o \
	res_query.o res_send.o getnetbyaddr.o getnetbyname.o getnetent.o \
	getnetnamadr.o gethnamaddr.o sethostent.o nsap_addr.o inet_addr.o \
	hostnamelen.o

# include library definitions
include ../Makefile.lib

MAPFILE   = mapfile-vers

SOLCOMPAT = -Dgethostbyname=res_gethostbyname \
	-Dgethostbyaddr=res_gethostbyaddr -Dgetnetbyname=res_getnetbyname \
	-Dgetnetbyaddr=res_getnetbyaddr -Dsethostent=res_sethostent \
	-Dendhostent=res_endhostent -Dgethostent=res_gethostent \
	-Dsetnetent=res_setnetent -Dendnetent=res_endnetent \
	-Dgetnetent=res_getnetent -Dsocket=_socket
LOCDEFS   = -DUSE_OPTIONS_H
NAMEDDIR  = ../../cmd/cmd-inet/usr.sbin/in.named
COMPDIR   = $(NAMEDDIR)/compat
COMPINCL  = $(COMPDIR)/include

CPPFLAGS +=	-DSVR4 -D_SYS_STREAM_H -D_REENTRANT $(SOLCOMPAT) $(LOCDEFS) \
		-I$(COMPINCL) -I$(NAMEDDIR)
LDLIBS += -lsocket -lnsl -lc
DYNFLAGS +=	-M $(MAPFILE)

$(LINTLIB):= SRCS=llib-lresolv
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)

LIBS += $(LIBRARY) $(DYNLIB) $(LINTLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)

$(DYNLIB): $(MAPFILE)

# include library targets
include ../Makefile.targ

# install rule for lint library target
$(ROOTLINTDIR)/%: common/%
	$(INS.file)
