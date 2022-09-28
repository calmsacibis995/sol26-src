#
#ident	"@(#)Makefile	1.10	96/03/19 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/write/Makefile

PROG= write

include ../Makefile.cmd

SED= sed
DCFILE= $(PROG).dc

FILEMODE = 02555
OWNER = bin
GROUP = tty
XGETFLAGS += -a -x write.xcl

.KEEP_STATE:

CFLAGS += -v

all: $(PROG)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

$(DCFILE):
	$(RM) messages.po
	$(COMPILE.cpp) $(PROG).c | \
	$(XGETTEXT) -c TRANSLATION_NOTE -t -
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

include ../Makefile.targ
