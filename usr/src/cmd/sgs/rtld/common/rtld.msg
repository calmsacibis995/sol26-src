#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)rtld.msg	1.13	96/10/07 SMI"


@ _START_

# Message file for cmd/sgs/rtld (ld.so.1)

@ MSG_ID_RTLD


# Argument processing errors

@ MSG_ARG_ILLMODE	"illegal mode: %d"
@ MSG_ARG_ILLPATH	"illegal pathname"
@ MSG_ARG_ILLSYM	"illegal symbol name"
@ MSG_ARG_INVADDR	"address 0x%x does not fall within any mapped object"
@ MSG_ARG_INVHNDL	"invalid handle"
@ MSG_ARG_UNKNCONTAG	"unknown concurrency tag: 0x%x"


# General error diagnostics

@ MSG_GEN_NULLFILE	"attempt to open file with null name"
@ MSG_GEN_NOFILE	"%s: can't find file"
@ MSG_GEN_CACHED	"%s: cached file in use"
@ MSG_GEN_UNKNFILE	"%s: unknown file type"
@ MSG_GEN_CORTRUNC	"%s: corrupt or truncated file"
@ MSG_GEN_NOSYM		"%s: can't find symbol"
@ MSG_GEN_OLDREV	"%s: has older revision than expected %s"
@ MSG_GEN_BADCLASS	"%s: wrong ELF class or data encoding"
@ MSG_GEN_BADMACH	"%s: bad ELF machine type"
@ MSG_GEN_BADFLAGS	"%s: bad ELF flags value: 0x%x"
@ MSG_GEN_BADVERS	"%s: bad ELF file version: %d"
@ MSG_GEN_BADTYPE	"%s: bad ELF type: %s"
@ MSG_GEN_INVPRGHDR	"%s: invalid ELF program header, segments out of order"
@ MSG_GEN_NOLOADSEG	"%s: no loadable ELF segments found"
@ MSG_GEN_PROFSZCHG	"%s: file being profiled has changed size;\n\
			 \trecreate profile buffer"
@ MSG_GEN_MAPINUSE	"%s: required mapping 0x%x size 0x%x, \
			 is already in use by file %s"


# Relocation processing messages (some of these are required to satisfy
# do_reloc(), which is common code used by cmd/sgs/libld - make sure both
# message files remain consistant).

@ MSG_REL_UNIMPL	"relocation error: file %s: symbol %s: \
			 unimplemented relocation type: %d"
@ MSG_REL_OVERFLOW	"relocation error: file %s: symbol %s: \
			 value 0x%x overflows %d bits at offset 0x%x"
@ MSG_REL_NOSYM		"relocation error: file %s: symbol %s: \
			 referenced symbol not found"

@ MSG_REL_NONALIGN	"relocation error: %s: file %s: symbol %s: \
			 offset 0x%x is non-aligned"
@ MSG_REL_UNSUPSZ	"relocation error: %s: file %s: symbol %s: \
			 offset size (%d bytes) is not supported"
@ MSG_REL_UNNOBITS	"relocation error: %s: file %s: symbol %s: \
			 unsupported number of bits: %d"
@ MSG_REL_LOOSEBITS	"relocation error: %s: file %s: symbol %s: \
			 value 0x%x looses %d bits at offset 0x%x"
@ MSG_REL_NOFIT		"relocation error: %s: file %s: symbol %s: \
			 value 0x%x does not fit"

@ MSG_REL_PLTENTRY	"relocation error: %s: unable to process .plt[%d]: \
			 called from 0x%x: %s"
@ MSG_REL_PLTREF	"unidentifiable procedure reference"
@ MSG_REL_PLTOFF	"non-aligned .plt offset"
@ MSG_REL_RELOFF	"non-aligned relocation offset"


# System call messages.

