#
#ident	"@(#)Makefile	1.9	96/09/18 SMI"
#
# Copyright (c) 1993, Sun Microsytems, Inc.
#
# lib/libmapmalloc/Makefile
#

LIBRARY= libmapmalloc.a
VERS= .1

OBJECTS=  \
textmem.o calloc.o mallopt.o valloc.o malloc_debug.o

# libsocket build rules
objs/%.o profs/%.o pics/%.o: inet/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: socket/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers

SRCS=	$(OBJECTS:%.o=%.c)

LIBS += $(DYNLIB)
LDLIBS +=   -ldl -lc
CPPFLAGS += -D_REENTRANT
DYNFLAGS += -M $(MAPFILE)

.KEEP_STATE:

all: $(LIBS)

install: $(LIBS) $(ROOTLIBS) $(ROOTLINKS)

$(DYNLIB): $(MAPFILE)

# include library targets
include ../Makefile.targ

clean clobber lint: 
