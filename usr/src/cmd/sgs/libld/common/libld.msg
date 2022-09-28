#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)libld.msg	1.11	96/10/23 SMI"


@ _START_

# Message file for cmd/sgs/libld.

@ MSG_ID_LIBLD


# Relocation processing messages (some of these are required to satisfy
# do_reloc(), which is common code used by cmd/sgs/rtld - make sure both
# message files remain consistant).


@ MSG_REL_UNIMPL	"relocation error: file %s: symbol %s: \
			 unimplemented relocation type: %d"

@ MSG_REL_NULL		"relocation error: file %s: section %s: \
			 skipping null relocation record"

@ MSG_REL_UNKNWSYM	"relocation error: %s: file %s: section %s: \
			 attempt to relocate with respect to unknown \
			 symbol %s: offset 0x%x, symbol index %d"

@ MSG_REL_EMPTYSEC	"relocation error: %s: file %s: symbol %s: \
			 attempted against empty section %s"
@ MSG_REL_EXTERNSYM	"relocation error: %s: file %s: symbol %s:\n\
			 \texternal symbolic relocation against \
			 non-allocatable section %s;\n\
			 \tcannot be processed at runtime: relocation ignored"

@ MSG_REL_NONALIGN	"relocation error: %s: file %s: symbol %s: \
			 offset 0x%x is non-aligned"
@ MSG_REL_UNSUPSZ	"relocation error: %s: file %s: symbol %s: \
			 offset size (%d bytes) is not supported"
@ MSG_REL_UNRELREL	"relocation error: %s: file %s: symbol %s: \
			 non-relative PLT relocation can't be used in \
			 building a shared object"
@ MSG_REL_UNNOBITS	"relocation error: %s: file %s: symbol %s: \
			 unsupported number of bits: %d"
@ MSG_REL_LOOSEBITS	"relocation error: %s: file %s: symbol %s: \
			 value 0x%x looses %d bits at offset 0x%x"
@ MSG_REL_NOFIT		"relocation error: %s: file %s: symbol %s: \
			 value 0x%x does not fit"

@ MSG_REL_UNEXPREL	"relocation error: %s: file %s: symbol %s: \
			 unexpected relocation; generic processing performed"
@ MSG_REL_UNEXPSYM	"relocation error: %s: file %s: symbol %s: \
			 unexpected symbol referenced from file %s"

@ MSG_REL_REMAIN_1	"Text relocation remains\t\t\t      referenced\n\
			"    against symbol\t\t    offset\tin file"
@ MSG_REL_REMAIN_2	"%-35s 0x%-8x\t%s"
@ MSG_REL_REMAIN_3	"relocations remain against allocatable but \
			 non-writable sections"

@ MSG_REL_ASSIGNGOT	"internal error: assign_got() invalid got index \
			 0x%x for symbol `%s'"
@ MSG_REL_SMALLGOT	"too many symbols require `small' PIC references:\n\
			 \thave %d, maximum %d -- recompile some \
			 modules -K PIC."
@ MSG_REL_GOTOVER	"small GOT overflows field for local symbol `%s' \
			 with value %d"


# Files processing messages

@ MSG_FIL_MULINC_1	"file %s: attempted multiple inclusion of file"
@ MSG_FIL_MULINC_2	"file %s: linked to %s: attempted multiple inclusion \
			 of file"
@ MSG_FIL_SOINSTAT	"input of shared object `%s' in static mode"
@ MSG_FIL_INVALSEC	"file %s: section %s has invalid type %s"
@ MSG_FIL_NOTFOUND	"file %s: required by %s, not found"
@ MSG_FIL_MALSTR	"file %s: section %s: malformed string table, \
			 initial or final byte"
@ MSG_FIL_PTHTOLONG	"`%s/%s' pathname too long"
@ MSG_FIL_RECMATCH_1	"file %s: recording name `%s' matches that supplied \
			 with -h option"
@ MSG_FIL_RECMATCH_2	"file %s: recording name `%s' matches that provided \
			 by file %s"
@ MSG_FIL_EXCLUDE	"file %s: section %s has both SHF_EXCLUDE and SHF_ALLOC on \
			(SHF_EXCLUDE flag ignored)"
@ MSG_FIL_INVRELOC	"file %s: section %s has invalid sh_info %d"


# System call messages

@ MSG_SYS_OPEN		"file %s: cannot open file: %s"
@ MSG_SYS_MMAP		"file %s: cannot mmap file: %s"


# ELF processing messages

@ MSG_ELF_LIBELF	"libelf: version not supported: %d"

@ MSG_ELF_INVALBYTE	"file %s: wrong machine byte order"
@ MSG_ELF_INVALCLASS	"file %s: wrong machine class"
@ MSG_ELF_INVALTYPE	"file %s: wrong machine type"
@ MSG_ELF_VERSHIGH	"file %s: creators elf(3E) library version is higher \
			 than the version known to ld"
