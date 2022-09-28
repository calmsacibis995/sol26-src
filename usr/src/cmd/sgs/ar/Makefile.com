#
#ident	"@(#)Makefile.com	1.1	96/04/11 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

PROG=		ar
XPG4PROG=	ar

include		$(SRC)/cmd/Makefile.cmd
include		$(SRC)/cmd/sgs/Makefile.com

COMOBJS=	main.o		file.o		cmd.o		global.o \
		message.o	sbfocus_enter.o

POFILE=		../ar.po

OBJS=		$(COMOBJS:%=objs/%)
XPG4OBJS=	$(COMOBJS:%=objs.xpg4/%)

CPPFLAGS=	-I../../include -DBROWSER $(CPPFLAGS.master)
CFLAGS +=	-v
LDLIBS +=	-lelf
LINTFLAGS +=	$(LDLIBS)

SED=		sed

$(XPG4) :=	CPPFLAGS += -DXPG4

SRCS=		$(COMOBJS:%.o=../common/%.c)

CLEANFILES +=	$(OBJS) $(XPG4OBJS) $(LINTOUT)


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1
