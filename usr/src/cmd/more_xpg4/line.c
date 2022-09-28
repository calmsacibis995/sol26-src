/*
 * COPYRIGHT NOTICE
 * 
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 * 
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED 
 */

/*
 * OSF/1 1.2
 */

#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: line.c,v $ $Revision: 1.1.2.3 $ (OSF) $Date: 1992/09/03 19:54:39 $";
#endif

#pragma	ident	"@(#)line.c 1.5	96/03/04 SMI"

/*
 * Copyright (c) 1988 Mark Nudleman
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef lint
static char sccsid[] = "@(#)line.c	5.5 (Berkeley) 7/24/91";
#endif /* not lint */

/*
 * Routines to manipulate the "line buffer".
 * The line buffer holds a line of output as it is being built
 * in preparation for output to the screen.
 * We keep track of the PRINTABLE length of the line as it is being built.
 */

#if defined(sun)
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <libintl.h>
#include <wchar.h>
#include <widec.h>
#include <sys/types.h>
#include "less.h"
#endif


static wchar_t linebuf[BUFSIZ];	/* Buffer which holds the current output line */
static wchar_t *curr;		/* Pointer into linebuf */
static int column;		/* Printable length, accounting for
				   backspaces, etc. */
/*
 * A ridiculously complex state machine takes care of backspaces.  The
 * complexity arises from the attempt to deal with all cases, especially
 * involving long lines with underlining, boldfacing or whatever.  There
 * are still some cases which will break it.
 *
 * There are four states:
 *	LN_NORMAL is the normal state (not in underline mode).
 *	LN_UNDERLINE means we are in underline mode.  We expect to get
 *		either a sequence like "_\bX" or "X\b_" to continue
 *		underline mode, or anything else to end underline mode.
 *	LN_BOLDFACE means we are in boldface mode.  We expect to get sequences
 *		like "X\bX\b...X\bX" to continue boldface mode, or anything
 *		else to end boldface mode.
 *	LN_UL_X means we are one character after LN_UNDERLINE
 *		(we have gotten the '_' in "_\bX" or the 'X' in "X\b_").
 *	LN_UL_XB means we are one character after LN_UL_X 
 *		(we have gotten the backspace in "_\bX" or "X\b_";
 *		we expect one more ordinary character, 
 *		which will put us back in state LN_UNDERLINE).
 *	LN_BO_X means we are one character after LN_BOLDFACE
 *		(we have gotten the 'X' in "X\bX").
 *	LN_BO_XB means we are one character after LN_BO_X
 *		(we have gotten the backspace in "X\bX";
 *		we expect one more 'X' which will put us back
 *		in LN_BOLDFACE).
 */
static int ln_state;		/* Currently in normal/underline/bold/etc mode? */
#define	LN_NORMAL	0	/* Not in underline, boldface or whatever mode */
#define	LN_UNDERLINE	1	/* In underline, need next char */
#define	LN_UL_X		2	/* In underline, got char, need \b */
#define	LN_UL_XB	3	/* In underline, got char & \b, need one more */
#define	LN_BOLDFACE	4	/* In boldface, need next char */
#define	LN_BO_X		5	/* In boldface, got char, need \b */
#define	LN_BO_XB	6	/* In boldface, got char & \b, need same char */

wchar_t *line;			/* Pointer to the current line.
				   Usually points to linebuf. */

extern int bs_mode;
extern int tabstop;
extern int bo_width, be_width;
extern int ul_width, ue_width;
extern int sc_width, sc_height;
extern int show_all_opt, show_opt;
extern wchar_t	ch_forw_get_w();
extern off_t	ch_tell_old();
extern wchar_t	ch_back_get_w(off_t *);

/*
 * Rewind the line buffer.
 */
void
prewind(void)
{
	line = curr = linebuf;
	ln_state = LN_NORMAL;
	column = 0;
}

/*
 * Append a character to the line buffer.
 * Expand tabs into spaces, handle underlining, boldfacing, etc.
 * Returns 0 if ok, 1 if couldn't fit in buffer.
 */
