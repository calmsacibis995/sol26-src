#
#ident	"@(#)Makefile	1.8	96/07/08 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG1=	inetinit
PROG2=	inetsvc
PROG3=	dhcp
PROG=	$(PROG1) $(PROG2) $(PROG3)

include ../../../Makefile.cmd

STARTINET1= $(ROOTETC)/rc2.d/S69inet
STARTINET2= $(ROOTETC)/rc2.d/S72inetsvc

INITD= $(ROOTETC)/init.d
DIRS= $(INITD) $(ROOTETC)/rc2.d $(ROOTETC)/rc3.d
FILEMODE= 0744
OWNER= root
GROUP= sys

ETCINITPROG=	$(PROG:%=$(INITD)/%)

.KEEP_STATE:

all: $(PROG)

install: all $(DIRS) $(ETCINITPROG) ln_PROG

$(INITD)/% : %
	$(INS.file)

ln_PROG :
	-$(RM) $(STARTINET1)
	$(LN) $(INITD)/$(PROG1) $(STARTINET1)
	-$(RM) $(STARTINET2)
	$(LN) $(INITD)/$(PROG2) $(STARTINET2)

$(DIRS):
	$(INS.dir)

clean:

lint:

include ../../../Makefile.targ
