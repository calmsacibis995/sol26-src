/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)__wcsxfrm_std.c 1.9	96/08/14  SMI"


/*
#if !defined(lint) && !defined(_NOIDENT)
static char sccsid[] = "@(#)72	1.6.2.2  "
	"src/bos/usr/ccs/lib/libc/__wcsxfrm_std.c, bos, "
	"bos410 1/12/93 11:11:58";
#endif
 */
/*
 * COMPONENT_NAME: (LIBCSTR) Standard C Library String Handling Functions
 *
 * FUNCTIONS: __wcsxfrm_std.c
 *
 * ORIGINS: 27
 *
 * IBM CONFIDENTIAL -- (IBM Confidential Restricted when
 * combined with the aggregated modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1989,1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */
#pragma alloca

#include <sys/localedef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <alloca.h>
#include <sys/isa_defs.h>


size_t
__wcsxfrm_std(_LC_collate_t *hdl, wchar_t *wcs_out, const wchar_t *wcs_in,
		size_t n)
{
	char *str_in;
	int len_in;
	int rc;
	int wc_size;

	len_in = wcslen(wcs_in) * __lc_collate->cmapp->cm_mb_cur_max + 1;
	if ((str_in = (char *)alloca(len_in)) == (char *)NULL) {
		perror("__wcsxfrm_std:alloca");
		_exit(-1);
	}

	if (wcstombs(str_in, wcs_in, len_in) == -1)
		errno = EINVAL;

	wc_size = sizeof (wchar_t);
	if ((wcs_out == (wchar_t *)NULL) || (n == 0)) {
		rc = strxfrm((char *)NULL, str_in, 0);
		return (rc / wc_size);
	} else
		rc = strxfrm((char *)wcs_out, str_in, n * wc_size);

	len_in = rc % wc_size;
	rc /= wc_size;

	if (rc >= n)
		wcs_out[n - 1] = L'\0';
	else {
		if (len_in > 0 && rc < (n - 1)) {
#if	defined(vax) || defined(_LITTLE_ENDIAN)
			wcs_out[rc] = (wcs_out[rc] << 16) >> 16;
#else
			wcs_out[rc] = (wcs_out[rc] >> 16) << 16;
#endif	/* defined(vax) || defined(_LITTLE_ENDIAN) */
			wcs_out[++rc] = L'\0';
		} else
			wcs_out[rc] = L'\0';
	}

#if	defined(vax) || defined(_LITTLE_ENDIAN)
	for (len_in = 0; wcs_out[len_in]; len_in++) {
		char hl[4];

		hl[0] = ((char *)(wcs_out + len_in))[3];
		hl[1] = ((char *)(wcs_out + len_in))[2];
		hl[2] = ((char *)(wcs_out + len_in))[1];
		hl[3] = ((char *)(wcs_out + len_in))[0];

		wcs_out[len_in] = *(wchar_t *)hl;
	}
#endif	/* defined(vax) || defined(_LITTLE_ENDIAN) */

	return (rc);
}
