#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)ld.msg	1.9	96/10/02 SMI"


@ _START_

# Message file for cmd/sgs/ld.

@ MSG_ID_LD


# Software identification

@ MSG_SGS_ID		"ld: Software Generation Utilities - Solaris/ELF (3.0)"


# System error messages

@ MSG_SYS_OPEN		"file %s: open failed: %s"
@ MSG_SYS_STAT		"file %s: stat failed: %s"
@ MSG_SYS_READ		"file %s: read failed: %s"
@ MSG_SYS_NOTREG	"file %s: is not a regular file"


# Argument processing messages

@ MSG_ARG_DYNEXEC	"building a dynamic executable"
@ MSG_ARG_INCOMP	"option %s and %s are incompatible"
@ MSG_ARG_MTONCE	"option %s appears more than once, first setting taken"
@ MSG_ARG_ILLEGAL	"option %s has illegal argument `%s'"
@ MSG_ARG_YP		"option -YP and -Y%c may not be specified concurrently"
@ MSG_ARG_STRIP		"option -r and -s both set; only debugging \
			 information stripped"
@ MSG_ARG_NOFILES	"no files on input command line"
@ MSG_ARG_NOFLTR	"option %s is only meaningful when building a filter"

@ MSG_ARG_USAGE		"usage: ld [-%s] file(s)\n"
@ MSG_ARG_DETAIL_A	"\t[-a]\t\tcreate an absolute file\n"
@ MSG_ARG_DETAIL_B	"\t[-b]\t\tdo not do special PIC relocations in a.out\n"
@ MSG_ARG_DETAIL_D	"\t[-d y|n]\toperate in dynamic|static mode\n"
@ MSG_ARG_DETAIL_E	"\t[-e sym]\tuse `sym' as entry point address\n"
@ MSG_ARG_DETAIL_F	"\t[-f name]\tspecify library for which this file is \
			 an auxiliary\n\
			 \t\t\tfilter\n"
@ MSG_ARG_DETAIL_H	"\t[-h name]\tuse `name' as internal shared object \
			 identifier\n"
@ MSG_ARG_DETAIL_I	"\t[-i]\t\tignore LD_LIBRARY_PATH setting\n"
@ MSG_ARG_DETAIL_L	"\t[-l x]\t\tsearch for libx.so or libx.a\n"
@ MSG_ARG_DETAIL_M	"\t[-m]\t\tprint memory map\n"
@ MSG_ARG_DETAIL_O	"\t[-o outfile]\tname the output file `outfile'\n"
@ MSG_ARG_DETAIL_R	"\t[-r]\t\tcreate a relocatable object\n"
@ MSG_ARG_DETAIL_S	"\t[-s]\t\tstrip any symbol and debugging information\n"
@ MSG_ARG_DETAIL_T	"\t[-t]\t\tdo not warn of multiply defined symbols of \
			 different\n\t\t\tsizes\n"
@ MSG_ARG_DETAIL_U	"\t[-u sym]\tcreate an undefined symbol `sym'\n"
@ MSG_ARG_DETAIL_ZD	"\t[-z defs|nodefs]\n\
			 \t\t\tdisallow|allow undefined symbols\n"
@ MSG_ARG_DETAIL_ZL	"\t[-z loadfltr]\tmark filter as requiring immediate \
			 loading of its\n\
			 \t\t\tfiltees at runtime\n"
@ MSG_ARG_DETAIL_ZM	"\t[-z muldefs]\tallow multiply defined symbols\n"
@ MSG_ARG_DETAIL_ZND	"\t[-z nodelete]\tmark object as non-deletable\n"
@ MSG_ARG_DETAIL_ZNV	"\t[-z noversion]\tdon't record any version sections\n"
@ MSG_ARG_DETAIL_ZB	"\t[-z now]\tmark object as requiring non-lazy \
			 binding\n"
@ MSG_ARG_DETAIL_ZT	"\t[-z text]\tdisallow output relocations against \
			 text\n"
@ MSG_ARG_DETAIL_ZW	"\t[-z weakextract]\n\
			 \t\t\tallow extraction of archive members to resolve \
			 weak\n\
			 \t\t\treferences\n"
@ MSG_ARG_DETAIL_CBD	"\t[-B dynamic|static]\n\
			 \t\t\tsearch for shared libraries|archives\n"
@ MSG_ARG_DETAIL_CBG	"\t[-B group]\trelocate object from within group\n"
@ MSG_ARG_DETAIL_CBL	"\t[-B local]\treduce unqualified global symbols to \
			 local\n"
@ MSG_ARG_DETAIL_CBR	"\t[-B reduce]\tprocess symbol reductions\n"
@ MSG_ARG_DETAIL_CBS	"\t[-B symbolic]\tbind external references to \
			 definitions when creating\n\
			 \t\t\tshared objects\n"
