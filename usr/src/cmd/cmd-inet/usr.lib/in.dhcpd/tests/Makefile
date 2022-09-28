#
#ident	"@(#)Makefile	1.4	96/07/08 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.lib/in.dhcpd/tests/Makefile
#

PROG = bootp_client
SRC =  bootp_client.c
OBJ = $(SRCS:%.c=%.o)

LDLIBS += -lsocket

$(PROG):

clean:
	$(RM) $(OBJ)

clobber: clean
	$(RM) $(PROG)
	
