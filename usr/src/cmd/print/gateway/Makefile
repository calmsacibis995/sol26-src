#
# ident "@(#)Makefile 1.13	96/06/10 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All Rights Reserved
#
# server/gateway/adaptor/Makefile
#

include 	../Makefile.sp

SUBDIRS=	translators

PROG=		in.lpd

SRCS= 		main.c adaptor.c

OBJS=		$(SRCS:.c=.o)


FILEMODE=	0555
OWNER=		root
GROUP=		bin


CPPFLAGS +=	-I. -I$(NPRTINC) #$(CPPFLAGS.master)
LDLIBS +=	-lsocket -lnsl -ldl $(LIBNPRT)

# for messaging catalog
#
POFILE= gateway.po
POFILES= $(SRCS:%.c=%.po)
XGETFLAGS +=-a -x $(LPROOT)/lp.xcl


.KEEP_STATE:

all:		$(PROG) $(SUBDIRS)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

catalog:        $(POFILE)

$(POFILE): $(POFILES)
	$(RM) $@
	cat     $(POFILES) > $@

install:	all $(ROOTLIBPRINTPROG) $(SUBDIRS)

clean:		$(SUBDIRS)
		$(RM) $(OBJS)

strip:		$(SUBDIRS)
		$(STRIP) $(PROG)

cstyle:
	cstyle $(SRCS)

lint:		lint_SRCS

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include		../../Makefile.targ
