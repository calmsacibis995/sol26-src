#
#ident	"@(#)Makefile.com	1.22	96/03/15 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# sgs/libelf/Makefile.com


LIBRARY=	libelf.a
VERS=		.1
M4=		m4

MACHOBJS=
COMOBJS=	ar.o		begin.o		cntl.o		cook.o \
		data.o		end.o		fill.o		flag.o \
		getarhdr.o	getarsym.o	getbase.o	getdata.o \
		getehdr.o	getident.o	getphdr.o	getscn.o \
		getshdr.o	hash.o		input.o		kind.o \
		ndxscn.o	newdata.o	newehdr.o	newphdr.o \
		newscn.o	next.o		nextscn.o	output.o \
		rand.o		rawdata.o	rawfile.o	rawput.o \
		strptr.o	update.o	error.o
BLTOBJS=	msg.o		xlate.o
MISCOBJS=	String.o	args.o		demangle.o	nlist.o \
		nplist.o

OBJECTS=	$(BLTOBJS)  $(MACHOBJS)  $(COMOBJS)  $(MISCOBJS)

DEMOFILES=	Makefile	README		acom.c		dcom.c \
		pcom.c		tpcom.c

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../common $(CPPFLAGS.master)
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lc

BUILD.AR=	$(RM) $@ ; \
		$(AR) q $@ `$(LORDER) $(OBJECTS:%=$(DIR)/%)| $(TSORT)`
		$(POST_PROCESS_A)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/libelf

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n libelf_msg

BLTSRCS=	$(BLTOBJS:%.o=%.c)
SRCS=		$(COMOBJS:%.o=../common/%.c)  $(MISCOBJS:%.o=../misc/%.c) \
		$(MACHOBJS:%.o=%.c)  $(BLTSRCS)

ROOTDEMODIR=	$(ROOT)/usr/demo/ELF
ROOTDEMOFILES=	$(DEMOFILES:%=$(ROOTDEMODIR)/%)

LIBS +=		$(DYNLIB) $(LINTLIB)

CLEANFILES +=	$(LINTOUT) $(BLTSRCS) $(BLTFILES) $(WARLOCKFILES) 

$(ROOTDEMODIR) :=	OWNER =		root
$(ROOTDEMODIR) :=	GROUP =		bin
$(ROOTDEMODIR) :=	DIRMODE =	775

.PARALLEL:	$(LIBS) $(ROOTDEMOFILES)
