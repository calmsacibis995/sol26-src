/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _HEADER_H
#define	_HEADER_H

#pragma ident	"@(#)prototype.h	1.2	96/01/11 SMI"

/*
 * Block comment which describes the contents of this file.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <includes.h>

#define	MACROS values

struct tag {
	type_t member;
};

typedef predefined_types new_types;

type_t global_variables;

void functions(void);

#ifdef __cplusplus
}
#endif

#endif /* _HEADER_H */
