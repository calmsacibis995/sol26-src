#
#ident	"@(#)Makefile.com	1.8	96/09/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	librtld.a
VERS=		.1

MACHOBJS=	_relocate.o
COMOBJS=	dldump.o	dynamic.o	relocate.o	syms.o \
		util.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ)  $(MACHOBJS)  $(COMOBJS)


include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers

ROOTLIBDIR=	$(ROOT)/usr/lib

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/krtld \
		-I$(SRCBASE)/uts/common/krtld \
		$(CPPFLAGS.master)
DYNFLAGS +=	-L ../../liblddbg/$(MACH)
ZDEFS=
LDLIBS +=	-lelf -lc
LINTFLAGS +=	-L ../../liblddbg/$(MACH) $(LDLIBS)

# A bug in pmake causes redundancy when '+=' is conditionally assigned, so
# '=' is used with extra variables.
# $(DYNLIB) :=  DYNFLAGS += -Yl,$(SGSPROTO) -M $(MAPFILE)
#
XXXFLAGS=
$(DYNLIB) :=	XXXFLAGS= -Yl,$(SGSPROTO) -M $(MAPFILE)
DYNFLAGS +=	$(XXXFLAGS)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/librtld

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n librtld_msg

SRCS=		$(MACHOBJS:%.o=%.c)  $(COMOBJS:%.o=../common/%.c)  $(BLTDATA)

CLEANFILES +=	$(LINTOUT) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
