#
#ident	"@(#)Makefile	1.29	96/06/05 SMI"
#
# Copyright (c) 1995 - 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/autofs/Makefile

FSTYPE=		autofs
AUTO=		automount
MOUNT=		mount
VERS=		.2
FNSO=		fn.so$(VERS)
LIBPROG=	$(AUTO) $(MOUNT) $(FNSO)
TYPEPROG=	automountd

LINKINSTALL=	$(ROOTUSRSBIN)/$(AUTO)
LINKVALUE=	../lib/fs/$(FSTYPE)/$(AUTO)
MAPS=		auto_master auto_home
MAPINSTALL=	$(MAPS:%=$(ROOTETC)/%)
OTHERINSTALL=	$(MAPINSTALL) $(LINKINSTALL);

include		../Makefile.fstype

$(MAPINSTALL)	:= OWNER= root
$(MAPINSTALL)	:= FILEMODE= 0644

COMMON=		ns_generic.o ns_files.o ns_nisplus.o ns_nis.o \
		auto_mnttab.o auto_subr.o

AUTOOBJS=	automount.o $(COMMON) fslib.o

MOUNTOBJS=	mount.o fslib.o

LOCAL=		autod_main.o key_generic.o \
		autod_parse.o autod_mount.o autod_nfs.o nfs_cast.o \
		autod_autofs.o autod_xdr.o autod_readdir.o autod_lookup.o
TYPEOBJS=	$(LOCAL) $(COMMON) replica.o nfs_sec.o nfs_subr.o fslib.o

FNOBJS=		ns_fnmount.o ns_fnreaddir.o ns_fnutils.o

POFILE= autofs.po
GREP= egrep
SED= sed

DYNFLAGS=	-G -ztext -h $(FNSO)

$(AUTO) := 	LDLIBS += -lnsl -ldl
$(TYPEPROG) :=	LDLIBS += -lrpcsvc -lsocket -lnsl -ldl -lthread
$(TYPEPROG) :=	LDFLAGS += -R/usr/lib/fs/$(FSTYPE)
$(FNSO) :=	LDLIBS += -lxfn -lc
$(FNSO) :=	LDFLAGS += $(DYNFLAGS)

CFLAGS +=	-v -D_FILE_OFFSET_BITS=64 -o $@
$(FNOBJS) :=	CFLAGS += -K pic
CPPFLAGS=	-I.. -I../nfs/lib $(CPPFLAGS.master) -D_REENTRANT
OBJS=		$(AUTOOBJS) $(MOUNTOBJS) $(TYPEOBJS) $(FNOBJS)

AUTOSRCS=	automount.c $(COMMON:%.o=%.c) ../fslib.c
MOUNTSRCS=	mount.c ../fslib.c
TYPESRCS=	$(LOCAL:%.o=%.c) $(COMMON:%.o=%.c) \
		../nfs/lib/replica.c ../nfs/lib/nfs_sec.c \
		../nfs/lib/nfs_subr.c ../fslib.c
FNSRCS=		$(FNOBJS:%.o=%.c)
SRCS=		$(OBJS:%.o=%.c)

ns_generic.o :=	CPPFLAGS += -DFN_SO=\"$(FNSO)\"

$(AUTO):	$(AUTOOBJS)
		$(LINK.c) -o $@ $(AUTOOBJS) $(LDLIBS)
		$(POST_PROCESS)

$(MOUNT):	$(MOUNTOBJS)
		$(LINK.c) -o $@ $(MOUNTOBJS) $(LDLIBS)
		$(POST_PROCESS)

$(TYPEPROG):	$(TYPEOBJS)
		$(LINK.c) -o $@ $(TYPEOBJS) $(LDLIBS)
		$(POST_PROCESS)

$(FNSO):	$(FNOBJS)
		$(LINK.c) -o $@ $(FNOBJS) $(LDLIBS)
		$(POST_PROCESS_SO)

$(ROOTUSRSBIN)/$(AUTO):
		$(RM) $@; $(SYMLINK) $(LINKVALUE) $@

fslib.o:	../fslib.c
		$(COMPILE.c) ../fslib.c

replica.o:	../nfs/lib/replica.c
		$(COMPILE.c) ../nfs/lib/replica.c

nfs_sec.o:	../nfs/lib/nfs_sec.c
		$(COMPILE.c) ../nfs/lib/nfs_sec.c

nfs_subr.o:	../nfs/lib/nfs_subr.c
		$(COMPILE.c) ../nfs/lib/nfs_subr.c

catalog: $(POFILE)

$(POFILE):
	$(RM) messages.po
	$(SED) -e 's/pr_msg/gettext/' `$(GREP) -l "pr_msg|gettext" *.[ch]` | \
	$(XGETTEXT) $(XGETFLAGS) -
	$(SED) -e '/^# msg/d' -e '/^domain/d' < messages.po > $@
	$(RM) messages.po

lint:
	$(LINT.c) $(AUTOSRCS)
	$(LINT.c) $(MOUNTSRCS)
	$(LINT.c) $(TYPESRCS)
	$(LINT.c) $(FNSRCS)

clean:
	$(RM) $(OBJS)
