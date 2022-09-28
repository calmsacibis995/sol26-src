/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_MODULE_TAGS_H
#define	_MODULE_TAGS_H

#pragma ident	"@(#)module_tags.h	1.2	96/06/06 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The PowerPC ABI defines 4 types of 8-byte tags which specify use of the
 * non-volatile registers.
 *
 * Note: + base_offset, frame_start, and range are word offsets and
 *	   and must be shifted before use in address calculations.
 *
 *	 + base_offset is a signed number and must be cast (and
 *         sign-extended for type 1) after shifting.
 */

struct __ppc_tag_notype {
	unsigned int type :2;
	unsigned int word_1 :30;
	unsigned int word_2;
};

struct __ppc_tag_type0 {
	unsigned int type :2;
	unsigned int base_offset :30;
	unsigned int lr_inreg :1;
	unsigned int c_reg :1;
	unsigned int range :14;
	unsigned int gr :5;
	unsigned int fr :5;
	unsigned int frame_start :6;
};

struct __ppc_tag_type1 {
	unsigned int type :2;
	unsigned int base_offset :24;
	unsigned int frame_start :6;
	unsigned int lr_inreg :1;
	unsigned int c_reg :1;
	unsigned int range :10;
	unsigned int gr :5;
	unsigned int gv :5;
	unsigned int fr :5;
	unsigned int fv :5;
};

struct __ppc_tag_type2 {
	unsigned int type :2;
	unsigned int start_offset: 12;
	unsigned int float_regs: 18;
	unsigned int :1;
	unsigned int c_reg :1;
	unsigned int range :12;
	unsigned int gen_regs: 18;
};

struct __ppc_tag_type3 {
	unsigned int type :2;
	unsigned int base_offset :30;
	unsigned int :2;
	unsigned int range :10;
	unsigned int :16;
	unsigned int lr_savereg :4;
};

union __ppc_tags {
	struct __ppc_tag_notype generic;
	struct __ppc_tag_type0 frame;
	struct __ppc_tag_type1 frame_valid;
	struct __ppc_tag_type2 register_valid;
	struct __ppc_tag_type3 special;
};

/*
 * Each module (a.out or .so) containing tags will contain an instance
 * of this structure.
 *
 * Note the different interpretation of the last* values with lasttag
 * pointing PAST the last tag and lastpc pointing AT the last instruction
 * covered.
 *
 * Note: the struct name here have been altered from the example shown
 * in the ABI to obey POSIX naming conventions.
 */

struct __module_tags {
	struct __module_tags *next;	/* next entry in list */
	struct __module_tags *prev;	/* previous entry in list */
	caddr_t firstpc;		/* first PC to which applicable */
	caddr_t firsttag;		/* beginning of tags */
	caddr_t lastpc;			/* last PC to which applicable */
	caddr_t lasttag;		/* first address beyond end of tags */
};

/*
 * The PowerPC ABI requires the following functions to be provided by
 * libc.  The uses are:
 *
 *	__add_module_tags - called from the .init section of a module
 *		to link the structure instance into a linked list
 *
 *	__delete_module_tags - called from the .fini section of a module
 *		to unlink the structure instance.
 *
 *	__tag_lookup_pc - given a pc, tries to find in the list the
 *		structure instance of the module containing that location.
 */

#ifdef __STDC__
extern void __add_module_tags(struct __module_tags *mt);
extern void __delete_module_tags(struct __module_tags *mt);
extern struct __module_tags *__tag_lookup_pc(caddr_t pc);
#else
extern void __add_module_tags();
extern void __delete_module_tags();
extern struct __module_tags *__tag_lookup_pc();
#endif

#ifdef __cplusplus
}
#endif

#endif /* _MODULE_TAGS_H */
