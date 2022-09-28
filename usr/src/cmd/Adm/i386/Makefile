#
#ident	"@(#)Makefile	1.1	92/06/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

ETCFILES= ioctl.syscon

include ../../Makefile.cmd

ROOTETCFILES= $(ETCFILES:%=$(ROOTETC)/%)

FILEMODE= 0644
TOUCHTIME= 0101000070
OWNER= root
GROUP= sys

.KEEP_STATE:

all: $(ETCFILES)

install: all $(ROOTETCFILES)

TOUCHIT=
$(CH)TOUCHIT= ; $(TOUCH) $(TOUCHTIME) $@

$(CH)$(ROOTETCFILES) := INS.file += $(TOUCHIT)

clean:

lint:

clobber:

clean:

lint:

clobber:

FRC:
