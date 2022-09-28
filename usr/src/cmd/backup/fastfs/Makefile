#
#ident  "@(#)Makefile 1.7     96/03/18 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/backup/fastfs/Makefile
#

include ../Makefile.backup

FASTFS=		fastfs

HSMPROG=	$(FASTFS)
PRODUCT=	$(HSMPROG)

OBJECTS=	fastfs.o
SRCS=		$(OBJECTS:.o=.c)

GENERAL=	../include
CPPFLAGS=	-I$(GENERAL) -DPARTIAL $(USG) \
			-DHSMROOT=\"$(HSMROOT)\" $(CPPFLAGS.master)
CCMODE=		$(ANSI)

CLOBBERFILES=	$(PRODUCT) $(DEBUGPRODUCTS)

FILEMODE=	0555

.KEEP_STATE:

all hsm:	$(PRODUCT) 

$(HSMPROG):	$(OBJECTS)
	$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS) $(HSMLDLIBS)
	$(POST_PROCESS)

FRC:

install hsminstall:	hsm $(HSMROOTPKGSBINPROG)

lint:		lint_SRCS

clean:
	$(RM) $(OBJECTS) $(DEBUGOBJS)

include ../Makefile.targ
