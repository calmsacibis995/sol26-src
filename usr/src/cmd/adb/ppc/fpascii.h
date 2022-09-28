/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)fpascii.h	1.3	94/03/31 SMI"

/*
 * fp_reg_address, the address of first word of register image,
 * is now kept in the adb_raddr structure (see sun.h/sparc.h etc).
 */
extern fpa_avail;

	/* We still need fprtos */
void fprtos(/* fpr, s */);		/* see expansion below */


/*
 * Description of sparc/PowerPC 128-bit extended floating point
 * (Only 80 of the bits are used right now.)
 * This should be in /usr/include/machine/reg.h
 */
typedef struct {
	unsigned s:1;			/* "Extended-e" */
	unsigned exp:15;
	unsigned unused_reserved: 16;

	unsigned j:1;			/* "Extended-f" */
	unsigned f_msb:31;

	unsigned f_lsb:32;		/* "Extended-f-low" */

	unsigned reserved_unused: 32;	/* "Extended-u" */
} ext_fp;
