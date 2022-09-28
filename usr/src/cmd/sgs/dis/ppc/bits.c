/*
 * Copyright (c) 1990-1994, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)bits.c	1.8	94/07/20 SMI"


#include	"libelf.h"
#include	<stdio.h>
#include	<string.h>
#include	"ppcdis_inst_fmt.h"

#define		FAILURE 0
#define		MAXERRS	1	/* maximum # of errors allowed before	*/
				/* abandoning this disassembly as a	*/
				/* hopeless case			*/

static short errlev = 0;	/* to keep track of errors encountered 	*/
				/* during the disassembly, probably due	*/
				/* to being out of sync.		*/

#define		OPLEN	35	/* maximum length of a single operand	*/
				/* (will be used for printing)		*/

static	char	operand[4][OPLEN];	/* to store operands as they	*/
					/* are encountered		*/
static	char	symarr[4][OPLEN];

static	long	start;			/* start of each instruction	*/
					/* used with jumps		*/

static	int	fpflag;		/* will indicate floating point instruction */
				/* (0=NOT FLOATING POINT, 1=SINGLE, 2=DOUBLE) */
				/* so that immediates will be printed in */
				/* decimal floating point.		*/
static int	bytesleft = 0;	/* will indicate how many unread bytes */
				/* are in buffer */

#define	TWO_8	256
#define	TWO_16	65536
#define	MIN(a, b)	((a) < (b) ? (a) : (b))

/* For communication to locsympr */
char **regname;

/*
 *	dis_text ()
 *
 *	disassemble a text section
 */


void
dis_text(Elf32_Shdr *shdr)
{
	extern void	exit();
	/* the following arrays are contained in tables.c	*/
	extern	struct	instable	opcodetbl[256];

	/* the following entries are from _extn.c	*/
	extern	int	resync();
	extern	char	*sname;
	extern	char	*fname;
	extern	int	Sflag;
	extern	int	Lflag;
	extern	int	sflag;
	extern	int	trace;
	extern	long	 loc;
	extern	char	mneu[];
	extern	char	object[];
	extern	char	symrep[];
	extern	unsigned short	cur1byte;
	extern	int	debug;

	extern	void	printline();
	extern	void	looklabel();
	extern	void	line_nums();
	extern	void	prt_offset();
	extern	int	convert();

	/* the following routines are in this file	*/
	static long		getword();
	static unsigned short	gethalfword();
	static void	get_operand();
	static void	PrintLine(char *);
	static void	mau_dis();
	static void	get_bjmp_oprnd(),
			get_macro(),
			get_hjmp_oprnd(),
			longprint();
	void		get1byte();

/*	*sech; */

	struct instable	*cp;
	unsigned	key;
	long 		lngtmp;
	char		ctemp[OPLEN];	/* to store EXTOP operand   */

	extern char * disassemble();
	long iii;
	char *sss;

	/* initialization for each beginning of text disassembly	*/

	bytesleft = 0;

	/*
	 * An instruction is disassembled with each iteration of the
	 * following loop.  The loop is terminated upon completion of the
	 * section (loc minus the section's physical address becomes equal
	 * to the section size) or if the number of bad op codes encountered
	 * would indicate this disassembly is hopeless.
	 */


	for (loc = shdr->sh_addr; ((loc - shdr->sh_addr) < shdr->sh_size) &&
	    errlev < MAXERRS; PrintLine(sss)) {
		start = loc;
		(void) sprintf(operand[0], "");
		(void) sprintf(operand[1], "");
		(void) sprintf(operand[2], "");
		(void) sprintf(operand[3], "");
		(void) sprintf(symarr[0], "");
		(void) sprintf(symarr[1], "");
		(void) sprintf(symarr[2], "");
		(void) sprintf(symarr[3], "");

		/* look for C source labels */
		if (Lflag && debug)
			looklabel(loc);

		line_nums();

		prt_offset();			/* print offset		   */

		/* key is the one byte op code		*/
		/* cp is the op code Class Pointer	*/

		iii = getword();

		sss = disassemble((union op *)&iii, loc - 4);

		}  /* end of for */

	if (errlev >= MAXERRS) {
		(void) printf("%s: %s: %s: section probably not text section\n",
			"dis", fname, sname);
		(void) printf("\tdisassembly terminated\n");
		errlev = 0;
		return;
	}
}


