#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.14	96/09/18 SMI"
#
# lib/libauth/Makefile
#
LIBRARY= libauth.a
VERS = .1

OBJECTS= ia_framework.o

# include library definitions
include ../Makefile.lib

# library dependency
LDLIBS += -lcmd -ldl -lc

LIBS += $(DYNLIB)

MAPFILE=	mapfile-vers

#SCHEMEDIR=../scheme/unix_scheme

#include $(SCHEMEDIR)/Makefile.objs

#SCH_OBJS=$(DEFOBJS:%=$(SCHEMEDIR)/%)
#SCH_PICS =$(DEFPICS:%=$(SCHEMEDIR)/%)
OBJS += $(SCH_OBJS)
PICS += $(SCH_PICS)
DYNFLAGS += -M $(MAPFILE)

$(DYNLIB): $(MAPFILE)

.KEEP_STATE:

all:  $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS)

lint: llib-lauth.ln

# include library targets
include ../Makefile.targ

#additional dependencies

$(LIBRARY) : $(OBJS)
$(DYNLIB) : $(PICS)

#defobjs defpics : FRC
#	@cd $(SCHEMEDIR); pwd; $(MAKE) $@
#	@pwd

FRC:
