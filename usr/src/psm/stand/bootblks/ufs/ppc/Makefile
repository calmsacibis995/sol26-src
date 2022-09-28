#
#ident	"@(#)Makefile	1.9	95/06/30 SMI"
#
# Copyright (c) 1995, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/ufs/ppc/Makefile
#

BASEDIR = ../..

include $(BASEDIR)/ufs/Makefile.ufs

# since installboot is a C program for PPC, this addition is needed
LDLIBS.cmd =	$(ENVLDLIBS1)  $(ENVLDLIBS2)  $(ENVLDLIBS3)
LDLIBS =	$(LDLIBS.cmd)

SUBDIRS	= prep unix

all	:=	TARGET= all
install	:=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint

.KEEP_STATE:

all: $(INSTALLBOOT) $(SUBDIRS)

install: $(INSTALLBOOT) $(SUBDIRS) $(USR_SBIN_INSTALLBOOT)

lint clean: $(SUBDIRS)

clobber: $(SUBDIRS)
	-$(RM) $(INSTALLBOOT)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

