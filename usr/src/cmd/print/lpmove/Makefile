#
# Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
# All Rights Reserved
#
#ident "@(#)Makefile	1.5	96/05/01 SMI"
#
# cmd/lp/client/lpmove/Makefile
#

include 	../Makefile.sp

PROG=		lpmove

SRCS= 		$(PROG).c

OBJS=		$(SRCS:.c=.o)

ROOTUSRSBINPROG=	$(PROG:%=$(ROOTUSRSBIN)/%)
ROOTUSRLIBSYMLINK=	$(PROG:%=$(ROOTLIB)/%)

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


install:	all $(ROOTUSRSBIN) $(ROOTUSRSBINPROG) $(ROOTUSRLIBSYMLINK)


$(ROOTUSRSBIN):
	$(INS.dir)

$(ROOTUSRLIBSYMLINK):
	$(RM) $@; $(SYMLINK) ../sbin/$(PROG) $@


clean:
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

cstyle:
		cstyle $(SRCS)

lint:		lint_PROG

include		../../Makefile.targ
