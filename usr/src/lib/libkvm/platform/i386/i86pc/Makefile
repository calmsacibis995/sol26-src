#
#ident	"@(#)Makefile	1.1	94/05/13 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# lib/libkvm/platform/i386/i86pc/Makefile
#
# Makefile for the platform dependent libkvm adjunct for i86pc.  This only
# has any utility as a shared object.
#
LIBKVM_BASE =	../../..
PLATFORM =	i86pc
KARCH =		i86pc

OBJECTS =	kvmplatdep.o

#
# Include library definitions.
#
include $(LIBKVM_BASE)/platform/Makefile.com

#
# Additional build rule to allow SMCC platforms to share source.
#
objs/%.o profs/%.o pics/%.o: ../i86pc/%.c
	$(COMPILE.c) -G -o $@ $<
	$(POST_PROCESS_O)
 
.KEEPSTATE:

all:		$(DYNLIB)

#
# Include library targets.
#
include $(LIBKVM_BASE)/platform/Makefile.targ
