/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_compress.c	1.13	96/04/16 SMI"

/*
 *
 *				LZRW1-A.C
 *
 *
 *
 * Author  : Ross Williams.
 * Date    : 25 June 1991.
 * Release : 1.
 *
 *
 * This file contains an implementation of the LZRW1-A data compression
 * algorithm in C.
 *
 * The algorithm is a general purpose compression algorithm that runs fast
 * and gives reasonable compression. The algorithm is a member of the Lempel
 * Ziv family of algorithms and bases its compression on the presence in the
 * data of repeated substrings.
 *
 * The algorithm/code is based on the LZRW1 algorithm/code. Changes are:
 *    1) The copy length range is now 3..18 instead of 3..16 as in LZRW1.
 *    2) The code for both the compressor and decompressor has been optimized
 *	 and made a little more portable.
 *
 * This algorithm and code is public domain. As the algorithm is based on the
 * LZ77 class of algorithms, it is unlikely to be the subject of a patent
 * challenge.
 *
 * WARNING: This algorithm is non-deterministic. Its compression performance
 * may vary slightly from run to run.
 *
 * For more information, see the original code and documents obtained by
 * anonymous FTP from:
 *	Machine		: sirius.itd.adelaide.edu.au	[IP=129.127.40.3]
 *	Directory	: pub/compression
 */

#include <sys/types.h>
#include <sys/systm.h>

uint_t		cpr_compress(uchar_t *, uint_t, uchar_t *);
#if !defined(lint)
uint_t		cpr_decompress(uchar_t *, uint_t, uchar_t *);
#endif  /* lint */

static uchar_t	*hash[4096];	/* Global hash table */


/*
 * The following define defines the length of the copy flag that appears at
 * the start of the compressed file. I have decided on four bytes so as to
 * make the source and destination longword aligned in the case where a copy
 * operation must be performed.
 * The actual flag data appears in the first byte. The rest are zero.
 */
#define	FLAG_BYTES	4

/*
 * The following defines define the meaning of the values of the copy
 * flag at the start of the compressed file.
 */
#define	FLAG_COMPRESS	0	/* Output was result of compression. */
#define	FLAG_COPY	1	/* Output was simply copied over. */

#define	ITEMMAX		18	/* Max number of bytes in an expanded item. */
#define	TOPWORD		(uint_t)0xFFFF0000

/*
 * Body of inner unrolled matching loop.
 */
#define	PSL	*p++ != *p_src++

/*
 * cpr_compress:
 * Input  : Specify input block using p_src_first and src_len.
 * Input  : Point p_dst_first to the start of the output zone (OZ).
 * Input  : Input block and output zone must not overlap.
 * Output : Returns length of output block written
 * Output : Output block in Mem[p_dst_first..p_dst_first+*p_dst_len-1].
 * Output : May write in OZ=Mem[p_dst_first..p_dst_first+src_len+288-1].
 * Output : Upon completion guaranteed *p_dst_len<=src_len+FLAG_BYTES.
 */
