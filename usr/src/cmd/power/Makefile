#
# Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.8	96/09/13 SMI"
#
# cmd/power/Makefile
#
#

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
cstyle :=	TARGET= cstyle

SUBDIRS	=	initpkg
DAEMON_SRCS =	powerd.c sysstat.c
DAEMON_OBJS =	$(DAEMON_SRCS:%.c=%.o)
DAEMON =	powerd
PMCFG_SRCS = 	pmconfig.c
PMCFG_OBJS =	$(PMCFG_SRCS:%.c=%.o)
PMCFG = 	$(PMCFG_SRCS:%.c=%)
SRCS = 		$(DAEMON_SRCS) $(PMCFG_SRCS)
OBJS = 		$(SRCS:%.c=%.o)
SCRIPTS =	sysidpm.sh
SYSIDPM = 	sysidpm
PROG =		$(DAEMON) $(PMCFG) $(SYSIDPM)
ETCFILES =	power.conf

include ../Makefile.cmd

LDLIBS +=       -lkstat

ROOTLIBPOWER= $(ROOTLIB)/power
ROOTLIBPOWERDAEMON= $(DAEMON:%=$(ROOTLIBPOWER)/%)
$(ROOTLIBPOWER) := OWNER= root
$(ROOTLIBPOWER) := GROUP= sys
$(ROOTLIBPOWER) := FILEMODE= 755

ROOTETCFILES= $(ETCFILES:%=$(ROOTETC)/%)
$(ROOTETCFILES) := OWNER= root
$(ROOTETCFILES) := GROUP= sys
$(ROOTETCFILES) := FILEMODE= 644

ROOTUSRSBINPMCFG= $(PMCFG:%=$(ROOTUSRSBIN)/%)
$(ROOTUSRSBINPMCFG) := OWNER= root
$(ROOTUSRSBINPMCFG) := GROUP= bin
$(ROOTUSRSBINPMCFG) := FILEMODE= 4555

ROOTUSRSBINSYSIDPM= $(SYSIDPM:%=$(ROOTUSRSBIN)/%)
$(ROOTUSRSBINSYSIDPM) := OWNER= root
$(ROOTUSRSBINSYSIDPM) := GROUP= sys
$(ROOTUSRSBINSYSIDPM) := FILEMODE= 711

#
# lint pass one enforcement
#
CFLAGS += -v

.KEEP_STATE:

all: $(PROG) $(SUBDIRS) $(ETCFILES) $(SCRIPTS)

install clean: $(SUBDIRS)

$(DAEMON): $(DAEMON_OBJS)
	$(LINK.c) $(DAEMON_OBJS) -o $@ $(LDLIBS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

install: all $(ROOTUSRSBINPMCFG) $(ROOTLIBPOWER) \
	$(ROOTLIBPOWERDAEMON) $(ROOTETCFILES) $(ROOTUSRSBINSYSIDPM)

$(ROOTLIBPOWER):
	$(INS.dir)

$(ROOTLIBPOWER)/%:	%
	$(INS.file)

clean:
	$(RM) $(OBJS) $(SYSIDPM)

lint := LINTFLAGS=-auxn
lint:
	$(LINT.c) $(DAEMON_SRCS)
	$(LINT.c) $(PMCFG_SRCS)

cstyle:
	$(CSTYLE) $(SRCS)

include ../Makefile.targ
