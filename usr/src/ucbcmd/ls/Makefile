#
#ident	"@(#)Makefile	1.4	96/04/18 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= ls

include ../Makefile.ucbcmd

CPPFLAGS += -D_FILE_OFFSET_BITS=64

FILEMODE=	755

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg
