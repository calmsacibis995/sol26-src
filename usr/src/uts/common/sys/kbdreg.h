/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#ifndef _SYS_KBDREG_H
#define	_SYS_KBDREG_H

#pragma ident	"@(#)kbdreg.h	1.13	95/08/07 SMI"	/* SunOS4.0 1.7	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Keyboard implementation private definitions.
 */

struct keyboardstate {
	int	k_id;
	u_char	k_idstate;
	u_char	k_state;
	u_char	k_rptkey;
	u_int	k_buckybits;
	u_int	k_shiftmask;
	struct	keyboard *k_curkeyboard;
	u_int	k_togglemask;	/* Toggle shifts state */
};

/*
 * States of keyboard ID recognizer
 */
#define	KID_NONE		0	/* startup */
#define	KID_GOT_PREFACE		1	/* got id preface */
#define	KID_OK			2	/* locked on ID */
#define	KID_GOT_LAYOUT		3	/* got layout prefix */
#if	defined(prep)
#define	KID_GOT_PRESSED		4	/* got "pressed" prefix */
#define	KID_GOT_RELEASED	5	/* got "released" prefix */
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_KBDREG_H */
