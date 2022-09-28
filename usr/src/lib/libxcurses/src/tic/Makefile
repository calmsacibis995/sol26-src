#
#ident	"@(#)Makefile 1.2	96/01/17 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libxcurses/src/tic/Makefile
#

#
# tic.mk
#
# Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
#
# This Software is unpublished, valuable, confidential property of
# Mortice Kern Systems Inc.  Use is authorized only in accordance
# with the terms and conditions of the source licence agreement
# protecting this Software.  Any unauthorized use or disclosure of
# this Software is strictly prohibited and will result in the
# termination of the licence agreement.
#
# If you have any questions, please consult your supervisor.
# 
#
###
### This makefile INTENTIONALLY AVOIDS MKS MAKE EXTENSIONS in order
### to be portable to other systems that supply only a vanilla/POSIX
### make.
###
#
#
# This makefile assumes that a make wrapper, $RD/bin/$SYS/make,
# loads the required make config file, $RD/h/$SYS/config.mk.
# The make wrapper is a shell script that does :
#
#	make -f $RD/h/$SYS/config.mk $@
#
# Note that $RD/bin/$SYS should be ahead of /bin and /usr/bin in PATH.
#
#
#MAKE		= rmake
#
#CFLAGS	= -D_XOPEN_SOURCE
#
#HDR	= tic.h
#TIC	= ticmain.c ticscan.c ticerror.c ticparse.c
#UNTIC 	= untic.c
#
#EXE	= tic$E untic$E
#
#$O$E :
#	$(LD) $(LDFLAGS) $(CFLAGS) -o $@ $< -lcurses 
#
#all :: $(EXE)
#
#install : all
#	$(CP) $(EXE) $(RD)/bin/$(SYS)
#
#clobber :: clean
#	$(RM) $(EXE)
#
#tic$E : $(TIC:.c=$O)
#	$(LD) $(LDFLAGS) $(CFLAGS) -o $@ $(TIC:.c=$O) -lcurses 

all:
	@echo this tic and untic is not currently used by Solaris
	@echo use tic and untic in usr/src/lib/libcurses
