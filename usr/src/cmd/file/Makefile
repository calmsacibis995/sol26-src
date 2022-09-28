#
#ident	"@(#)Makefile	1.9	96/04/20 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= file
MAGIC= magic

include ../Makefile.cmd

LDLIBS += -lcmd -lelf
CPPFLAGS += -D_FILE_OFFSET_BITS=64

ROOTETCMAGIC= $(MAGIC:%=$(ROOTETC)/%)

$(ROOTETCMAGIC) :=	FILEMODE =	$(LIBFILEMODE)

.KEEP_STATE:

all: $(PROG) $(MAGIC) 

install: all $(ROOTPROG) $(ROOTETCMAGIC)

clean:

lint: lint_PROG

include ../Makefile.targ
