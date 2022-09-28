#
# Copyright (c) 1992-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	96/09/18 SMI"
#
VERS = .1

include Makefile.objs

OBJECTS= $(SCHOBJECTS)
SRCS= $(OBJECTS:%.o=%.c)

# include library definitions
include ../../Makefile.lib

# library dependency
LDLIBS += -lc -lpam -lnsl -lsocket

# resolve with local variables in shared library
DYNFLAGS += -Bsymbolic

LINTFLAGS=-I$(ROOT)/usr/include

DYNLIB= pam_dial_auth.so$(VERS)
LIBS = $(DYNLIB)

# override ROOTLIBDIR and ROOTLINKS
ROOTLIBDIR=	$(ROOT)/usr/lib/security
ROOTLIBS=       $(LIBS:%=$(ROOTLIBDIR)/%)
OWNER = root
GROUP = sys

OBJ_DIR=	objs
PIC_DIR=	pics

.KEEP_STATE:

.PARALLEL:	$(DEFOBJS) $(DEFPICS)

BUILD.SO=	$(CC) -o $@ -M mapfile -G $(DYNFLAGS) $(PICS) $(LDLIBS)

all: $(LIBS) 

defobjs: $(OBJ_DIR) .WAIT $(DEFOBJS)

defpics: $(PIC_DIR) .WAIT $(DEFPICS)

$(ROOTLIBDIR):
	$(INS.dir)

install: all $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLIBDIR)/pam_dial_auth.so

$(ROOTLIBDIR)/pam_dial_auth.so:
	$(RM) $@; $(SYMLINK) $(DYNLIB) $@

lint: $(SRCS:.c=.ln)

# include library targets
include ../../Makefile.targ
