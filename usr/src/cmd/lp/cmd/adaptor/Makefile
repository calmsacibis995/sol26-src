#
# ident	"@(#)Makefile	1.7	96/09/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/lp/cmd/adaptor/Makefile
#

LIBRARY =	bsd_lpsched.a
VERS=.1

OBJECTS =	show_queue.o cancel_job.o submit_job.o misc.o

include		../../../../lib/Makefile.lib
include         ../../Makefile.lp

ROOTLIBDIR=	$(ROOT)/usr/lib/print/bsd-adaptor

CPPFLAGS +=	-I$(LPINC)
CPPFLAGS +=	-I$(SRC)/lib
LDLIBS +=	-lprint -lnsl -lsocket -ldl -lc
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/msgs -llpmsg
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/printers -llpprt
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/class -llpcls
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/lp -llp
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/requests -llpreq
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/secure -llpsec


LIBS = $(DYNLIB)	# could be += for static and dynamic
# for messaging catalog
#
POFILE= adaptor.po
SRCS= $(OBJECTS:%.o=%.c)
XGETFLAGS +=-a -x $(LPROOT)/lp.xcl
 
.KEEP_STATE:
  
catalog:        $(POFILE)

$(POFILE):      $(SRCS)
	  $(RM)	$@
	  $(COMPILE.cpp)   $(SRCS) > $(POFILE).i
	  $(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	  sed "/^domain/d"        messages.po > $@
	  $(RM) messages.po $(POFILE).i

all :	$(LIBS)

install:	$(ROOTLIBS) $(ROOTLINKS)

cstyle:
	cstyle $(SRCS)

lint:
	$(LINT.c) $(SRCS)

strip:

include		../../../../lib/Makefile.targ
