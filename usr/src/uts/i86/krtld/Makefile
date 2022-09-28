#
# uts/i86/krtld/Makefile
# Copyright (c) 1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.3	96/06/11 SMI"
#
#	kernel linker/loader makefile.
#
#	i86 architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= krtld
OBJECTS		= $(KRTLD_BOOT_OBJS:%=$(OBJS_DIR)/%) \
		  $(KRTLD_OBJS:%=$(OBJS_DIR)/%)
		  
LINTS		= $(KRTLD_BOOT_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(KRTLD_OBJS:%.o=$(LINTS_DIR)/%.ln)
PICS		= $(KRTLD_BOOT_OBJS)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

INC_PATH	+= -I$(UTSBASE)/common/krtld -I.

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

#
#	Overrides.
#
CLOBBERFILES	+= $(MODULE)
CFLAGS		+= -v
CPPFLAGS	+= -DELF_TARGET_386
LINKFLAGS	= -e _kobj_boot -M$(UTSBASE)/common/krtld/mapfile \
		  -Breduce -znoversion
LD		+= $(LINKFLAGS)

$(PICS)		:= CFLAGS += -K pic

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(MODULE)

lint:		$(MODULE).lint

install:  	$(MODULE) $(ROOTMODULE)

#
#	Include common targets.
#
include $(UTSBASE)/i86/Makefile.targ
