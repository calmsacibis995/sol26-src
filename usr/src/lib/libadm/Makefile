#
#ident	"@(#)Makefile	1.13	96/09/18 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# lib/libadm/Makefile
#

LIBRARY=	libadm.a
VERS=		.1

OBJECTS= \
ckdate.o     ckgid.o      ckint.o      ckitem.o     ckkeywd.o    ckpath.o  \
ckrange.o    ckstr.o      cktime.o     ckuid.o      ckyorn.o     data.o  \
devattr.o    devreserv.o  devtab.o     dgrpent.o    getdev.o     getdgrp.o  \
getinput.o   getvol.o     listdev.o    listdgrp.o   memory.o     pkginfo.o  \
pkgnmchk.o   pkgparam.o   putdev.o     putdgrp.o    puterror.o   puthelp.o  \
putprmpt.o   puttext.o    rdwr_vtoc.o  regexp.o     space.o      fulldevnm.o

MAPFILE=	mapfile-vers

include		../Makefile.lib

LIBS +=		$(DYNLIB)

DYNFLAGS +=	-M $(MAPFILE)
LDLIBS += -lc
LINTFLAGS=	-u
LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

LDLIBS +=       -lelf

$(DYNLIB):	$(MAPFILE)

.KEEP_STATE:

all:		$(LIBS) $(TXTS)

install:	all $(ROOTLIBS) $(ROOTLINKS)

lint:		$(LINTLIB)

include		../Makefile.targ
