#
# Copyright (c) 1992-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	96/09/18 SMI"
#
# lib/pam_modules/sample/Makefile
#
VERS = .1

SCHOBJECTS=	sample_authenticate.o \
		sample_setcred.o \
		sample_acct_mgmt.o \
		sample_close_session.o \
		sample_open_session.o \
		sample_password.o \
		sample_utils.o

DEFOBJECTS=$(SCHOBJECTS:%=def_%)

DEFOBJS=$(DEFOBJECTS:%=objs/%)

DEFPICS=$(DEFOBJECTS:%=pics/%)



OBJECTS= $(SCHOBJECTS)
SRCS= $(OBJECTS:%.o=%.c)

# include library definitions
include ../../Makefile.lib

BUILD.SO=	$(CC) -o $@ -M mapfile -G $(DYNFLAGS) $(PICS) $(LDLIBS)

#CPPFLAGS += -I../common

# library dependency
# LDLIBS+= -lcmd
LDLIBS += -lc -lpam -lnsl

# resolve with local variables in shared library
DYNFLAGS += -Bsymbolic

LINTFLAGS=

###
# EXPORT DELETE END

DYNLIB= pam_sample.so$(VERS)
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

all: $(LIBS) 

defobjs: $(OBJ_DIR) .WAIT $(DEFOBJS)

defpics: $(PIC_DIR) .WAIT $(DEFPICS)

$(ROOTLIBDIR):
	$(INS.dir)

install: all $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLIBDIR)/pam_sample.so

$(ROOTLIBDIR)/pam_sample.so:
	$(RM) $@; $(SYMLINK) $(DYNLIB) $@

lint: $(SRCS:.c=.ln)

# include library targets
include ../../Makefile.targ

