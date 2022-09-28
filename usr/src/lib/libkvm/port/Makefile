#
#ident	"@(#)Makefile	1.14	96/09/19 SMI"
#
# Copyright (c) 1990, 1993, 1996 by Sun Microsystems, Inc.
#
# lib/libkvm/port/Makefile
#
# This Makefile build and installs the platform independent libkvm.so.
#

#
# Relative path to $(SRC)/lib/libkvm.
#
LIBKVM_BASE=	..

#
# Actually, no archive is built for libkvm (see psarc/1992/171), but this
# definition is needed to make the standard rule work.
#
LIBRARY=	libkvm.a

OBJECTS= kvmgetcmd.o kvmgetu.o kvmnextproc.o kvmnlist.o kvmopen.o kvmrdwr.o

#
# Include library definitions
#
include $(LIBKVM_BASE)/../Makefile.lib

#
# Override LIBS so that only a dynamic library is built.
#
LIBS =		$(DYNLIB)

MAPFILE=	mapfile-vers

#
# VERS is the version of the user interface to libkvm.
#
# VERS10K is the version of the KBI interface between libkvm.so and the
# platform dependent adjunct, lkvm_pd.so.
#
VERS =		.1
VERS10K=	.1

#
# This causes the local copy of kvm.h to be used.  Delete this line to get
# the installed version.
#
IFLAGS =	-I$(LIBKVM_BASE)

CPPFLAGS = \
	-D_KMEMUSER -DVERS10K=\"$(VERS10K)\" -D_LARGEFILE64_SOURCE=1 $(IFLAGS) $(CPPFLAGS.master)

#CPPFLAGS += -D_KVM_DEBUG


DYNFLAGS += -Wl,-M,$(MAPFILE) -Wl,-f,/usr/platform/\$$PLATFORM/lib/$(DYNLIBPSR)
#DYNFLAGS += -Wl,-f/usr/platform/\$$PLATFORM/lib/$(DYNLIBPSR)
LDLIBS += -lelf -ldl -lc


#
# If and when somebody gets around to messaging this, CLOBBERFILE should not
# be cleared (so that any .po file will be clobbered.
#
CLOBBERFILES=	test

$(DYNLIB): 	$(MAPFILE)

.KEEP_STATE:

all: $(DYNLIB)

install: all $(ROOTLIBS) $(ROOTLINKS) 

test: test.c
	$(CC) $(CPPFLAGS) test.c -o test -lkvm -lelf

#
# Include library targets
#
include ../../../lib/Makefile.targ
