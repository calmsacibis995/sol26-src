/*
 *	Copyright(c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright(c) 1995 by Sun Microsystems, Inc.
 *	All Rights Reserved
 */
#pragma ident	"@(#)compress.c	1.4	95/08/23 SMI"

#include "mcs.h"
#include "extern.h"
char *compress(char *, int *);

/*
 * ACT_COMPRESS
 */
#define	HALFLONG 16
#define	low(x)  (x&((1L<<HALFLONG)-1))
#define	high(x) (x>>HALFLONG)

void
docompress(section_info_table *info)
{
	Elf_Data *data;
	int size;
	char *buf;

	if (info->mdata == 0) {
		/*
		 * mdata is not allocated yet.
		 * Allocate the data and set it.
		 */
		char *p;
		info->mdata = data = (Elf_Data *) calloc(1, sizeof (Elf_Data));
		if (data == NULL)
			error_malloc();
		*data = *info->data;
		p = (char *) malloc(data->d_size);
		(void) memcpy(p, (char *)data->d_buf, data->d_size);
		data->d_buf = p;
	}
	size = info->mdata->d_size;
	buf = (char *)info->mdata->d_buf;
	buf = compress(buf, &size);
	info->mdata->d_buf = buf;
	info->mdata->d_size = size;
}

char *
compress(char *str, int *size)
{
	int hash;
	int i;
	int temp_string_size = *size;
	int o_size = *size;
	char *temp_string = str;

	int *hash_key;
	int hash_num;
	int hash_end;
	int *hash_str;
	char *strings;
	int next_str;
	int str_size;

	hash_key = (int *) malloc((unsigned) sizeof (int) * 200);
	hash_end = 200;
	hash_str = (int *) malloc((unsigned) sizeof (int) * 200);
	str_size = 10000;
	strings = (char *) malloc((unsigned) str_size);

	if (hash_key == NULL || hash_str == NULL || strings == NULL) {
		(void) fprintf(stderr,
		MALLOC_ERROR,
		prog);
		mcs_exit(FAILURE);
	}

	hash_num = 0;
	next_str = 0;

	while (temp_string_size > 0) {
		int pos;
		char c;
		/*
		 * Get a string
		 * pos = getstr(info);
		 */
		pos = next_str;

		while ((c = *(temp_string++)) != '\0' &&
		    (temp_string_size - (next_str - pos)) != 0) {
			if (next_str >= str_size) {
				str_size *= 2;
				if ((strings = (char *)
					realloc(strings,
					    (unsigned) str_size)) == NULL) {
					(void) fprintf(stderr,
					MALLOC_ERROR,
					prog);
					mcs_exit(FAILURE);
				}
			}
			strings[next_str++] = c;
		}

		if (next_str >= str_size) {
			str_size *= 2;
			if ((strings = (char *)
			    realloc(strings, (unsigned) str_size)) == NULL) {
				(void) fprintf(stderr,
				MALLOC_ERROR,
				prog);
				mcs_exit(FAILURE);
			}
		}
		strings[next_str++] = NULL;
		/*
		 * End get string
		 */

		temp_string_size -= (next_str - pos);
		hash = dohash(pos + strings);
		for (i = 0; i < hash_num; i++) {
			if (hash != hash_key[i])
				continue;
			if (strcmp(pos + strings, hash_str[i] + strings) == 0)
				break;
		}
		if (i != hash_num) {
			next_str = pos;
			continue;
		}
		if (hash_num == hash_end) {
			hash_end *= 2;
			hash_key = (int *) realloc((char *) hash_key,
				(unsigned) hash_end * sizeof (int));
			hash_str = (int *) realloc((char *) hash_str,
				(unsigned) hash_end * sizeof (int));
			if (hash_key == NULL || hash_str == NULL) {
				fprintf(stderr,
				MALLOC_ERROR,
				prog);
				mcs_exit(FAILURE);
			}
		}
		hash_key[hash_num] = hash;
		hash_str[hash_num++] = pos;
	}

	/*
	 * Clean up
	 */
	free(hash_key);
	free(hash_str);

	/*
	 * Return
	 */
	if (next_str != o_size) {
		/*
		 * string compressed.
		 */
		*size = next_str;
		free(str);
		str = malloc(next_str);
		(void) memcpy(str, strings, next_str);
	}
	free(strings);
	return (str);
}

int
dohash(str)
char    *str;
{
	long    sum;
	register unsigned shift;
	register t;
	sum = 1;
	for (shift = 0; (t = *str++) != NULL; shift += 7) {
		sum += (long)t << (shift %= HALFLONG);
	}
	sum = low(sum) + high(sum);
	return ((short) low(sum) + (short) high(sum));
}