@ MSG_SYS_OPEN		"%s: open failed: %s"
@ MSG_SYS_MMAP		"%s: mmap failed: %s"
@ MSG_SYS_MPROT		"%s: mprotect failed: %s"
@ MSG_SYS_FTRUNC	"%s: ftruncate failed: %s"
@ MSG_SYS_PROC		"%s: /proc error: %s"


# Debugging initialization diagnostics and formats

@ MSG_DBG_NOINABLE	"debugging is not enabled; recompile with -DDEBUG"
@ MSG_DBG_DISABLE	"debugging disabled; unable to process debug library"


# Versioning diagnostics.

@ MSG_VER_NFOUND	"%s: version `%s' not found (required by file %s)"


# Diagnostics generated under the control of ldd(1).

@ MSG_LDD_VER_FIND	"   find version=%s\n"
@ MSG_LDD_VER_FOUND	"\t%s (%s) =>\t %s\n"
@ MSG_LDD_VER_NFOUND	"\t%s (%s) =>\t (version not found)\n"

@ MSG_LDD_SYM_NFOUND	"\tsymbol not found: %s\t\t(%s)\n"

@ MSG_LDD_PTH_TRYING	"    trying path=%s\n"
@ MSG_LDD_PTH_ENVDIR	"    search path=%s  (LD_LIBRARY_PATH)\n"
@ MSG_LDD_PTH_RPATH	"    search path=%s  (RPATH from file %s)\n"
@ MSG_LDD_PTH_BGNDFL	"    search path="
@ MSG_LDD_PTH_ENDDFL	" (default)\n"
@ MSG_LDD_PTH_IGNORE	"    ignore path=%s  (insecure directory name)\n"
@ MSG_LDD_FMT_FILE	"%s "

@ MSG_LDD_FIL_FIND	"\n   find library=%s; required by %s\n"
@ MSG_LDD_FIL_PATH	"\t%s%s\n"
@ MSG_LDD_FIL_EQUIV	"\t%s =>\t %s%s\n"
@ MSG_LDD_FIL_NFOUND	"\t%s =>\t (file not found)\n"
@ MSG_LDD_FIL_CACHE	"  (cache)"

@ MSG_LDD_CPY_SIZDIF	"\tcopy relocation sizes differ: %s\n\
			 \t\t(file %s size=%x; file %s size=%x);\n"
@ MSG_LDD_CPY_INSDATA	"\t\t%s size used; possible insufficient data copied\n"
@ MSG_LDD_CPY_DATRUNC	"\t\t%s size used; possible data truncation\n"


# Error diagnostic standard prefixes.

@ MSG_ERR_WARNING	"warning: "
@ MSG_ERR_FATAL		"fatal: "
@ MSG_ERR_ELF		"elf error: "

@ MSG_STR_UNKNOWN	"(unknown)"

@ _END_


# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.

@ MSG_STR_EMPTY		""
@ MSG_STR_NEGATE	"-"
@ MSG_STR_ZERO		"0"
@ MSG_STR_HEX		"0x"
@ MSG_STR_ELF		"ELF"
@ MSG_STR_AOUT		"AOUT"
@ MSG_STR_MSGDEL	": "
@ MSG_STR_HEXNUM	"0123456789abcdef"
@ MSG_STR_CFLAG		"\t(C)"
@ MSG_STR_NL		"\n"
@ MSG_STR_SLASH		"/"

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"
@ MSG_SUNW_OST_OSLIB	"SUNW_OST_OSLIB"

@ MSG_TKN_ISA		"ISA"
@ MSG_TKN_PLATFORM	"PLATFORM"
@ MSG_TKN_ORIGIN	"ORIGIN"
@ MSG_TKN_BINDINGS	"bindings"
@ MSG_TKN_POSIX		"POSIX"

@ MSG_FMT_PATH		"%s/%s"
@ MSG_FMT_CWD		"."
@ MSG_FMT_4XPATH	"./%s"
@ MSG_FMT_4XLIB		"lib%s.so.%d.%d"
@ MSG_FMT_PROFILE	"%s/%s%s"
@ MSG_FMT_PROC		"/proc/%d"
@ MSG_FMT_MSGFILE	"/usr/lib/locale/%s/LC_MESSAGES/%s.mo"

