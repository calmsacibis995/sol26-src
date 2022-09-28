#
# ident	"@(#)Makefile	1.18	96/09/04 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/lp/Makefile
#

include 	./Makefile.lp

#
# for messaging caralog, SUBDIRS2 is defined.
#
SUBDIRS1=               etc       filter crontab terminfo
SUBDIRS2=       lib cmd     model
SUBDIRS =       $(SUBDIRS1) $(SUBDIRS2)

ROOTDIRS =					\
		$(ROOTVAR)/lp			\
		$(ROOTVAR)/lp/logs		\
		$(ROOTLIBLP)			\
		$(ROOTLIBLP)/bin		\
		$(ROOTLIBLP)/model		\
		$(ROOTLIBLPLOCL)		\
		$(ROOTLIB)/print		\
		$(ROOTLIB)/print/bsd-adaptor	\
		$(ROOTETCLP)			\
		$(ROOTETCLP)/classes		\
		$(ROOTETCLP)/forms		\
		$(ROOTETCLP)/interfaces	\
		$(ROOTETCLP)/printers		\
		$(ROOTETCLP)/pwheels		\
		$(ROOTVARSP)/lp			\
		$(ROOTVARSP)/lp/admins		\
		$(ROOTVARSP)/lp/requests 	\
		$(ROOTVARSP)/lp/system		\
		$(ROOTVARSP)/lp/fifos		\
		$(ROOTVARSP)/lp/fifos/private 	\
		$(ROOTVARSP)/lp/fifos/public

PRIVATEDIR =	$(ROOTVARSP)/lp/fifos/private
PUBLICDIR =	$(ROOTVARSP)/lp/fifos/public

SYMDIR1 =	$(ROOTVARSP)/lp/admins/lp
SYMDIR2 =	$(ROOTVARSP)/lp/bin
SYMDIR3 =	$(ROOTVARSP)/lp/logs
SYMDIR4 =	$(ROOTETCLP)/logs
SYMDIR5 =	$(ROOTVARSP)/lp/model

$(SYMDIR1) :=	SYMLNKDEST =	../../../../etc/lp
$(SYMDIR2) :=	SYMLNKDEST =	../../../usr/lib/lp/bin
$(SYMDIR3) :=	SYMLNKDEST =	../../lp/logs
$(SYMDIR4) :=	SYMLNKDEST =	../../var/lp/logs
$(SYMDIR5) :=	SYMLNKDEST =	../../../usr/lib/lp/model

ROOTSYMLINKDIRS =	$(SYMDIR1) $(SYMDIR2) $(SYMDIR3) $(SYMDIR4) $(SYMDIR5)

$(PRIVATEDIR) :=		DIRMODE = 0771
$(PUBLICDIR) :=			DIRMODE = 0773
$(ROOTVAR)/lp :=		DIRMODE = 775
$(ROOTVAR)/lp/logs :=		DIRMODE = 775
$(ROOTETCLP) :=			DIRMODE = 775
$(ROOTETCLP)/classes :=		DIRMODE = 775
$(ROOTETCLP)/forms :=		DIRMODE = 775
$(ROOTETCLP)/interfaces :=	DIRMODE = 775
$(ROOTETCLP)/printers :=	DIRMODE = 775
$(ROOTETCLP)/pwheels :=		DIRMODE = 775
$(ROOTVARSP)/lp :=		DIRMODE = 775
$(ROOTVARSP)/lp/admins :=	DIRMODE = 775
$(ROOTVARSP)/lp/requests :=	DIRMODE = 775
$(ROOTVARSP)/lp/system :=	DIRMODE = 775
$(ROOTVARSP)/lp/fifos :=	DIRMODE = 775
$(ROOTLIB)/print :=		OWNER = root
$(ROOTLIB)/print :=		GROUP = lp
$(ROOTLIB)/print :=		DIRMODE = 0755
$(ROOTLIB)/print/bsd-adaptor :=		OWNER = lp
$(ROOTLIB)/print/bsd-adaptor :=		GROUP = lp
$(ROOTLIB)/print/bsd-adaptor :=		DIRMODE = 0755
#
# for messaging catalog
#
_msg    :=      TARGET = catalog
POFILE= lp.po
POFILES= $(SUBDIRS2:%=%/%.po)

.KEEP_STATE:

all:	$(TXTS) $(SUBDIRS)

# for messaging catalog
_msg:   $(MSGDOMAIN) $(SUBDIRS2)
	$(RM)	$(POFILE)
	cat     $(POFILES)      > $(POFILE)
	$(RM)  $(MSGDOMAIN)/$(POFILE)
	cp $(POFILE)    $(MSGDOMAIN)

install: $(ROOTDIRS) $(ROOTSYMLINKDIRS) $(SUBDIRS)

clean strip lint: $(SUBDIRS)

clobber: $(SUBDIRS) local_clobber

local_clobber:
	$(RM) $(CLOBBERFILES)

$(ROOTDIRS) $(MSGDOMAIN):
	$(INS.dir)

$(ROOTSYMLINKDIRS) :
	-$(RM) $@; $(SYMLINK) $(SYMLNKDEST) $@

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

# only for lpsched
CSPATHS = ./include ./lib ./cmd/lpsched
# for all commands including lpsched
#CSPATHS = ./include ./lib ./filter ./model ./cmd
CSCOPE	= cscope
CTAGS	= ctags
WC = wc

tags: cscope.files
	${CTAGS} -wt `cat cscope.files`

cscope.out: cscope.files
	${CSCOPE} -b -i ./cscope.files -f `pwd`/$@

cscope.files:   FRC
	@$(RM) -f cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    \( -type d -name '.del*' \) -prune -o -type f \
	    \( -name '*.[ch]' -o -name 'Makefile*' \) \
	    -a ! -name '.del*' -print > cscope.files
	@${WC} -l cscope.files

FRC:
