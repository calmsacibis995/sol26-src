#
#ident	"@(#)Makefile	1.3	96/06/18 SMI"
#
# Copyright (c) 1994-1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# makefile for libproc, the support functions for the proc tools
#
# make		- make libproc in local directory
# make install	- make libproc in local directory (don't install anywhere)
# make lint	- check program consistency
# make clean	- as your mother told you
# make clobber	- make it squeeky clean

include ../../Makefile.cmd

CFLAGS	+= -v

lint	:= LINTFLAGS =	-mux

PROG =

LIB = libproc.a

HDRS = pcontrol.h ramdata.h dynlib.h

OBJS = pcontrol.o ramdata.o	\
	prclose.o	\
	prfcntl.o	\
	prfstat.o	\
	prgetrlimit.o	\
	prmmap.o	\
	prmunmap.o	\
	propen.o	\
	prsetitimer.o	\
	prsigaction.o	\
	scantext.o	\
	dynlib.o	\
	fltname.o	\
	signame.o	\
	sysname.o	\
	read_string.o	\
	sleep.o		\
	isprocdir.o

SRCS = $(OBJS:%.o=%.c)

all:	$(LIB)

$(LIB):	$(OBJS)
	$(RM) $@
	$(AR) q $@ $(OBJS)

install: all

CLOBBERFILES = $(LIB)

clean:
	rm -f $(OBJS)

lint:   lint_SRCS

include ../../Makefile.targ
