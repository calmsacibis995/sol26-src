#
#ident	"@(#)Makefile	1.41	96/06/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/Makefile

include		$(SRC)/cmd/Makefile.cmd

SUBDIRS-common= libdl		\
		libelf		\
		libconv		\
		liblddbg	\
		.WAIT		\
		libld		\
		libldmake	\
		libldstab	\
		librtld		\
		.WAIT		\
		0@0		\
		ld		\
		ldd		\
		lddstub		\
		rtld		\
		librtld_db	\
		pvs		\
		ar		\
		dis		\
		dump		\
		error		\
		gprof		\
		lex		\
		lorder		\
		m4		\
		mcs		\
		nm		\
		prof		\
		ranlib		\
		size		\
		symorder	\
		tsort		\
		unifdef		\
		yacc

SUBDIRS-i386=
SUBDIRS-sparc=	rtld.4.x

SUBDIRS=	$(SUBDIRS-common) $(SUBDIRS-$(MACH))

# Messaging support
#
POSUBDIRS=	ar		m4		nm \
		tsort		yacc
POFILE=		sgs.po
POFILES=	$(POSUBDIRS:%=%/%.po)

MSGSUBDIRS=	ld		ldd		libld		liblddbg \
		libldstab	librtld		rtld		libelf \
		pvs
MSGDIR=		messages


all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
lint :=		TARGET= lint
_msg :=		TARGET= catalog


.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all install:	native-add .WAIT $(SUBDIRS)

include		$(SRC)/cmd/Makefile.targ

# Messaging support
#
_msg:		$(MSGDOMAIN)/$(POFILE) $(MSGDIR)

$(MSGDOMAIN)/$(POFILE): \
		$(MSGDOMAIN) $(POFILE)

$(POFILE):	$(POSUBDIRS)
		$(RM) $(POFILE)
		cat $(POFILES) > $(POFILE)

$(MSGDIR):	$(MSGSUBDIRS) FRC
		@ cd $@; pwd; $(MAKE) $(TARGET)

# built from lib/Makefile
install_lib:	FRC
		@ cd lex; pwd; $(MAKE) $@
		@ cd yacc; pwd; $(MAKE) $@

lint:		$(SUBDIRS)

delete \
clean clobber:	native-clobber .WAIT $(SUBDIRS) $(MSGDIR)

$(SUBDIRS):	FRC
		@ cd $@; pwd; $(MAKE) $(TARGET)


# Integration of ld and ld.so.1 in some developement cycles requires that both
# of these modules be built using the new ld.  This `native' target allows us
# to build a local ld which will then be used to build the delivered version of
# itself and ld.so.1.  Once this new functionality appears in the standard ld
# this target can be disabled.

native-add:	native-proto FRC
		@ cd tools/$(MACH); pwd; $(MAKE) native
		@ cd libdl/$(MACH); pwd; $(MAKE) native
		@ cd libelf/$(MACH); pwd; $(MAKE) native
		@ cd libconv/$(MACH); pwd; $(MAKE)
		@ cd liblddbg/$(MACH); pwd; $(MAKE) native
		@ cd libldstab/$(MACH); pwd; $(MAKE) native
		@ cd libld/$(MACH); pwd; $(MAKE) native
		@ cd ld/$(MACH); pwd; $(MAKE) native

native-clobber:
		@ cd tools; pwd; $(MAKE) $(TARGET)
		rm -rf proto/$(MACH)

native-proto:
		-@mkdir -p proto/$(MACH)

FRC:
