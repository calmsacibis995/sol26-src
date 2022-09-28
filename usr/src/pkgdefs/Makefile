#
#ident	"@(#)Makefile	1.52	96/07/17 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# pkgdefs/Makefile

include $(SRC)/Makefile.master

sparc_SUBDIRS= \
	SUNWbcp  \
	SUNWcar.c  \
	SUNWcar.d  \
	SUNWcar.m  \
	SUNWcar.u  \
	SUNWcg6.c  \
	SUNWcg6.d  \
	SUNWcg6.m  \
	SUNWcg6.u  \
	SUNWcg6h  \
	SUNWdfb.c  \
	SUNWdfb.d  \
	SUNWdfb.m  \
	SUNWdfb.u  \
	SUNWkey  \
	SUNWkvm.c  \
	SUNWkvm.d  \
	SUNWkvm.m  \
	SUNWkvm.u  \
	SUNWssadv  \
	SUNWssaop  \
	SUNWsxr.m  \
	SUNWhmd    \
	SUNWhmdu   \
	SUNWpd    \
	SUNWpdu	  \
	SUNWcpr.m \
	SUNWcpr.u \
	SUNWvygdr.m 

i386_SUBDIRS= \
	SUNWcar.i  \
	SUNWcoff  \
	SUNWdfb.i  \
	SUNWkvm.i  \
	SUNWs53

ppc_SUBDIRS= \
	SUNWkey  \
	SUNWkvm.p \
	SUNWcar.p \
	SUNWdfb.p

COMMON_SUBDIRS= \
	SUNWaccr  \
	SUNWaccu  \
	SUNWapppr  \
	SUNWapppu  \
	SUNWatfsr  \
	SUNWatfsu  \
	SUNWarc  \
	SUNWast  \
	SUNWbnur  \
	SUNWbnuu  \
	SUNWbtool  \
	SUNWcsd  \
	SUNWcsr  \
	SUNWcsu  \
	SUNWdfbh  \
	SUNWdhcsr  \
	SUNWdhcsu  \
	SUNWdoc  \
	SUNWesu  \
	SUNWfac  \
	SUNWfns  \
	SUNWfnsx5  \
	SUNWhea  \
	SUNWipc  \
	SUNWloc  \
	SUNWnisr  \
	SUNWnisu  \
	SUNWosdem \
	SUNWypr \
	SUNWypu \
	SUNWpcmci  \
	SUNWpcmcu  \
	SUNWpcr	\
	SUNWpcu	\
	SUNWpppk  \
	SUNWpsf	\
	SUNWpmu \
	SUNWpsr	\
	SUNWpsu	\
	SUNWscpr  \
	SUNWscpu  \
	SUNWscplp  \
	SUNWsra  \
	SUNWsrh  \
	SUNWsutl  \
	SUNWter  \
	SUNWtnfc  \
	SUNWtnfd  \
	SUNWtoo  \
	SUNWvolr  \
	SUNWvolu  \
	SUNWxcu4  \
	SUNWxwdv \
	SUNWpmr

SUBDIRS= $($(MACH)_SUBDIRS) $(COMMON_SUBDIRS)

XMODS= \
	SUNWcry  \
	SUNWstatl  \
	SUNWsvvs

all :=         		TARGET= all
install := 		TARGET= install
install_h :=            TARGET= install_h
clean :=                TARGET= clean
clobber :=              TARGET= clobber

%: %.ksh
	$(RM) $@
	cp $< $@
	chmod +x $@

.KEEP_STATE:

.PARALLEL: $(SUBDIRS) $(XMODS)

all install: awk_pkginfo $(SUBDIRS) $(XMODS)

clean clobber: $(SUBDIRS) $(XMODS)
	$(RM) awk_pkginfo bld_awk_pkginfo

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) -i $(TARGET)

$(XMODS):       FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

awk_pkginfo: bld_awk_pkginfo
	./bld_awk_pkginfo -m $(MACH) -p "$(RELEASE)/$(VERSION)" -o $@

FRC:
