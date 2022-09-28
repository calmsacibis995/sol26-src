#
#ident	"@(#)Makefile	1.3	96/08/19 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=	patch

SRCS=	patch.c pch.c util.c file.c

OBJS=	$(SRCS:%.c=%.o)

include ../Makefile.cmd

CFLAGS += -v

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG)

install: all $(ROOTPROG)

lint:	lint_SRCS

clean:
	$(RM) $(OBJS)

$(PROG):	$(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

include		../Makefile.targ

