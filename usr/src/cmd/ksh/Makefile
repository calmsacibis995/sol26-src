#
#ident	"@(#)Makefile	1.20	96/08/16 SMI"
#
# Copyright (c) 1990, 1995,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= ksh
LIBSH= libsh.a
BUILTINS= ksh-builtins
ROOTXPG4SYMLINK= $(ROOTXPG4BIN)/sh
ROOTLINK= $(ROOTBIN)/r$(PROG)

include ../Makefile.cmd

OBJS= \
	arith.o		args.o		builtin.o			\
	cmd.o		ctype.o		defs.o		edit.o		\
	echo.o		error.o		expand.o	fault.o		\
	history.o	io.o		jobs.o		macro.o		\
	main.o		msg.o		name.o		print.o		\
	service.o	string.o	stak.o		test.o		\
	word.o		xec.o		emacs.o		vi.o

LIBSHOBJS= \
	adjust.o	assign.o	assnum.o	cannon.o	\
	chkid.o		convert.o	findnod.o	gettree.o	\
	strmatch.o	growaray.o	gsort.o		linknod.o	\
	namscan.o	rjust.o		strdata.o	streval.o	\
	tilde.o		unassign.o	utos.o		valup.o		\
	optget.o	csi.o

TXTS=	sh/apollo.c sh/editlib.c sh/vfork.c

MAINSRCS= $(OBJS:%.o=sh/%.c)
LIBSHSRCS= $(LIBSHOBJS:%.o=shlib/%.c)
SRCS= $(MAINSRCS) $(LIBSHSRCS)

CPPFLAGS = -DWEXP -DKSHELL -Iinclude $(CPPFLAGS.master)
CPPFLAGS += -D_FILE_OFFSET_BITS=64
CPPFLAGS += -DCSI_ASCIIACCEL
LDLIBS +=	-lsocket -lnsl

install	:= TARGET= install
clobber	:= TARGET= clobber

.KEEP_STATE:

.PARALLEL: $(OBJS) $(LIBSHOBJS)

all: $(PROG) $(TXTS)

%.o: sh/%.c
	$(COMPILE.c) $<

%.o: shlib/%.c
	$(COMPILE.c) $<

$(BUILTINS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(LIBSH): $(LIBSHOBJS)
	$(AR) cr $(LIBSH) $(LIBSHOBJS)
	
$(PROG): $(OBJS) $(LIBSH)
	$(LINK.c) $(OBJS) $(LIBSH) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG) $(BUILTINS) $(ROOTXPG4SYMLINK) $(ROOTLINK)

$(ROOTXPG4SYMLINK):
	$(RM) $@
	$(SYMLINK) ../../bin/$(PROG) $@

$(ROOTLINK): $(ROOTPROG)
	$(RM) $@
	$(LN) $(ROOTPROG) $@

clean:
	$(RM) $(OBJS) $(LIBSHOBJS)

clobber: $(BUILTINS)

lint: lint_SRCS

include ../Makefile.targ

FRC:
