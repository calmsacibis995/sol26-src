#
#ident	"@(#)Makefile	1.8	95/09/08 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= tr
XPG4PROG= tr

include ../Makefile.cmd

.KEEP_STATE:

CFLAGS += -v
$(XPG4):= CPPFLAGS += -DXPG4

all: $(PROG) $(XPG4)

install: all $(ROOTPROG) $(ROOTXPG4PROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
