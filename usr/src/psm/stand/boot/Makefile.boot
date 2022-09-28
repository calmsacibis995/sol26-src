#
#ident	"@(#)Makefile.boot	1.6	96/05/16 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/boot/Makefile.boot

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/Makefile.psm

STANDDIR	= $(TOPDIR)/stand
PSMSTANDDIR	= $(TOPDIR)/psm/stand

SYSHDRDIR	= $(STANDDIR)
SYSLIBDIR	= $(STANDDIR)/lib/$(MACH)

PSMSYSHDRDIR	= $(PSMSTANDDIR)
PSMNAMELIBDIR	= $(PSMSTANDDIR)/lib/names/$(MACH)
PSMPROMLIBDIR	= $(PSMSTANDDIR)/lib/promif/$(MACH)

#
# XXX	one day we should just be able to set PROG to 'cfsboot'..
#	and everything will become a lot easier.
#
# XXX	note that we build but -don't- install the HSFS boot
#	program - it's unused and untested, and until it is we
#	shouldn't ship it!
#
UNIBOOT		= boot.bin
UFSBOOT		= ufsboot
NFSBOOT		= inetboot
HSFSBOOT	= hsfsboot

#
# Common install modes and owners
#
FILEMODE	= 644
DIRMODE		= 755
OWNER		= root
GROUP		= sys

#
# Install locations
#
ROOT_PSM_UNIBOOT= $(ROOT_PSM_SOL_DIR)/$(UNIBOOT)
ROOT_PSM_UFSBOOT= $(ROOT_PSM_DIR)/$(UFSBOOT)
USR_PSM_NFSBOOT	= $(USR_PSM_LIB_NFS_DIR)/$(NFSBOOT)
USR_PSM_HSFSBOOT= $(USR_PSM_LIB_HSFS_DIR)/$(HSFSBOOT)

#
# Lint rules (adapted from Makefile.uts)
#
LHEAD		= ( $(ECHO) "\n$@";
LGREP		= grep -v "pointer cast may result in improper alignment"
LTAIL		= ) 2>&1 | $(LGREP)

LINT.c		= $(LINT) $(LINTFLAGS) $(LINT_DEFS) $(CPPFLAGS) -c
LINT.s		= $(LINT.c)
LINT.2		= $(LINT) $(LINTFLAGS.2) $(LINT_DEFS) $(CPPFLAGS)

LINTFLAGS	= -nsxmu
LINTFLAGS.2	= -nsxm
