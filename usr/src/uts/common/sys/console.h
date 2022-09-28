/*
 * Copyright (c) 1990-1994, by Sun Microsystems, Inc.
 */

#ifndef	_SYS_CONSOLE_H
#define	_SYS_CONSOLE_H

#pragma ident	"@(#)console.h	1.15	95/07/06 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

/*
 * These functions are in common/os/console.c
 */
extern void consoleconfig(void);
extern void consoleunconfig(void);
extern void consolepreload(void);
extern void console_write_str(char *str, u_int len);
extern void console_write_char(char c);
extern void ddi_bell(void);
extern void bell_provider(void (*bellfunc)());
extern void console_get_size(ushort *r, ushort *c, ushort *x, ushort *y);
extern void consoleloadfonts(void);

/*
 * These funcrions are in <isa>/os/cons_subr.c
 */
extern void console_get_devname(char *devname);
extern void console_connect(void);
extern int use_bifont(void);
extern int stdout_is_framebuffer(void);
extern char * stdout_path(void);

/*
 * Global used to mark progress of bringup for use by
 * output_line() in cmn_err.c
 */
extern int post_consoleconfig;
extern void console_default_bell(clock_t);

#if	defined(i386) || defined(__ppc)
/*
 * These functions are in .../os/cons_subr.c
 */
extern int console_kadb_write_char(char c);
extern int console_char_no_output(char c);
#endif

/*
 * Flags used during system boot -a to configure the console
 */
#define	CONS_DOASK	0x001
#define	CONS_BIFONTS	0x002
#define	CONS_DOCONFIG	0x004
#define	CONS_TEXTMODE	0x008

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONSOLE_H */