@ MSG_ARG_DETAIL_CD	"\t[-D options]\tprint diagnostic messages\n"
@ MSG_ARG_DETAIL_CF	"\t[-F name]\tspecify library for which this file is \
			 a filter\n"
@ MSG_ARG_DETAIL_CG	"\t[-G]\t\tcreate a shared object\n"
@ MSG_ARG_DETAIL_CI	"\t[-I interp]\tuse `interp' as path name of \
			 interpreter\n"
@ MSG_ARG_DETAIL_CL	"\t[-L path]\tsearch for libraries in directory \
			 `path'\n"
@ MSG_ARG_DETAIL_CM	"\t[-M mapfile]\tuse processing directives contained \
			 in `mapfile'\n"
@ MSG_ARG_DETAIL_CN	"\t[-N file]\tcreate a dynamic dependency on `file'\n"
@ MSG_ARG_DETAIL_CQ	"\t[-Q y|n]\tdo|do not place version information in \
			 output file\n"
@ MSG_ARG_DETAIL_CR	"\t[-R path]\tspecify a library search path to be \
			 used at run time\n"
@ MSG_ARG_DETAIL_CS	"\t[-S name]\tspecify a link-edit support library\n"
@ MSG_ARG_DETAIL_CV	"\t[-V]\t\tprint version information\n"
@ MSG_ARG_DETAIL_CY	"\t[-Y P,dirlist]\tuse `dirlist' as a default path \
			 when searching for\n\
			 \t\t\tlibraries\n"

@ MSG_ARG_FLAGS		"Flags processing errors"
@ MSG_ARG_FILES		"File processing errors.  No output written to %s"
@ MSG_ARG_SYM_WARN	"Symbol referencing errors"
@ MSG_ARG_SYM_FATAL	"Symbol referencing errors. No output written to %s"


# ELF messages

@ MSG_ELF_UPDATE	"file %s: elf_update"


# Enterance criteria messages

@ MSG_ENT_MAP_TITLE_1	"\t\tLINK EDITOR MEMORY MAP\n\n"
@ MSG_ENT_MAP_TITLE_2	"\noutput\t\tinput\t\tnew\
			 \nsection\t\tsection\t\tdisplacement\tsize\n\n"
@ MSG_ENT_MAP_TITLE_3	"\noutput\t\tinput\t\tvirtual\
			 \nsection\t\tsection\t\taddress\t\tsize\n\n"

@ MSG_ENT_MAP_ENTRY_1	"%-8.8s\t\t\t%08.2lx\t%08.2lx\n"
@ MSG_ENT_MAP_ENTRY_2	"\t\t%-8.8s\t%08.2lx\t%08.2lx %s\n"

@ MSG_ENT_MUL_TITLE	"\n\n\t\tMULTIPLY DEFINED SYMBOLS\n\n\n\
			 symbol\t\t\t\t  definition used     also defined \
			 in\n\n"

@ MSG_ENT_MUL_ENTRY_1	"%-35s %s\n"
@ MSG_ENT_MUL_ENTRY_2	"\t\t\t\t\t\t\t%s\n"

@ MSG_ENT_NOSEC_1	"mapfile: %s segment: section `%s' does not appear \
			 in file `%s'"
@ MSG_ENT_NOSEC_2	"mapfile: %s segment: section `%s' does not appear \
			 in any input file"

# Library messages

@ MSG_LIB_TOOLONG_1	"%s/lib%s: pathname too long"
@ MSG_LIB_TOOLONG_2	"lib%s: library name too long"

@ MSG_LIB_NOTFOUND	"library -l%s: not found"
@ MSG_LIB_MALFORM	"LD_LIBRARY_PATH malformed"
@ MSG_LIB_BADYP		"-YP library path malformed"


# Mapfile processing messages

@ MSG_MAP_BADFLAG	"%s: %d: badly formed section flags `%s'"
@ MSG_MAP_REDEFATT	"%s: %d: redefining %s attribute for `%s'"
@ MSG_MAP_PREMEOF	"%s: %d: premature EOF"
@ MSG_MAP_ILLCHAR	"%s: %d: illegal character `\\%03o'"
@ MSG_MAP_MALFORM	"%s: %d: malformed entry"
@ MSG_MAP_NONLOAD	"%s: %d: %s not allowed on non-LOAD segments"
@ MSG_MAP_MOREONCE	"%s: %d: %s set more than once on same line"
@ MSG_MAP_NOTERM	"%s: %d: string does not terminate with a `\"'"
@ MSG_MAP_SECINSEG	"%s: %d: section within segment ordering done on \
			 a non-existent segment `%s'"
@ MSG_MAP_INCOMFLG	"%s: %d: incompatible flags $SPECVERS and $ADDVERS \
			 used on the same object `%s'"
