/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_files_internal.cc	1.12	96/06/27 SMI"

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ndbm.h>

#include <xfn/xfn.hh>
#include <xfn/fn_xdr.hh>
#include <FNSP_Syntax.hh>
#include "FNSP_filesImpl.hh"
#include "fnsp_files_internal.hh"

/* /var files map names and directory */
static const char *FNSP_files_map_dir = FNSP_FILES_MAP_DIR;
static const char *FNSP_user_map = FNSP_USER_MAP_PRE;
static const char *FNSP_user_attr_map = FNSP_USER_ATTR_MAP;
static const char *FNSP_map_suffix = FNSP_MAP_SUFFIX;
static const char *FNSP_lock_file = "fns.lock";

// ----------------------------------------------
// File maipulation routines for /var files maps
// This routine can be used to selectively
// insert, delete, store and modify an entry
// in the map speficied
// ----------------------------------------------
static unsigned
FNSP_is_update_valid(const char *map)
{
	// Check for *root* permissions
	uid_t pid = geteuid();
	if (pid == 0)
		return (FN_SUCCESS);

	// Check if users' attribute map
	if (strcmp(map, FNSP_user_attr_map) == 0)
		return (FN_SUCCESS);

	// Else check for user's DBM file
	struct passwd *user_entry, user_buffer;
	char buffer[FNS_FILES_SIZE];
	// %%% Should we use fgetent instead?
	user_entry = getpwuid_r(pid, &user_buffer, buffer,
	    FNS_FILES_SIZE);
	if (user_entry == NULL)
		return (FN_E_CTX_NO_PERMISSION);

	// Construct the map name
	char mapfile[FNS_FILES_INDEX];
	strcpy(mapfile, FNSP_user_map);
	strcat(mapfile, "_");
	strcat(mapfile, user_buffer.pw_name);
	strcat(mapfile, FNSP_map_suffix);
	if (strcmp(mapfile, map) == 0)
		return (FN_SUCCESS);
	else
		return (FN_E_CTX_NO_PERMISSION);
}

static char *
FNSP_legalize_map_name(const char *name)
{
	// Parse the string to check for "/"
	// If present replace with "#"
	// if "#" is present replace it with "~#"
	size_t i,j;
	int count = 0;
	int replace_count = 0;
	char *answer;
	char internal_name = '/';
	char replace_name = '#';
	for (i = 0; i < strlen(name); i++) {
		if (name[i] == internal_name)
			count++;
		if (name[i] == replace_name)
			replace_count++;
	}
	if ((count == 0) && (replace_count == 0))
		return (strdup(name));
	answer = (char *) malloc(strlen(name) + 2*replace_count + 1);
	if (answer == 0)
		return (0);
	for (i = 0, j = 0; i < strlen(name); i++, j++) {
		if (name[i] == internal_name)
			answer[j] = replace_name;
		else if (name[i] == replace_name) {
			answer[j] = replace_name; j++;
			answer[j] = '~'; j++;
			answer[j] = name[i];
		} else
			answer[j] = name[i];
	}
	answer[strlen(name) + replace_count] = '\0';
	return (answer);
}

