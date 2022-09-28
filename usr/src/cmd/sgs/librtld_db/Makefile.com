#
# ident	"@(#)Makefile.com	1.4	96/09/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	librtld_db.a
VERS=		.1

COMOBJS=	rtld_db.o
MACHOBJS=	rd_mach.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ) $(COMOBJS) $(MACHOBJS)

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/krtld \
		$(CPPFLAGS.master)
DYNFLAGS +=	-Yl,$(SGSPROTO) -Wl,-M$(MAPFILE)
ZDEFS=


BLTDEFS=	msg.h
BLTDATA=	msg.c

BLTFILES=	$(BLTDEFS) $(BLTDATA)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA)

SRCS=		$(COMOBJS:%.o=../common/%.c) $(MACHOBJS:%.o=%.c) $(BLTDATA)

CLEANFILES +=	$(LINTOUT) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
