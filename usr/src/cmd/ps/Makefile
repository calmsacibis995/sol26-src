#
#ident	"@(#)Makefile	1.7	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= ps

include ../Makefile.cmd

XGETFLAGS += -a -x ps.xcl

FILEMODE= 04555
OWNER= root
GROUP= sys

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