#define	NEW_COLUMN(addon) \
	if (column + addon + (ln_state ? ue_width : 0) > sc_width) \
		return(1); \
	else \
		column += addon

int
pappend(int c)
{
	int	len;
	char	multi[MB_LEN_MAX];
	static int escape = 0;
	int	c_len;

	if (c == '\0') {
		/*
		 * Terminate any special modes, if necessary.
		 * Append a '\0' to the end of the line.
		 */
		switch (ln_state) {
		case LN_UL_X:
			curr[1] = curr[-1];
			curr[-1] = ESC_CHAR;
			curr[0] = UE_CHAR;
			curr +=2;
			break;
		case LN_BO_X:
			curr[1] = curr[-1];
			curr[-1] = ESC_CHAR;
			curr[0] = BE_CHAR;
			curr +=2;
			break;
		case LN_UL_XB:
		case LN_UNDERLINE:
			*curr++ = ESC_CHAR;
			*curr++ = UE_CHAR;
			break;
		case LN_BO_XB:
		case LN_BOLDFACE:
			*curr++ = ESC_CHAR;
			*curr++ = BE_CHAR;
			break;
		}
		ln_state = LN_NORMAL;
		*curr = '\0';
		return(0);
	}

	if (curr > &linebuf[BUFSIZ - 12])
		/*
		 * Almost out of room in the line buffer.
		 * Don't take any chances.
		 * {{ Linebuf is supposed to be big enough that this
		 *    will never happen, but may need to be made 
		 *    bigger for wide screens or lots of backspaces. }}
		 */
		return(1);

	if (!bs_mode) {
		/*
		 * Advance the state machine.
		 */
		switch (ln_state) {
		case LN_NORMAL:
			if (curr <= linebuf + 1
			    || curr[-1] != '\b')
				break;
			column -= 2;
			if (c == curr[-2])
				goto enter_boldface;
			if (c == '_' || curr[-2] == '_')
				goto enter_underline;
			curr -= 2;
			break;

enter_boldface:
			/*
			 * We have "X\bX" (including the current char).
			 * Switch into boldface mode.
			 */
			column--;
			if (column + bo_width + be_width + 1 >= sc_width)
				/*
				 * Not enough room left on the screen to 
				 * enter and exit boldface mode.
				 */
				return (1);

			if (bo_width > 0 && curr > linebuf + 2
			    && curr[-3] == ' ') {
				/*
				 * Special case for magic cookie terminals:
				 * if the previous char was a space, replace 
				 * it with the "enter boldface" sequence.
				 */
				curr[-1] = curr[-2];
				curr[-2] = BO_CHAR;
				curr[-3] = ESC_CHAR;
				column += bo_width-1;
				curr++;
			} else {
				curr[0] = curr[-2];
				curr[-2] = ESC_CHAR;
				curr[-1] = BO_CHAR;
				column += bo_width;
				curr += 2;
			}
			goto ln_bo_xb_case;

enter_underline:
			/*
			 * We have either "_\bX" or "X\b_" (including
			 * the current char).  Switch into underline mode.
			 */
			column--;
			if (column + ul_width + ue_width + 1 >= sc_width)
				/*
				 * Not enough room left on the screen to 
				 * enter and exit underline mode.
				 */
				return (1);

			if (ul_width > 0 && 
			    curr > linebuf + 2 && curr[-3] == ' ')
			{
				/*
				 * Special case for magic cookie terminals:
				 * if the previous char was a space, replace 
				 * it with the "enter underline" sequence.
				 */
				curr[-1] = curr[-2];
				curr[-2] = UL_CHAR;
				curr[-3] = ESC_CHAR;
				column += ul_width-1;
				curr++;
			} else
			{
				curr[0] = curr[-2];
				curr[-1] = UL_CHAR;
				curr[-2] = ESC_CHAR;
				column += ul_width;
				curr +=2;
			}
			goto ln_ul_xb_case;
			/*NOTREACHED*/
		case LN_UL_XB:
			/*
			 * Termination of a sequence "_\bX" or "X\b_".
			 */
			if (c != '_' && curr[-2] != '_' && c == curr[-2])
			{
				/*
				 * We seem to have run on from underlining
				 * into boldfacing - this is a nasty fix, but
				 * until this whole routine is rewritten as a
				 * real DFA, ...  well ...
				 */
				curr[2] = curr[-2];
				curr[-2] = ESC_CHAR;
				curr[-1] = UE_CHAR;
				curr[0] = ESC_CHAR;
				curr[1] = BO_CHAR;
				curr += 4; /* char & non-existent backspace */
				ln_state = LN_BO_XB;
				goto ln_bo_xb_case;
			}
ln_ul_xb_case:
			if (c == '_')
				c = curr[-2];
			curr -= 2;
			ln_state = LN_UNDERLINE;
			break;
		case LN_BO_XB:
			/*
			 * Termination of a sequnce "X\bX".
			 */
			if (c != curr[-2] && (c == '_' || curr[-2] == '_'))
			{
				/*
				 * We seem to have run on from
				 * boldfacing into underlining.
				 */
				curr[2] = curr[-2];
				curr[-2] = ESC_CHAR;
				curr[-1] = BE_CHAR;
				curr[0] = ESC_CHAR;
				curr[1] = UL_CHAR;
				curr += 4; /* char & non-existent backspace */
				ln_state = LN_UL_XB;
				goto ln_ul_xb_case;
			}
ln_bo_xb_case:
			curr -= 2;
			ln_state = LN_BOLDFACE;
			break;
		case LN_UNDERLINE:
			if (column + ue_width + bo_width + 1 + be_width >= sc_width)
				/*
				 * We have just barely enough room to 
				 * exit underline mode and handle a possible
				 * underline/boldface run on mixup.
				 */
				return (1);
			ln_state = LN_UL_X;
			break;
		case LN_BOLDFACE:
			if (c == '\b')
			{
				ln_state = LN_BO_XB;
				break;
			}
			if (column + be_width + ul_width + 1 + ue_width >= sc_width)
				/*
				 * We have just barely enough room to 
				 * exit underline mode and handle a possible
				 * underline/boldface run on mixup.
				 */
				return (1);
			ln_state = LN_BO_X;
			break;
		case LN_UL_X:
			if (c == '\b')
				ln_state = LN_UL_XB;
			else
			{
				/*
				 * Exit underline mode.
				 * We have to shuffle the chars a bit
				 * to make this work.
				 */
				curr[1] = curr[-1];
				curr[-1] = ESC_CHAR;
				curr[0] = UE_CHAR;
				column += ue_width;
				curr++;
				if (ue_width > 0 && curr[0] == ' ')
					/*
					 * Another special case for magic
					 * cookie terminals: if the next
					 * char is a space, replace it
					 * with the "exit underline" sequence.
					 */
					column--;
				else
					curr++;
				ln_state = LN_NORMAL;
			} 
			break;
		case LN_BO_X:
			if (c == '\b')
				ln_state = LN_BO_XB;
			else
			{
				/*
				 * Exit boldface mode.
				 * We have to shuffle the chars a bit
				 * to make this work.
				 */
				curr[1] = curr[-1];
				curr[-1] = ESC_CHAR;
				curr[0] = BE_CHAR;
				column += be_width;
				curr++;
				if (be_width > 0 && curr[0] == ' ')
					/*
					 * Another special case for magic
					 * cookie terminals: if the next
					 * char is a space, replace it
					 * with the "exit boldface" sequence.
					 */
					column--;
				else
					curr++;
				ln_state = LN_NORMAL;
			} 
			break;
		}
	}

	if (c == '\t') {
		/*
		 * Expand a tab into spaces.
		 */
		do {
			NEW_COLUMN(1);
		} while (!show_all_opt && (column % tabstop) != 0);
		*curr++ = '\t';
		return (0);
	}

	if (c == '\b') {
		if (ln_state == LN_NORMAL)
			NEW_COLUMN(2);
		else
			column--;
		*curr++ = '\b';
		return(0);
	} 

	if (c == ESC_CHAR) {
		if (!escape) {
			NEW_COLUMN(2);	/* assuming ESC_CHAR is a control */
			*curr++ = ESC_CHAR;
			escape = 1;
			return(0);
		} else {
			/*
			 * This is a literal ESC_CHAR.
			 * Already did the NEW_COLUMN above.
			 */
			escape = 0;
			*curr++ = ESC_CHAR;
			return(0);
		}
	}

	/*
	 * Make sure non-printable don't wrap lines.
	 * Three cases:
	 * 	ctrl-X (^X)	    == 2 columns
	 * 	meta-X:	(M-X)	    == 3 columns
	 * 	meta-ctrl-X: M-^X   == 4 columns
	 * 
	 * put_line() prints them out.
	 */
	if (show_opt) {
		if (!iswprint(c)) {
			if (iswcntrl(toascii(c)))
				NEW_COLUMN(4);		/* M-^X */
			else
				NEW_COLUMN(3);		/* M-X */
			*curr++ = c;
			return(0);
		} else if (iswcntrl(c)) {
			NEW_COLUMN(2);			/* ^X */
			*curr++ = c;
			return(0);
		}
	}

	/*
	 * Ordinary character.  Just put it in the buffer.
	 */
	if ((c_len = wcwidth(c)) > 0)
		NEW_COLUMN(c_len);
	*curr++ = c;
	return (0);
}

