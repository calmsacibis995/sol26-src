#
# ident	"@(#)Makefile	1.13	96/04/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/cmd/bsd/lpc/Makefile
#

include 	../../Makefile.lp

PROG=		lpc

SRCS= 		cmds.c cmdtab.c lpc.c process.c topq.c fatalmsg.c sndrcv.c \
		global.c

OBJS=		$(SRCS:.c=.o)


CPPFLAGS =	-I. -I$(LPINC) $(CPPFLAGS.master)
LDLIBS +=	$(LIBREQ) $(LIBMSG) $(LIBOAM) \
		$(LIBPRT) $(LIBSEC) $(LIBLP) -lgen -lcurses -lintl -lw

# for messaging catalog
#
POFILE = lpc.po
XGETFLAGS +=-a -x $(LPROOT)/lp.xcl

.KEEP_STATE:

.PARALLEL: $(OBJS)

all:		$(PROG)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

catalog:        $(POFILE)
 
$(POFILE):      $(SRC)
	 $(RM)	$@
	 $(COMPILE.cpp)   $(SRCS) > $(POFILE).i
	 $(XGETTEXT)     $(XGETFLAGS)    $(POFILE).i
	 sed "/^domain/d" messages.po    > $@
	 $(RM) $(POFILE).i messages.po


install:	all .WAIT $(ROOTUSRUCBPROG)

clean:
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

lint:		lint_SRCS

include		../../../Makefile.targ
