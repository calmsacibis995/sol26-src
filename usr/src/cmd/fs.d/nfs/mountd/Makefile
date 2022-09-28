#
#ident	"@(#)Makefile	1.25	96/04/30 SMI"	/* SVr4.0 1.17 */
#
# Copyright (c) 1989-1996, by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		nfs
TYPEPROG=	mountd
ATTMK=		$(TYPEPROG)

include		../../Makefile.fstype

COMMON=		fslib.o nfs_sec.o sharetab.o
LOCAL=		mountd.o netgroup.o rmtab.o nfsauth.o \
		nfsauth_xdr.o svc_local.o exportlist.o
OBJS= 		$(LOCAL) $(COMMON)
SRCS=		$(LOCAL:%.o=%.c) \
		../../fslib.c ../lib/nfs_sec.c ../lib/sharetab.c
LDLIBS +=	-lrpcsvc -lnsl -lbsm -lthread -lsocket
CFLAGS +=	-o $@
CPPFLAGS +=	-D_REENTRANT

$(TYPEPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

nfsauth_xdr.c:	$(ROOT)/usr/include/rpcsvc/nfsauth_prot.x
	$(RPCGEN) -C -c -o $@ $(ROOT)/usr/include/rpcsvc/nfsauth_prot.x

fslib.o:	../../fslib.c
		$(COMPILE.c) ../../fslib.c

nfs_sec.o:	../lib/nfs_sec.c
		$(COMPILE.c) ../lib/nfs_sec.c

sharetab.o:	../lib/sharetab.c
		$(COMPILE.c) ../lib/sharetab.c

lint:	lint_SRCS

clean:     
	$(RM) $(OBJS) nfsauth_xdr.c
