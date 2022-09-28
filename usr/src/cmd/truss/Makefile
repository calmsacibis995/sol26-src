#
# ident	"@(#)Makefile	1.10	96/05/31 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/truss/Makefile
#

PROG=	truss

OBJS=	main.o listopts.o ipc.o actions.o expound.o codes.o print.o \
	pcontrol.o ramdata.o systable.o procset.o xstat.o

SRCS=	$(OBJS:%.o=%.c)

include	../Makefile.cmd

CFLAGS	+= -v

lint	:= LINTFLAGS =	-mux

CPPFLAGS += -D_LARGEFILE64_SOURCE=1

LDLIBS += -lsocket -lnsl

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include	../Makefile.targ
