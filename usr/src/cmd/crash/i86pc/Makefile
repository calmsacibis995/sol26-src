#
#ident  "@(#)Makefile 1.9     93/10/22 SMI"
#
# Copyright (c) 1992 by Sun Microsystems, Inc.
#

# The common Makefile does most of the work; this Makefile is here
# to give the NSE a private place to record dependencies.

KARCH =		i86pc
KARCHOBJS =	vm_i86mmu.o dumphdr.o

include ../Makefile.com

CPPFLAGS += -Di386

install: all $(ROOTARCHKVMPROG) $(ROOTETCLINK) $(ROOTSBINLINK)

