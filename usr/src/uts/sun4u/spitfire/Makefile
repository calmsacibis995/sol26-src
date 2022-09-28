#
# uts/sun4u/spitfire/Makefile
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.4	96/10/15 SMI"
#
#	This makefile drives the production of the sun4u UltraSPARC driver module.
#
#	sun4u implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..
#
#	Define the module and object file sets.
#
MODULE		= SUNW,UltraSPARC
OBJECTS		= $(SPITFIRE_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SPITFIRE_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_CPU_DIR)/$(MODULE)
SOFTLINKS	= SUNW,UltraSPARC-II
ROOTSOFTLINKS	= $(SOFTLINKS:%=$(ROOT_PSM_CPU_DIR)/%)
CPU_DIR		= .
HERE		= ../spitfire

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/Makefile.sun4u

#
#	clean and clobber files
#
CLEANFILES	+= $(CPULIB) $(SYM_MOD)
CLOBBERFILES	+= $(MODULE)

#
# lint pass one enforcement
#
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(SYM_MOD)

$(CPULIB):	$(MODULE)
	${LD} -o $(CPULIB) -G $(MODULE)

$(SYM_MOD):	$(UNIX_O) $(CPULIB)
	@echo "resolving symbols against unix.o"
	@(cd $(UNIX_DIR); pwd; \
	    CPU_DIR=$(HERE) SYM_MOD=$(HERE)/$(SYM_MOD) $(MAKE) symcheck)

lint:		$(MODULE).lint

install:	$(MODULE) $(ROOTMODULE) $(ROOTSOFTLINKS)

$(ROOTSOFTLINKS): $(ROOTMODULE)
	$(RM) $@; $(SYMLINK) $(MODULE) $@ $(CHOWNLINK) $(CHGRPLINK)

#	Include common targets.
#
include $(UTSBASE)/sun4u/Makefile.targ
