#
# ident	"@(#)Makefile	1.12	96/04/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/model/Makefile
#

include			../Makefile.lp

SUBDIRS =		netpr

PROG =			lp.set		\
			lp.cat		\
			lp.tell		\
			drain.output

SRCS =			$(PROG:%=%.c)

OBJS =			$(PROG:%=%.o)

MODELS =		standard	\
			netstandard

MISC =			alert.proto


ROOTLIBLPMODEL=		$(ROOTLIBLP)/model
ROOTLIBLPBIN =		$(ROOTLIBLP)/bin

ROOTMISC =		$(MISC:%=$(ROOTLIBLPBIN)/%)
ROOTLPPROG =		$(PROG:%=$(ROOTLIBLPBIN)/%)
ROOTMODELS =		$(MODELS:%=$(ROOTLIBLPMODEL)/%)

CPPFLAGS =		-I$(LPINC) $(CPPFLAGS.master)

# conditional assignments
lp.tell :=			LDLIBS += $(LIBMSG) $(LIBLP) -lcurses
lp.set drain.output lp.cat :=	LDLIBS += $(LIBLP) -lcurses

$(ROOTMISC) :=		FILEMODE = 0444

.KEEP_STATE:

# for messaging catalog
#
POFILE= model.po
XGETFLAGS=-a -x $(LPROOT)/lp.xcl
POFILES= $(SRCS:%.c=%.po)


all:			$(PROG) $(MODELS) $(MISC) $(SUBDIRS)

install:		all .WAIT $(ROOTLPPROG) $(ROOTMODELS) $(ROOTMISC) \
			$(SUBDIRS)

$(ROOTLIBLPMODEL)/% $(ROOTLIBLPBIN)/%:	%
			$(INS.file)

catalog:        $(POFILE)

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat     $(POFILES)      > $@

clean:			$(SUBDIRS)
			$(RM) $(OBJS)

strip:
			$(STRIP) $(PROG)

lint :			lint_SRCS

include			../../Makefile.targ

$(SUBDIRS) :	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
