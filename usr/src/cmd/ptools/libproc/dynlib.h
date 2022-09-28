/*
 * dynlib.h
 * Function prototypes for dynlib.c
 */

/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef DYNLIB_H
#define	DYNLIB_H

#pragma	ident	"@(#)dynlib.h	1.3	96/06/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern	void	clear_names(void);
extern	void	load_lib_name(const char *name);
extern	void	load_lib_dir(const char *dir);
extern	void	load_ldd_names(int asfd, pid_t pid);
extern	void	load_exec_name(const char *name);
extern	void	make_exec_name(const char *name);
extern	char	*lookup_raw_file(dev_t dev, ino_t ino);
extern	char	*lookup_file(dev_t dev, ino_t ino);
extern	char	*index_name(int index);
extern	ssize_t	read_string(int asfd, char *buf, size_t size, off_t addr);

#ifdef	__cplusplus
}
#endif

#endif	/* DYNLIB_H */
