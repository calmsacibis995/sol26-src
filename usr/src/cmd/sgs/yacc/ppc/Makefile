#
# ident	"@(#)Makefile	1.1	94/01/13 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# cmd/sgs/yacc/ppc/Makefile
#

PROG=		yacc
LIBRARY=	liby.a
DEFLIST=	-DELF

include 	../../../Makefile.cmd
include 	../../../../lib/Makefile.lib
include		../Makefile.com

.KEEP_STATE:

all:		$(PROG)

all_lib:	$(LIBS)

install:	all $(ROOTCCSBINPROG) $(ROOTYACCPAR)

install_lib:	$(ROOTLIBS)

include		../Makefile.targ
include		../../../Makefile.targ
