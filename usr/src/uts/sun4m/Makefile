#
# uts/sun4m/Makefile
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.33	96/09/04 SMI"
#
#	This makefile drives the production of all implementation architecture
#	dependent modules for the sun4m architecture.
#

UTSBASE = ..

include Makefile.sun4m

#
# Allow parallel build of all kernel modules except for
# those specified by $(CPU_KMODS_SERIAL)
#
.PARALLEL:	$(KMODS) $(XMODS) $(CONFS)

KMODS	+= $(CPU_KMODS_SERIAL)

#
#	The following are SPARC specific (rather than sun4m) specific modules
#	which are required for the sun4m kernel to completely lint. They are
#	not involved in the build in any other way. In order to minimize
#	build time, it is assumed that they are up to date.
#
SPARC_LIB_DIR	 = $(UTSBASE)/sparc/lint-libs

SPARC_LINTS	 = genunix

LINT_LIBS	 = $(LINT_LIB) \
		   $(LINT_KMODS:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(SPARC_LINTS:%=$(SPARC_LIB_DIR)/llib-l%.ln)

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


all clean clobber clean.lint: genassym unix .WAIT $(KMODS) $(XMODS)

install: install_platforms genassym unix .WAIT $(KMODS) $(XMODS) $(CONFS)

lintlib:	unix

machmodlintlib:	$(KMODS)

genassym unix $(KMODS) $(CONFS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(XMODS):	FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

install_h check:	FRC
	@cd sys; pwd; $(MAKE) $(TARGET)
	@cd vm; pwd; $(MAKE) $(TARGET)

#
# Definitions for the /platforms directories.  This is hardwired here because
# the first stage of the project (KBI) only implements the userland
# changes, but the only reasonable place to record the aliases is
# here in kernel land.
#
PLAT_LINKS	 = SUNW,Sun_4_600 SUNW,SPARCsystem-600 SUNW,SPARCstation-10
PLAT_LINKS	+= SUNW,SPARCstation-10,SX
PLAT_LINKS	+= SUNW,SPARCclassic SUNW,SPARCclassic-X SUNW,SPARCstation-LX
PLAT_LINKS	+= SUNW,SPARCstation-LX+ SUNW,SPARCengine-EC-3
PLAT_LINKS	+= SUNW,SPARCstation-5 SUNW,SPARCstation-20 SUNW,S240
PLAT_LINKS	+= SUNW,SPARCstation-4

#
# Make the /platforms directories.  This is hardwired here because
# the first stage of the project (KBI) only implements the userland
# changes, but the only reasonable place to record the aliases is
# here in kernel land.
#
install_platforms:	$(ROOT_PSM_DIR) $(USR_PSM_DIR) \
			$(ROOT_PLAT_LINKS) $(USR_PLAT_LINKS)

#
#	Full kernel lint target.
#
lint:	lintlib .WAIT machmodlintlib .WAIT $(SPARC_LINTS)
	@-$(ECHO) "\nFULL KERNEL: global crosschecks:"
	@$(LINT) $(LINTFLAGS) $(LINT_LIBS) 2>&1 | $(LGREP.2)

$(SPARC_LINTS):
	@cd $(UTSBASE)/sparc/$@; pwd; $(MAKE) modlintlib

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
CSDIRS	= sun4m srmmu sparc sun common ../psm/promif/obp
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
	    -name 'v9' -prune -o \
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
