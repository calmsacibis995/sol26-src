#
#ident	"@(#)Makefile	1.2	95/04/25 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/eeprom/common/Makefile
#
include ../../Makefile.cmd

PROG		= eeprom
FILEMODE	= 555
OWNER		= bin
GROUP		= sys

.KEEP_STATE:

all install : $(ROOTUSRSBINPROG)

clean clobber : FRC
	$(RM) $(ROOTUSRSBINPROG)

FRC:
