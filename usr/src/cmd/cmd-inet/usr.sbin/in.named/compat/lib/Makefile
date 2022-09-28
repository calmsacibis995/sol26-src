#
#pragma ident	"@(#)Makefile	1.2	96/05/15 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/in.named/compat/lib/Makefile
#
LIBRARY= lib44bsd.a

objs/%.o : %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

OBJECTS   = mktemp.o strcasecmp.o strerror.o strpbrk.o strtoul.o \
	putenv.o setenv.o setitimer.o writev.o ftruncate.o gettimeofday.o

# include library definitions
include ../../../../../../lib/Makefile.lib

SOLCOMPAT = -Dgethostbyname=res_gethostbyname \
	-Dgethostbyaddr=res_gethostbyaddr -Dgetnetbyname=res_getnetbyname \
	-Dgetnetbyaddr=res_getnetbyaddr -Dsethostent=res_sethostent \
	-Dendhostent=res_endhostent -Dgethostent=res_gethostent \
	-Dsetnetent=res_setnetent -Dendnetent=res_endnetent \
	-Dgetnetent=res_getnetent
LOCDEFS   =
COMPDIR   = ../../compat
COMPINCL  = $(COMPDIR)/include

CPPFLAGS += -DSVR4 -D_SYS_STREAM_H $(SOLCOMPAT) $(LOCDEFS) -I$(COMPINCL)

.KEEP_STATE:

LIBS += $(LIBRARY)

all: $(LIBS)

install: all

# include library targets
include ../../../../../../lib/Makefile.targ
