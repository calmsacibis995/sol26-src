/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * Copyright (c) 1986, 1987, 1988, Sun Microsystems, Inc.
 * All Rights Reserved.
 */
/* #ident	"@(#)compress.c	1.10	94/06/07 SMI"	SVr4.0 1.4	*/

#pragma ident	"@(#)decompress.c	1.4	96/04/26 SMI"

/*
 * Compress - data compression program
 */
#define	min(a, b)	((a > b) ? b : a)

/*
 * Decompression routines derived from compress.c.  Work was done
 * for use in ufsboot, but intent is to be useful anywhere.
 *
 * Caller needs to supply routine decomp_getchar(), decomp_putchar()
 * and decomp_getbytes().
 *
 * Call decomp_init() and then decompress().
 */

extern int decomp_getchar(void);
extern int decomp_getbytes(char *, int);
extern void decomp_putchar(char);

/*
 * Set USERMEM to the maximum amount of physical user memory available
 * in bytes.  USERMEM is used to determine the maximum BITS that can be used
 * for compression.
 *
 * SACREDMEM is the amount of physical memory saved for others; compress
 * will hog the rest.
 */
#ifndef	SACREDMEM
#define	SACREDMEM	0
#endif

#ifndef USERMEM
#define	USERMEM 	450000	/* default user memory */
#endif

/* BEGIN CSTYLED */
#ifdef USERMEM
# if USERMEM >= (433484+SACREDMEM)
#  define PBITS	16
# else
#  if USERMEM >= (229600+SACREDMEM)
#   define PBITS	15
#  else
#   if USERMEM >= (127536+SACREDMEM)
#    define PBITS	14
#   else
#    if USERMEM >= (73464+SACREDMEM)
#     define PBITS	13
#    else
#     define PBITS	12
#    endif
#   endif
#  endif
# endif
# undef USERMEM
#endif /* USERMEM */
/* END CSTYLED */


/* BEGIN CSTYLED */
#ifdef PBITS		/* Preferred BITS for this memory size */
# ifndef BITS
#  define BITS PBITS
# endif /* BITS */
#endif /* PBITS */
/* END CSTYLED */

#if BITS == 16
#define	HSIZE	69001		/* 95% occupancy */
#endif
#if BITS == 15
#define	HSIZE	35023		/* 94% occupancy */
#endif
#if BITS == 14
#define	HSIZE	18013		/* 91% occupancy */
#endif
#if BITS == 13
#define	HSIZE	9001		/* 91% occupancy */
#endif
#if BITS <= 12
#define	HSIZE	5003		/* 80% occupancy */
#endif


/*
 * a code_int must be able to hold 2**BITS values of type int, and also -1
 */
#if BITS > 15
typedef long int	code_int;
#else
typedef int		code_int;
#endif

typedef long int	  count_int;

typedef	unsigned char	char_type;

static char_type magic_header[] = { "\037\235" };	/* 1F 9D */

/* Defines for third byte of header */
#define	BIT_MASK	0x1f
#define	BLOCK_MASK	0x80
/*
 * Masks 0x40 and 0x20 are free.  I think 0x20 should mean that there is
 * a fourth header byte (for expansion).
 */
#define	INIT_BITS 9			/* initial number of bits/code */

static int n_bits;			/* number of bits/code */
static int maxbits = BITS;		/* user settable max # bits/code */
static code_int maxcode;		/* maximum code, given n_bits */
static code_int maxmaxcode = 1 << BITS;	/* should NEVER generate this code */
#define	MAXCODE(n_bits)	((1 << (n_bits)) - 1)

typedef unsigned short u_short;

static count_int *htab, *htab_i;	/* Requires HSIZE entries */
static u_short *codetab, *codetab_i;	/* Requires HSIZE entries */

#define	htabof(i)	htab[i]
#define	codetabof(i)	codetab[i]

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**BITS characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

#define	tab_prefixof(i)	codetabof(i)
#define	tab_suffixof(i)	((char_type *)(htab))[i]
#define	de_stack		((char_type *)&tab_suffixof(1<<BITS))

static code_int free_ent = 0;			/* first unused entry */
static code_int getcode();

/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */
static int block_compress = BLOCK_MASK;
static int clear_flg = 0;

/*
 * the next two codes should not be changed lightly, as they must not
 * lie within the contiguous general code space.
 */
#define	FIRST	257	/* first free entry */
#define	CLEAR	256	/* table clear output code */

