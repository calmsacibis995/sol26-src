#
# Copyright (c), 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile 1.17	96/08/22 SMI"
#

PROG= localedef

OBJS=	check.o \
	copy.o \
	err.o \
	gen.o \
	gram.o \
	init.o \
	list.o \
	localedef.o \
	method.o \
	scan.o \
	sem_chr.o \
	sem_ctype.o \
	sem_xlat.o \
	sem_method.o  \
	semstack.o \
	sem_coll.o \
	symtab.o

SRCS= $(OBJS:%.o=%.c)

POFILES= $(OBJS:%.o=%.po)

include ../Makefile.cmd

$(PROG) := CFLAGS += -D_LOCALEDEF
YFLAGS += -d
LDLIBS += -ldl

include ./Makefile.locale

#
# generic extension files
#
SB_DENSE_EXTENSIONS = single_byte.x
GENERIC_EUCBC_EXTENSIONS = generic_eucbc.x

CLOBBERFILES =	$(TMPLIBC) $(ISOOBJECT) $(ISOLOCALE)

CLEANFILES = $(OBJS) gram.c y.tab.h

POFILE=localedef.po
XGETFLAGS=

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG) .WAIT $(ISOOBJECT)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

$(ROOTI18NEXT)/$(SB_DENSE_EXTENSIONS) :=	FILEMODE = 0444
$(ROOTI18NEXT)/$(GENERIC_EUCBC_EXTENSIONS) :=	FILEMODE = 0444

install: all $(ROOTPROG) $(ROOTI18NEXT)/$(SB_DENSE_EXTENSIONS) \
	 $(ROOTI18NEXT)/$(GENERIC_EUCBC_EXTENSIONS) \
	 $(ROOTDIRS) $(ROOTSYMLN) \
	 $(ROOTISOOBJECT) \
	 $(ROOTISOSRCDIR) $(ROOTISOSRCS)

clean:
	$(RM) $(CLEANFILES)

lint:   lint_SRCS

gram.c + y.tab.h :	gram.y
	$(RM) gram.c y.tab.h
	$(YACC.y) gram.y
	mv y.tab.c gram.c

scan.o :	y.tab.h

include ./Makefile.target
include ../Makefile.targ
