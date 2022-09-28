/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_nis_internal.cc	1.14	96/07/22 SMI"

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ndbm.h>
#include <synch.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <xfn/xfn.hh>
#include <xfn/fn_xdr.hh>
#include <FNSP_Syntax.hh>
#include "fnsp_nis_internal.hh"
#include "fnsp_internal_common.hh"
#include "FNSP_nisImpl.hh"

/* NIS map names and directory */
static const char *FNSP_nis_map_dir = "/var/yp/";
static const char *FNSP_nis_dir = "/etc/fn/";
static const char *FNSP_org_map = FNSP_ORG_MAP;
static const char *FNSP_passwd_map = "passwd.byname";
static const char *FNSP_hosts_map = "hosts.byname";
static const char *FNSP_map_suffix = ".ctx";
static const char *FNSP_attr_suffix = ".attr";
static const char *FNSP_lock_file = "/fns.lock";
static const char *FNSP_count_file = "/fns.count";
static char *FNSP_yp_master = "YP_MASTER_NAME";
static char *FNSP_yp_last_modified = "YP_LAST_MODIFIED";


// -----------------------------------------
// Routine to talk to foreign NIS domains
// -----------------------------------------
#include <dlfcn.h>

// Function defined in yp_bind.c to talk to another YP domain
// extern "C" {
// int __yp_add_binding(char * /* Domain */,
// char * /* IP address */);
// }

typedef int (*add_binding_func)(char *, char *);

static mutex_t yp_add_binding_lock = DEFAULTMUTEX;
static void *nis_bind_function = 0;

// Function to add bindings to the foreign domain
static int
yp_add_binding(char *domain, char *ip_address)
{
	// Check if we have cached the function
	mutex_lock(&yp_add_binding_lock);
	if (nis_bind_function != 0) {
		mutex_unlock(&yp_add_binding_lock);
		return ((*((add_binding_func) nis_bind_function))(domain,
		    ip_address));
	}

	// Try to dlsym to the function __yp_add_binding
	// if present in shared libraries
	void *mh;
	if ((mh = dlopen(0, RTLD_LAZY)) != 0) {
		if (nis_bind_function = dlsym(mh, "__yp_add_binding")) {
			mutex_unlock(&yp_add_binding_lock);
			return ((*((add_binding_func) nis_bind_function))(
			    domain, ip_address));
		}
	}

	// Default behaviour, just try to bind and maybe fail
	mutex_unlock(&yp_add_binding_lock);
	return (yp_bind(domain));
}

// Struct to hold the domainname and operations to
// add and check domainnames. yp_unbind is done at exit

class FNSP_nis_domainname {
protected:
	char *domainname;
	FNSP_nis_domainname *next;
public:
	FNSP_nis_domainname();
	~FNSP_nis_domainname();
	int add_domain(char *domain);
	int check_domain(char *domain);
};

FNSP_nis_domainname::FNSP_nis_domainname()
{
	domainname = 0;
	next = 0;
}

FNSP_nis_domainname::~FNSP_nis_domainname()
{
	if (domainname) {
		yp_unbind(domainname);
		delete[] domainname;
	}
	delete next;
}

int
FNSP_nis_domainname::add_domain(char *domain)
{
	if (!domainname) {
		domainname = new char[strlen(domain) + 1];
		if (domainname == 0)
			return (0);
		strcpy(domainname, domain);
		return (1);
	}

	if (!next) {
		next = new FNSP_nis_domainname();
		if (next == 0)
			return (0);
	}
	return (next->add_domain(domain));
}

int
FNSP_nis_domainname::check_domain(char *domain)
{
	if (domainname)
		if (strcmp(domain, domainname) == 0)
			return (1);
	if (next)
		return (next->check_domain(domain));
	return (0);
}

// Lock for the domain name the current process is
// bound to.
static mutex_t domainname_lock = DEFAULTMUTEX;
static FNSP_nis_domainname nis_domainname;

