#
#ident	"@(#)Makefile	1.8	96/03/18 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/last/Makefile
#

PROG= last
UTMPS= wtmp wtmpx
ROOTADM= $(ROOT)/var/adm
ROOTUTMPS= $(UTMPS:%=$(ROOTADM)/%)
SYMSETC= $(UTMPS:%=symlinks_%)
ETCTOVARADM= ../var/adm

include ../Makefile.cmd

$(ROOTUTMPS):= FILEMODE= 664
$(ROOTUTMPS):= OWNER= adm
$(ROOTUTMPS):= GROUP= adm

$(ROOTADM)/%: %
	$(INS.file)

$(UTMPS):
	touch $@

symlinks_% : %
	-$(RM) $(ROOTETC)/$< ; $(SYMLINK) $(ETCTOVARADM)/$< $(ROOTETC)/$<
	$(CH)@-$(CHOWN) adm $(ROOTETC)/$< ; $(CHGRP) adm $(ROOTETC)/$<

.KEEP_STATE:

all: $(PROG) $(UTMPS)

install: all $(ROOTPROG) $(SYMSETC) $(ROOTUTMPS)

clean:
	$(RM) $(UTMPS)

lint:	lint_PROG

include ../Makefile.targ