@ MSG_ELF_UNKNOWN	"file %s: unknown type, unable to process \
			 using elf(3E) libraries"
@ MSG_ELF_ARMEM		"file %s: unable to locate archive member;\n\t\
			 offset=%x, symbol[%d]=%s"

@ MSG_ELF_ARSYM		"file %s ignored: unable to locate archive symbol table"

@ MSG_ELF_BEGIN		"file %s: elf_begin"
@ MSG_ELF_GETARHDR	"file %s: elf_getarhdr"
@ MSG_ELF_GETDATA	"file %s: elf_getdata"
@ MSG_ELF_GETEHDR	"file %s: elf_getehdr"
@ MSG_ELF_GETPHDR	"file %s: elf_getphdr"
@ MSG_ELF_GETSCN	"file %s: elf_getscn"
@ MSG_ELF_GETSHDR	"file %s: elf_getshdr"
@ MSG_ELF_MEMORY	"file %s: elf_memory"
@ MSG_ELF_NEWDATA	"file %s: elf_newdata"
@ MSG_ELF_NEWEHDR	"file %s: elf_newehdr"
@ MSG_ELF_NEWSCN	"file %s: elf_newscn"
@ MSG_ELF_NEWPHDR	"file %s: elf_newphdr"
@ MSG_ELF_STRPTR	"file %s: elf_strptr"
@ MSG_ELF_UPDATE	"file %s: elf_update"
@ MSG_ELF_FSIZE		"file %s: elf_fsize"


# Section processing errors

@ MSG_SCN_NONALLOC	"%s: non-allocatable section `%s' directed to a \
			 loadable segment"


# Symbol processing errors

@ MSG_SYM_NOSECDEF	"symbol `%s' in file %s has no section definition"
@ MSG_SYM_MULDEF	"symbol `%s' is multiply defined:"
@ MSG_SYM_DIFFTYPE	"symbol `%s' has differing types:"
@ MSG_SYM_DIFFATTR	"symbol `%s' has differing %s:\n\
			 \t(file %s value=0x%x; file %s value=0x%x);"
@ MSG_SYM_FILES		"\t(file %s and file %s);"
@ MSG_SYM_FILETYPES	"\t(file %s type=%s; file %s type=%s);"
@ MSG_SYM_DEFTAKEN	"\t%s definition taken"
@ MSG_SYM_DEFUPDATE	"\t%s definition taken and updated with larger size"
@ MSG_SYM_LARGER	"\tlargest value applied"
@ MSG_SYM_TENTERR	"\ttentative symbol cannot override defined symbol \
			 of smaller size"

@ MSG_SYM_INVSHNDX	"local symbol `%s' has invalid section index; \
			 ignored:\n\t(file %s value=%s);"
@ MSG_SYM_NONGLOB	"global symbol `%s' has non-global binding:\n\
			 \t(file %s value=%s);"
@ MSG_SYM_RESERVE	"reserved symbol `%s' already defined in file %s"
@ MSG_SYM_NOTNULL	"undefined symbol `%s' with non-zero value encountered \
			 from file %s"

@ MSG_SYM_UNDEF		"Undefined\t\t\tfirst referenced\
			 \n symbol  \t\t\t    in file"
@ MSG_SYM_UND_UNDEF	"%-35s %s"
@ MSG_SYM_UND_NOVER	"%-35s %s  (symbol has no version assigned)"
@ MSG_SYM_UND_IMPL	"%-35s %s  (symbol belongs to implicit dependency %s)"
@ MSG_SYM_UND_NOTA	"%-35s %s  (symbol belongs to unavailable version %s \
			 (%s))"

@ MSG_SYM_ENTRY		"entry point symbol `%s' is undefined"


# Output file update messages

@ MSG_UPD_NOREADSEG	"No read-only segments found.  Setting `_etext' to 0"
@ MSG_UPD_NORDWRSEG	"No read-write segments found.  Setting `_edata' to 0"
@ MSG_UPD_NOSEG		"Setting `end' and `_end' to 0"

@ MSG_UPD_SEGOVERLAP	"%s: segment address overlap;\n\
			 \tprevious segment ending at address 0x%x overlaps\n\
			 \tuser defined segment `%s' starting at address 0x%x"
@ MSG_UPD_LARGSIZE	"%s: segment %s calculated size 0x%x\n\
			 \tis larger than user-defined size 0x%x"

@ MSG_UPD_LINKRANGE	"%s: %s: sh_link out of range %d, not translated"
@ MSG_UPD_NOBITS	"NOBITS section found before end of initialized data"


# Version processing messages

@ MSG_VER_HIGHER	"file %s: version revision %d is higher than \
			 expected %d"
