#
#pragma ident	"@(#)Makefile	1.7	96/05/09 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/in.named/tools/Makefile
#

PROG=		nstest
HOST_PROG=	host
DIG_PROG=	dig
DNSQUERY_PROG=	dnsquery
OBJS=		$(PROG:%=%.o)
SRCS=		$(OBJS:%.o=%.c)
HOST_OBJS=	host.o
HOST_SRCS=	host.c
DIG_OBJS=	dig.o nslookup/list.o nslookup/subr.o nslookup/debug.o \
		nslookup/send.o
DIG_SRCS=	dig.c
DNSQUERY_OBJS=	dnsquery.o
DNSQUERY_SRCS=	dnsquery.c
SUBDIRS=	nslookup

include ../../../../Makefile.cmd

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint

SOLCOMPAT = -Dgethostbyname=res_gethostbyname \
	-Dgethostbyaddr=res_gethostbyaddr -Dgetnetbyname=res_getnetbyname \
	-Dgetnetbyaddr=res_getnetbyaddr -Dsethostent=res_sethostent \
	-Dendhostent=res_endhostent -Dgethostent=res_gethostent \
	-Dsetnetent=res_setnetent -Dendnetent=res_endnetent \
	-Dgetnetent=res_getnetent
COMPDIR   = ../compat
COMPINCL  = $(COMPDIR)/include
COMPLIB   = $(COMPDIR)/lib/lib44bsd.a

CPPFLAGS +=	-DSVR4 -D_SYS_STREAM_H $(SOLCOMPAT) -I$(COMPINCL)
LDLIBS +=	$(COMPLIB) -lresolv -lsocket -lnsl

.KEEP_STATE:

all: $(SUBDIRS) $(PROG) $(HOST_PROG) $(DIG_PROG) $(DNSQUERY_PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(HOST_PROG): $(HOST_OBJS)
	$(LINK.c) $(HOST_OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(DIG_PROG): $(DIG_OBJS)
	$(LINK.c) $(DIG_OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(DNSQUERY_PROG): $(DNSQUERY_OBJS)
	$(LINK.c) $(DNSQUERY_OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG) $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:

clean: $(SUBDIRS)
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../../Makefile.targ
