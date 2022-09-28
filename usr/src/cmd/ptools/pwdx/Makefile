#
#ident	"@(#)Makefile	1.5	96/08/01 SMI"
#
# Copyright (c) 1994-1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/pwdx/Makefile
#

PROG = pwdx
LIBS = libcwd.so.1
OBJS = cwd.o
SRCS = $(OBJS:%.o=%.c)

include ../../Makefile.cmd
include ../Makefile.com

$(ROOTPROCBINPROG) := FILEMODE = 0555
$(ROOTPROCBINPROG) := OWNER = bin
$(ROOTPROCBINPROG) := GROUP = bin

$(PROG)	:= CPPFLAGS +=	-I../libproc
$(PROG)	:= LDLIBS +=	../libproc/libproc.a

lint	:= CPPFLAGS +=	-I../libproc

# Optimization ruins libcwd
$(OBJS)	:= CFLAGS +=	-K PIC $(MACH).il

.KEEP_STATE:

all: $(PROG) $(LIBS)

$(LIBS):	$(OBJS)
	$(LD) -B local -z defs -z text -i -o $(LIBS) -e cwd -G $(OBJS)

install: all $(ROOTPROCBINPROG) $(ROOTPROCLIBLIB)

CLOBBERFILES += $(LIBS) $(OBJS)

clean:

lint:   lint_PROG lint_SRCS

include ../../Makefile.targ
