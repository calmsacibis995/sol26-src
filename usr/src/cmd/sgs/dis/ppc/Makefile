#
#ident	"@(#)Makefile	1.5	94/07/29 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# cmd/sgs/dis/ppc/Makefile
#
# Optional flags:
#
# -DMC98603 to activate 603-specific definitions.
# -DMC98601 to activate 601-specific instructions/register definitions.

MACHOBJS=	bits.o ppcdis_parse.o ppcdis_utl.o ppcdis_print.o

# flags for Big Endian version
# DEFLIST =	-Ui386 -DMC98601 -DMC98603 -DELF -DDIS
# DEFLIST +=	-D__BIG_ENDIAN -DM32 -DFBO

# flags for Little Endian version
DEFLIST =	-Ui386 -DMC98601 -DMC98603 -DELF -DDIS
DEFLIST	+=	-D__LITTLE_ENDIAN -DM32 -DRBO

include 	../common/Makefile.com