@ MSG_MAP_MULSONAME	"%s: %d: multiple definition of SONAME field for %s\
			 \n\tfirst `%s', second `%s'; first value used"
@ MSG_MAP_UNEXDEP	"%s: %d: dependency `%s' unexpected; ignored"
@ MSG_MAP_UNEXTOK	"%s: %d: unexpected occurrence of `%c' token"

@ MSG_MAP_SEGEMPEXE	"%s: %d: empty segment definition for this output \
			 file is unsupported"
@ MSG_MAP_SEGEMPLOAD	"%s: %d: empty segment must be of type LOAD"
@ MSG_MAP_SEGEMPATT	"%s: %d: empty segment must have an address and size"
@ MSG_MAP_SEGEMPSEC	"%s: %d: empty segment can not have sections \
			 assigned to it"

@ MSG_MAP_UNKSYMSCO	"%s: %d: unknown symbol scope definition `%s'"
@ MSG_MAP_UNKSYMDEF	"%s: %d: unknown symbol definition `%s'"
@ MSG_MAP_UNKSEGTYP	"%s: %d: unknown internal segment type %d"
@ MSG_MAP_UNKSOTYP	"%s: %d: unknown shared object type `%s'"
@ MSG_MAP_UNKSEGATT	"%s: %d: unknown segment attribute `%s'"
@ MSG_MAP_UNKSEGFLG	"%s: %d: unknown segment flag `?%c'"
@ MSG_MAP_UNKSECTYP	"%s: %d: unknown section type `%s'"

@ MSG_MAP_SEGSIZE	"%s: %d: segment `%s' already has a size symbol"
@ MSG_MAP_SEGADDR	"%s: %d: segment address or length `%s' %s"
@ MSG_MAP_SEGRESV	"%s: %d: segments `interp' or `dynamic' may not be \
			 changed"

@ MSG_MAP_SYMDEF	"%s: %d: symbol `%s' is already defined in file: %s"

@ MSG_MAP_EXPSCOL	"%s: %d: expected a `;'"
@ MSG_MAP_EXPEQU	"%s: %d: expected a `=', `:', `|', or `@'"
@ MSG_MAP_EXPSEGATT	"%s: %d: expected one or more segment attributes \
			 after an `='"
@ MSG_MAP_EXPSEGNAM	"%s: %d: expected a segment name at the beginning \
			 of a line"
@ MSG_MAP_EXPSYM_1	"%s: %d: expected a symbol name after `@'"
@ MSG_MAP_EXPSYM_2	"%s: %d: expected a symbol name after `{'"
@ MSG_MAP_EXPSEC	"%s: %d: expected a section name after `|'"
@ MSG_MAP_EXPSO		"%s: %d: expected a shared object definition after `-'"
@ MSG_MAP_EXPVERS	"%s: %d: expected a `;' or version reference after `}'"

@ MSG_MAP_SEGSAME	"segments `%s' and `%s' have the same assigned \
			 virtual address"
@ MSG_MAP_EXCLIMIT	"exceeds internal limit"
@ MSG_MAP_NOBADFRM	"number is badly formed"

@ MSG_MAP_SEGTYP	"segment type"
@ MSG_MAP_SEGVADDR	"segment virtual address"
@ MSG_MAP_SEGPHYS	"segment physical address"
@ MSG_MAP_SEGLEN	"segment length"
@ MSG_MAP_SEGFLAG	"segment flags"
@ MSG_MAP_SEGALIGN	"segment alignment"
@ MSG_MAP_SEGROUND	"segment rounding"

@ MSG_MAP_SECTYP	"section type"
@ MSG_MAP_SECFLAG	"section flags"
@ MSG_MAP_SECNAME	"section name"

@ MSG_MAP_SYMVAL	"symbol value"
@ MSG_MAP_SYMSIZE	"symbol size"


# Debugging messages

@ MSG_DBG_NOINABLE	"debugging is not enabled; recompile with -DDEBUG"
@ MSG_DBG_DISABLE	"debugging disabled; unable to process debug \
			 library:\n\t%s"

@ MSG_DBG_FMT		"debug: "

# Generic error diagnostic labels

@ MSG_ERR_WARNING	"warning: "
@ MSG_ERR_FATAL		"fatal: "
@ MSG_ERR_ELF		"elf error: "

@ MSG_STR_COMMAND	"(command line)"
@ MSG_STR_UNKNOWN	"(unknown)"
@ MSG_STR_NULL		"(null)"

@ _END_


# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.

@ MSG_STR_EMPTY		""
@ MSG_STR_NL		"\n"
@ MSG_STR_COLON		":"
@ MSG_STR_DOT		"."
@ MSG_STR_PATHTOK	";:"
@ MSG_STR_LDDIAG	"ld: "
@ MSG_STR_ELFDIAG	": %s"
@ MSG_STR_AOUT		"a.out"

