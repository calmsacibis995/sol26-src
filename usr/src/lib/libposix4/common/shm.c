/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)shm.c	1.4	93/10/07	SMI"

#include "synonyms.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include "pos4obj.h"

int
shm_open(const char *name, int oflag, mode_t mode)
{

	int flag;

	oflag = oflag & (O_RDWR|O_RDONLY|O_WRONLY|O_TRUNC|O_CREAT|O_EXCL);
	return (__pos4obj_open(name, SHM_DATA_TYPE, oflag, mode, &flag));
}


int
shm_unlink(const char *name)
{

	return (__pos4obj_unlink(name, SHM_DATA_TYPE));

}
