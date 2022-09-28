#
#pragma ident	"@(#)Makefile	1.11	96/05/09 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/in.named/named/Makefile
#

NMD=		in.named
XFER=		named-xfer
PROG=		$(NMD) $(XFER)
NMDOBJS=	db_dump.o db_load.o db_lookup.o db_reload.o db_save.o \
		db_update.o db_secure.o db_glue.o \
		ns_forw.o ns_init.o ns_main.o ns_maint.o ns_req.o \
		ns_resp.o ns_sort.o ns_stats.o ns_validate.o ns_ncache.o \
		storage.o dmalloc.o tree.o version.o
XFEROBJS=	named-xfer.o db_glue.o version.o
OBJS=		$(NMDOBJS) named-xfer.o
SRCS=		$(OBJS:%.o=%.c)
SUBDIRS=

include ../../../../Makefile.cmd

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint


SOLCOMPAT = -Dgethostbyname=res_gethostbyname \
	-Dgethostbyaddr=res_gethostbyaddr -Dgetnetbyname=res_getnetbyname \
	-Dgetnetbyaddr=res_getnetbyaddr -Dsethostent=res_sethostent \
	-Dendhostent=res_endhostent -Dgethostent=res_gethostent \
	-Dsetnetent=res_setnetent -Dendnetent=res_endnetent \
	-Dgetnetent=res_getnetent
COMPDIR   = ../compat
COMPINCL  = $(COMPDIR)/include
COMPLIB   = $(COMPDIR)/lib/lib44bsd.a

CPPFLAGS +=	-DSVR4 -D_SYS_STREAM_H $(SOLCOMPAT) \
		-D_PATH_XFER=\"/usr/sbin/named-xfer\" \
		-D_PATH_PIDFILE=\"/etc/named.pid\" \
		-I$(COMPINCL)

LDLIBS +=	$(COMPLIB) -lresolv -lsocket -lnsl

.KEEP_STATE:

all: $(PROG) $(SUBDIRS)

$(NMD): $(NMDOBJS)
	$(LINK.c) $(NMDOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(XFER): $(XFEROBJS)
	$(LINK.c) $(XFEROBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG) $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

clean:	$(SUBDIRS)
	$(RM) $(OBJS)

clobber: $(SUBDIRS)
	$(RM) $(OBJS) $(PROG)

lint: $(SUBDIRS)
	$(LINT.c) $(SRCS)

