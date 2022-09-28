#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)libelf.msg	1.4	96/03/11 SMI"


@ _START_

# Message file for cmd/sgs/libelf.

@ MSG_ID_LIBELF


# Null Error

@ EINF_NULLERROR	"Elf error 0"


# BUG	bug	Internal error

@ EBUG_COOKSTAT		"Internal error: cook status"
@ EBUG_COOKTYPE		"Internal error: cook data type"
@ EBUG_SCNLIST		"Internal error: broken section list"
@ EBUG_THRDKEY		"Internal error: thr_keycreate() failed. \
			 Unable to allocate error buffer"
@ EBUG_THRDSET		"Internal error: thr_setspecific failed. \
			 Unable to create TSD for error message."


# FMT	fmt	Format error

@ EFMT_ARBUG		"Format error: archive header collision"
@ EFMT_ARFMAG		"Format error: archive fmag"
@ EFMT_ARHDRSZ		"Format error: archive header truncated"
@ EFMT_ARMEMSZ		"Format error: archive member truncated"
@ EFMT_ARSTRNM		"Format error: archive strtab name"
@ EFMT_ARSYM		"Format error: archive symtab truncated"
@ EFMT_ARSYMSTR		"Format error: archive symtab strings"
@ EFMT_ARSYMSZ		"Format error: archive symtab size"
@ EFMT_ARUNKNM		"Format error:archive special name unknown"
@ EFMT_DATA		"Format error:data region truncated"
@ EFMT_EHDRSZ		"Format error:ehdr size"
@ EFMT_ENTSZ		"Format error:shdr entry size"
@ EFMT_PHDRSZ		"Format error:phdr entry size"
@ EFMT_PHTAB		"Format error:phdr table truncated"
@ EFMT_SCNSZ		"Format error:section sh_size too small for data"
@ EFMT_SHDRSZ		"Format error:shdr entry size"
@ EFMT_SHTAB		"Format error:shdr table truncated"
@ EFMT_VER2		"Format error:ehdr EI_VERSION/e_version mismatch"
@ EFMT_VM		"Format error:reference outside file"


# IO	io	I/O error

@ EIO_FBIG		"I/O error: file too big for memory"
@ EIO_FSZ		"I/O error: get file size"
@ EIO_READ		"I/O error: raw read"
@ EIO_SEEK		"I/O error: seek"
@ EIO_SYNC		"I/O error: output msync"
@ EIO_VM		"I/O error: region read"
@ EIO_WRITE		"I/O error: output write"


# MEM	mem	Memory error

@ EMEM_ARMEM		"Memory allocation error: archive member header"
@ EMEM_ARSTR		"Memory allocation error: archive long file name \
			 strings"
@ EMEM_ARSYM		"Memory allocation error: archive symtab"
@ EMEM_DATA		"Memory allocation error: section data"
@ EMEM_DNODE		"Memory allocation error: data node"
@ EMEM_EHDR		"Memory allocation error: ehdr"
@ EMEM_ELF		"Memory allocation error: elf descriptor"
@ EMEM_OUT		"Memory allocation error: output file space"
@ EMEM_PHDR		"Memory allocation error: phdr table"
@ EMEM_SCN		"Memory allocation error: scn descriptors"
@ EMEM_SHDR		"Memory allocation error: shdr table"
@ EMEM_SNODE		"Memory allocation error: section node"
@ EMEM_VM		"Memory allocation error: vm stats"
@ EMEM_ERRMSG		"Memory allocation error: durring errmsg generation"


# REQ	req	Request error

@ EREQ_AR		"Request error: not archive"
@ EREQ_ARMEMFD		"Request error: archive/member fildes mismatch"
@ EREQ_ARRDWR		"Request error: archive/member begin() for writing"
@ EREQ_BEGIN		"Request error: begin() ELF_C_..."
@ EREQ_CLASS		"Request error: class file/memory mismatch"
@ EREQ_CNTLCMD		"Request error: cntl() ELF_C_..."
@ EREQ_CNTLWRT		"Request error: cntl() ELF_C_FD... on output file"
@ EREQ_DATA		"Request error: data/scn mismatch"
@ EREQ_DSZ		"Request error: destination too small"
@ EREQ_ENCODE		"Request error: unknown data encoding"
@ EREQ_FLAG		"Request error: flag() ELF_C_..."
@ EREQ_NDX		"Request error: no section at index"
@ EREQ_NOFD		"Request error: file descriptor disabled"
@ EREQ_RAND		"Request error: offset out of range"
@ EREQ_RDWR		"Request error: begin() ELF_C_... mismatch with parent"
@ EREQ_SCNNULL		"Request error: can't manipulate null section"
@ EREQ_STROFF		"Request error: string offset out of range"
@ EREQ_STRSCN		"Request error: no string table"
@ EREQ_TYPE		"Request error: unknown ELF_T_..."
@ EREQ_UPDATE		"Request error: update() ELF_C_..."
@ EREQ_UPDWRT		"Request error: update() for write on read-only file"
@ EREQ_NOSYNC		"Request error: update() ELF_C_SYNC - sync not needed"
@ EREQ_VER		"Request error: unknown version"
@ EREQ_NOWRIMAGE	"Request error: update(WRIMAGE) not performed on parent"

#  SEQ	seq	Sequence error

@ ESEQ_EHDR		"Sequence error: ehdr not created"
@ ESEQ_VER		"Sequence error: version not set"


