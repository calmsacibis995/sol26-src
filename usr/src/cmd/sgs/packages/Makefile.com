#
#ident	"@(#)Makefile.com	1.4	96/08/19 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

include		$(SRC)/Makefile.master

PKGARCHIVE=	./
DATAFILES=	copyright prototype preinstall postremove depend
README=		SUNWonld-README
FILES=		$(DATAFILES) pkginfo
PACKAGE= 	SUNWonld
ROOTONLD=	$(ROOT)/opt/SUNWonld
ROOTREADME=	$(README:%=$(ROOTONLD)/%)

CLEANFILES=	$(FILES) awk_pkginfo ../bld_awk_pkginfo
CLOBBERFILES=	$(PACKAGE)

../%:		../common/%.ksh
		$(RM) $@
		cp $< $@
		chmod +x $@

$(ROOTONLD)/%:	../common/%
		$(RM) $@
		cp $< $@
		chmod +x $@