uint_t
cpr_compress(uchar_t *p_src_first, uint_t src_len, uchar_t *p_dst_first)
{
	uchar_t	*p_src = p_src_first, *p_dst = p_dst_first;
	uchar_t	*p_src_post = p_src_first + src_len,
		*p_dst_post = p_dst_first + src_len;
	uchar_t	*p_src_max1, *p_src_max16;
	uchar_t	*p_control;
	uint_t	control;

	control = TOPWORD;
	p_src_max1 = p_src_post - ITEMMAX;
	p_src_max16 = p_src_post - 16 * ITEMMAX;
	*p_dst = FLAG_COMPRESS;
	{
		uint_t i;

		for (i = 1; i < FLAG_BYTES; i++)
			p_dst[i] = 0;
	}
	p_dst += FLAG_BYTES;
	p_control = p_dst;
	p_dst += 2;
	while (/*CONSTCOND*/1) {
		register uchar_t *p, **p_entry;
		register uint_t unroll = 16;
		register uint_t offset;

		if (p_dst > p_dst_post)
			goto overrun;
		if (p_src > p_src_max16) {
			unroll = 1;
			if (p_src > p_src_max1) {
				if (p_src == p_src_post)
					break;
				goto literal;
			}
		}
begin_unrolled_loop:
		p_entry = &hash[((40543 * ((((uint_t)(p_src[0] << 4) ^
		    (uint_t)p_src[1]) << 4) ^ (uint_t)p_src[2])) >> 4) & 0xFFF];
		p = *p_entry;
		*p_entry = p_src;
		offset = p_src - p;
		if (offset > 4095 || p < p_src_first || offset == 0 ||
		    PSL || PSL || PSL) {
			p_src = *p_entry;
literal:
			*p_dst++ = *p_src++;
			control &= 0xFFFEFFFF;
		} else {
			PSL || PSL || PSL || PSL || PSL || PSL || PSL || PSL ||
			PSL || PSL || PSL || PSL || PSL || PSL || PSL ||
			p_src++;
			*p_dst++ = ((offset & 0xF00) >>4) |
			    (--p_src - *p_entry - 3);
			*p_dst++ = offset & 0xFF;
		}
		control >>= 1;
end_unrolled_loop:
		if (--unroll)
			goto begin_unrolled_loop;
		if ((control & TOPWORD) == 0) {
			*p_control = control & 0xFF;
			*(p_control + 1) = (control >> 8) & 0xFF;
			p_control = p_dst;
			p_dst += 2;
			control = TOPWORD;
		}
	}
	while (control & TOPWORD)
		control >>= 1;
	*p_control++ = control & 0xFF;
	*p_control++ = control >> 8;
	if (p_control == p_dst)
		p_dst -= 2;
	return (p_dst - p_dst_first);

overrun:
	bcopy((caddr_t)p_src_first, (caddr_t)p_dst_first + FLAG_BYTES, src_len);
	*p_dst_first = FLAG_COPY;
	return (src_len + FLAG_BYTES);
}

#if !defined(lint)
/*
 * cpr_decompress:
 * Input  : Specify input block using p_src_first and src_len.
 * Input  : Point p_dst_first to the start of the output zone.
 * Input  : Input block and output zone must not overlap. User knows
 * Input  : upperbound on output block length from earlier compression.
 * Input  : In any case, maximum expansion possible is nine times.
 * Output : Return length of output block written
 * Output : Output block in Mem[p_dst_first..p_dst_first+*p_dst_len-1].
 * Output : Writes only  in Mem[p_dst_first..p_dst_first+*p_dst_len-1].
 *
 */
uint_t
cpr_decompress(uchar_t *p_src_first, uint_t src_len, uchar_t *p_dst_first)
{
	uchar_t	*p_src = p_src_first + FLAG_BYTES, *p_dst = p_dst_first;
	uchar_t	*p_src_post = p_src_first + src_len;
	uchar_t	*p_src_max16 = p_src_first + src_len - (16 * 2);
	uint_t	control = 1;

	if (*p_src_first == FLAG_COPY) {
		bcopy((caddr_t)p_src_first + FLAG_BYTES, (caddr_t)p_dst_first,
		    src_len - FLAG_BYTES);
		return (src_len - FLAG_BYTES);
	}
	while (p_src != p_src_post) {
		uint_t	unroll;

		if (control == 1) {
			control = 0x10000 | *p_src++;
			control |= (*p_src++) << 8;
		}
		unroll = p_src <= p_src_max16 ? 16 : 1;
		while (unroll--) {
			if (control & 1) {
				uint_t	lenmt;
				uchar_t	*p;

				lenmt = *p_src++;
				p = p_dst - (((lenmt & 0xF0) << 4) | *p_src++);
				*p_dst++ = *p++;
				*p_dst++ = *p++;
				*p_dst++ = *p++;
				lenmt &= 0xF;
				while (lenmt--)
					*p_dst++ = *p++;
			} else
				*p_dst++ = *p_src++;
			control >>= 1;
		}
	}
	return (p_dst - p_dst_first);
}
#endif  /* lint */

/*
 * This function is included here because it needs to be used by both
 * cprboot and the kernel cpr module, and this file is already linked in
 * to both.  It is also related to compression as it is used to verify
 * the compression algorithm (as well as the file system).
 */

u_int
cpr_sum(u_char *cp, int length)
{
	u_char *ep;
	u_int sum;

	ep = cp + length - 1;
	sum = 0;
	while (cp <= ep) {
		/*
		 * this code from sum.c, extended to 32 bits
		 */
		if (sum & 01)
			sum = (sum >> 1) + 0x80000000;
		else
			sum >>= 1;
		sum += *cp++;
	}
	return (sum);
}