/*
 * Analogous to forw_line(), but deals with "raw lines":
 * lines which are not split for screen width.
 * {{ This is supposed to be more efficient than forw_line(). }}
 */
off_t
forw_raw_line(off_t curr_pos)
{
	register wchar_t *p;
	register int c;
	int	len;
#if defined(sun)
	off_t new_pos;
#else
	off_t new_pos, ch_tell();
#endif

	if (curr_pos == NULL_POSITION || ch_seek(curr_pos) ||
		(c = ch_forw_get_w()) == EOI)
		return (NULL_POSITION);

	p = linebuf;

	for (;;)
	{
		if (c == '\n' || c == EOI)
		{
			new_pos = ch_tell();
			break;
		}
		if (p >= &linebuf[BUFSIZ-1])
		{
			/*
			 * Overflowed the input buffer.
			 * Pretend the line ended here.
			 * {{ The line buffer is supposed to be big
			 *    enough that this never happens. }}
			 */
			new_pos = ch_tell_old();
			break;
		}
		*p++ = c;
		c = ch_forw_get_w();
	}
	*p = '\0';
	line = linebuf;
	return (new_pos);
}

/*
 * Analogous to back_line(), but deals with "raw lines".
 * {{ This is supposed to be more efficient than back_line(). }}
 */
