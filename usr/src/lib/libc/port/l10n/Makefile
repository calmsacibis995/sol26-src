#
#pragma ident	"@(#)Makefile	1.1	96/06/19 SMI"
#

LIBRARY= methods_l10n.a
VERS = .1

all := TARGET=all
install := TARGET=install
clean := TARGET=clean
clobber := TARGET=clobber

include ../../../Makefile.lib

CPPFLAGS= $(INCLIST) $(CPPFLAGS.master)

LIBS = $(DYNLIB)


.KEEP_STATE:

all: $(MACHLIBS)

#install: all $(ROOTLIBS) $(ROOTLINKS)

install: all

$(ROOTLIBDIR):
	$(INS.dir)

clean clobber: FRC
	@cd $(MACH); pwd; $(MAKE) $@

include ../../../Makefile.mach

FRC:

