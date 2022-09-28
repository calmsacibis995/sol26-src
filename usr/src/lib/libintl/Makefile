#
#ident	"@(#)Makefile	1.14	96/05/22 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
# lib/libintl/Makefile

LIBRARY=	libintl.a
VERS=		.1

ROOTARLINK=	$(ROOTLIBDIR)/$(LIBRARY)

# include library definitions
include		../Makefile.lib

MAPFILES=	mapfile-vers $(MAPFILE-FLTR)
MAPOPTS=	$(MAPFILES:%=-M %)

DYNFLAGS +=	-F libc.so.1 $(MAPOPTS)

$(ROOTLIBDIR)/$(DYNLIB) :=	FILEMODE= 755

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) -Bstatic -lc 2>&1 | \
		fgrep -v "No read-write segments found" | cat

CLOBBERFILES +=	$(DYNLIB) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)

$(DYNLIB):	$(MAPFILES)
		$(BUILD.SO)
		$(POST_PROCESS_SO)

all:		$(DYNLIB) $(ROOTARLINK)
 
install_h:

check:

install:	all $(ROOTDYNLIB) $(ROOTLINKS)

$(ROOTLIBDIR)/%: %
		$(INS.file)

$(ROOTLINKS): $(ROOTDYNLIB)
		$(INS.liblink)

$(ROOTARLINK):
		-$(RM) $@; $(SYMLINK) $(LIBNULL) $@
 
clean \
clobber:
		-$(RM) $(DYNLIB)

lint:
