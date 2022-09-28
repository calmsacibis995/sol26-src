/*
 * Copyright (c) 1993-1994 Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident	"@(#)ppcdis_debug.h	1.2	94/03/14 SMI"
#pragma ident   "@(#)ppcdis_debug.h 1.2	93/10/05 vla"

#if !defined(PPCDIS_DEBUG_H)
#define	PPCDIS_DEBUG_H

/*
 * PowerPC Disassembler - Debug Flag Definitions
 *
 * This file contains debug flag definitions for the PowerPC disassembler.
 * Judicious use of these flags allows per-module granularity of debug output.
 *
 * How to use disassembler debug mode(s):
 *
 * A. Preparation for debug mode:
 *	  use the "-DDEBUG" flag to include the debug ifdef's when compiling.
 *
 * B. Using debug mode:	 (two recommended methods)
 *	  1. From the command line:
 *			select the desired debug level by setting an environment
 *   		variable "PPCDIS_DEBUG_LEVEL".
 *
 *	  2. From a debugger session:
 * 			select the desired debug level by modifying the
 *			"DEBUG_LEVEL" global variable during a debugger session.
 *
 * EXAMPLE: "export PPCDIS_DEBUG_LEVEL=4" is often useful, because it
 *			displays the original instruction in hex alongside the
 *			disassembled output.
 */

#define	DEBUG_MAIN			0x0001
#define	DEBUG_MAIN_PARSER	0x0004
#define	DEBUG_PARSE_19		0x0010
#define	DEBUG_PARSE_31		0x0020
#define	DEBUG_PARSE_59		0x0040
#define	DEBUG_PARSE_63		0x0080
#define	DEBUG_PARSE_TO		0x0100
#define	DEBUG_PARSE_BO		0x0200

#endif		/* PPCDIS_DEBUG_H */
