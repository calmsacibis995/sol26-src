#
# uts/sun4m/colorado/Makefile
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
#ident   "@(#)Makefile 1.2     96/05/20 SMI"
#
#	This makefile drives the production of the Supersparc sun4m cpu driver module
#
#	sun4m implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..
#
#	Define the module and object file sets.
#
MODULE		= TI,TMS390Z55
OBJECTS		= $(VIK_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(VIK_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_CPU_DIR)/$(MODULE)
SOFTLINKS	= TI,TMS390Z50
ROOTSOFTLINKS	= $(SOFTLINKS:%=$(ROOT_PSM_CPU_DIR)/%)

#
#	Include common rules.
#
include $(UTSBASE)/sun4m/Makefile.sun4m

#
#	Override defaults to build a unique, local modstubs.o.
#
CLOBBERFILES	+= $(MODULE) $(CPULIB) $(CPUSYM_MOD)

#
# lint pass one enforcement
#
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

.PARALLEL:	$(OBJECTS)

all:		$(CPUSYM_MOD)

$(CPUSYM_MOD):	$(MODULE)
	@echo "resolving symbols against unix.o"
	${LD} -o $(CPULIB) -G $(MODULE)
	@(cd $(UNIX_DIR); CPU_DIR=../viking $(MAKE) symcheck)

lint:		$(MODULE).lint

install:	$(MODULE) $(ROOTMODULE) $(ROOTSOFTLINKS)

$(ROOTSOFTLINKS): $(ROOTMODULE)
	$(RM) $@; $(SYMLINK) $(MODULE) $@ $(CHOWNLINK) $(CHGRPLINK)

#
#	Include common targets.
#
include $(UTSBASE)/sun4m/Makefile.targ