unsigned
FNSP_nis_bind(const char *in_buffer)
{
	char *domain, *machine_name, *ip_address = 0;
	char ip_addr_buf[FNS_NIS_INDEX];
	int ret_error, fns_error = FN_SUCCESS;
	char *temp, buffer[FNS_NIS_SIZE];

	strcpy(buffer, in_buffer);
	domain = strparse(buffer, " ", &temp);
	machine_name = strparse(0, " ", &temp);
	if (machine_name) {
		if (inet_addr(machine_name) == -1)
			ip_address = strparse(0, " ", &temp);
		else {
			ip_address = machine_name;
			machine_name = 0;
		}
	}

	if ((machine_name) && (ip_address == 0)) {
		// Get the IP address from machine name
		struct hostent host_result;
		char host_buffer[FNS_NIS_SIZE];
		int host_error;
		struct hostent *hostentry = gethostbyname_r(
		    machine_name, &host_result, host_buffer,
		    FNS_NIS_SIZE, &host_error);
		if (hostentry == NULL)
			return (FN_E_CONFIGURATION_ERROR);

		// Get the IP address
		char **p;
		struct in_addr in;
		p = hostentry->h_addr_list;
		memcpy(&in.s_addr, *p, sizeof (in.s_addr));
		strcpy(ip_addr_buf, inet_ntoa(in));
		ip_address = ip_addr_buf;
	}


	// Obtain mutex lock
	mutex_lock(&domainname_lock);
	if (nis_domainname.check_domain(domain)) {
		mutex_unlock(&domainname_lock);
		return (FN_SUCCESS);
	}

	// If ip address is provided, then exteral binding has to be done
	if (ip_address) {
		if ((ret_error = yp_add_binding(domain, ip_address)) == 1) {
			if (nis_domainname.add_domain(domain) == 0)
				fns_error = FN_E_INSUFFICIENT_RESOURCES;
			mutex_unlock(&domainname_lock);
			return (fns_error);
		}
	} else if ((ret_error = yp_bind(domain)) == 0) {
		if (nis_domainname.add_domain(domain) == 0)
			fns_error = FN_E_INSUFFICIENT_RESOURCES;
		mutex_unlock(&domainname_lock);
		return (fns_error);
	}
	fns_error = FNSP_nis_map_status(ret_error);
	mutex_unlock(&domainname_lock);
	return (fns_error);
}

// -----------------------------------
// Mapping YP errors to FNS errors
// -----------------------------------
unsigned
FNSP_nis_map_status(int yp_error)
{
#ifdef DEBUG
	if (yp_error != 0)
		fprintf(stderr, "YP Error: %s\n",
		    yperr_string(ypprot_err(yp_error)));
#endif
	switch (yp_error) {
	case 0:
		return (FN_SUCCESS);
	case YPERR_ACCESS:
		return (FN_E_CTX_NO_PERMISSION);
	case YPERR_BADARGS:
		return (FN_E_ILLEGAL_NAME);
	case YPERR_BUSY:
		return (FN_E_CTX_UNAVAILABLE);
	case YPERR_KEY:
	case YPERR_MAP:
	case YPERR_NOMORE:
		return (FN_E_NAME_NOT_FOUND);
	case YPERR_DOMAIN:
	case YPERR_NODOM:
	case YPERR_BADDB:
		return (FN_E_CONFIGURATION_ERROR);
	case YPERR_RESRC:
		return (FN_E_INSUFFICIENT_RESOURCES);
	case YPERR_PMAP:
	case YPERR_RPC:
	case YPERR_YPBIND:
	case YPERR_YPERR:
	case YPERR_YPSERV:
	case YPERR_VERS:
		return (FN_E_COMMUNICATION_FAILURE);
	}
}

// ------------------------------------------
// Dynamically adding maps to NIS,
// by modifying the Makefile for FNS
// ------------------------------------------
#define	MAKE_FILE "/Makefile"
#define	MAKEFILE_0 "\n%s.time : %s.pag\n"
#define	MAKEFILE_1 "\t-@if [ -f /var/yp/$(DOM)/%s.pag ]; then \\\n"
#define	MAKEFILE_6 "\t\tif [ ! $(NOPUSH) ]; then \\\n"
#define	MAKEFILE_7 "\t\t\t$(YPPUSH) %s; \\\n"
#define	MAKEFILE_8 "\t\t\techo \"pushed %s\"; \\\n"
#define	MAKEFILE_9 "\t\telse \\\n"
#define	MAKEFILE_10 "\t\t: ; \\\n"
#define	MAKEFILE_11 "\t\tfi \\\n"
#define	MAKEFILE_12 "\telse \\\n"
#define	MAKEFILE_13 "\t\techo \"couldn't find %s\"; \\\n"
#define	MAKEFILE_14 "\tfi\n"

