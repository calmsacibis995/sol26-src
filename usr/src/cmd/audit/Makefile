# @(#)Makefile 1.8 96/03/18 SMI
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/audit/Makefile
#

PROG = audit

include ../Makefile.cmd
TEXT_DOMAIN=SUNW_BSM_AUDIT
POS=audit.po
include ../Makefile.cmd.bsm

OBJS = audit.o
SRCS = $(OBJS:.o=.c)

CFLAGS += -DNO_AUDIT_WRITE

.KEEP_STATE:

all: $(PROG)

clean:
	rm -f $(OBJS) $(POS)

include ../Makefile.targ
