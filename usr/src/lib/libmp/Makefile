#
#ident	"@(#)Makefile	1.12	96/09/18 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
# lib/libmp/Makefile

LIBRARY= libmp.a
VERS= .2

OBJECTS= gcd.o madd.o mdiv.o mout.o msqrt.o mult.o pow.o util.o

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers

LIBS += $(DYNLIB)
LDLIBS += -lc
DYNFLAGS += -M $(MAPFILE)

$(ROOTLIBDIR)/libmp.so.1 := FILEMODE= 755

CLOBBERFILES += libmp.so.1
CLEANFILES += old_mp.o pics_mp.o

.KEEP_STATE:

all: $(LIBS) libmp.so.1

$(LIBRARY):	objs .WAIT $(OBJS) old_mp.o

libmp.so.1:	pics_mp.o libmp.so.2
	$(CC) -o $@ -G -h libmp.so.1 -ztext pics_mp.o $(LDLIBS) libmp.so.2

old_mp.o:	old_mp.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

pics_mp.o:	old_mp.c
	$(COMPILE.c) -o $@ -K pic old_mp.c
	$(PROCESS_COMMENT) $@

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLIBDIR)/libmp.so.1

$(DYNLIB): $(MAPFILE)

# include library targets
include ../Makefile.targ