static unsigned
FNSP_files_local_update_map(const char *old_map,
    const char *index, const void *data,
    FNSP_map_operation op)
{
	unsigned status;
	// Check if update is valid
	if ((status = FNSP_is_update_valid(old_map))
	    != FN_SUCCESS)
		return (status);

	// Lock FNS update map
	int lock_fs;
	int chmod_for_attr_map = 0;
	char chmod_map_file[FNS_FILES_INDEX];
	char lock_file[FNS_FILES_INDEX];
	strcpy(lock_file, FNSP_files_map_dir);
	strcat(lock_file, "/");
	strcat(lock_file, FNSP_lock_file);
	if ((lock_fs = open(lock_file, O_WRONLY)) == -1)
		return (FN_E_INSUFFICIENT_RESOURCES);
	if (lockf(lock_fs, F_LOCK, 0L) == -1) {
		close(lock_fs);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	// Legalize the map name
	char *map = FNSP_legalize_map_name(old_map);
	if (map == 0)
		return (FN_E_INSUFFICIENT_RESOURCES);

	int dbm_ret;
	char mapfile[FNS_FILES_INDEX];
	DBM *db;
	datum dbm_key, dbm_value, dbm_val;

	dbm_key.dptr = (char *) index;
	dbm_key.dsize = strlen(index);

	strcpy(mapfile, FNSP_files_map_dir);
	strcat(mapfile, "/");
	strcat(mapfile, map);
	if (strcmp(map, FNSP_user_attr_map) == 0) {
		// Check if the file exists
		struct stat map_stat;
		strcpy(chmod_map_file, mapfile);
		strcat(chmod_map_file, ".dir");
		int stat_ret = stat(chmod_map_file, &map_stat);
		db = dbm_open(mapfile, O_RDWR | O_CREAT, 0666);
		if (stat_ret != 0) 
			chmod_for_attr_map = 1;
	} else
		db = dbm_open(mapfile, O_RDWR | O_CREAT, 0644);
	if (db == 0) {
		lockf(lock_fs, F_ULOCK, 0L); close(lock_fs);
		free (map);
		// %%% print error message?
		// perror("unable to open file for adding");
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	dbm_ret = 1;
	switch (op) {
	case FNSP_map_store:
	case FNSP_map_modify:
		dbm_value.dptr = (char *) data;
		dbm_value.dsize = strlen((char *) data);
		dbm_val = dbm_fetch(db, dbm_key);
		if (dbm_val.dptr != 0)
			dbm_ret = dbm_store(db, dbm_key, dbm_value,
			    DBM_REPLACE);
		else
			dbm_ret = dbm_store(db, dbm_key, dbm_value,
			    DBM_INSERT);
		if (dbm_ret != 0) {
			status = FN_E_INSUFFICIENT_RESOURCES;
		}
		break;
	case FNSP_map_delete:
		dbm_ret = dbm_delete(db, dbm_key);
		if (dbm_ret != 0)
			status = FN_E_NAME_NOT_FOUND;
		break;
	case FNSP_map_insert:
		dbm_val = dbm_fetch(db, dbm_key);
		if (dbm_val.dptr != 0)
			status = FN_E_NAME_IN_USE;
		else {
			dbm_value.dptr = (char *) data;
			dbm_value.dsize = strlen((char *) data);
			dbm_ret = dbm_store(db, dbm_key, dbm_value,
			    DBM_INSERT);
			if (dbm_ret != 0) {
				status = FN_E_INSUFFICIENT_RESOURCES;
			}
		}
		break;
	default:
		status = FN_E_CONFIGURATION_ERROR;
		break;
	}
	dbm_close(db);
	if (chmod_for_attr_map) {
		chmod(chmod_map_file, 0666);
		strcpy(chmod_map_file, mapfile);
		strcat(chmod_map_file, ".pag");
		chmod(chmod_map_file, 0666);
	}
		
	lockf(lock_fs, F_ULOCK, 0L);
	close(lock_fs);
	free (map);
	return (status);
}

unsigned
FNSP_files_update_map(const char *map,
    const char *index, const void *data,
    FNSP_map_operation op)
{
	// If the operation is store or modify, delete first
	unsigned status;
	switch (op) {
	case FNSP_map_store:
	case FNSP_map_modify:
		status = FNSP_files_update_map(map, index, data,
		    FNSP_map_delete);
		if (status == FN_E_INSUFFICIENT_RESOURCES)
			return (status);
	default:
		break;
	}

	// Need to split the data to fit 1024 byte limitation
	char new_index[FNS_FILES_SIZE], next_index[FNS_FILES_SIZE];
	char new_data[FNS_FILES_SIZE];
	unsigned stat, count = 1;
	size_t length = 0;
	for (status = FNSP_get_first_index_data(op, index, data,
	    new_index, new_data, length, next_index);
	    status == FN_SUCCESS;
	    status = FNSP_get_next_index_data(op, index, data,
	    new_index, new_data, length, next_index)) {
		stat = FNSP_files_local_update_map(map,
		    new_index, new_data, op);
		if (stat != FN_SUCCESS) {
			if ((count == 1) ||
			    (op != FNSP_map_delete))
				return (stat);
			else
				return (FN_SUCCESS);
		}
		count++;
	}
	if ((count == 1) && (status == FN_E_INSUFFICIENT_RESOURCES))
		return (status);
	return (FN_SUCCESS);
}

static unsigned
FNSP_files_local_lookup(char *old_map, char *map_index, int,
    char **mapentry, int *maplen)
{
	DBM *db;
	char mapfile[FNS_FILES_INDEX];
	datum dbm_key, dbm_value;
	struct stat buffer;
	unsigned status = FN_SUCCESS;

	// Obtain the FNS lock
	int lock_fs;
	char lock_file[FNS_FILES_INDEX];
	strcpy(lock_file, FNSP_files_map_dir);
	strcat(lock_file, "/");
	strcat(lock_file, FNSP_lock_file);
	if ((lock_fs = open(lock_file, O_WRONLY)) == -1)
		return (FN_E_INSUFFICIENT_RESOURCES);
	if (lockf(lock_fs, F_LOCK, 0L) == -1) {
		close(lock_fs);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	// Legalize the map name
	char *map = FNSP_legalize_map_name(old_map);
	if (map == 0)
		return (FN_E_INSUFFICIENT_RESOURCES);

	dbm_key.dptr = map_index;
	dbm_key.dsize = strlen(map_index);
	strcpy(mapfile, FNSP_files_map_dir);
	strcat(mapfile, "/");
	strcat(mapfile, map);
	if ((db = dbm_open(mapfile, O_RDONLY, 0444)) == 0) {
		strcat(mapfile, ".pag");
		if (stat(mapfile, &buffer) == 0) {
			// perror("Unable to iopen file for lookup");
			status = FN_E_INSUFFICIENT_RESOURCES;
		} else
			status = FN_E_NAME_NOT_FOUND;
	} else {
		dbm_value = dbm_fetch(db, dbm_key);
		if (dbm_value.dptr != 0) {
			*maplen = dbm_value.dsize;
			*mapentry = (char *) malloc(*maplen + 1);
			strncpy(*mapentry, dbm_value.dptr, (*maplen));
			(*mapentry)[(*maplen)] = '\0';
		} else
			status = FN_E_NAME_NOT_FOUND;
		dbm_close(db);
	}

	// Release the FNS lock
	lockf(lock_fs, F_ULOCK, 0L);
	close(lock_fs);
	free (map);
	return (status);
}

class FNSP_files_individual_entry {
public:
	char *mapentry;
	size_t maplen;
	FNSP_files_individual_entry *next;
};

unsigned FNSP_files_lookup(char *map, char *map_index, int,
    char **mapentry, int *maplen)
{
	char new_index[FNS_FILES_SIZE], next_index[FNS_FILES_SIZE];
	unsigned status, stat;
	FNSP_files_individual_entry *prev_entry, *new_entry, *entries;

	entries = 0;
	prev_entry = 0;
	for (status = FNSP_get_first_lookup_index(map_index,
	    new_index, next_index); status == FN_SUCCESS;
	    status = FNSP_get_next_lookup_index(new_index, next_index,
	    *mapentry, *maplen)) {
		stat = FNSP_files_local_lookup(map, new_index,
		    strlen(new_index), mapentry, maplen);
		if (stat != FN_SUCCESS)
			break;
		new_entry = (FNSP_files_individual_entry *)
		    malloc(sizeof(FNSP_files_individual_entry));
		new_entry->mapentry = *mapentry;
		new_entry->maplen = *maplen;
		new_entry->next = 0;
		if (entries == 0)
			entries = new_entry;
		else
			prev_entry->next = new_entry;
		prev_entry = new_entry;
	}
	if (!entries)
		return (stat);

	// Construct the mapentry from the linked list
	size_t length = 0;
	new_entry = entries;
	while (new_entry) {
		length += new_entry->maplen;
		new_entry = new_entry->next;
	}
	*maplen = (int) length;
	*mapentry = (char *) malloc(sizeof(char)*(length + 1));
	if (*mapentry == 0)
		// Malloc error
		return (FN_E_INSUFFICIENT_RESOURCES);

	memset((void *) (*mapentry), 0, sizeof(char)*(length + 1));
	new_entry = entries;
	while (new_entry) {
		if (new_entry == entries)
			strcpy((*mapentry), new_entry->mapentry);
		else
			strcat((*mapentry), new_entry->mapentry);
		new_entry = new_entry->next;
	}

	// Destroy entries
	new_entry = entries;
	while (new_entry) {
		free (new_entry->mapentry);
		prev_entry = new_entry;
		new_entry = prev_entry->next;
		free (prev_entry);
	}
	return (FN_SUCCESS);
}

int
FNSP_files_is_fns_installed(const FN_ref_addr *)
{
	// Check if fns.lock file exits
	char fns_lock[FNS_FILES_INDEX];
	struct stat buffer;
	strcpy(fns_lock, FNSP_files_map_dir);
	strcat(fns_lock, "/");
	strcat(fns_lock, FNSP_lock_file);
	if (stat(fns_lock, &buffer) == 0)
		return (FN_SUCCESS);
	else
		return (0);
}

int
FNSP_change_user_ownership(const char *username)
{
	char mapfile[FNS_FILES_INDEX];

	// Open the passwd file
	FILE *passwdfile;
	if ((passwdfile = fopen("/etc/passwd", "r")) == NULL) {
		// fprintf(stderr,
		// gettext("%s: could not open file %s for read\n"
		// "Permissions for user context %s not changed\n"),
		// program_name, FNSP_get_user_source_table(),
		// username);
		return (0);
	}

	// Seach for the user entry
	char buffer[MAX_CANON+1];
	struct passwd *owner_passwd, buf_passwd;
	while ((owner_passwd = fgetpwent_r(passwdfile, &buf_passwd,
	    buffer, MAX_CANON)) != 0) {
		if (strcmp(owner_passwd->pw_name, username) == 0)
			break;
	}
	fclose(passwdfile);

	// Check if the user entry exists in the passwd table
	if (owner_passwd == NULL) {
		// fprintf(stderr,
		// gettext("Unable to get user ID of %s\n"), username);
		// fprintf(stderr, gettext("Permissions for the user context "
		// "%s not changed\n"), username);
		return (0);
	}

	// Change the ownership of the .pag file
	strcpy(mapfile, "/var/fn/fns_user_");
	strcat(mapfile, username);
	strcat(mapfile, ".ctx.pag");
	if (chown(mapfile, owner_passwd->pw_uid, -1) != 0) {
		// fprintf(stderr, gettext("Permissions for the user context "
		// "%s not changed\n"), username);
		return (0);
	}

	// Change the ownership of the .dir file
	strcpy(&mapfile[strlen(mapfile) - strlen("dir")], "dir");
	if (chown(mapfile, owner_passwd->pw_uid, -1) != 0) {
		// fprintf(stderr, gettext("Permissions for the user "
		// "context %s not changed\n"), username);
		return (0);
	}
	return (1);
}
