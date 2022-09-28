#
#ident	"@(#)Makefile	1.5	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= ps

include ../Makefile.ucbcmd

LDLIBS +=	-lelf

FILEMODE=	4755
OWNER= root
GROUP= sys

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg
