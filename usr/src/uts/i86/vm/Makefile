#
#ident	"@(#)Makefile	1.7	96/05/22 SMI"
#
# Copyright (c) 1992-1994, by Sun Microsystems, Inc.
#
# uts/i86/vm/Makefile
#
UTSBASE = ../..

#
# include global definitions
#
include ../Makefile.i86

#
# Override defaults.
#
FILEMODE = 644
OWNER = bin
GROUP = bin

#
# One would think that it would be obvious that platform specific things
# would be built under the platform directory (i86pc) in this case, but
# somehow this didn't happen.  I suspect that this is because Intel tried
# to be like sparc in where these headers installed, but wasn't like sparc
# in positioning the directory.  I guess there is no value in consistancy.
# Anyway, I hope the helps you understand the following kludge.
#
PLATFORM=	i86pc

#
# End of kludge.
#
HDRS=		hat_i86.h seg_kmem.h seg_vpix.h mach_page.h

ROOTHDRS=	$(HDRS:%=$(USR_PSM_IVM_DIR)/%)

ROOTDIR=	$(ROOT)/usr/share/src
ROOTDIRS=	$(ROOTDIR)/uts $(ROOTDIR)/uts/$(PLATFORM)

ROOTLINK=	$(ROOTDIR)/uts/$(PLATFORM)/vm
LINKDEST=	../../../../platform/$(PLATFORM)/include/vm

CHECKHDRS=	$(HDRS:%.h=%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTLINK)

check:	$(CHECKHDRS)

#
# install rules
#
$(USR_PSM_IVM_DIR)/%: ../../srmmu/vm/% $(USR_PSM_IVM_DIR)
	$(INS.file)

$(ROOTDIRS):
	$(INS.dir.bin.bin)

# -r because this used to be a directory and is now a link.
$(ROOTLINK):    $(ROOTDIRS)
	-$(RM) -r $@; $(SYMLINK) $(LINKDEST) $@ $(CHOWNLINK) $(CHGRPLINK)

#%.check:
#	$(DOT_H_CHECK)

CLEANFILES=

CLOBBERFILES=

CLEANLINTFILES=

include ../../Makefile.targ
