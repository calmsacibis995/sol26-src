#
# ident "@(#)Makefile 1.4	96/05/01 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/lp/client/cancel/Makefile
#

include 	../Makefile.sp

PROG=		cancel

SRCS= 		cancel.c cancel_list.c

OBJS=		$(SRCS:.c=.o)

ROOTBINPROG=	$(PROG:%=$(ROOTBIN)/%)
ROOTUSRBINSYMLINK=	$(ROOTUSRUCB)/lprm

FILEMODE=	04511
OWNER=		root

CPPFLAGS +=	-I. -I$(NPRTINC) #$(CPPFLAGS.master)
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

lint:		lint_PROG

include		../../Makefile.targ
