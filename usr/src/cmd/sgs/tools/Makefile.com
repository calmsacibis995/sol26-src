#
#ident  "@(#)Makefile.com 1.8     96/06/11 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile to support tools used for linker development:
#
#  o	elfdump provides a mechanism of dumping the information within elf
#	files (see also dump(1)).
#
#  o	sgsmsg creates message headers/arrays/catalogs (a native tool).
#
# Note, these tools are not part of the product.
#
# cmd/sgs/tools/Makefile.com

include		$(SRC)/cmd/Makefile.cmd

include		$(SRC)/cmd/sgs/Makefile.com

SGSPROTO=	../../proto/$(MACH)

COMOBJS=	elfdump.o	comment_filter.o

NATOBJS=	sgsmsg.o

OBJECTS=	$(COMOBJS)  $(NATOBJS)

PROGS=		$(COMOBJS:%.o=%)
NATIVE=		$(NATOBJS:%.o=%)
SRCS=		$(COMOBJS:%.o=../common/%.c)  $(NATOBJS:%.o=../common/%.c)

CPPFLAGS +=	-I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/krtld
LDFLAGS +=	-Yl,$(SGSPROTO)
CLEANFILES +=	$(LINTOUT)
LINTFLAGS=	-ax

ROOTDIR=	$(ROOT)/opt/SUNWonld
ROOTPROGS=	$(PROGS:%=$(ROOTDIR)/bin/%)

FILEMODE=	0755