/*
 *	get1byte()
 *
 *	This routine will read the next byte in the object file from
 *	the buffer (filling the 4 byte buffer if necessary).
 *
 */


void
get1byte()
{
	extern	void	exit();
	extern	long	loc;
	extern	int	oflag;
	extern	char	object[];
	extern	int	trace;
	extern	char	bytebuf[];
	extern	unsigned short	cur1byte;
	static void	fillbuff();

	if (bytesleft == 0) {
		fillbuff();
		if (bytesleft == 0) {
			(void) fprintf(stderr, "\ndis:  premature EOF\n");
			exit(4);
		}
	}
	cur1byte = ((unsigned short) bytebuf[4-bytesleft]) & 0x00ff;
	bytesleft--;
	(oflag > 0)?	(void) sprintf(object, "%s%.3o ", object, cur1byte):
			(void) sprintf(object, "%s%.2x ", object, cur1byte);
	loc++;
	if (trace > 1)
		(void) printf("\nin get1byte object<%s> cur1byte<%.2x>\n",
		    object, cur1byte);
}



/*
 *	gethalfword()
 *
 *	This routine will read the next 2 bytes in the object file from
 *	the buffer (filling the 4 byte buffer if necessary).
 */

static unsigned short
gethalfword()
{
	extern	unsigned short	cur1byte;
	extern	char	object[];
	extern	int	trace;
	union {
		unsigned short 	half;
		char		bytes[2];
	} curhalf;

	curhalf.half = 0;
#if defined(AR32W)
	get1byte();
	curhalf.bytes[1] = (char)cur1byte;
	get1byte();
	curhalf.bytes[0] = (char)cur1byte;
#else	/* !defined(AR32W) */
	get1byte();
	curhalf.bytes[0] = cur1byte;
	get1byte();
	curhalf.bytes[1] = cur1byte;
#endif	/* defined(AR32W) */
	if (trace > 1)
		(void) printf("\nin gethalfword object<%s> halfword<%.4x>\n",
		    object, curhalf.half);
	return (curhalf.half);
}

/*
 *	getword()
 *	This routine will read the next 4 bytes in the object file from
 *	the buffer (filling the 4 byte buffer if necessary).
 *
 */

static long
getword()
{
	extern	void	exit();
	extern	long	loc;
	extern	int	oflag;
	extern	char	object[];
	extern	char	bytebuf[];
	extern	int	trace;
	char	temp1;
	short	byte0, byte1, byte2, byte3;
	int	i, j, bytesread;
	union {
		char	bytes[4];
		long	word;
	} curword;
	static void fillbuff();

	curword.word = 0;
	for (i = 0, j = 4 - bytesleft; i < bytesleft; i++, j++)
		curword.bytes[i] = bytebuf[j];
	if (bytesleft < 4) {
		bytesread = bytesleft;
		fillbuff();
		if ((bytesread + bytesleft) < 4) {
			(void) fprintf(stderr, "\ndis:  premature EOF\n");
			exit(4);
		}
		for (i = bytesread, j = 0; i < 4; i++, j++) {
			bytesleft--;
			curword.bytes[i] = bytebuf[j];
		}
	}
	byte0 = ((short)curword.bytes[0]) & 0x00ff;
	byte1 = ((short)curword.bytes[1]) & 0x00ff;
	byte2 = ((short)curword.bytes[2]) & 0x00ff;
	byte3 = ((short)curword.bytes[3]) & 0x00ff;
	(oflag > 0) ?	(void) sprintf(object, "%s%.3o %.3o %.3o %.3o ", object,
			    byte0, byte1, byte2, byte3):
			(void) sprintf(object, "%s%.2x %.2x %.2x %.2x ", object,
			    byte0, byte1, byte2, byte3);

#if defined(AR16WR)
	temp1 = curword.bytes[0];
	curword.bytes[0] = curword.bytes[2];
	curword.bytes[2] = temp1;
	temp1 = curword.bytes[1];
	curword.bytes[1] = curword.bytes[3];
	curword.bytes[3] = temp1;
#endif	/* defined(AR16WR) */
#if defined(AR32W)
	temp1 = curword.bytes[0];
	curword.bytes[0] = curword.bytes[3];
	curword.bytes[3] = temp1;
	temp1 = curword.bytes[1];
	curword.bytes[1] = curword.bytes[2];
	curword.bytes[2] = temp1;
#endif	/* defined(AR32W) */

	loc += 4;
	if (trace > 1)
		(void) printf("\nin getword object<%s>> word<%.8lx>\n",
		    object, curword.word);
	return (curword.word);
}

