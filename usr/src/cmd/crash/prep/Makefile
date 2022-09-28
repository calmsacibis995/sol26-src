#
#pragma ident "@(#)Makefile 1.1	94/06/07 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#

# The common Makefile does most of the work; this Makefile is here
# to give the NSE a private place to record dependencies.

KARCH =		prep
KARCHOBJS =	vm_ppcmmu.o

include ../Makefile.com

CPPFLAGS += -Dprep

install: all $(ROOTARCHKVMPROG)