@ MSG_STR_SUF_A		".a"
@ MSG_STR_SUF_SO	".so"
@ MSG_STR_PRE_LIB	"/lib"
@ MSG_STR_PATH		"%s/%s"

@ MSG_STR_INTERP	"interp"
@ MSG_STR_DYNAMIC	"dynamic"
@ MSG_STR_SYMBOLIC	"symbolic"
@ MSG_STR_PROGBITS	"progbits"
@ MSG_STR_SYMTAB	"symtab"
@ MSG_STR_DYNSYM	"dynsym"
@ MSG_STR_STRTAB	"strtab"
@ MSG_STR_HASH		"hash"
@ MSG_STR_LIB		"lib"
@ MSG_STR_NOTE		"note"
@ MSG_STR_NOBITS	"nobits"

@ MSG_STR_OPTIONS	"abd:e:f:h:il:mo:rstu:z:B:D:F:GI:L:M:N:Q:R:S:VY:"


# Argument processing strings

@ MSG_ARG_A		"-a"
@ MSG_ARG_B		"-b"
@ MSG_ARG_CB		"-B"
@ MSG_ARG_BDYNAMIC	"-Bdynamic"
@ MSG_ARG_BSYMBOLIC	"-Bsymbolic"
@ MSG_ARG_BGROUP	"-Bgroup"
@ MSG_ARG_C		"-c"
@ MSG_ARG_D		"-d"
@ MSG_ARG_E		"-e"
@ MSG_ARG_DN		"-dn"
@ MSG_ARG_DY		"-dy"
@ MSG_ARG_F		"-f"
@ MSG_ARG_CF		"-F"
@ MSG_ARG_CG		"-G"
@ MSG_ARG_H		"-h"
@ MSG_ARG_CI		"-I"
@ MSG_ARG_N		"-N"
@ MSG_ARG_O		"-o"
@ MSG_ARG_CQ		"-Q"
@ MSG_ARG_R		"-r"
@ MSG_ARG_CY		"-Y"
@ MSG_ARG_CYL		"-YL"
@ MSG_ARG_CYP		"-YP"
@ MSG_ARG_CYU		"-YU"
@ MSG_ARG_Z		"-z"
@ MSG_ARG_ZDEFNODEF	"-zdefs/nodefs"
@ MSG_ARG_ZNODEF	"-znodefs"
@ MSG_ARG_ZTEXT		"-ztext"
@ MSG_ARG_ZLOADFLTR	"-zloadfltr"

@ MSG_ARG_NOW		"now"
@ MSG_ARG_DEFS		"defs"
@ MSG_ARG_NODEFS	"nodefs"
@ MSG_ARG_NOVERSION	"noversion"
@ MSG_ARG_TEXT		"text"
@ MSG_ARG_MULDEFS	"muldefs"
@ MSG_ARG_NODELETE	"nodelete"
@ MSG_ARG_WEAKEXT	"weakextract"
@ MSG_ARG_LOADFLTR	"loadfltr""

@ MSG_ARG_GROUP		"group"
@ MSG_ARG_LOCAL		"local"
@ MSG_ARG_REDUCE	"reduce"
@ MSG_ARG_STATIC	"static"

@ MSG_ARG_LCOM		"L,"
@ MSG_ARG_PCOM		"P,"
@ MSG_ARG_UCOM		"U,"


# File and pathname strings

@ MSG_FIL_LIBSTAB	"libldstab.so.1"
@ MSG_FIL_LIBDBG	"liblddbg.so.3"


# Environment variable strings

@ MSG_LD_OPTIONS	"LD_OPTIONS"
@ MSG_LD_RUN_PATH	"LD_RUN_PATH"
@ MSG_LD_LIBPATH	"LD_LIBRARY_PATH"

@ MSG_SGS_SUPPORT	"SGS_SUPPORT"


# Symbol names

@ MSG_SYM_FINI_U	"_fini"
@ MSG_SYM_INIT_U	"_init"
@ MSG_SYM_LIBVER_U	"_lib_version"


# Mapfile tokens

@ MSG_MAP_TOK_1		"\"\n"
@ MSG_MAP_TOK_2		" \"\t\n:;=#"
@ MSG_MAP_LOAD		"load"
@ MSG_MAP_NOTE		"note"
@ MSG_MAP_USED		"used"
@ MSG_MAP_NEED		"need"
@ MSG_MAP_SPECVERS	"specvers"
@ MSG_MAP_ADDVERS	"addvers"
@ MSG_MAP_LOCAL		"local"
@ MSG_MAP_GLOBAL	"global"
@ MSG_MAP_FUNCTION	"function"
@ MSG_MAP_DATA		"data"
@ MSG_MAP_COMMON	"common"


# Localization identification

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"
