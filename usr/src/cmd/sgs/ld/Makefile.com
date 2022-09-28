#
#ident	"@(#)Makefile.com	1.15	96/06/11 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

PROG=		ld

include 	$(SRC)/cmd/Makefile.cmd
include 	$(SRC)/cmd/sgs/Makefile.com

COMOBJS=	main.o		args.o		entry.o		globals.o \
		libs.o		util.o		map.o		debug.o
BLTOBJ=		msg.o

OBJS =		$(BLTOBJ) $(MACHOBJS) $(COMOBJS)
.PARALLEL:	$(OBJS)

MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../common -I../../include \
		-I$(SRCBASE)/uts/$(ARCH)/krtld \
		-I../../include/$(MACH) \
		$(CPPFLAGS.master)
LDFLAGS +=	-Yl,$(SGSPROTO) -M $(MAPFILE)
LLDLIBS=	-L ../../libld/$(MACH) -lld -lelf -ldl \
		-L ../../liblddbg/$(MACH) -llddbg
LDLIBS +=	$(LLDLIBS)
LINTFLAGS +=	$(LDLIBS)
CLEANFILES +=	$(LINTOUT)

native :=	LDFLAGS = -R$(SGSPROTO)
native :=	LLDLIBS = -L$(SGSPROTO) -lld -lelf -ldl -llddbg \
			/usr/lib/libintl.so.1

BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/ld
 
BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n ld_msg

SRCS=		$(MACHOBJS:%.o=%.c)  $(COMOBJS:%.o=../common/%.c)  $(BLTDATA)

ROOTCCSBIN=	$(ROOT)/usr/ccs/bin
ROOTCCSBINPROG=	$(PROG:%=$(ROOTCCSBIN)/%)

CLEANFILES +=	$(BLTFILES)

FILEMODE=	0755


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1
