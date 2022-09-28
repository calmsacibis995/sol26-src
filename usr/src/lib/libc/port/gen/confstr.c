/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * change to use /etc/default/confstr file for variables.
 */

#ident	"@(#)confstr.c	1.8	96/05/30 SMI"

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* Keep in synch with execvp.c */

typedef struct {
	int	config_value;
	char	*value;
} config;

/*
 * keep these in the same order as in sys/unistd.h
 */
static const config	default_conf[] = {
	/*
	 * Leave _CS_PATH as the first entry.  There is a performance
	 * issue here since exec calls this function asking for CS_PATH.
	 * Also chack out execvp.c if the path must change.  This value
	 * may be hard coded there too...
	 */
	{ _CS_PATH,		"/usr/xpg4/bin:/usr/ccs/bin:/usr/bin"	},
	{ _CS_LFS_CFLAGS,	"-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64" },
	{ _CS_LFS_LDFLAGS,	""					},
	{ _CS_LFS_LIBS,		""					},
	{ _CS_LFS_LINTFLAGS,	"-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64" },
	{ _CS_LFS64_CFLAGS,	"-D_LARGEFILE64_SOURCE"			},
	{ _CS_LFS64_LDFLAGS,	""					},
	{ _CS_LFS64_LIBS,	""					},
	{ _CS_LFS64_LINTFLAGS,	"-D_LARGEFILE64_SOURCE"			}
};
#define	CS_ENTRY_COUNT (sizeof (default_conf) / sizeof (config))


size_t
confstr(int name, char *buf, size_t length)
{
	size_t			conf_length = 0;
	config			*entry;
	int			i;

	/*
	 * Make sure it is a know configuration parameter
	 */
	entry = (config *)default_conf;
	for (i = 0; i < CS_ENTRY_COUNT; i++) {
		if (name == entry->config_value) {
			/*
			 * Copy out the parameter from our tables.
			 */
			conf_length = strlen(entry->value) + 1;
			if (length != 0) {
				strncpy(buf, entry->value, length);
				buf[length - 1] = '\0';
			}
			return (conf_length);
		}
		entry++;
	}

	/* If the entry was not found in table return an error */
	errno = EINVAL;
	return ((size_t)0);
}
