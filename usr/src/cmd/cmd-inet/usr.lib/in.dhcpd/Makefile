#
#ident	"@(#)Makefile	1.43	96/07/01 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.lib/in.dhcpd/Makefile
#

PROG =		in.dhcpd
OBJS=		bootp.o convert.o dhcp.o dhcptab.o dlprims.o encode.o \
		ethers.o generic.o hash.o icmp.o interfaces.o main.o misc.o \
		packet.o per_network.o pf.o relay.o scan.o

SRCS=		$(OBJS:%.o=%.c)

include ../../../Makefile.cmd
include ../Makefile.lib

LDLIBS +=       -ldhcp -lsocket -lnsl

# for messaging catelog
POFILE = ${PROG}.po
POFILES = $(OBJS:%.o=%.po)

.KEEP_STATE:

all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(POFILE):	$(POFILES)
	$(RM)	$@
	cat $(POFILES) > $@

install:	all $(ROOTLIBINETPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../Makefile.targ
