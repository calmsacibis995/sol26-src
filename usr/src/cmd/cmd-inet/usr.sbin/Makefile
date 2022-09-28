#
#ident	"@(#)Makefile	1.51	96/09/20 SMI"
#
# Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/Makefile

SYNCPROG=	syncinit syncloop syncstat

PROG=		arp gettable hostconfig in.comsat in.fingerd in.rarpd \
		in.rexecd in.rlogind in.rshd in.rwhod in.telnetd in.tftpd \
		in.tnamed inetd ping route ndd in.rdisc \
		$(SYNCPROG)
STATPROG=	soconfig

SYNCOBJS=	$(SYNCPROG:%=%.o)
DLPIOBJS=	dlcommon.o
DLPISRC=	dlcommon.c
PS= 		hostconfig.ps
SUIDPROG=	arp ping
RPCSVCPROG=	hostconfig
AUDITPROG=	in.rexecd in.rlogind in.rshd in.telnetd inetd
KSTATPROG=	in.rwhod
THREADPROG=	in.rarpd
PROGSRCS=	$(PROG:%=%.c)
TFTPDOBJS=	in.tftpd.o tftpsubs.o
OTHERSRC=	../usr.bin/tftp/tftpsubs.c
SRCS=		$(PROGSRCS) $(OTHERSRC) $(DLPISRC)

SUBDIRS=	dhcp htable ifconfig in.ftpd in.named in.routed in.talkd \
		snoop

# As programs get lint-clean, add them here.  Eventually.
# This hack should go away, and all in PROG should be lint-clean.
LINTCLEAN=	in.rshd in.rexecd $(THREADPROG)

# duplicate ROOTSBIN value needed for installation rule
ROOTSBIN = $(ROOT)/sbin

$(ROOTSBIN)/%: %.ps
	$(INS.rename)

# install rule for static version
# deliberately ahead of a similar rule in ../Makefile.cmd
#
$(ROOTSBIN)/%: %.static
	$(INS.rename)

ROOTPSPROG =	$(PS:%.ps=$(ROOTSBIN)/%)
ROOTSTATICPROG =$(STATIC:%.static=$(ROOTSBIN)/%)

include ../../Makefile.cmd
#
# Message catalog
#
POFILES= in.comsat.po
POFILE= usr.sbin.po
#


all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

CLOBBERFILES +=	$(PS) $(STATIC)
#
# We need to make these become externally available so
# that tcpip.so (ps) can find them at run time.
NEEDU=		-u t_optmgmt \
		-u sscanf \
		-u inet_netof \
		-u strpbrk \
		-u yp_get_default_domain \
		-u yp_first \
		-u strrchr \
		-u strncmp \
		-u strtok \
		-u t_alloc \
		-u memset \
		-u t_free \
		-u t_unbind

$(ROOTUSRSBIN)/arp	:=	FILEMODE= 02555
$(ROOTUSRSBIN)/arp	:=	OWNER= root
$(ROOTUSRSBIN)/ping	:=	FILEMODE= 04555
$(ROOTUSRSBIN)/ping	:=	OWNER= root

CPPFLAGS +=	-DSYSV -DSTRNET -DBSD_COMP -DSOCKADDR_DL
LDLIBS +=	-lsocket -lnsl -ldl

# Eventually just plain CFLAGS should be += -v, but not until all in
# PROGS are lint clean.
$(LINTCLEAN)		:=	CFLAGS += -v

$(AUDITPROG)		:=	LDLIBS += -lbsm -lpam
$(KSTATPROG)		:=	LDLIBS += -lkstat
$(RPCSVCPROG)		:=	LDLIBS += -lrpcsvc
$(THREADPROG)		:=	LDLIBS += -lthread -lsocket -lnsl

$(PS) 			:= 	LDLIBS = $(LDLIBS.cmd) $(NEEDU) -Bdynamic \
				-ldl -Bstatic -lsocket -lnsl \
				-lrpcsvc -lc
$(PS) 			:= 	LDFLAGS = $(LDFLAGS.cmd) -dy 
$(STATIC)		:=	LDLIBS = $(LDLIBS.cmd) -Bstatic -lsocket
in.telnetd		:=	LDLIBS += -lcmd

.KEEP_STATE:

all: $(PROG) $(PS) $(STATIC) $(SUBDIRS) $(DLPIOBJS)

#
# message catalog
#
catalog: $(POFILE)

syncutil: $(SYNCPROG)

$(SYNCPROG):	$$@.o dlcommon.o
	$(LINK.c) $@.o dlcommon.o -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@
#

in.tftpd: $(TFTPDOBJS)
	$(LINK.c) $(TFTPDOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

tftpsubs.o: $(OTHERSRC)
	$(COMPILE.c) $(OTHERSRC) -c $@
	$(POST_PROCESS_O)

dlcommon.o: $(DLPISRC)
	$(COMPILE.c) $(DLPISRC) -c $@
	$(POST_PROCESS_O)

hostconfig: hostconfig.o
	$(LINK.c) hostconfig.o -o $@ $(LDLIBS)

hostconfig.ps: hostconfig.o
	LD_OPTIONS="-I/etc/lib/ld.so.1" $(LINK.c) hostconfig.o -o hostconfig.ps $(LDLIBS)
	$(POST_PROCESS)

soconfig.static: soconfig.o
	$(LINK.c) soconfig.o -o soconfig.static $(LDLIBS)
	$(POST_PROCESS)

install: $(PROG) $(PS) $(STATIC) $(DIRS) $(ROOTUSRSBINPROG) $(ROOTPSPROG) \
	$(ROOTSTATICPROG) $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

clean:	$(SUBDIRS)
	$(RM) $(TFTPDOBJS) $(DLPIOBJS) $(SYNCOBJS)

clobber: $(SUBDIRS)
	$(RM) $(TFTPDOBJS) $(DLPIOBJS) $(SYNCOBJS) $(PROG)

lint: $(SUBDIRS)
	$(LINT.c) $(SRCS)