unsigned
FNSP_update_makefile(const char *mapfile)
{
	char makefile[FNS_NIS_INDEX];
	char tempfile[FNS_NIS_INDEX];
	char line[FNS_NIS_SIZE], temp[FNS_NIS_SIZE], *ret, *tp;
	FILE *rf, *wf;

	// First obtain the domainname from variable "mapfile"
	// It is of the form /var/fn/'domainname'/mapfile
	char domain[FNS_NIS_INDEX];
	const char *start;
	start = mapfile + strlen(FNSP_nis_map_dir);
	int ptr = 0;
	while (start[ptr] != '/')
		ptr++;
	strncpy(domain, start, ptr);
	domain[ptr] = '\0';

	// Obtain map name from "mapfile".
	char map[FNS_NIS_INDEX];
	start = mapfile + strlen(FNSP_nis_map_dir) + strlen(domain) + 2;
	strcpy(map, start);

	// Construct the path for the "Makefile"
	strcpy(makefile, FNSP_nis_dir);
	strcat(makefile, domain);
	strcat(makefile, MAKE_FILE);
	strcpy(tempfile, makefile);
	strcat(tempfile, ".tmp");
	if ((rf = fopen(makefile, "r")) == NULL)
		return (FN_E_INSUFFICIENT_RESOURCES);
	if ((wf = fopen(tempfile, "w")) == NULL)
		return (FN_E_INSUFFICIENT_RESOURCES);

	// Update the *all* line
	while (fgets(line, sizeof (line), rf)) {
		if (FNSP_match_map_index(line, "all:")) {
			// Check if the map name already exists
			strcpy(temp, line);
			ret = strparse(temp, map, &tp);
			if (strcmp(ret, line) == 0) {
				if (iscntrl(line[strlen(line) - 1]))
					line[strlen(line) - 1] = '\0';
				fprintf(wf, "%s %s.time\n",
				    line, map);
			} else {
				unlink(tempfile);
				fclose(rf);
				fclose(wf);
				return (FN_SUCCESS);
			}
		} else
			fputs(line, wf);
	}

	fprintf(wf, MAKEFILE_0
	    MAKEFILE_1
	    MAKEFILE_6
	    MAKEFILE_7
	    MAKEFILE_8
	    MAKEFILE_9
	    MAKEFILE_10
	    MAKEFILE_11
	    MAKEFILE_12
	    MAKEFILE_13
	    MAKEFILE_14,
	    map, map, map, map, map, map);

	fclose(rf);
	fclose(wf);
	if (rename(tempfile, makefile) < 0)
		return (FN_E_INSUFFICIENT_RESOURCES);
	else
		return (FN_SUCCESS);
}

// --------------------------------------------
// File maipulation routines for NIS maps
// This routine can be used to selectively
// insert, delete, store and modify an entry
// in the map speficied
// --------------------------------------------
static unsigned
FNSP_is_update_valid(char *domain, char *map)
{
	char mapname[FNS_NIS_INDEX];

	// Check if map is valid for update ie.,
	// neither passwd not hosts table
	if ((strncasecmp(map, FNSP_passwd_map,
	    strlen(FNSP_passwd_map)) == 0) ||
	    (strncasecmp(map, FNSP_hosts_map,
	    strlen(FNSP_hosts_map)) == 0))
		return (FN_E_CTX_NO_PERMISSION);

	// Check for *root* permissions
	uid_t pid = geteuid();
	if (pid != 0)
		return (FN_E_CTX_NO_PERMISSION);

	struct stat buf;
	strcpy(mapname, FNSP_nis_dir);
	strcat(mapname, domain);
	strcat(mapname, MAKE_FILE);
	if (stat(mapname, &buf) != 0)
		return (FN_E_CTX_NO_PERMISSION);
	else
		return (FN_SUCCESS);
}

