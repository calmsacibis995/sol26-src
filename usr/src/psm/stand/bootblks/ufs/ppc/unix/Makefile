#
#ident	"@(#)Makefile	1.4	95/06/27 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/ufs/ppc/unix/Makefile
#

#
# Build the "Unix" version of the boot block.
# Useful solely for testing.  DO NOT INSTALL IT!
#

BASEDIR		= ../../..
PLATFORM	= unix
PLATLINKS	=
PLATFORM_INCS	=

include $(BASEDIR)/ufs/Makefile.ufs
include $(BASEDIR)/obp-c/Makefile.rules

#CPPINCS		+= -I$(PLATFORM_INCS)

UNIX_OBJS	= ufs.o unix_devio.o

$(UNIX_OBJS) :=	CFLAGS += -g

.KEEP_STATE:

all:	$(PROG)

$(PROG): $(UNIX_OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -g -o $@ $(UNIX_OBJS)

lint: FRC

clean:
	$(RM) $(UNIX_OBJS)

clobber: clean
	$(RM) $(PROG)

install: FRC

FRC:

