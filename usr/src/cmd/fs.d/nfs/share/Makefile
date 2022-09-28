#
#ident	"@(#)Makefile	1.16	96/04/30 SMI"	/* SVr4.0 1.12	*/
#
# Copyright (c) 1993,1995 by Sun Microsystems, Inc.
#

FSTYPE=		nfs
LIBPROG=	share
ATTMK=		$(LIBPROG)

COMMON=		nfs_sec.o sharetab.o
LOCAL=		$(LIBPROG).o issubdir.o
OBJS=		$(LOCAL) $(COMMON)
SRCS=		$(LOCAL:%.o=%.c) ../lib/nfs_sec.c ../lib/sharetab.c
OTHERINSTALL=	$(ROOTETC)/dfs/fstypes

include		../../Makefile.fstype

LDLIBS +=	-lnsl
CFLAGS += -o $@

$(ROOTETC)/dfs/fstypes :=	FILEMODE= 644
$(ROOTETC)/dfs/fstypes :=	OWNER= root
$(ROOTETC)/dfs/fstypes :=	GROUP= root

$(ROOTETC)/dfs/%: %
	$(INS.file)

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

nfs_sec.o:	../lib/nfs_sec.c
		$(COMPILE.c) ../lib/nfs_sec.c

sharetab.o:	../lib/sharetab.c
		$(COMPILE.c) ../lib/sharetab.c

lint:	lint_SRCS

clean:     
	$(RM) $(OBJS)
