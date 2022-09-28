#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.12	96/09/18 SMI"
#

LIBRARY=	liblddbg.a
VERS=		.3

COMOBJS=	args.o		bindings.o	debug.o	\
		dynamic.o	entry.o		elf.o		files.o \
		libs.o		map.o		note.o		phdr.o \
		relocate.o	sections.o	segments.o	shdr.o \
		support.o	syms.o		util.o		version.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ)  $(COMOBJS)

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/krtld \
		$(CPPFLAGS.master)
DYNFLAGS +=	-M $(MAPFILE) -L../../libconv/$(MACH)
ZDEFS=
LDLIBS=		-lconv
LINTFLAGS +=	-L ../../libconv/$(MACH) $(LDLIBS)


# A bug in pmake causes redundancy when '+=' is conditionally assigned, so
# '=' is used with extra variables.
# $(DYNLIB) :=  DYNFLAGS += -Yl,$(SGSPROTO)
#
XXXFLAGS=
$(DYNLIB) :=    XXXFLAGS= -Yl,$(SGSPROTO)
DYNFLAGS +=     $(XXXFLAGS)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/liblddbg

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n liblddbg_msg

SRCS=		$(COMOBJS:%.o=../common/%.c)  $(BLTDATA)

CLEANFILES +=	$(LINTOUT) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB)  $(LINTLIB) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
