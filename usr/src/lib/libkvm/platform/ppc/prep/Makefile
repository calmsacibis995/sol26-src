#
#ident	"@(#)Makefile	1.2	94/08/04 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# lib/libkvm/platform/ppc/prep/Makefile
#
# Makefile for the platform dependent libkvm adjunct for prep.  This only
# has any utility as a shared object.
#
LIBKVM_BASE =	../../..
PLATFORM =	prep
KARCH =		prep

OBJECTS =	kvmplatdep.o

#
# Include library definitions.
#
include $(LIBKVM_BASE)/platform/Makefile.com

#
# Additional build rule to allow SMCC platforms to share source.
#
objs/%.o profs/%.o pics/%.o: ../prep/%.c
	$(COMPILE.c) -G -o $@ $<
	$(POST_PROCESS_O)
 
.KEEPSTATE:

all:		$(DYNLIB)

#
# Include library targets.
#
include $(LIBKVM_BASE)/platform/Makefile.targ
