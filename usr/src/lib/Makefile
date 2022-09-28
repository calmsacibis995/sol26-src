#
#pragma ident	"@(#)Makefile	1.103	96/09/27 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/Makefile
#
# include global definitions
include ../Makefile.master

#	Note that libcurses installs commands along with its library.
#	This is a minor bug which probably should be fixed.
#
# Certain libraries are linked with, hence depend on, other libraries.
# These subordinate libraries must appear in the *_SUBDIRS list after
# the libraries on which they depend.
# The current dependency list is:
#	library		depends on
#	
#	libadm		libelf
#	libauth		libcmd
#	libdevinfo	libthread
#	libdhcp		libnsl libxfn
#	libkrb		libc
#	liblm		libelf
#	libnisdb	libc
#	libnsl		libc libmp
#	libpam		libc
#	librac		libc libnsl
#	libresolv	libnsl libsocket
#	libresolv2	libnsl libsocket
#	librpcsvc	libc libnsl
#	libsocket	libnsl
#	libtnf		libc
#	libtnfctl	libc libelf libintl
#	libvolmgt	libadm libc
#	libxnet		libsocket
#	nsswitch	libc libnsl libresolv libsocket libxfn
#	print		libnsl libsocket libxfn
#
#	Also note that a few extra libraries are kept in cmd source.

COMMON_SUBDIRS= \
	../cmd/sgs/libdl		\
	../cmd/sgs/libelf		\
	libnull		\
	libdl_stubs	\
	libw		\
	libintl		\
	libc		\
	libmp		\
	libTL		\
	libaio		\
	libadm		\
	libbsdmalloc	\
	libcmd		\
	libcrypt	\
	libcurses	\
	libdoor		\
	libeti		\
	libgen		\
	libgenIO	\
	libkstat	\
	libkvm		\
	liblm		\
	libmalloc	\
	libmapmalloc	\
	libnls		\
	libnsl		\
	libpkg		\
	libplot		\
	libsocket	\
	libresolv2	\
	libresolv	\
	librpcsvc	\
	libsys		\
	libnisdb	\
	libkrb		\
	nametoaddr	\
	librac		\
	libthread	\
	libauth		\
	libpam		\
	scheme		\
	pam_modules	\
	libbsm		\
	libposix4	\
	libvolmgt	\
	libdevice	\
	libdevid	\
	libdevinfo	\
	libc2		\
	libthread_db	\
	libsec		\
	libtnfprobe	\
	libtnf		\
	libtnfctl	\
	fn		\
	libdhcp		\
	nsswitch	\
	print		\
	libpthread	\
	libsched	\
	libxcurses	\
	libxnet

sparc_SUBDIRS= \
	libc_psr

SUBDIRS= $(COMMON_SUBDIRS) $($(MACH)_SUBDIRS)

#
# For message catalogue file creation
#
MSGSUBDIRS= libc libcmd libnsl libpkg libcurses

BWOSDIRS= libg libm libmsg libwindows

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
_msg :=		TARGET= _msg

.KEEP_STATE:

all clean clobber: $(SUBDIRS)

install: $(SUBDIRS) install_extra

# extra libraries kept in other source areas
install_extra:
	@cd ../cmd/mail; pwd; $(MAKE) install_lib
	@cd ../cmd/sgs; pwd; $(MAKE) install_lib
	@pwd

install_h check:
	@cd libc; pwd; $(MAKE) $@
	@cd libcurses; pwd; $(MAKE) $@
	@cd libdhcp; pwd; $(MAKE) $@
	@cd libeti; pwd; $(MAKE) $@
	@cd libgen; pwd; $(MAKE) $@
	@cd libintl; pwd; $(MAKE) $@
	@cd libkstat; pwd; $(MAKE) $@
	@cd libkvm; pwd; $(MAKE) $@
	@cd libw; pwd; $(MAKE) $@
	@cd librpcsvc; pwd; $(MAKE) $@
	@cd libkrb; pwd; $(MAKE) $@
	@cd libaio; pwd; $(MAKE) $@
	@cd libbsm; pwd; $(MAKE) $@
	@cd libvolmgt; pwd; $(MAKE) $@
	@cd libdevice; pwd; $(MAKE) $@
	@cd libdevid; pwd; $(MAKE) $@
	@cd libdevinfo; pwd; $(MAKE) $@
	@cd libtnfprobe; pwd; $(MAKE) $@
	@cd libtnf; pwd; $(MAKE) $@
	@cd libtnfctl; pwd; $(MAKE) $@
	@cd fn; pwd; $(MAKE) $@
	@cd libxcurses; pwd; $(MAKE) $@

_msg: $(MSGSUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
