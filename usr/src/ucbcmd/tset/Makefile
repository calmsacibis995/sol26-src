#
#ident	"@(#)Makefile	1.6	96/09/10 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= tset

include ../Makefile.ucbcmd

FILEMODE=	755

#LDLIBS +=  -lcurses
LDLIBS =  -L$(ROOT)/usr/ucblib $(LDLIBS.cmd) -lucb -ltermcap

CPPFLAGS = -I$(ROOT)/usr/ucbinclude $(CPPFLAGS.master)

ROOTSYMLINK= $(ROOTBIN)/reset

.KEEP_STATE:

all: $(PROG) 

$(PROG):	$(PROG).c
	$(SETRUNPATH) $(LINK.c) -o $@ $(PROG).c $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTBIN) $(ROOTPROG) $(ROOTSYMLINK)

$(ROOTSYMLINK):
	$(RM) $@
	$(SYMLINK) $(PROG) $@

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg
