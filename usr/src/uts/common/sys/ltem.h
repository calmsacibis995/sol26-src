/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef	_SYS_LTEM_H
#define	_SYS_LTEM_H

#pragma ident	"@(#)ltem.h	1.8	95/07/10 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These are the ioctls used by the kernel and applications wishing to
 * send data to the system console.  The kernel and applications do this
 * by opening up the layered terminal emulator and using the ioctls and
 * write(2) system call.  To send text to the console, just do a write to
 * the layered driver.  To send graphics data, use the LTEM_DISPLAY ioctl.
 */

#define	LTEMIOC	('l' << 8)

#ifdef _KERNEL
/*
 * LTEM_OPEN is used by the kernel to link the layered terminal emulator
 * with a default console framebuffer.  The framebuffer is spcified by the
 * physical pathname. Takes a physical pathname (in a string
 * of MAXPATHLEN length) as an argument.
 *
 * This is a kernel <-> ltem private ioctl to initialize the system
 * console.
 */
#define	LTEM_OPEN		(LTEMIOC|1)

/*
 * The LTEM_STAND_WRITE ioctl is reserved for exclusive use of the kernel and
 * should not be set by a user level applications.  The kernel uses this
 * ioctl when it is asked to display characters for kadb.  The argument is
 * the character to display.
 */
#define	LTEM_STAND_WRITE		(LTEMIOC|2)
#endif _KERNEL

#ifdef __cplusplus
}
#endif

#endif /* _SYS_LTEM_H */
