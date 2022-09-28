#
#ident	"@(#)Makefile.com	1.5	96/06/11 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#

PROG=		ldd

include		$(SRC)/cmd/Makefile.cmd
include		$(SRC)/cmd/sgs/Makefile.com

COMOBJ=		ldd.o
BLTOBJ=		msg.o

OBJS=		$(BLTOBJ) $(COMOBJ)

MAPFILE=	../common/mapfile-vers

CPPFLAGS +=	-I. -I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/krtld \
		$(CPPFLAGS.master)
LDFLAGS +=	-Yl,$(SGSPROTO) -M $(MAPFILE)
LDLIBS +=	-lelf 
LINTFLAGS +=	$(LDLIBS)

BLTDEFS=        msg.h
BLTDATA=        msg.c
BLTMESG=        $(SGSMSGDIR)/ldd

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n ldd_msg

SRCS=		$(COMOBJ:%.o=../common/%.c) $(BLTDATA)

CLEANFILES +=	$(LINTOUT) $(BLTFILES)


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1