@ MSG_VER_NOEXIST	"file %s: version `%s' does not exist:\n\
			 \trequired by file %s"
@ MSG_VER_UNDEF		"version `%s' undefined, referenced by version `%s':\n\
			 \trequired by file %s"
@ MSG_VER_UNAVAIL	"file %s: version `%s' is unavailable:\n\
			 \trequired by file %s"
@ MSG_VER_DEFINED	"version symbol `%s' already defined in file %s"
@ MSG_VER_INVALNDX	"version symbol `%s' from file %s has an invalid \
			 version index (%d)"
@ MSG_VER_ADDVERS	"unused $ADDVERS specification from file `%s' \
			 for object `%s'\nversion(s):"
@ MSG_VER_ADDVER	"\t%s"


@ MSG_MAP_SECORDER	"section ordering set but no matching section found;\
			 \n\tsegment: %s section: %s"

# Basic strings

@ MSG_STR_ALIGNMENTS	"alignments"
@ MSG_STR_COMMAND	"(command line)"
@ MSG_STR_DYNAMIC	"(.dynamic)"
@ MSG_STR_SIZES		"sizes"
@ MSG_STR_UNKNOWN       "<unknown>"


@ _END_

# The following strings represent reserved names.  Reference to these strings
# is via the MSG_ORIG() macro, and thus translations are not required.

@ MSG_STR_EMPTY		""
@ MSG_STR_COLON		":"
@ MSG_STR_DOT		"."
@ MSG_STR_SLASH		"/"

@ MSG_FMT_ARMEM		"%s(%s)"
@ MSG_FMT_COLPATH	"%s:%s"

@ MSG_PTH_RTLD		"/usr/lib/ld.so.1"
@ MSG_PTH_DEVZERO	"/dev/zero"

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"

@ MSG_LD_NOVERSION	"LD_NOVERSION"


# Section strings

@ MSG_SCN_BSS		".bss"
@ MSG_SCN_COMMENT	".comment"
@ MSG_SCN_DEBUG		".debug"
@ MSG_SCN_DYNAMIC	".dynamic"
@ MSG_SCN_DYNSTR	".dynstr"
@ MSG_SCN_DYNSYM	".dynsym"
@ MSG_SCN_EXCL		".excl"
@ MSG_SCN_GOT		".got"
@ MSG_SCN_HASH		".hash"
@ MSG_SCN_INDEX		".index"
@ MSG_SCN_INTERP	".interp"
@ MSG_SCN_LINE		".line"
@ MSG_SCN_PLT		".plt"
@ MSG_SCN_SHSTRTAB	".shstrtab"
@ MSG_SCN_STAB		".stab"
@ MSG_SCN_STABEXCL	".stab.exclstr"
@ MSG_SCN_STRTAB	".strtab"
@ MSG_SCN_SYMTAB	".symtab"
@ MSG_SCN_VERSION	".SUNW_version"


# Entry criteria strings (segment names)

@ MSG_ENT_PHDR		"phdr"
@ MSG_ENT_INTERP	"interp"
@ MSG_ENT_TEXT		"text"
@ MSG_ENT_DATA		"data"
@ MSG_ENT_DYNAMIC	"dynamic"
@ MSG_ENT_NOTE		"note"


# Symbol names

@ MSG_SYM_START		"_start"
@ MSG_SYM_MAIN		"main"

@ MSG_SYM_FINI_U	"_fini"
@ MSG_SYM_INIT_U	"_init"
@ MSG_SYM_DYNAMIC	"DYNAMIC"
@ MSG_SYM_DYNAMIC_U	"_DYNAMIC"
@ MSG_SYM_EDATA		"edata"
@ MSG_SYM_EDATA_U	"_edata"
@ MSG_SYM_END		"end"
@ MSG_SYM_END_U		"_end"
@ MSG_SYM_ETEXT		"etext"
@ MSG_SYM_ETEXT_U	"_etext"
@ MSG_SYM_GOFTBL	"GLOBAL_OFFSET_TABLE_"
@ MSG_SYM_GOFTBL_U	"_GLOBAL_OFFSET_TABLE_"
@ MSG_SYM_PLKTBL	"PROCEDURE_LINKAGE_TABLE_"
@ MSG_SYM_PLKTBL_U	"_PROCEDURE_LINKAGE_TABLE_"

@ MSG_SYM_L_END		"END_"
@ MSG_SYM_L_END_U	"_END_"
@ MSG_SYM_L_START	"START_"
@ MSG_SYM_L_START_U	"_START_"


# Support functions

@ MSG_SUP_START		"ld_start"
@ MSG_SUP_ATEXIT	"ld_atexit"
@ MSG_SUP_FILE		"ld_file"
@ MSG_SUP_SECTION	"ld_section"
