#
#ident	"@(#)Makefile	1.11	96/04/20 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		ufs
LIBPROG=	mkfs
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

# for messaging catalog
#
POFILE= mkfs.po

catalog:        $(POFILE)


CPPFLAGS +=	-D_LARGEFILE64_SOURCE
LDLIBS +=	-lm -ladm

lint:		lint_PROG
