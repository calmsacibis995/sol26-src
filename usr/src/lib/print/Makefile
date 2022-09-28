#
# Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
# All Right Reserved
#
# "@(#)Makefile	1.14	96/09/18 SMI"
#
# cmd/lp/client/lib/Makefile
#

LIBRARY =	libprint.a
VERS=.2

FILEMODE=	0555

NS_CMN =	ns_bsd_addr.o ns_cmn_kvp.o ns_cmn_order.o ns_cmn_printer.o
NS_RW =		ns_rw_file.o ns_rw_nis.o ns_rw_xfn.o
NS_CVT =	ns_cvt_pconf.o ns_cvt_printers.o ns_cvt_xfn.o ns_cvt_nisplus.o
NS_SVC =	ns_svc_user.o ns_svc_etc.o ns_svc_nis.o ns_svc_xfn.o \
		ns_svc_nisplus.o
NS_SUPPORT =	ns.o ${NS_CMN} ${NS_CVT} ${NS_RW} ${NS_SVC}

OBJECTS =	network.o job.o list.o misc.o dynamic.o $(NS_SUPPORT)


include		../Makefile.lib

MAPFILES=	mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

CPPFLAGS +=	-I../ -I.  -D_REENTRANT
LDLIBS +=	-lnsl -lsocket -ldl -lxfn -lc
DYNFLAGS +=	$(MAPOPTS)


LIBS = $(DYNLIB)	# could be += for static and dynamic
pics/ns_cmn_order.o:=	CPPFLAGS += -DHAVE_XFN
# for messaging catalog
#
POFILE= bsd.po
SRCS= $(OBJECTS:%.o=%.c)
XGETFLAGS +=-a -x $(LPROOT)/lp.xcl

 
.KEEP_STATE:
  
catalog:        $(POFILE)

$(POFILE):      $(SRCS)
	  $(RM)	$@
	  $(COMPILE.cpp)   $(SRCS) > $(POFILE).i
	  $(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	  sed "/^domain/d"        messages.po > $@
	  $(RM) messages.po $(POFILE).i

all :	$(LIBS)

install:	$(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)

lint:
	$(LINT.c) $(SRCS)

cstyle:
	cstyle $(SRCS)

strip :

$(ROOTLIBDIR):
	$(INS.dir)

include		../Makefile.targ
