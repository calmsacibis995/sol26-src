#
#ident	"@(#)Makefile	1.5	96/07/01 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/dhcp/dhcpconfig/Makefile

PROG =	dhcpconfig
SRCS =	$(PROG:%=%.ksh)
POFILE = $(PROG).po

include ../../../../Makefile.cmd

.KEEP_STATE:

all:	$(PROG)

$(PROG): $(PROG).ksh
	$(RM) $@
	$(CP) $(PROG).ksh $@
	$(CHMOD) +x $@

$(POFILE):
	touch $(POFILE)

install: all $(ROOTUSRSBINPROG)

clean:

lint: lint_PROG

include ../../../../Makefile.targ
