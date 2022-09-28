#
# ident	"@(#)Makefile	1.9	96/06/10 SMI"
#
# Copyright (c) 1990, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/lp/cmd/lptest/Makefile
#

include 	../../Makefile.lp

PROG=		lptest

SRCS= 		lptest.c

OBJS=		$(SRCS:.c=.o)


CPPFLAGS =	-I$(LPINC) $(CPPFLAGS.master)
LDLIBS +=	$(LIBBSD) $(LIBREQ) $(LIBMSG) $(LIBOAM)

# for messaging catalog
#
POFILE = lptest.po
XGETFLAGS +=-a -x $(LPROOT)/lp.xcl

.KEEP_STATE:

all:		$(PROG)

catalog:        $(POFILE)
 
$(POFILE):      $(SRC)
	 $(RM)	$@
	 $(COMPILE.cpp)   $(SRCS) > $(POFILE).i
	 $(XGETTEXT)     $(XGETFLAGS)    $(POFILE).i
	 sed "/^domain/d" messages.po    > $@
	 $(RM) $(POFILE).i messages.po

install:	all .WAIT $(ROOTUSRUCBPROG)

clean:
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

lint :		lint_PROG

include		../../../Makefile.targ
