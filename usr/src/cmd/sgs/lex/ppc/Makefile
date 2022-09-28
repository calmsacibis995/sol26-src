#
# ident	"@(#)Makefile	1.1	94/01/13 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# cmd/sgs/lex/ppc/Makefile
#

PROG=		lex
LIBRARY=	libl.a
INCLIST_ppc=	-I../common

include 	../../../Makefile.cmd
include 	../../../../lib/Makefile.lib
include		../Makefile.com

.KEEP_STATE:

all:		$(PROG)

all_lib:	$(LIBS)

install:	all $(ROOTCCSBINPROG) $(ROOTFORMS)

install_lib:	$(ROOTLIBS)

include		../Makefile.targ
include		../../../Makefile.targ
