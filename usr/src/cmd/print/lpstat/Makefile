#
# Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
# All Rights Reserved
#
# ident "@(#)Makefile	1.5	96/05/01 SMI"
#
# cmd/lp/client/lpstat/Makefile
#

include 	../Makefile.sp

PROG=		lpstat

SRCS= 		lpstat.c sysv-functions.c bsd-functions.c parse_bsd.c

OBJS=		$(SRCS:.c=.o)

ROOTBINPROG=	$(PROG:%=$(ROOTBIN)/%)
ROOTUSRBINSYMLINK=	$(ROOTUSRUCB)/lpq

FILEMODE=	04511
OWNER=		root

CPPFLAGS +=	-I. -I$(NPRTINC)
LDLIBS +=	$(LIBNPRT)

# for messaging catalog
#
XGETFLAGS +=-a -x $(LPROOT)/lp.xcl


.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

catalog:        $(POFILE)


install:	all $(ROOTBIN) $(ROOTBINPROG) $(ROOTUSRBINSYMLINK)

$(ROOTBIN):
	$(INS.dir)

$(ROOTUSRBINSYMLINK):
	$(RM) $@; $(SYMLINK) ../bin/$(PROG) $@

clean:
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

cstyle:
	cstyle $(SRCS)

lint:		lint_PROG

include		../../Makefile.targ
