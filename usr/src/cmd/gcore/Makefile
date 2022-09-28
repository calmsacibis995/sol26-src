#
#ident	"@(#)Makefile	1.3	96/06/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= gcore

OBJS=	gcore.o elf_notes.o old_notes.o

SRCS=	$(OBJS:%.o=%.c)

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../Makefile.targ
