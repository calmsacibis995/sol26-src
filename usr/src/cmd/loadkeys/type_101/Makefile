#
# @(#)Makefile 1.24 95/06/21 SMI
#
# Copyright (c) 1995, by Sun Microsystems, Inc.
#
# cmd/loadkeys/type_101/Makefile

KEYTABLES = \
	canadian_french2 \
	denmark \
	france \
	germany \
	italy \
	japan \
	korea \
	netherlands \
	norway \
	portugal \
	reset \
	spain \
	sweden \
	swiss_french \
	swiss_german \
	uk \
	us

include ../../Makefile.cmd

ROOTKEYTABLESDIR= $(ROOTSHLIB)/keytables
ROOTKEYDIR= $(ROOTKEYTABLESDIR)/type_101
ROOTKEYTABLES= $(KEYTABLES:%=$(ROOTKEYDIR)/%)

# there is an install target for each ROOT layout link
#
LAYOUTS= \
	layout_00 \
	layout_01 \
	layout_23 \
	layout_24 \
	layout_25 \
	layout_26 \
	layout_27 \
	layout_28 \
	layout_29 \
	layout_2a \
	layout_2b \
	layout_2c \
	layout_2d \
	layout_2e \
	layout_2f \
	layout_31 \
	layout_32

ROOTLINKS= $(LAYOUTS:%=$(ROOTKEYDIR)/%)

$(ROOTKEYTABLES) := FILEMODE = 444

# install rule
$(ROOTKEYDIR)/%: %
	$(INS.file)

.KEEP_STATE:

all:  $(KEYTABLES)

install: all $(ROOTKEYTABLESDIR) $(ROOTKEYDIR) $(ROOTKEYTABLES) $(ROOTLINKS)

clean:

$(ROOTKEYTABLESDIR):
	$(INS.dir)

$(ROOTKEYDIR): $(ROOTKEYTABLESDIR)
	$(INS.dir)

# install targets for ROOT layout links
#
$(ROOTKEYDIR)/layout_00: $(ROOTKEYDIR)/us
	$(RM) $@; $(LN) $(ROOTKEYDIR)/us $@

$(ROOTKEYDIR)/layout_01:	$(ROOTKEYDIR)/us
	$(RM) $@; $(LN) $(ROOTKEYDIR)/us $@

# $(ROOTKEYDIR)/layout_22:	$(ROOTKEYDIR)/j3100
# 	$(RM) $@; $(LN) $(ROOTKEYDIR)/j3100 $@

$(ROOTKEYDIR)/layout_23:	$(ROOTKEYDIR)/france
	$(RM) $@; $(LN) $(ROOTKEYDIR)/france $@

$(ROOTKEYDIR)/layout_24:	$(ROOTKEYDIR)/denmark
	$(RM) $@; $(LN) $(ROOTKEYDIR)/denmark $@

$(ROOTKEYDIR)/layout_25:	$(ROOTKEYDIR)/germany
	$(RM) $@; $(LN) $(ROOTKEYDIR)/germany $@

$(ROOTKEYDIR)/layout_26:	$(ROOTKEYDIR)/italy
	$(RM) $@; $(LN) $(ROOTKEYDIR)/italy $@

$(ROOTKEYDIR)/layout_27:	$(ROOTKEYDIR)/netherlands
	$(RM) $@; $(LN) $(ROOTKEYDIR)/netherlands $@

$(ROOTKEYDIR)/layout_28:	$(ROOTKEYDIR)/norway
	$(RM) $@; $(LN) $(ROOTKEYDIR)/norway $@

$(ROOTKEYDIR)/layout_29:	$(ROOTKEYDIR)/portugal
	$(RM) $@; $(LN) $(ROOTKEYDIR)/portugal $@

$(ROOTKEYDIR)/layout_2a:	$(ROOTKEYDIR)/spain
	$(RM) $@; $(LN) $(ROOTKEYDIR)/spain $@

$(ROOTKEYDIR)/layout_2b:	$(ROOTKEYDIR)/sweden
	$(RM) $@; $(LN) $(ROOTKEYDIR)/sweden $@

$(ROOTKEYDIR)/layout_2c:	$(ROOTKEYDIR)/swiss_french
	$(RM) $@; $(LN) $(ROOTKEYDIR)/swiss_french $@

$(ROOTKEYDIR)/layout_2d:	$(ROOTKEYDIR)/swiss_german
	$(RM) $@; $(LN) $(ROOTKEYDIR)/swiss_german $@

$(ROOTKEYDIR)/layout_2e:	$(ROOTKEYDIR)/uk
	$(RM) $@; $(LN) $(ROOTKEYDIR)/uk $@

$(ROOTKEYDIR)/layout_2f:	$(ROOTKEYDIR)/korea
	$(RM) $@; $(LN) $(ROOTKEYDIR)/korea $@

# $(ROOTKEYDIR)/layout_30:	$(ROOTKEYDIR)/taiwan
# 	$(RM) $@; $(LN) $(ROOTKEYDIR)/taiwan $@

$(ROOTKEYDIR)/layout_31:	$(ROOTKEYDIR)/japan
	$(RM) $@; $(LN) $(ROOTKEYDIR)/japan $@

$(ROOTKEYDIR)/layout_32:	$(ROOTKEYDIR)/canadian_french2
	$(RM) $@; $(LN) $(ROOTKEYDIR)/canadian_french2 $@

include ../../Makefile.targ
