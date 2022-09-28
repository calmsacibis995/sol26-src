#
#ident	"@(#)Makefile	1.2	95/06/27 SMI"
#
# Copyright (c) 1995, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/hsfs/prep/unix/Makefile
#

#
# Build the "Unix" version of the boot block.
# Useful solely for testing.  DO NOT INSTALL IT!
#
BASEDIR		= ../../..
PLATFORM	= unix
PLATFORM_INCS	=
PLATLINKS	=

include $(BASEDIR)/hsfs/Makefile.hsfs
include $(BASEDIR)/obp-c/Makefile.rules

#CPPINCS		+= -I$(PLATFORM_INCS)

UNIX_OBJS	= hsfs_small.o unix_devio.o

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

