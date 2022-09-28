#
#ident	"@(#)Makefile	1.27	96/07/25 SMI"
#
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libtnfctl/Makefile
#

LIBRARY=	libtnfctl.a
VERS=		.1
OBJECTS.c=	open.o		\
		prb_child.o	\
		prb_shmem.o	\
		prb_proc.o	\
		prb_lmap.o	\
		prb_rtld.o	\
		prb_findexec.o	\
		prb_status.o	\
		util.o		\
		traverse.o	\
		sym.o		\
		elf.o		\
		continue.o	\
		checklib.o	\
		probes.o	\
		probes_ext.o	\
		close.o		\
		targmem.o	\
		comb.o		\
		kernel_int.o	\
		kernel.o	\
		internal.o	\
		status.o


OBJECTS.s=	$(MACH)_assm.o

OBJECTS=	$(OBJECTS.c) $(OBJECTS.s)

include ../Makefile.lib

# We omit $(OBJECTS.s:%.o=%.s) in the next line, because lint no like
SRCS= $(OBJECTS.c:%.o=%.c)

LIBS=		$(DYNLIB)

MAPFILES=	mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)
DYNFLAGS +=	$(MAPOPTS)

HDRS=		tnfctl.h
ROOTHDRDIR=	$(ROOT)/usr/include/tnf
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)
$(ROOTHDRS) := 	FILEMODE = 0644
CHECKHDRS =	$(HDRS:%.h=%.check)

LDLIBS += -lc -ldl -lelf

# Uncomment the following line for a debug build
# COPTFLAG =	-g -DDEBUG -v
CPPFLAGS +=	-I. -I../libtnfprobe -D_REENTRANT

LINTFLAGS +=	-y

$(ROOTHDRS) :=	FILEMODE = 644

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)

install_h: $(ROOTHDRDIR) $(ROOTHDRS)

lint:
	$(LINT.c) $(SRCS)

check: $(CHECKHDRS)

$(ROOTLIBDIR) $(ROOTHDRDIR):
	$(INS.dir)

$(ROOTHDRDIR)/% : %
	$(INS.file)

BUILD.s=	$(AS) $< -o $@

pics/%.o:	./%.s
		$(BUILD.s)
		$(POST_PROCESS_O)

objs/%.o:	./%.s
		$(BUILD.s)
		$(POST_PROCESS_O)

WARLOCK_FILES= $(OBJECTS.c:%.o=%.ll)
CLEANFILES= $(WARLOCK_FILES)
warlock_files:	$(WARLOCK_FILES)
%.ll:		%.c
		wlcc $(CFLAGS) $(CPPFLAGS) -o $@ $<
warlock:	warlock_files
		warlock -c wlcmd $(WARLOCK_FILES)

include ../Makefile.targ
