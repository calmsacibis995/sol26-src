#
# ident	"@(#)Makefile	1.5	96/09/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/lp/cmd/lpsched/lpsched/Makefile
#

LIBRARY =	bsd_test.a
VERS=.1

OBJECTS =	test.o

include		../../../../../lib/Makefile.lib
include         ../../../Makefile.sp

ROOTLIBDIR=	$(ROOT)/usr/lib/print/bsd-adaptor

#CPPFLAGS +=	-I$(LPINC)
CPPFLAGS +=	-I$(NPRTINC)
LDLIBS +=	-lprint -lnsl -lsocket -ldl -lc		# with XFN API
#LDLIBS +=	$(LIBCLS) $(LIBPRT) $(LIBMSG)



LIBS = $(DYNLIB)	# could be += for static and dynamic
# for messaging catalog
#
POFILE= bsd.po
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

install:	$(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)

cstyle:
	cstyle $(SRCS)

lint:
	$(LINT.c) $(SRCS)

strip:

$(ROOTLIBDIR):
	$(INS.dir)

include		../../../../../lib/Makefile.targ
