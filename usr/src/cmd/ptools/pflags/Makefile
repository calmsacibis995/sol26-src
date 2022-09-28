#
#ident	"@(#)Makefile	1.3	96/06/18 SMI"
#
# Copyright (c) 1994-1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/pflags/Makefile
#

PROG = pflags

include ../../Makefile.cmd
include ../Makefile.com

FILEMODE = 0555
OWNER = bin
GROUP = bin

CPPFLAGS +=	-I../libproc
LDLIBS +=	../libproc/libproc.a

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROCBINPROG)

clean:

lint:   lint_PROG

include ../../Makefile.targ
