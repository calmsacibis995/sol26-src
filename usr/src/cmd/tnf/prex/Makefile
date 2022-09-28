#
#ident	"@(#)Makefile	1.15	96/05/22 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/tnf/prex/Makefile
#

PROG=		prex

OBJS.c=		source.o	\
		main.o		\
		util.o		\
		expr.o		\
		spec.o		\
		set.o		\
		queue.o		\
		cmd.o		\
		new.o		\
		list.o		\
		fcn.o		\
		prbk.o		\
		help.o

OBJS.yl=	prexgram.o	\
		prexlex.o

OBJS=		$(OBJS.c) $(OBJS.yl)

SRCS= $(OBJS.c:%.o=%.c) $(OBJS.yl:%.o=%.c)

include	../../Makefile.cmd

POFILE= prex.po
POFILES= $(OBJS.c:%.o=%.po)

#YFLAGS=	-d -t -v
YFLAGS=		-d
LFLAGS=		-v
LDLIBS +=	-lgen -ltnfctl -lc
# Uncomment the following line for a debug build
# COPTFLAG =	-g -DDEBUG -v

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTBIN) $(ROOTPROG)

$(ROOTBIN):
	$(INS.dir)

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat     $(POFILES)      > $@

clean:
	$(RM) $(OBJS) y.tab.h prexlex.c prexgram.c

lint: $(SRCS) lint_SRCS

include	../../Makefile.targ
