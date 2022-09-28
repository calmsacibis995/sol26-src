#
#ident	"@(#)Makefile	1.15	96/04/03 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# ucblib/libucb/i386/Makefile
#
# This builds all objects needed for libucb, including those derived
# from source under ../port. 
#
LIBRARY= libucb.a
VERS = .1

# objects are grouped by source directory.

# objects from source under ../port
# the last two objects in PORTGEN come from derived C source
#
PORTGEN= \
_psignal.o  bcmp.o      bcopy.o     bzero.o    ftime.o \
getwd.o     index.o \
nice.o      nlist.o     psignal.o \
rand.o      readdir.o   regex.o     rindex.o \
scandir.o   setbuffer.o setpriority.o \
siglist.o   statfs.o \
times.o     ualarm.o    usleep.o    sleep.o

PORTSTDIO= \
doprnt.o    fopen.o     fprintf.o   printf.o    sprintf.o  \
vfprintf.o  vprintf.o   vsprintf.o

# NOTE GETRUSAGE IS BEING BOUGHT BACK INTO BASE, REMOVE
# FROM THAT LIST WHEN THAT HAPPENS

PORTSYS= \
flock.o          getdtblsize.o    gethostid.o    gethostname.o\
getpagesize.o    gettimeofday.o   killpg.o       mctl.o \
reboot.o         setjmp.o \
wait3.o          wait4.o          getrusage.o    setpgrp.o

MACHSYS= signal.o _times.o syscall.o ucbcerror.o

OBJECTS= $(PORTGEN) $(PORTSTDIO) $(PORTSYS) $(MACHSYS)

# build rules for objects built from "portable" source in ../port
objs/%.o profs/%.o pics/%.o %.o: ../port/gen/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../port/stdio/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../port/sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# build rule for objects built from machine specific source in ../i386
objs/%.o profs/%.o pics/%.o: ../i386/sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o: ../i386/sys/%.s
	$(BUILD_OBJS.s)

pics/%.o: ../i386/sys/%.s
	$(BUILD_PICS.s)


# include common library definitions
include ../../../lib/Makefile.lib

MAPFILES=	../port/mapfile-vers  mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

CPPFLAGS= -Di386 -I$(ROOT)/usr/ucbinclude -Iinc -I../inc $(CPPFLAGS.master)
ASFLAGS=  #-P -D__STDC__ -DLOCORE -D_SYS_SYS_S -D_ASM $(CPPFLAGS)
ASOBJDEFS=	-m m4.def sys.def ansi.def nonpic.def -DMCOUNT=/
ASPICDEFS=	-m -DDSHLIB m4.def sys.def ansi.def pic.def -DMCOUNT=/
LDLIBS += -lelf -lc
DYNFLAGS += $(MAPOPTS)


# BUILD.s=  $(AS) $(ASFLAGS) $< -o $@
BUILD_OBJS.s=  $(AS) $(ASFLAGS) $(ASOBJDEFS) $< && mv $(@F) $@
BUILD_PICS.s=  $(AS) $(ASFLAGS)  $(ASPICDEFS) $< && mv $(@F) $@


# libc method of building an archive, using AT&T ordering
BUILD.AR= $(RM) $@ ; \
	$(AR) q $@ `$(LORDER) $(OBJECTS:%=$(DIR)/%)| $(TSORT)`

LIBS = $(LIBRARY) $(DYNLIB) $(LINTLIB)

ROOTLIBDIR=     $(ROOT)/usr/ucblib

# list of C source for lint
SRCS= $(PORTGEN:%.o=../port/gen/%.c)      \
$(PORTSTDIO:%.o=../port/stdio/%.c)  $(PORTSYS:%.o=../port/sys/%.c)

$(LINTLIB) := SRCS=llib-lucb
$(LINTLIB) := LINTFLAGS=-nvx
$(LINTLIB) := TARGET_ARCH=

$(DYNLIB):	$(MAPFILES)

.KEEP_STATE:

all: $(TXTS) $(LIBS) $(PLIB)

# include common library targets
include ../../../lib/Makefile.targ