static int
FNSP_get_number_of_lines(const char *mapfile)
{
	// Get the domain name
	char domain[FNS_NIS_INDEX];
	const char *start;
	start = mapfile + strlen(FNSP_nis_map_dir);
	int prt = 0;
	while (start[prt] != '/')
		prt++;
	strncpy(domain, start, prt);
	domain[prt] = '\0';

	int count = 0;
	FILE *rf;
	char count_file[FNS_NIS_INDEX], *num;
	char line[FNS_NIS_SIZE];

	// Construnct the count_file name
	strcpy(count_file, FNSP_nis_dir);
	strcat(count_file, domain);
	strcat(count_file, FNSP_count_file);
	if ((rf = fopen(count_file, "r")) == NULL) {
		if ((rf = fopen(count_file, "w")) == NULL)
			return (-1);
		fclose(rf);
		return (0);
	}

	while (fgets(line, sizeof (line), rf)) {
		if (FNSP_match_map_index(line, mapfile)) {
			num = line + strlen(mapfile);
			while (isspace(*num))
				num++;
			count = atoi(num);
		}
	}
	fclose(rf);
	return (count);
}

static unsigned
FNSP_set_number_of_lines(const char *mapfile, int count)
{
	// Get the domain name
	char domain[FNS_NIS_INDEX];
	const char *start;
	start = mapfile + strlen(FNSP_nis_map_dir);
	int prt = 0;
	while (start[prt] != '/')
		prt++;
	strncpy(domain, start, prt);
	domain[prt] = '\0';

	FILE *wf, *rf;
	char count_file[FNS_NIS_INDEX];
	char temp_file[FNS_NIS_INDEX];
	char line[FNS_NIS_SIZE];

	// Construnct the count_file name
	strcpy(count_file, FNSP_nis_dir);
	strcat(count_file, domain);
	strcat(count_file, FNSP_count_file);
	if ((rf = fopen(count_file, "r")) == NULL) {
		if ((wf = fopen(count_file, "w")) == NULL)
			return (FN_E_INSUFFICIENT_RESOURCES);
		fprintf(wf, "%s  %d\n", mapfile, count);
		fclose(wf);
		return (FN_SUCCESS);
	}
	strcpy(temp_file, count_file);
	strcat(temp_file, ".tmp");
	if ((wf = fopen(temp_file, "w")) == NULL) {
		fclose(rf);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	int found = 0;
	while (fgets(line, sizeof (line), rf)) {
		if ((!found) &&
		    (FNSP_match_map_index(line, mapfile))) {
			found = 1;
			fprintf(wf, "%s  %d\n", mapfile, count);
		} else
			fputs(line, wf);
	}
	if (!found)
		fprintf(wf, "%s  %d\n", mapfile, count);
	fclose(rf);
	fclose(wf);
	if (rename(temp_file, count_file) < 0)
		return (FN_E_INSUFFICIENT_RESOURCES);
	return (FN_SUCCESS);
}

unsigned
FNSP_compose_next_map_name(char *map)
{
	int ctx = 0;

	// Check if the last char are of FNSP_map_suffix
	size_t length = strlen(map) - strlen(FNSP_map_suffix);
	if (strcmp(&map[length], FNSP_map_suffix) == 0) {
		map[length] = '\0';
		ctx = 1;
	} else {
		length = strlen(map) - strlen(FNSP_attr_suffix);
		map[length] = '\0';
	}

	if (!isdigit(map[strlen(map) - 1])) {
		strcat(map, "_0");
		if (ctx)
			strcat(map, FNSP_map_suffix);
		else
			strcat(map, FNSP_attr_suffix);
		return (FN_SUCCESS);
	}

	char num[FNS_NIS_INDEX];
	int map_num;
	int i = strlen(map) - 1;
	while (map[i] != '_') i--;
	strcpy(num, &map[i+1]);
	map_num = atoi(num) + 1;
	if (ctx)
		sprintf(&map[i+1], "%d%s", map_num, FNSP_map_suffix);
	else
		sprintf(&map[i+1], "%d%s", map_num, FNSP_attr_suffix);
	return (FN_SUCCESS);
}

static unsigned
FNSP_insert_last_modified_key(DBM *db)
{
	datum dbm_key, dbm_value;
	dbm_key.dptr = FNSP_yp_last_modified;
	dbm_key.dsize = strlen(FNSP_yp_last_modified);

	// Get time of the day
	struct timeval time;
	gettimeofday(&time, NULL);
	char timeofday[FNS_NIS_INDEX];
	sprintf(timeofday, "%ld", time.tv_sec);
	dbm_value.dptr = timeofday;
	dbm_value.dsize = strlen(timeofday);

	if (dbm_store(db, dbm_key, dbm_value, DBM_REPLACE) != 0)
		return (FN_E_INSUFFICIENT_RESOURCES);
	return (FN_SUCCESS);
}


static unsigned
FNSP_fast_update_map(const char *domain, const char *map,
    const char *index, const void *data)
{
	// Compose the map name
	char mapfile[FNS_NIS_INDEX];
	int lines;
	strcpy(mapfile, FNSP_nis_map_dir);
	strcat(mapfile, domain);
	strcat(mapfile, "/");
	strcat(mapfile, map);
	while ((lines = FNSP_get_number_of_lines(mapfile)) > FNS_MAX_ENTRY)
		FNSP_compose_next_map_name(mapfile);

	int dbm_ret;
	DBM *db;
	datum dbm_key, dbm_value;
	dbm_key.dptr = (char *) index;
	dbm_key.dsize = strlen(index);
	dbm_value.dptr = (char *) data;
	dbm_value.dsize = strlen((char *) data);

	unsigned status = FN_SUCCESS;
	if ((db = dbm_open(mapfile, O_RDWR | O_CREAT, 0644)) != 0) {
		dbm_ret = dbm_store(db, dbm_key, dbm_value, DBM_INSERT);
		if (dbm_ret == 0) {
			if (lines == 0) {
				FNSP_update_makefile(mapfile);
				// Insert the YP_MASTER_NAME key
				char hostname[MAXHOSTNAMELEN];
				sysinfo(SI_HOSTNAME, hostname, MAXHOSTNAMELEN);
				dbm_key.dptr = FNSP_yp_master;
				dbm_key.dsize = strlen(FNSP_yp_master);
				dbm_value.dptr = hostname;
				dbm_value.dsize = strlen(hostname);
				dbm_ret = dbm_store(db, dbm_key,
				    dbm_value, DBM_INSERT);
				if (dbm_ret != 0)
					status = FN_E_INSUFFICIENT_RESOURCES;
			}
			if (status == FN_SUCCESS) {
				FNSP_set_number_of_lines(mapfile, lines+1);
				status = FNSP_insert_last_modified_key(db);
			}
		} else if (dbm_ret == 1)
			status = FN_E_NAME_IN_USE;
		else
			status = FN_E_INSUFFICIENT_RESOURCES;
		dbm_close(db);
	} else
		status = FN_E_INSUFFICIENT_RESOURCES;
	return (status);
}

static unsigned
FNSP_local_update_map(const char *domain, const char *map,
    const char *index, const void *data,
    FNSP_map_operation op)
{
	unsigned status;

	// Check if update is valid
	if ((status = FNSP_is_update_valid((char *) domain,
	    (char *) map)) != FN_SUCCESS)
		return (status);

	// Lock FNS update map
	int lock_fs;
	char lock_file[FNS_NIS_INDEX];
	strcpy(lock_file, FNSP_nis_dir);
	strcat(lock_file, domain);
	strcat(lock_file, FNSP_lock_file);
	if ((lock_fs = open(lock_file, O_WRONLY)) == -1)
		return (FN_E_INSUFFICIENT_RESOURCES);
	if (lockf(lock_fs, F_LOCK, 0L) == -1) {
		close(lock_fs);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	int count, dbm_ret;
	struct stat buffer;
	char mapfile[FNS_NIS_INDEX], dbmfile[FNS_NIS_INDEX];
	DBM *db;
	datum dbm_key, dbm_value, dbm_val;

	dbm_key.dptr = (char *) index;
	dbm_key.dsize = strlen(index);

	strcpy(mapfile, FNSP_nis_map_dir);
	strcat(mapfile, domain);
	strcat(mapfile, "/");
	strcat(mapfile, map);
	strcpy(dbmfile, mapfile);
	strcat(dbmfile, ".dir");
	dbm_ret = 1;
	while (stat(dbmfile, &buffer) == 0) {
		if ((db = dbm_open(mapfile, O_RDWR, 0644)) == 0) {
			lockf(lock_fs, F_ULOCK, 0L); close(lock_fs);
			return (FN_E_INSUFFICIENT_RESOURCES);
		}
		switch (op) {
		case FNSP_map_store:
		case FNSP_map_modify:
			dbm_val = dbm_fetch(db, dbm_key);
			if (dbm_val.dptr != 0) {
				dbm_value.dptr = (char *) data;
				dbm_value.dsize = strlen((char *) data);
				dbm_ret = dbm_store(db, dbm_key, dbm_value,
				    DBM_REPLACE);
				if (dbm_ret != 0) {
					status = FN_E_INSUFFICIENT_RESOURCES;
					dbm_ret = 0;
				} else
					status =
					    FNSP_insert_last_modified_key(db);
			}
			break;
		case FNSP_map_delete:
			dbm_ret = dbm_delete(db, dbm_key);
			if (dbm_ret == 0) {
				count = FNSP_get_number_of_lines(mapfile);
				FNSP_set_number_of_lines(mapfile, count-1);
				status = FNSP_insert_last_modified_key(db);
			}
			break;
		case FNSP_map_insert:
			dbm_val = dbm_fetch(db, dbm_key);
			if (dbm_val.dptr != 0) {
				dbm_ret = 0;
				status = FN_E_NAME_IN_USE;
			}
			break;
		}
		dbm_close(db);
		if (!dbm_ret) {
			// Found entry
			lockf(lock_fs, F_ULOCK, 0L); close(lock_fs);
			return (status);
		}
		FNSP_compose_next_map_name(mapfile);
		strcpy(dbmfile, mapfile);
		strcat(dbmfile, ".dir");
	}
	switch (op) {
	case FNSP_map_modify:
	case FNSP_map_delete:
		lockf(lock_fs, F_ULOCK, 0L);
		close(lock_fs);
		return (FN_E_NAME_NOT_FOUND);
	case FNSP_map_insert:
	case FNSP_map_store:
		break;
	}

	status = FNSP_fast_update_map(domain, map, index, data);
	lockf(lock_fs, F_ULOCK, 0L); close(lock_fs);
	return (status);
}

unsigned FNSP_update_map(const char *domain, const char *map,
    const char *index, const void *data,
    FNSP_map_operation op)
{
	// If the operation is store or modify, delete first
	switch (op) {
	case FNSP_map_store:
	case FNSP_map_modify:
		FNSP_update_map(domain, map, index, data,
		    FNSP_map_delete);
	default:
		break;
	}

	// Need to split the data to fit 1024 byte limitation
	char new_index[FNS_NIS_SIZE], next_index[FNS_NIS_SIZE];
	char new_data[FNS_NIS_SIZE];
	unsigned status, stat, count = 1;
	size_t length = 0;
	for (status = FNSP_get_first_index_data(op, index, data,
	    new_index, new_data, length, next_index);
	    status == FN_SUCCESS;
	    status = FNSP_get_next_index_data(op, index, data,
	    new_index, new_data, length, next_index)) {
		stat = FNSP_local_update_map(domain, map,
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
FNSP_yp_map_local(char *domain, char *map, char *map_index,
    char **mapentry, int *maplen)
{
	DBM *db;
	char mapfile[FNS_NIS_INDEX], dbmfile[FNS_NIS_INDEX];

	// Lock FNS file
	int lock_fs;
	char lock_file[FNS_NIS_INDEX];
	strcpy(lock_file, FNSP_nis_dir);
	strcat(lock_file, domain);
	strcat(lock_file, FNSP_lock_file);

	if ((lock_fs = open(lock_file, O_WRONLY)) == -1)
		return (FN_E_INSUFFICIENT_RESOURCES);
	if (lockf(lock_fs, F_LOCK, 0) == -1) {
		perror("Unable to obtain lock\n");
		close(lock_fs);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	datum dbm_key, dbm_value;
	dbm_key.dptr = map_index;
	dbm_key.dsize = strlen(map_index);
	strcpy(mapfile, FNSP_nis_map_dir);
	strcat(mapfile, domain);
	strcat(mapfile, "/");
	strcat(mapfile, map);
	strcpy(dbmfile, mapfile);
	strcat(dbmfile, ".dir");
	while ((db = dbm_open(mapfile, O_RDONLY, 0644))) {
		dbm_value = dbm_fetch(db, dbm_key);
		if (dbm_value.dptr != 0) {
			*maplen = dbm_value.dsize;
			*mapentry = (char *) malloc(*maplen);
			strncpy(*mapentry, dbm_value.dptr, (*maplen));
			dbm_close(db);
			lockf(lock_fs, F_ULOCK, 0L); close(lock_fs);
			return (FN_SUCCESS);
		}
		dbm_close(db);
		FNSP_compose_next_map_name(mapfile);
		strcpy(dbmfile, mapfile);
		strcat(dbmfile, ".dir");
	}
	lockf(lock_fs, F_ULOCK, 0L);
	close(lock_fs);
	return (FN_E_NAME_NOT_FOUND);
}

static unsigned
FNSP_local_yp_map_lookup(char *domain, char *map, char *map_index,
    int len, char **mapentry, int *maplen)
{
	int yperr;
	unsigned status;
	char mapfile[FNS_NIS_INDEX];

	strcpy(mapfile, map);
	if (FNSP_is_update_valid(domain, mapfile) == FN_SUCCESS)
		return (FNSP_yp_map_local(domain, mapfile, map_index,
		    mapentry, maplen));

	mutex_lock(&domainname_lock);
	if (!nis_domainname.check_domain(domain)) {
		mutex_unlock(&domainname_lock);
		if ((status = FNSP_nis_bind(domain)) != FN_SUCCESS)
			return (status);
	} else
		mutex_unlock(&domainname_lock);

	while ((yperr = yp_match((char *) domain, mapfile, map_index,
	    len, mapentry, maplen)) == YPERR_KEY)
		FNSP_compose_next_map_name(mapfile);
	return (FNSP_nis_map_status(yperr));
}

class FNSP_nis_individual_entry {
public:
	char *mapentry;
	size_t maplen;
	FNSP_nis_individual_entry *next;
};

unsigned
FNSP_yp_map_lookup(char *domain, char *map, char *map_index,
    int /* len */, char **mapentry, int *maplen)
{
	char new_index[FNS_NIS_SIZE], next_index[FNS_NIS_SIZE];
	unsigned status, stat;
	FNSP_nis_individual_entry *prev_entry = 0, *new_entry;
	FNSP_nis_individual_entry *entries = 0;

	for (status = FNSP_get_first_lookup_index(map_index,
	    new_index, next_index); status == FN_SUCCESS;
	    status = FNSP_get_next_lookup_index(new_index, next_index,
	    *mapentry, *maplen)) {
		stat = FNSP_local_yp_map_lookup(domain, map, new_index,
		    strlen(new_index), mapentry, maplen);
		if (stat != FN_SUCCESS)
			break;
		new_entry = (FNSP_nis_individual_entry *)
		    malloc(sizeof(FNSP_nis_individual_entry));
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
	memset((void *) (*mapentry), 0, sizeof(char)*(length + 1));
	new_entry = entries;
	while (new_entry) {
		if (new_entry == entries)
			strncpy((*mapentry), new_entry->mapentry,
			    new_entry->maplen);
		else
			strncat((*mapentry), new_entry->mapentry,
			    new_entry->maplen);
		new_entry = new_entry->next;
	}
	(*mapentry)[sizeof(char)*length] = '\0';

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
	
// Structure to hold information about FNS installation
// The value of installation is as follows
// status == 2 (unknown)
// status == 1 (fns installed)
// status == 0 (fns not installed)
#define	FNS_NIS_UNKNOWN_INSTALLTION 2
#define	FNS_NIS_INSTALLED 1
#define	FNS_NIS_NOT_INSTALLED 0

class FNSP_fns_nis_installation : public FNSP_nis_domainname {
protected:
	unsigned status;
public:
	FNSP_fns_nis_installation();
	~FNSP_fns_nis_installation();
	int add_nis_installation(char *domain, unsigned stat);
	unsigned check_nis_installation(char *domain);
};

FNSP_fns_nis_installation::FNSP_fns_nis_installation()
: FNSP_nis_domainname()
{
}

FNSP_fns_nis_installation::~FNSP_fns_nis_installation()
{
}

int
FNSP_fns_nis_installation::add_nis_installation(char *domain,
    unsigned stat)
{
	if (!domainname) {
		status = stat;
		return (add_domain(domain));
	}

	if (!next) {
		next = new FNSP_fns_nis_installation;
		if (next == 0)
			return (0);
	}
	return (((FNSP_fns_nis_installation *)
	    next)->add_nis_installation(domain, stat));
}

unsigned
FNSP_fns_nis_installation::check_nis_installation(char *domain)
{
	if (check_domain(domain))
		return (status);
	if (next)
		return (((FNSP_fns_nis_installation *)
		    next)->check_nis_installation(domain));
	return (FNS_NIS_UNKNOWN_INSTALLTION);
}

// Cache the domainname with info about FNS installation
static mutex_t fns_installation_lock = DEFAULTMUTEX;
static FNSP_fns_nis_installation install_domainnames;

int
FNSP_is_fns_installed(const FN_ref_addr *caddr)
{
	FN_string index_name, table_name;
	FN_string *map, *domain;
	unsigned status;

	FN_string *iname = FNSP_address_to_internal_name(*caddr);
	if (FNSP_decompose_nis_index_name(*iname, table_name,
	    index_name)) {
		status = split_internal_name(table_name, &map, &domain);
		if (status != FN_SUCCESS)
			return (0);
		delete map;
	} else {
		char d_name[FNS_NIS_INDEX], *chr_ptr;
		strcpy(d_name, (char *) iname->str());
		if ((chr_ptr = strchr(d_name, ' ')) == NULL)
			domain = new FN_string(*iname);
		else {
			chr_ptr[0] = '\0';
			domain = new FN_string(
			    (unsigned char *) d_name);
			FNSP_nis_bind((char *) iname->str());
		}
	}
	delete iname;

	// Check the cache for FNS installation
	mutex_lock(&fns_installation_lock);
	status = install_domainnames.check_nis_installation(
	    (char *) domain->str());
	mutex_unlock(&fns_installation_lock);
	if (status != FNS_NIS_UNKNOWN_INSTALLTION) {
		delete domain;
		return (status);
	}

	// Reset status value
	status = 0;

	char mapname[FNS_NIS_INDEX], *nis_master;
	int yperr;
	strcpy(mapname, FNSP_org_map);
	if ((yperr = yp_master((char *) domain->str(),
	    mapname, &nis_master)) == 0) {
		free(nis_master);
		status = FN_SUCCESS;
	} else {
		// Check for root ID
		if (geteuid() != 0) {
			delete domain;
			return (status);
		}

		// Check if the make file exists
		char tname[FNS_NIS_INDEX];
		strcpy(tname, FNSP_nis_dir);
		strcat(tname, (char *) domain->str());
		strcat(tname, MAKE_FILE);
		struct stat buffer;
		if (stat(tname, &buffer) == 0)
			status = FN_SUCCESS;
	}

	// Update the cache
	mutex_lock(&fns_installation_lock);
	install_domainnames.add_nis_installation((char *)
	    domain->str(), status);
	mutex_unlock(&fns_installation_lock);

	delete domain;
	return (status);
}
