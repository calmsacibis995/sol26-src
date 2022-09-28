#
#ident	"@(#)Makefile.com	1.3	96/06/11 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/Makefile.com

.KEEP_STATE:

SRCBASE=	../../../..

i386_ARCH=	i86
ppc_ARCH=	ppc
sparc_ARCH=	sparc

ARCH=$($(MACH)_ARCH)

# Establish the local tools, proto and package area.

SGSHOME=	$(SRC)/cmd/sgs
SGSPROTO=	$(SGSHOME)/proto/$(MACH)
SGSTOOLS=	$(SGSHOME)/tools/$(MACH)
SGSMSGID=	$(SGSHOME)/messages
SGSMSGDIR=	$(SGSHOME)/messages/$(MACH)
SGSONLD=	$(ROOT)/opt/SUNWonld


# The cmd/Makefile.com and lib/Makefile.com define TEXT_DOMAIN.  We don't need
# this definition as the sgs utilities obtain their domain via sgsmsg(1l).

DTEXTDOM=


# Define any generic sgsmsg(1l) flags.  The defualt message generation system
# is to use gettext(3i), add the -C flag to switch to catgets(3c).

SGSMSGFLAGS=	-i $(SGSMSGID)/sgs.ident
