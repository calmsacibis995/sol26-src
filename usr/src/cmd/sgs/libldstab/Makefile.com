#
#ident	"@(#)Makefile.com	1.11	96/09/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	libldstab.a
VERS=		.1

COMOBJS=	stab.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ) $(COMOBJS)

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

SRCBASE=	../../../..

MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/krtld \
		$(CPPFLAGS.master)
ZDEFS=
LDLIBS +=	-lelf
DYNFLAGS +=	-M $(MAPFILE)


# A bug in pmake causes redundancy when '+=' is conditionally assigned, so
# '=' is used with extra variables.
# $(DYNLIB) :=        DYNFLAGS += -Yl,$(SGSPROTO) -M $(MAPFILE)
#
XXXFLAGS=
$(DYNLIB) :=	XXXFLAGS= -Yl,$(SGSPROTO)

DYNFLAGS +=	$(XXXFLAGS)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/libldstab

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n libldstab_msg

SRCS=		$(COMOBJS:%.o=../common/%.c) $(BLTDATA)

CLEANFILES +=	$(LINTOUT) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