@ MSG_SUF_PROFILE	".profile"

@ MSG_FIL_DOTSODOT	".so."
@ MSG_FIL_LIB		"lib"
@ MSG_FIL_LIBLD		"libld.so"
@ MSG_FIL_LIBDBG	"liblddbg.so"
@ MSG_FIL_LIBRTLD	"librtld.so"
@ MSG_FIL_RTLD		"ld.so.1"
@ MSG_FIL_CACHE		"ld.cache"
@ MSG_FIL_DEBUG		"ld.debug"

@ MSG_SYM_ELFERRMSG	"elf_errmsg"
@ MSG_SYM_ELFERRNO	"elf_errno"
@ MSG_SYM_ENVIRON	"_environ"
@ MSG_SYM_CTYPE		"__ctype"
@ MSG_SYM_IOB		"__iob"
@ MSG_SYM_PLT		"_PROCEDURE_LINKAGE_TABLE_"
@ MSG_SYM_GOT		"_GLOBAL_OFFSET_TABLE_"

@ MSG_PTH_LIBSYS	"/usr/lib/libsys.so.1"
@ MSG_PTH_LIBC		"/usr/lib/libc.so.1"
@ MSG_PTH_RTLD		"/usr/lib/ld.so.1"
@ MSG_PTH_USRLIB	"/usr/lib"
@ MSG_PTH_USRLCLIB	"/usr/local/lib"
@ MSG_PTH_USR4LIB	"/usr/4lib"
@ MSG_PTH_USRUCBLIB	"/usr/ucblib"
@ MSG_PTH_VARTMP	"/var/tmp"
@ MSG_PTH_DEVNULL	"/dev/null"
@ MSG_PTH_DEVZERO	"/dev/zero"
@ MSG_PTH_DUMP		"/var/ld/default"
@ MSG_PTH_CACHE		"/var/ld/default/ld.cache"
@ MSG_PTH_DEBUG		"/var/ld/default/ld.debug"

@ MSG_LD_LIBPATH	"LIBRARY_PATH="
@ MSG_LD_LOADFLTR	"LOADFLTR="
@ MSG_LD_PRELOAD	"PRELOAD="
@ MSG_LD_TRACEOBJS	"TRACE_LOADED_OBJECTS"
@ MSG_LD_TRACE_E	"_E="
@ MSG_LD_TRACE_A	"_A="
@ MSG_LD_TRACEPTHS	"TRACE_SEARCH_PATHS="
@ MSG_LD_VERBOSE	"VERBOSE="
@ MSG_LD_WARN		"WARN="
@ MSG_LD_BINDINGS	"BINDINGS="
@ MSG_LD_BIND_NOW	"BIND_NOW="
@ MSG_LD_BIND_NOT	"BIND_NOT="
@ MSG_LD_NOAUXFLTR	"NOAUXFLTR="
@ MSG_LD_NOVERSION	"NOVERSION="
@ MSG_LD_NOCACHE	"NOCACHE="
@ MSG_LD_CACHEDIR	"CACHEDIR="
@ MSG_LD_TIMING		"TIMING="
@ MSG_LD_PROFILE	"PROFILE"
@ MSG_LD_DEBUG		"DEBUG"
@ MSG_LD_OUTPUT		"_OUTPUT="

@ MSG_LC_ALL		"ALL="
@ MSG_LC_MESSAGES	"MESSAGES="

@ MSG_EMG_NOSPACE	"ld.so.1: internal: malloc failed"
@ MSG_EMG_INVRELOC	"ld.so.1: internal: invalid relocation type \
			 (non relative/jmp relocation)\n"

@ MSG_DBG_FMT_DIAG	"%5.5d: "
@ MSG_DBG_FMT_FILE	"%s.%5.5d"
@ MSG_DBG_FMT_THREAD	"%5.5d: %d: "
