#
# uts/i86/Makefile
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#ident "@(#)Makefile	1.7	96/09/04 SMI"
#
#	This makefile drives the production of all implementation architecture
#	independent modules for the Intel x86 processor.

UTSBASE = ..

include Makefile.i86

LINT_LIBS	 = $(LINT_LIB) \
		   $(KMODS:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(SVVS_KMODS:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(XMODS:%=$(LINT_LIB_DIR)/llib-l%.ln)

#
#
#
all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
modlintlib	:=	TARGET= modlintlib
clean.lint	:=	TARGET= clean.lint
check		:=	TARGET= check

.KEEP_STATE:

.PARALLEL:	$(KMODS) $(SVVS) $(XMODS) devfs

all install clean clobber: $(KMODS) $(SVVS) $(XMODS) devfs

modlintlib clean.lint: $(KMODS) $(SVVS) $(XMODS)

$(KMODS) devfs:	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(SVVS) $(XMODS):	FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

install_h check:	FRC
	@cd sys; pwd; $(MAKE) $(TARGET)

#
#	Full kernel lint target.
#
lint:	modlintlib
	@-$(ECHO) "\nFULL KERNEL: global crosschecks:"
	@$(LINT) $(LINTFLAGS) $(LINT_LIBS) 2>&1 | $(LGREP.2)

FRC:

