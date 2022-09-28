#
# ident	"@(#)Makefile	1.6	96/04/23 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
# cmd/lp/model/netpr
#

PROG=		netpr

include		../../Makefile.lp

PURIFYOPTS      = -logfile=/tmp/errs.%p
PURIFY          = purify $(PURIFYOPTS)

LIBNPRT =	-L$(ROOT)/usr/lib -lprint $(LIBINTL)


CPPFLAGS =	-I. -I$(LPINC) 	\
		$(CPPFLAGS.master)

HDRS=				\
		netpr.h		\
		netdebug.h

SRCS=				\
		netpr.c		\
		misc.c		\
		net.c		\
		tcp_misc.c	\
		bsd_misc.c

OBJS=		$(SRCS:.c=.o)


LPLIBS =			\
		$(LIBMSG)	\
		$(LIBFRM)	\
		$(LIBREQ)	\
		$(LIBPRT)	\
		$(LIBCLS)	\
		$(LIBACC)	\
		$(LIBFLT)	\
		$(LIBUSR)	\
		$(LIBOAM)	\
		$(LIBLP)	\
		$(LIBSEC)	\
		$(LIBSYS)	\
		$(LIBNPRT)

# SYSLIBS=	-lcurses -lgen -lcurses -lnsl
SYSLIBS=	-lgen -lnsl -lsocket

LDLIBS +=	$(LPLIBS) $(SYSLIBS) -lxfn 
ROOTLIBLPBIN =		$(ROOTLIBLP)/bin

ROOTNETPRPROG =            $(PROG:%=$(ROOTLIBLPBIN)/%)

FILEMODE=	04511
OWNER=		root
GROUP=		bin


# for message catalog
#
POFILE= netpr.po
XGETFLAGS += -a -x $(LPROOT)/lp.xcl


.KEEP_STATE:

all:		$(PROG)

install:	all $(ROOTLIBLPBIN) $(ROOTNETPRPROG)

$(ROOTLIBLPBIN):
	$(INS.dir)

$(ROOTLIBLPBIN)/%:	%
	$(INS.file)


$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

$(PROG).pure:	$(OBJS)
		$(PURIFY) $(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

catalog:        $(POFILE)

$(POFILE):      $(SRCS)
	$(RM)	$(POFILE)
	$(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	$(XGETTEXT)     $(XGETFLAGS)    $(POFILE).i
	sed "/^domain/d"        messages.po > $(POFILE)
	$(RM) $(POFILE).i messages.po



clean:
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

lint:		lint_SRCS

include		../../../Makefile.targ