/*
 * 	longprint
 *	simply a routine to print a long constant with an optional
 *	prefix string such as "*" or "$" for operand descriptors
 */

static void
longprint(char *result, char *prefix, long value)
{
	extern	int	oflag;

	if (oflag)
		(void) sprintf(result, "%s0%lo", prefix, value);
	else
		(void) sprintf(result, "%s0x%lx", prefix, value);
}

/*
 *	fillbuff()
 *
 *	This routine will read 4 bytes from the object file into the
 *	4 byte buffer.
 *	The bytes will be stored in the buffer in the correct order
 *	for the disassembler to process them. This requires a knowledge
 *	of the type of host machine on which the disassembler is being
 *	executed (AR32WR = vax, AR32W = maxi or 3B, AR16WR = 11/70), as
 *	well as a knowledge of the target machine (FBO = forward byte
 *	ordered, RBO = reverse byte ordered).
 *
 */

static void
fillbuff()
{
	extern	char	bytebuf[];
	extern  unsigned char *p_data;
	int i = 0;

	while (p_data != NULL && i < 4) {
		bytebuf[i] = *p_data;
		bytesleft = i+1;
		i++;
		p_data++;
	}

	switch (bytesleft) {
	case 0:
	case 4:
		break;
	case 1:
		bytebuf[1] = bytebuf[2] = bytebuf[3] = 0;
		break;
	case 2:
		bytebuf[2] = bytebuf[3] = 0;
		break;
	case 3:
		bytebuf[3] = 0;
		break;
	}
	/*
	 * NOTE		The bytes have been read in the correct order
	 *		if one of the following is true:
	 *
	 *		host = AR32WR  and  target = FBO
	 *			or
	 *		host = AR32W   and  target = RBO
	 *
	 */
#if !defined(M32)
#if defined(RBO && AR32WR) || defined(FBO && AR32W)
	bytebuf[0] = (char)((tlong >> 24) & 0x000000ffL);
	bytebuf[1] = (char)((tlong >> 16) & 0x000000ffL);
	bytebuf[2] = (char)((tlong >>  8) & 0x000000ffL);
	bytebuf[3] = (char)(tlong	& 0x000000ffL);
#endif	/* defined(RBO && AR32WR) || defined(FBO && AR32W) */

#if defined(FBO && AR32WR) || defined(RBO && AR32W)
	bytebuf[0] = (char)(tlong	& 0x000000ffL);
	bytebuf[1] = (char)((tlong >>  8) & 0x000000ffL);
	bytebuf[2] = (char)((tlong >> 16) & 0x000000ffL);
	bytebuf[3] = (char)((tlong >> 24) & 0x000000ffL);
#endif	/* defined(FBO && AR32WR) || defined(RBO && AR32W) */

#if defined(RBO && AR16WR)
	bytebuf[0] = (char)((tlong >>  8) & 0x000000ffL);
	bytebuf[1] = (char)(tlong	& 0x000000ffL);
	bytebuf[2] = (char)((tlong >> 24) & 0x000000ffL);
	bytebuf[3] = (char)((tlong >> 16) & 0x000000ffL);
#endif	/* defined(RBO && AR16WR) */
#if defined(FBO && AR16WR)
	bytebuf[0] = (char)((tlong >> 16) & 0x000000ffL);
	bytebuf[1] = (char)((tlong >> 24) & 0x000000ffL);
	bytebuf[2] = (char)(tlong	& 0x000000ffL);
	bytebuf[3] = (char)((tlong >>  8) & 0x000000ffL);
#endif	/* defined(FBO && AR16WR) */
#endif	/* !defined(M32) */
}

static void
PrintLine(char *str)
{
	printf("%s", str);
}
