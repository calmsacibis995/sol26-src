#
#pragma ident	"@(#)Makefile.com	1.6	95/10/24 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# cmd/adb/ppc/kadb/Makefile.com
#
# to be included by kernel-architecture makefiles in local subdirectories.
# The builds in those subdirectories permit the NSE to track separate
# kernel-architecture dependencies
#

PROG= 		../$(ARCH)-kadb.o

COMMON=		../../../common
PPC=		../..
SYSDIR=		../../../../../uts
ARCHDIR=	${SYSDIR}/${ARCH}
MACHDIR=	$(SYSDIR)/$(MACH)
MMUDIR=		$(SYSDIR)/$(MMU)
HEADDIR=	../../../../../head
DISHOME=	../../../../sgs/dis/$(MACH)

OBJ_TARGET=	adb_ptrace.o accesspr.o opsetpr.o setuppr.o \
		runpcs.o printpr.o command_$(MACH).o
OBJ_COM=	access.o command.o expr.o fio.o format.o input.o \
		output.o pcs.o print.o sym.o
OBJ_DIS=	ppcdis_parse.o ppcdis_utl.o ppcdis_print.o
OBJS=		$(OBJ_COM) $(OBJ_TARGET) $(OBJ_DIS)

SRC_TARGET=	$(OBJ_TARGET:%.o=%.c)
SRC_COM=	$(OBJ_COM:%.o=$(COMMON)/%.c)
SRC_DIS=	$(OBJ_DIS:%.o=$(DISHOME)/%.c)
SRCS=		$(SRC_TARGET) $(SRC_COM) $(SRC_DIS)

include ../../../../Makefile.cmd

# override default ARCH value
#ARCH=	$(KARCH)

KREDEFS= -Dprintf=_printf -Dopenfile=_openfile -Dexit=_exit \
	-Dopen=_open -Dclose=_close -Dlseek=_lseek -Dread=_read	\
	-Dwrite=_write -Dsetjmp=_setjmp -Dlongjmp=_longjmp

CPPINCS=	-I${PPC} -I${COMMON} -I$(SYSDIR)/prep -I$(SYSDIR)/ppc \
		-I${SYSDIR}/common -I$(HEADDIR)

CPPFLAGS=	-D$(ARCH) $(CPPINCS) -D_KERNEL -D_MACHDEP \
		-D_KADB -DKADB -D__ELF ${KREDEFS} $(ARCHOPTS) $(CPPFLAGS.master)

# flags needed to build the disassembler in little endian mode
#HUH   DISDEFS=	-DAR32WR -DM32 -DMC98601 -DMC98603 -DRBO -DPORTAR -DELF -DPPC
DISDEFS=	-Ui386 -DM32 -DMC98601 -DMC98603 -DRBO -DELF -D__LITTLE_ENDIAN

OUTPUT_OPTIONS =	-o $@

# build rules
%.o : $(COMMON)/%.c
	$(COMPILE.c) $<

%.o : $(PPC)/%.c
	$(COMPILE.c) $<

%.o : $(DISHOME)/%.c
	$(COMPILE.c) $(DISDEFS) $(OUTPUT_OPTIONS) $<

.KEEP_STATE:

.PARALLEL:	$(OBJS)

all:	$(PROG)

$(PROG): $(OBJS)
	$(LD) -r -o $@ $(OBJS) 

install:

clean:
	$(RM) ${OBJS}

link:	lint_SRCS

include	../../../../Makefile.targ
