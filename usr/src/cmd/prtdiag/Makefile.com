#
#ident	"@(#)Makefile.com	1.21	96/03/19 SMI"
#
# Copyright (c) 1992 by Sun Microsystems, Inc.
#
# cmd/prtdiag/Makefile.com
#

include $(SRCDIR)/../Makefile.cmd
include $(SRCDIR)/../../Makefile.psm

PROG		= prtdiag

FILEMODE	= 2755
DIRMODE		= 755
OWNER		= root
GROUP		= sys

OBJS=	main.o pdevinfo.o display.o anlyzerr.o

# allow additional kernel-architecture dependent objects to be specified.

OBJS		+= $(KARCHOBJS)

SRCS		= $(OBJS:%.o=%.c)

LINT_OBJS	= $(OBJS:%.o=%.ln)


POFILE		= prtdiag_$(PLATFORM).po
POFILES		= $(OBJS:%.o=%.po)


# These names describe the layout on the target machine

IFLAGS = -I$(SRCDIR) -I$(USR_PSM_INCL_DIR)

CPPFLAGS = $(IFLAGS) $(CPPFLAGS.master)

.PARALLEL: $(OBJS)

# build rules

%.o: $(SRCDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

%.po: $(SRCDIR)/%.c
	$(COMPILE.cpp) $<  > $<.i
	$(BUILD.po)

%.ln: $(SRCDIR)/%.c
	$(LINT) -c $(CPPFLAGS) $<
