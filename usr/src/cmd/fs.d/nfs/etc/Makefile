#
#ident  "@(#)Makefile 1.2     96/04/20 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#

include ../../../Makefile.cmd

TXTS= nfssec.conf

IETCFILES=			$(TXTS:%=$(ROOTETC)/%)

FILEMODE= 0644
OWNER= root
GROUP= sys

$(ROOTETC)/%: %
	$(INS.file)

.KEEP_STATE:

all: $(TXTS)

install: all $(IETCFILES) 


FRC:

clean clobber lint:

