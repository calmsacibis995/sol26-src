#
# Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
# All Rights Reserved
#
#ident "@(#)Makefile	1.4	96/04/22 SMI"
#
# cmd/lp/client/lpset/Makefile
#

include 	../Makefile.sp

PROG=		lpset

SRCS= 		$(PROG).c

OBJS=		$(SRCS:.c=.o)

ROOTBINPROG=	$(PROG:%=$(ROOTBIN)/%)

FILEMODE=	04511
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
