#
#pragma ident	"@(#)Makefile	1.9	96/07/25 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/in.named/tools/nslookup/Makefile
#

PROG=		nslookup
HELPFILE=	nslookup.help
OBJS=		main.o getinfo.o debug.o send.o skip.o list.o subr.o \
		commands.o
SRCS=		$(OBJS:%.o=%.c)

include ../../../../../Makefile.cmd

COMPDIR   = ../../compat
COMPINCL  = $(COMPDIR)/include
COMPLIB   = $(COMPDIR)/lib/lib44bsd.a

CPPFLAGS +=	-DSVR4 -D_SYS_STREAM_H \
		-D_PATH_HELPFILE=\"/usr/lib/$(HELPFILE)\" -I$(COMPINCL)
LDLIBS +=	$(COMPLIB) -ll -lresolv -lsocket -lnsl

.KEEP_STATE:


all: $(PROG) $(HELPFILE)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

ROOTLIBHELP=    $(ROOTLIB)/$(HELPFILE)

$(ROOTLIBHELP):=	FILEMODE = 0444

install: all $(ROOTUSRSBINPROG) $(ROOTLIBHELP)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../../../Makefile.targ
