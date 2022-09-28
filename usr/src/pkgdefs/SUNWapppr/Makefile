#
#ident	"@(#)Makefile	1.5	96/01/10 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
#

include ../Makefile.com


DATAFILES += depend i.preserve

ACTION_sparc=
ACTION_i386=grep os86r depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWos86r	Platform Support, OS Functionality (Root)" >> depend; \
	chmod 444 depend )
ACTION_ppc=grep os86r depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWos86r	Platform Support, OS Functionality (Root)" >> depend; \
	chmod 444 depend )

.KEEP_STATE:

all: $(FILES) action

# action is a pseudotarget denoting completed work on the depend file
action: depend
	grep SUNWpppk depend > /dev/null || \
	(chmod 666 depend; \
	echo "P SUNWpppk      PPP/IP and IPdialup Device Drivers" >> depend; \
	chmod 444 depend )
	$(ACTION_$(MACH))
	touch $@

install: all pkg

include ../Makefile.targ
