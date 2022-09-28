#
#ident	"@(#)Makefile	1.4	94/11/30 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# uts/adb/platform/ppc/prep/Makefile
#

ADB_PLAT_DIR =	../..

KARCH =		prep
PLATFORM =	$(KARCH)

INCLUDES =	-I${SYSDIR}/${KARCH} \
		-I${SYSDIR}/${MACH} -I${SYSDIR}/sun

SRCS =		ctx.adb msgbuf.adb mutex.adb regs.adb rwlock.adb sema.adb

include $(ADB_PLAT_DIR)/Makefile.psm

.KEEP_STATE:

.PARALLEL:	$(SCRIPTS)

all: $(SCRIPTS)

install: all $(USR_PSM_ADB_DIR) .WAIT $(ROOTSCRIPTS)

include $(ADB_PLAT_DIR)/Makefile.targ
