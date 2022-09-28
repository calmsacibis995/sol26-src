#
# uts/prep/Makefile
# Copyright (c) 1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.36	96/09/04 SMI"
#
#	This makefile drives the production of all implementation architecture
#	dependent modules for the prep architecture.
#

UTSBASE = ..

include Makefile.prep

#
#	The following are PPC specific (rather than prep) specific modules
#	which are required for the prep kernel to completely lint. They are
#	not involved in the build in any other way. In order to minimize
#	build time, it is assumed that they are up to date.
#
PPC_LIB_DIR	 = $(UTSBASE)/ppc/lint-libs

PPC_LINTS	 = genunix

LINT_LIBS	 = $(LINT_LIB) \
		   $(KMODS:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(PPC_LINTS:%=$(PPC_LIB_DIR)/llib-l%.ln)

#
#
#
all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
lintlib		:=	TARGET= lintlib
machmodlintlib	:=	TARGET= modlintlib
clean.lint	:=	TARGET= clean.lint
check		:=	TARGET= check

.KEEP_STATE:

.PARALLEL:	$(KMODS) $(XMODS) $(CONFS)

all clean clobber clean.lint: genassym unix .WAIT $(KMODS) $(XMODS) audiocs

install: install_platforms genassym unix .WAIT $(KMODS) $(XMODS) $(CONFS) \
	audiocs

lintlib:	unix

machmodlintlib:	$(KMODS)

genassym unix $(KMODS) $(CONFS) audiocs:	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(XMODS):	FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

install_h check:	install_platforms FRC
	@cd sys; pwd; $(MAKE) $(TARGET)

#
# To make a sparsely populated /platform/`uname -i`, we need to
# create the minimal directory tree:
#
#	/platform/`uname -i`			directory
#	/platform/`uname -i`/kernel		directory
#	/platform/`uname -i`/kernel/unix	symlink to ../../prep/kernel/unix
#	/platform/`uname -i`/kadb		symlink to ../prep/kadb
#
# This relies on a change to the boot to add /platform/prep to the
# module search path whenever "uname -i" is not "prep".
#
$(ROOT_PLAT_DIRS): FRC
	-@if [ -h $@ ]; then \
		$(RM) $@ 2> /dev/null; \
	fi
	-$(INS.dir.root.sys)
	$(RM) -r $@/kadb
	$(SYMLINK) ../$(PLATFORM)/kadb $@/kadb

$(ROOT_PLAT_KERNEL_DIRS): $(ROOT_PLAT_DIRS)
	-$(INS.dir.root.sys)
	$(RM) -r $@/unix
	$(SYMLINK) ../../$(PLATFORM)/kernel/unix $@/unix

#
# Currently, there is no need to change /usr/platform structure,
# so we continue to define /usr/platform/`uname -i` to be a
# symlink to /usr/platform/prep.
#
$(USR_PLAT_DIRS):
	$(RM) -r $@; $(SYMLINK) $(PLATFORM) $@

#
# Make the /platform directories.  This is hardwired here because
# the first stage of the project (KBI) only implements the userland
# changes, but the only reasonable place to record the aliases is
# here in kernel land.
#
install_platforms:	$(ROOT_PSM_DIR) $(USR_PSM_DIR) \
			$(ROOT_PLAT_DIRS) $(USR_PLAT_DIRS) \
			$(ROOT_PLAT_KERNEL_DIRS)

#
#	Full kernel lint target.
#
lint:	lintlib .WAIT machmodlintlib .WAIT $(PPC_LINTS)
	@-$(ECHO) "\nFULL KERNEL: global crosschecks:"
	@$(LINT) $(LINTFLAGS) $(LINT_LIBS) 2>&1 | $(LGREP.2)

$(PPC_LINTS):
	@cd $(UTSBASE)/ppc/$@; pwd; $(MAKE) modlintlib

#
# The cscope.out file is made in the current directory and spans only
# its architecture.
# Things to note:
#	1. We use relative names for cscope and tags.
#	2. We *don't* remove the old cscope.out file, because cscope is
#	   smart enough to only build what has changed.  It can be
#	   confused, however, if files are renamed or removed, so it may
#	   be necessary to manually remove cscope.out if a lot of
#	   reorganization has occured.
#
CSDIR	= ..
CSPROM	= ../psm/promif/ieee1275/common ../psm/promif/ieee1275/prep
CSDIRS	= prep ppc common $(CSPROM)
CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)
CSINCS	= $(CSPATHS:%=-I%)
CSCOPE	= cscope
CTAGS	= ctags
#
# Set CSFLAGS env variable to -bq when using fast cscope to
# build the fast (but large) cscope data bases.
#
CSFLAGS	= -b

.PRECIOUS: cscope.out

cscope.out: cscope.files FRC
	${CSCOPE} ${CSFLAGS}

cscope.files: FRC
	@-$(RM) cscope.files cscope.files.raw
	echo "$(CSINCS)" > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    -type d -name '.del-*' -prune -o -type f \
	    \( -name '*.[csh]' -o -name 'Makefile*' -o -name '*.il' \) \
	    -print > cscope.files.raw
	grep -v Makefile cscope.files.raw >> cscope.files
	grep Makefile cscope.files.raw >> cscope.files
	-$(RM) cscope.files.raw
	@wc -l cscope.files

#
# Create a tags data base, similar to above.
# Since assembler files now contain C fragments for lint,
# the lint fragments will allow ctags to "work" on assembler.
# Things to note:
#	1. We put plain files before headers, and .c before .s (because
#	   ctags doesn't understand assembly comments).
#	2. We *don't* sort the output of find, as we want functions in
#	   architecture directories to take precedence over those in
#	   sun, and those over those in common.
#
tags: tags.list FRC
	${CTAGS} -wt `cat tags.list`

# THIS IS A KLUDGE
# We should really only list the files we use to build the kernel; this
# picks up things like mpsas directories which are not used in normal
# kernels.

tags.list: cscope.files
	@-$(RM) tags.list
	grep '\.c$$' cscope.files | grep -v '/mpsas/' | grep -v '/sas/' \
		> tags.list
	grep '\.h$$' cscope.files | grep -v '/mpsas/' | grep -v '/sas/' \
		>> tags.list
	grep '\.s$$' cscope.files | grep -v '/mpsas/' | grep -v '/sas/' \
		>> tags.list

FRC:

include ../Makefile.targ