@ _END_


# The following strings represent reserved names.  Reference to these strings
# is via the MSG_ORIG() macro, and thus translations are required.

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"


# System error format

@ MSG_FMT_ERR		"%s: %s"


# More C++ gibberish

@ MSG_STR_LT		"lt"
@ MSG_STR_LS		"ls"
@ MSG_STR_DV		"dv"
@ MSG_STR_GT		"gt"
@ MSG_STR_RS		"rs"
@ MSG_STR_MD		"md"
@ MSG_STR_LE		"le"
@ MSG_STR_ML		"ml"
@ MSG_STR_PL		"pl"
@ MSG_STR_GE		"ge"
@ MSG_STR_AD		"ad"
@ MSG_STR_MI		"mi"
@ MSG_STR_NE		"ne"
@ MSG_STR_OR		"or"
@ MSG_STR_ER		"er"
@ MSG_STR_AA		"aa"
@ MSG_STR_OO		"oo"
@ MSG_STR_AS		"as"
@ MSG_STR_APL		"apl"
@ MSG_STR_AMI		"ami"
@ MSG_STR_AMU		"amu"
@ MSG_STR_ADV		"adv"
@ MSG_STR_AMD		"amd"
@ MSG_STR_AAD		"aad"
@ MSG_STR_AOR		"aor"
@ MSG_STR_AER		"aer"
@ MSG_STR_ALS		"als"
@ MSG_STR_ARS		"ars"
@ MSG_STR_PP		"pp"
@ MSG_STR_MM		"mm"
@ MSG_STR_VC		"vc"
@ MSG_STR_CL		"cl"
@ MSG_STR_RF		"rf"
@ MSG_STR_EQ		"eq"
@ MSG_STR_CO		"co"
@ MSG_STR_NT		"nt"
@ MSG_STR_NW		"nw"
@ MSG_STR_DL		"dl"

@ MSG_STR_OPNANGBRAK	"<"
@ MSG_STR_DBLOPNANGBRAK	"<<"
@ MSG_STR_SLASH		"/"
@ MSG_STR_CLOANGBRAK	">"
@ MSG_STR_DBLCLOANGBRAK	">>"
@ MSG_STR_PERCENT	"%"
@ MSG_STR_OPNANGBRAKEQU	"<="
@ MSG_STR_STAR		"*"
@ MSG_STR_PLUS		"+"
@ MSG_STR_CLOANGBRAKEQU	">="
@ MSG_STR_AMP		"&"
@ MSG_STR_MINUS		"-"
@ MSG_STR_BANGEQU	"!="
@ MSG_STR_PIPE		"|"
@ MSG_STR_CARAT		"^"
@ MSG_STR_DBLAMP	"&&"
@ MSG_STR_DBLPIPE	"||"
@ MSG_STR_EQU		"="
@ MSG_STR_PLUSEQU	"+="
@ MSG_STR_MINUSEQU	"-="
@ MSG_STR_STARTEQU	"*="
@ MSG_STR_SLASHEQU	"/="
@ MSG_STR_PERCENTEQU	"%="
@ MSG_STR_AMPEQU	"&="
@ MSG_STR_PIPEEQU	"|="
@ MSG_STR_CARATEQU	"^="
@ MSG_STR_DBLOPENABKEQU	"<<="
@ MSG_STR_DBLCLOSABKEQU	">>="
@ MSG_STR_DBLPLUS	"++"
@ MSG_STR_DBLMINUS	"--"
@ MSG_STR_OPENCLOBRAK	"[]"
@ MSG_STR_OPENCLOSEPAR	"()"
@ MSG_STR_MINUSCLOANGBK	"->"
@ MSG_STR_DBLEQU	"=="
@ MSG_STR_TILDE		"~"
@ MSG_STR_BANG		"!"

@ MSG_STR_EMPTY		""
@ MSG_STR_COMMA		","
@ MSG_STR_DBLCOL	"::"
@ MSG_STR_DBLCOLSTAR	"::*"
@ MSG_STR_SPACE		" "
@ MSG_STR_ELIPSE	"..."
@ MSG_STR_OPENPAR	"("
@ MSG_STR_CLOSEPAR	")"
@ MSG_STR_OPENBRAK	"["
@ MSG_STR_CLOSEBRAK	"]"
@ MSG_STR_DBLUNDBAR	"__"
@ MSG_STR_TDBLUNDBAR	"t__"

@ MSG_STR_CONST_1	"const "
@ MSG_STR_CONST_2	" const"
@ MSG_STR_VOLATILE_1	"volatile "
@ MSG_STR_VOLATILE_2	" volatile"
@ MSG_STR_UNSIGNED	"unsigned "
@ MSG_STR_SIGNED	"signed "
@ MSG_STR_INT		"int"
@ MSG_STR_CHAR		"char"
@ MSG_STR_SHORT		"short"
@ MSG_STR_LONG		"long"
@ MSG_STR_FLOAT		"float"
@ MSG_STR_DOUBLE	"double"
@ MSG_STR_LONGDOUBLE	"long double"
@ MSG_STR_VOID		"void"
@ MSG_STR_OP		"op"
@ MSG_STR_OPERATOR_1	"operator"
@ MSG_STR_OPERATOR_2	"operator "
@ MSG_STR_NEW		" new"
@ MSG_STR_DELETE	" delete"
