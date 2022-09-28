#
#ident	"@(#)Makefile	1.17	96/06/06 SMI"
#
# Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=		in.ftpd
OBJS1=		ftpd.o glob.o popen.o logwtmp.o vers.o
OTHEROBJ=	ftpcmd.o
OBJS=		$(OBJS1) $(OTHEROBJ)
SRCS=		$(OBJS1:%.o=%.c)

include ../../../Makefile.cmd

CPPFLAGS +=	-DSYSV -DSTRNET -DBSD_COMP -DBSDSECURE -D_FILE_OFFSET_BITS=64
LDLIBS +=	-lcmd -lsocket -lnsl -lbsm -lpam -ldl
YFLAGS +=		-d

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS) y.tab.h y.tab.c ftpcmd.c

lint:	lint_SRCS

include ../../../Makefile.targ
