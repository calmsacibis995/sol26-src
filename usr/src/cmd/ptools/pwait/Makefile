#
#ident	"@(#)Makefile	1.4	96/06/19 SMI"
#
# Copyright (c) 1994-1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/pwait/Makefile
#

PROG = pwait

include ../../Makefile.cmd
include ../Makefile.com

FILEMODE = 0555
OWNER = bin
GROUP = bin

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROCBINPROG)

clean:

lint:   lint_PROG

include ../../Makefile.targ
