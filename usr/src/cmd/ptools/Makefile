#
#ident	"@(#)Makefile	1.2	96/06/18 SMI"
#
# Copyright (c) 1994-1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/Makefile
#

# include global definitions
include ../Makefile.cmd

SUBDIRS = libproc \
	pcred	\
	pfiles	\
	pflags	\
	pldd	\
	pmap	\
	prun	\
	psig	\
	pstack	\
	pstop	\
	ptime	\
	ptree	\
	pwait	\
	pwdx

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
