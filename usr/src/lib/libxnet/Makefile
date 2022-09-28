#
#ident	"@(#)Makefile	1.5	96/09/26 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libxnet/Makefile

LIBRARY=	libxnet.a
VERS=		.1

CLOBBERFILES += lint.out

OBJECTS=	init.o

# libxnet build rules
pics/%.o: %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# include library definitions
include		../Makefile.lib

MAPFILES=	mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

SRCS=		init.c

LIBS +=		$(DYNLIB) $(LINTLIB)

$(LINTLIB):= SRCS=llib-lxnet
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%) $(LINTLIB:%=$(ROOTLINTDIR)/%)

BUILD.SO=	$(CC) -o $@ -G -K pic -Wl,"-Flibsocket.so.1" $(MAPOPTS) $(PICS) $(LDLIBS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

CLOBBERFILES += $(DYNLIB) $(LIBLINKS)

ROOTDYNLIB=     $(DYNLIB:%=$(ROOTLIBDIR)/%)

$(ROOTLIBDIR)/$(DYNLIB) :=	FILEMODE= 755

.KEEP_STATE:

all:            $(DYNLIB)

$(DYNLIB):      $(MAPFILES) pics $(PICS)
		$(BUILD.SO)
		$(POST_PROCESS_SO)
pics:
	-@mkdir -p $@

install_h:

check:

install:        all $(ROOTDYNLIB) $(ROOTLINKS) $(ROOTLINT)

$(ROOTLIBDIR)/%: %
		$(INS.file)

$(ROOTLIBDIR)/$(LIBLINKS): $(ROOTLIBDIR)/$(LIBLINKS)$(VERS)
	$(INS.liblink)

# install rule for lint library target
$(ROOTLINTDIR)/%: %
	$(INS.file)

$(LINTLIB): $$(SRCS)
	$(LINT.c) -o $(LIBNAME) $(SRCS) > $(LINTOUT) 2>&1

clean clobber: 
		-$(RM) $(DYNLIB) $(PICS)
