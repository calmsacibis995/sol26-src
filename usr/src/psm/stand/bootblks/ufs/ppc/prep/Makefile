#
#ident	"@(#)Makefile	1.6	95/06/27 SMI"
#
# Copyright (c) 1995, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/ufs/ppc/prep/Makefile
#

#
# Platform specific Makefile for the boot block.
#
# PLATFORM is the target for the binary installation.
#
# PLATFORM_INCS is a list of directories to look in for platform specific
#	header files to include
#
# PLATLINKS is a list of platforms which are to be linked to this binary
#	at the file level.
#
# PROM_TYPE is the type of prom OBP, IEEE1275, etc...
#
BASEDIR		= ../../..
PLATFORM	= prep
PLATLINKS	=
PLATFORM_INCS	=
PROM_TYPE	= IEEE1275

include $(BASEDIR)/ufs/Makefile.ufs
include $(BASEDIR)/obp-c/Makefile.rules

#CPPINC		+= -I$(PLATFORM_INCS)

.KEEP_STATE:

all:	$(PROG).elf
	@-$(MV) $(PROG).elf $(PROG)

include $(BASEDIR)/obp-c/Makefile.targ
include $(BASEDIR)/Makefile.targ
