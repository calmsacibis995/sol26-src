#
# Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
# All Rights Reserved
#
#ident "@(#)Makefile	1.5	96/05/02 SMI"
#
# cmd/lp/client/lpget/Makefile
#

include 	../Makefile.sp

PROG=		lpget

SRCS= 		$(PROG).c

OBJS=		$(SRCS:.c=.o)

ROOTBINPROG=	$(PROG:%=$(ROOTBIN)/%)

FILEMODE=	0511
OWNER=		root

CPPFLAGS +=	-I$(NPRTINC)
LDLIBS +=	$(LIBNPRT)

# for messaging catalog
#
XGETFLAGS +=-a -x $(LPROOT)/lp.xcl


.KEEP_STATE:

all:		$(PROG)

catalog:        $(POFILE)


install:	all $(ROOTBIN) $(ROOTBINPROG) 

$(ROOTBIN):
	$(INS.dir)


clean:
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

cstyle:
		cstyle $(SRCS)

lint:		lint_PROG

include		../../Makefile.targ
