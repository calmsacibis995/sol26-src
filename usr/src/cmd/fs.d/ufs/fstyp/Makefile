#
#ident	"@(#)Makefile	1.8	96/04/20 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		ufs
LIBPROG=	fstyp
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

# for messaging catalog
#
POFILE= fstyp.po

catalog:        $(POFILE)

CPPFLAGS +=	-D_LARGEFILE64_SOURCE
lint:		lint_PROG
