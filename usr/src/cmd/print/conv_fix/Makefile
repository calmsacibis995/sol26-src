#
# Copyright (c) 1994 by SunSoft
#
# "@(#)Makefile	1.7	96/05/10 SunSoft"
#
# cmd/lp/client/conv_fix/Makefile
#

include 	../Makefile.sp

PROG=		conv_fix

SRCS= 		$(PROG).c

OBJS=		$(SRCS:.c=.o)

ROOTLIBPRINTPROG=	$(PROG:%=$(ROOTLIBPRINT)/%)

FILEMODE=	0555
OWNER=		lp

CPPFLAGS +=	-I$(NPRTINC)
LDLIBS +=	$(LIBNPRT)

# for messaging catalog
#
XGETFLAGS +=-a -x $(LPROOT)/lp.xcl


.KEEP_STATE:

all:		$(PROG)

catalog:        $(POFILE)


install:	all $(ROOTLIBPRINTPROG) 

$(ROOTLIBPRINT)/%: %
	$(INS.file)

clean:
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

lint:		lint_PROG

include		../../Makefile.targ