static char_type rmask[9] =
	{0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

extern char *bkmem_alloc();
extern void bkmem_free();

/*
 * Allocate required table space for decompress operation and
 * initialize pointers to beginning of tables.
 *
 * Returns: 0 - success, -1 - failure.
 */

int
decompress_init(void)
{
	char *tp;
	unsigned sz;

	if (htab_i == 0) {
		sz = sizeof (count_int) * HSIZE + sizeof (u_short) * HSIZE;
		tp = bkmem_alloc(sz);
		if (tp == 0)
			return (-1);
		htab_i = (count_int *)tp;
		tp += sizeof (count_int) * HSIZE;
		codetab_i = (u_short *)tp;
	}
	htab = htab_i;
	codetab = codetab_i;
	return (0);
}

/*
 * Free decompress table space.
 */

void
decompress_fini(void)
{
	unsigned sz;

	if (htab_i) {
		sz = sizeof (count_int) * HSIZE + sizeof (u_short) * HSIZE;
		bkmem_free((char *)htab_i, sz);
	}
	htab_i = 0;
	codetab_i = 0;
}

/*
 * Decompress stdin to stdout.  This routine adapts to the codes in the
 * file building the "string" table on-the-fly; requiring no table to
 * be stored in the compressed file.  The tables used herein are shared
 * with those of the compress() routine.  See the definitions above.
 */

void
decompress()
{
	register char_type *stackp;
	register int finchar;
	register code_int code, oldcode, incode;

	/* The following introductory code was in main in compress.c */

	if ((decomp_getchar() != (magic_header[0] & 0xFF)) ||
	(decomp_getchar() != (magic_header[1] & 0xFF)))
	return;		/* Bad magic bytes */
	maxbits = decomp_getchar();	/* set -b from file */
	block_compress = maxbits & BLOCK_MASK;
	maxbits &= BIT_MASK;
	maxmaxcode = 1 << maxbits;
	if (maxbits > BITS)
	return;		/* Code size too big for this program */

	/* End of code moved from main */

	/*
	 * As above, initialize the first 256 entries in the table.
	 */
	maxcode = MAXCODE(n_bits = INIT_BITS);
	for (code = 255; code >= 0; code--) {
		tab_prefixof(code) = 0;
		tab_suffixof(code) = (char_type)code;
	}
	free_ent = ((block_compress) ? FIRST : 256);

	finchar = oldcode = getcode();
	if (oldcode == -1)	/* EOF already? */
		return;		/* Get out of here */
	decomp_putchar((char)finchar); /* first code must be 8 bits = char */
	stackp = de_stack;

	while ((code = getcode()) > -1) {

		if ((code == CLEAR) && block_compress) {
		    for (code = 255; code >= 0; code--)
			tab_prefixof(code) = 0;
		    clear_flg = 1;
		    free_ent = FIRST - 1;
		    if ((code = getcode()) == -1)	/* O, untimely death! */
			break;
		}
		incode = code;
		/*
		 * Special case for KwKwK string.
		 */
		if (code >= free_ent) {
		    *stackp++ = (char)finchar;
		    code = oldcode;
		}

		/*
		 * Generate output characters in reverse order
		 */
		while (code >= 256) {
		    *stackp++ = tab_suffixof(code);
		    code = tab_prefixof(code);
		}
		*stackp++ = finchar = tab_suffixof(code);

		/*
		 * And put them out in forward order
		 */
		do
		    decomp_putchar (*--stackp);
		while (stackp > de_stack);

		/*
		 * Generate the new entry.
		 */
		if ((code = free_ent) < maxmaxcode) {
		    tab_prefixof(code) = (unsigned short)oldcode;
		    tab_suffixof(code) = (char_type)finchar;
		    free_ent = code+1;
		}
		/*
		 * Remember previous code.
		 */
		oldcode = incode;
	}
}

/*
 * TAG( getcode )
 *
 * Read one code from the standard input.  If EOF, return -1.
 * Inputs:
 * 	stdin
 * Outputs:
 * 	code or -1 is returned.
 */

static code_int
getcode()
{
	register code_int code;
	static int offset = 0, size = 0;
	static char_type buf[BITS];
	register int r_off, bits;
	register char_type *bp = buf;

	if (clear_flg > 0 || offset >= size || free_ent > maxcode) {
		/*
		* If the next entry will be too big for the current code
		* size, then we must increase the size.  This implies reading
		* a new buffer full, too.
		*/
		if (free_ent > maxcode) {
			n_bits++;
			if (n_bits == maxbits)
				/* won't get any bigger now */
				maxcode = maxmaxcode;
			else
				maxcode = MAXCODE(n_bits);
		}
		if (clear_flg > 0) {
			maxcode = MAXCODE(n_bits = INIT_BITS);
			clear_flg = 0;
		}
		size = decomp_getbytes((char *)buf, n_bits);
		if (size <= 0)
			return (-1);	/* end of file */
		offset = 0;
		/* Round size down to integral number of codes */
		size = (size << 3) - (n_bits - 1);
	}
	r_off = offset;
	bits = n_bits;
	/*
	 * Get to the first byte.
	 */
	bp += (r_off >> 3);
	r_off &= 7;
	/* Get first part (low order bits) */
	code = (*bp++ >> r_off);
	bits -= (8 - r_off);
	r_off = 8 - r_off;	/* now, offset into code word */
	/* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
	if (bits >= 8) {
		code |= *bp++ << r_off;
		r_off += 8;
		bits -= 8;
	}
	/* high order bits. */
	code |= (*bp & rmask[bits]) << r_off;
	offset += n_bits;

	return (code);
}
