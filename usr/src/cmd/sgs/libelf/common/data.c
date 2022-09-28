/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)data.c	1.8	96/03/04 SMI" 	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include <dlfcn.h>


/*
 * Global data
 * _elf_byte		Fill byte for file padding.  See elf_fill().
 * _elf32_ehdr_init	Clean copy for to initialize new headers.
 * _elf_encode		Host/internal data encoding.  If the host has
 *			an encoding that matches one known for the
 *			ELF format, this changes.  An machine with an
 *			unknown encoding keeps ELFDATANONE and forces
 *			conversion for host/target translation.
 * _elf_work		Working version given to the lib by application.
 *			See elf_version().
 * _elf_globals_mutex	mutex to protect access to all global data items.
 */

/*
 * __threaded is a private symbol exported on Solaris2.5 systems and
 * later.  It is used to tell if we are running in a threaded world or
 * not.
 */
#pragma weak		__threaded
extern int		__threaded;

int			_elf_byte = 0;
const Elf32_Ehdr	_elf32_ehdr_init = { 0 };
unsigned		_elf_encode = ELFDATANONE;
const Snode		_elf_snode_init = { 0 };
unsigned		_elf_work = EV_NONE;
mutex_t			_elf_globals_mutex = DEFAULTMUTEX;

int *			_elf_libc_threaded = &__threaded;