off_t
back_raw_line(off_t curr_pos)
{
	register wchar_t *p;
	register int c;
	int	len;
	off_t	i, fpos;
#if defined(sun)
	off_t new_pos;
#else
	off_t new_pos, ch_tell();
#endif

	if (curr_pos == NULL_POSITION || curr_pos <= (off_t)0 ||
		ch_seek(curr_pos-1))
		return (NULL_POSITION);

	ch_seek(curr_pos);
	fpos = curr_pos;
	(void) ch_back_get_w(&fpos);
	p = &linebuf[BUFSIZ];
	*--p = '\0';
	
	for (;;) {
		i = ch_tell();
		if (fpos >= i) {
			fpos = i;
		}
		c = ch_back_get_w(&fpos);
		if (c == '\n') {
			/*
			 * This is the newline ending the previous line.
			 * We have hit the beginning of the line.
			 */
			new_pos = ch_tell() + 1;
			break;
		}
		if (c == EOI) {
			/*
			 * We have hit the beginning of the file.
			 * This must be the first line in the file.
			 * This must, of course, be the beginning of the line.
			 */
			new_pos = (off_t)0;
			break;
		}
		if (p <= linebuf) {
			/*
			 * Overflowed the input buffer.
			 * Pretend the line ended here.
			 */
			(void) ch_forw_get_w();
			new_pos = ch_tell();
			break;
		}
		*--p = c;
	}
	line = p;
	return (new_pos);
}
