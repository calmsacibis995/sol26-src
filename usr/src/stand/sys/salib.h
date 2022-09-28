/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SALIB_H
#define	_SYS_SALIB_H

#pragma ident	"@(#)salib.h	1.3	96/09/20 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern void bzero(char *, int);
extern void bcopy(char *, char *, int);
extern int bcmp(char *, char *, int);

extern int strlen(char *);
extern char *strcat(char *, char *);
extern char *strcpy(char *, char *);
extern char *strncpy(char *, char *, size_t);
extern char *strncat(char *, char *, int);
extern int strcmp(char *, char *);
extern int strncmp(char *, char *, int);
extern char *strchr(char *, char);
extern char *strrchr(char *, char);
extern char *strstr(char *, char *);

/*PRINTFLIKE1*/
extern void printf(char *, ...);
/*PRINTFLIKE2*/
extern char *sprintf(char *, char *, ...);

extern char *bkmem_alloc(unsigned int);
extern void bkmem_free(char *, u_int);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SALIB_H */
