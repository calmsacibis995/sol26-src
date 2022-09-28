#
# Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
# All Rights Reserved
#
# ident "@(#)Makefile	1.6	96/05/09 SMI"
#
# cmd/lp/client/lp/Makefile
#

include 	../Makefile.sp

PROG=		lp

SRCS= 		$(PROG).c

OBJS=		$(SRCS:.c=.o)

ROOTBINPROG=	$(PROG:%=$(ROOTBIN)/%)
ROOTUSRBINSYMLINK=	$(ROOTUSRUCB)/lpr
PRINTSYMLINK=	$(ROOTLIB)/print/printd
$(ROOTLIB)/print	:=	OWNER=root
$(ROOTLIB)/print	:=	GROUP=lp

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


install:	all $(ROOTBIN) $(ROOTLIB)/print $(ROOTBINPROG) $(ROOTUSRBINSYMLINK) $(PRINTSYMLINK)

$(ROOTBIN) $(ROOTLIB)/print:
	$(INS.dir)

$(ROOTUSRBINSYMLINK):
	$(RM) $@; $(SYMLINK) ../bin/$(PROG) $@

$(PRINTSYMLINK):
	$(RM) $@; $(SYMLINK) ../../bin/$(PROG) $@


clean:
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

cstyle:
		cstyle $(SRCS)

lint:		lint_PROG

include		../../Makefile.targ
