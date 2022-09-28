#
# ident	"@(#)Makefile	1.12	96/08/07 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/fs.d/nfs/statd/Makefile

FSTYPE=		nfs
TYPEPROG=	statd
ATTMK=		$(TYPEPROG)

include		../../Makefile.fstype
CPPFLAGS +=     -D_REENTRANT -DSUN_THREADS

OBJS= 		sm_svc.o sm_proc.o sm_statd.o 

SRCS=		$(OBJS:%.o=%.c)

LDLIBS +=	-lsocket -lrpcsvc -lnsl -lthread

$(TYPEPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)
		$(LOCK_LINT)

lint:		lint_SRCS

clean:
		$(RM) $(OBJS) $(TYPEPROG)



