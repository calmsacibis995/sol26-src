#
#ident	"@(#)Makefile.com	1.20	96/09/24 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	libld.a
VERS=		.2

G_MACHOBJS=	doreloc.o
L_MACHOBJS= 	machrel.o
COMOBJS=	entry.o		files.o		globals.o	libs.o \
		order.o		outfile.o	place.o		relocate.o \
		resolve.o	sections.o	support.o	syms.o\
		update.o	util.o		version.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ)  $(G_MACHOBJS)  $(L_MACHOBJS)  $(COMOBJS)

include 	$(SRC)/lib/Makefile.lib
include 	$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/common/krtld \
		-I$(SRCBASE)/uts/$(ARCH)/krtld \
		$(CPPFLAGS.master)
DYNFLAGS +=	-M $(MAPFILE) -L ../../liblddbg/$(MACH) -L ../../libconv/$(MACH)
LLDLIBS=	-llddbg
ZDEFS=
LDLIBS +=	-lelf -lconv $(LLDLIBS) -lc
LINTFLAGS +=	-L ../../liblddbg/$(MACH) -L ../../libconv/$(MACH) $(LDLIBS)


# A bug in pmake causes redundancy when '+=' is conditionally assigned, so
# '=' is used with extra variables.
# $(DYNLIB) :=	DYNFLAGS += -Yl,$(SGSPROTO)
#
XXXFLAGS=
$(DYNLIB) :=			XXXFLAGS= -Yl,$(SGSPROTO)
$(SGSPROTO)/$(DYNLIB) :=	XXXFLAGS= -R$(SGSPROTO)

DYNFLAGS +=	$(XXXFLAGS)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/libld

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n libld_msg

SRCS=		$(L_MACHOBJS:%.o=%.c) $(COMOBJS:%.o=../common/%.c) $(BLTDATA) \
		$(G_MACHOBJS:%.o=$(SRCBASE)/uts/$(ARCH)/krtld/%.c)

CLEANFILES +=	$(LINTOUT) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
