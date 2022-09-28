#
#ident	"@(#)Makefile	1.8	96/03/19 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# lib/librac/Makefile
#
LIBRARY= librac.a
VERS = .1

# objects are listed by source directory

RPC=  \
clnt_generic.o clnt_dg.o rac.o clnt_vc.o  rpcb_clnt.o xdr_rec_subr.o xdr_rec.o


OBJECTS= $(RPC)

# librac build rules

objs/%.o profs/%.o pics/%.o: rpc/%.c
	$(COMPILE.c) -DPORTMAP -DNIS  -o $@  $<
	$(POST_PROCESS_O)

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers

LIBS += $(DYNLIB)

LDLIBS += -lnsl -ldl -lc
DYNFLAGS += -M $(MAPFILE)


.KEEP_STATE:

all:  $(LIBS)

txts: $(TXTS)

install: $(LIBS) $(ROOTLIBS) $(ROOTLINKS)

$(DYNLIB):	$(MAPFILE)

include ../Makefile.targ
