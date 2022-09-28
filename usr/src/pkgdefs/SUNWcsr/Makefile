#
#ident	"@(#)Makefile	1.21	96/09/21 SMI"
#
# Copyright (c) 1993-1996, by Sun Microsystems, Inc.
# All rights reserved.

include ../Makefile.com

DATAFILES +=	i.cronroot i.deflogin i.defsu i.etcremote 	\
		i.group	i.inittab i.etcrpc i.etcsystem			\
		i.inetdconf i.iuap i.nsswitch i.netconfig i.passwd	\
		i.preserve i.renamenew i.syslogconf i.tiservices	\
		i.ttysrch i.tzonecvt i.etcprofile i.mailxrc i.shadow	\
		i.voldconf i.rmmconf i.renameold i.sock2path

MACHDATAFILES = i.devlinktab i.drvalias i.kclasses i.minorperm i.nametomajor
CLOBBERFILES += $(MACHDATAFILES)

.KEEP_STATE:

all: $(FILES) $(MACHDATAFILES)
install: all pkg

include ../Makefile.targ
