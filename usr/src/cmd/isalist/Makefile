#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma	ident	"@(#)Makefile	1.4	96/10/08 SMI"
#

PROG= isalist
SHFILES= optisa
OBJS= isalist.o  

include ../Makefile.cmd

FILEMODE= 0555
OWNER= bin
GROUP= bin

.KEEP_STATE:

all: $(PROG) $(SHFILES)

$(SHFILES): $(SHFILES).sh
	-$(RM) $(SHFILES)
	$(CP) $(SHFILES).sh $(SHFILES)

install: all $(ROOTPROG) $(ROOTSHFILES)

$(PROG):	$(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)
	
clean:
	-$(RM) $(OBJS)

lint:	lint_PROG

include ../Makefile.targ
