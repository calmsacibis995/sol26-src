#
#ident	"@(#)Makefile.com	1.5	96/09/18 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#

LIBRARY=	libldmake.a
VERS=		.1

include		../../../../lib/Makefile.lib

ROOTLIBDIR=	$(ROOT)/opt/SUNWonld/lib

SGSPROTO=	../../proto/$(MACH)

COMOBJS=	ld_file.o lock.o

OBJECTS=	$(COMOBJS)

MAPFILE=	../common/mapfile-vers

DYNFLAGS +=	-Yl,$(SGSPROTO) -M $(MAPFILE)
CPPFLAGS=	-I../common -I../../include \
		-I../../include/$(MACH) $(CPPFLAGS.master)

CFLAGS +=	-K pic

SRCS=		$(OBJECTS:%.o=../common/%.c)
LDLIBS +=	-lelf -lc

CLEANFILES +=	$(LINTOUT)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
