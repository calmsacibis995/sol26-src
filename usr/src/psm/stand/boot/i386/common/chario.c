/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident "@(#)chario.c	1.5	96/04/08 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/bootlink.h>
#include <sys/machine.h>
#include <sys/salib.h>
#include "devtree.h"
#include "chario.h"

extern void serial_init();
extern void prop_free(struct dprop *, int);
extern int doint(void);
extern struct dprop *prop_alloc(int size);

int chario_mark(char *buf, int len, int flag);
int chario_parse_mode(int port, char *buf, int len);
int chario_mark_one(char *name, int flag);

struct _chario_storage_ {
	struct _chario_storage_		*next;
	void				*name;
	char				*value;
};

typedef struct _chario_storage_ _chario_storage_t, *_chario_storage_p;

_chario_storage_p chario_storage_head;
#define	NULL_STORAGE ((_chario_storage_p)0)

_chario_storage_p
chario_alloc(char *buf, int len, void *cookie)
{
	_chario_storage_p p;

	p = (_chario_storage_p)prop_alloc(sizeof (_chario_storage_t));
	if (p == (_chario_storage_p)0) {
		printf("Failed to alloc storage for %s\n", cookie);
		return (NULL_STORAGE);
	}
	p->name = cookie;
	p->value = (char *)prop_alloc(len + 1);
	if (p->value == (char *)0) {
		printf("Failed to alloc storage for %s\n", buf);
		prop_free((struct dprop *)p, sizeof (_chario_storage_t));
		return (NULL_STORAGE);
	}
	bcopy(buf, p->value, len);
	if (chario_storage_head)
		p->next = chario_storage_head;
	chario_storage_head = p;

	return (p);
}

chario_getprop(char *buf, int len, void *cookie)
{
	_chario_storage_p p;
	int valuelen;

	/*
	 * First we must find the values associated with the requested
	 * cookie. If not found report it as an error.
	 */
	for (p = chario_storage_head; p; p = p->next)
		if (strcmp(cookie, p->name) == 0)
			break;

	if (p == NULL_STORAGE) {
		printf("Can't find prop %s\n", cookie);
		return (BOOT_FAILURE);
	}

	/*
	 * Actually copy the information if there's space.
	 */
	valuelen = strlen(p->value) + 1;
	if (buf) {
		if (valuelen > len) {
			printf("Not enough storage for %s\n", cookie);
			return (BOOT_FAILURE);
		}
		bcopy(p->value, buf, valuelen);
	}
	return (valuelen);
}

/*ARGSUSED*/
int
chario_put_dev(struct dnode *node, char *buf, int len, void *cookie)
{
	if (chario_alloc(buf, len, cookie) == NULL_STORAGE)
		return (BOOT_FAILURE);

	if (strcmp(cookie, "input-device") == 0) {
		return (chario_mark(buf, len, CHARIO_IN_ENABLE));
	} else if (strcmp(cookie, "output-device") == 0) {
		return (chario_mark(buf, len, CHARIO_OUT_ENABLE));
	}
	return (BOOT_SUCCESS);		/* XXX is this right? */
}

/*ARGSUSED*/
int
chario_get_dev(struct dnode *node, char *buf, int len, void *cookie)
{
	return (chario_getprop(buf, len, cookie));
}

/*ARGSUSED*/
int
chario_put_mode(struct dnode *node, char *buf, int len, void *cookie)
{
	int rtn;

	if (strcmp(cookie, "ttya-mode") == 0) {
		rtn = chario_parse_mode(0, buf, len);
	} else if (strcmp(cookie, "ttyb-mode") == 0) {
		rtn = chario_parse_mode(1, buf, len);
	}

	if ((rtn == BOOT_SUCCESS) &&
	    (chario_alloc(buf, len, cookie) == NULL_STORAGE))
		return (BOOT_FAILURE);

	return (BOOT_SUCCESS);

}

/*ARGSUSED*/
int
chario_get_mode(struct dnode *node, char *buf, int len, void *cookie)
{
	return (chario_getprop(buf, len, cookie));
}

int
chario_mark(char *buf, int len, int flag)
{
	_char_io_p p;
	char *vp, *vp1, *value;
	int current_set = 0, current_idx = 0, current_valid = 0;
	extern _char_io_t console;

	if ((value = (char *)prop_alloc(len + 1)) == 0) {
		printf("Can't alloc space for %s\n", buf);
		return (BOOT_FAILURE);
	} else {
		bcopy(buf, value, len);
	}

	/*
	 * clear the flag in all of the output devices. Only those devices
	 * that match the buf values will be enabled or created. Save the
	 * current devices which are enabled in case the input value does
	 * match anything so that we can reset the values.
	 */
	for (p = &console; p; p = p->next, current_idx++) {
		if (p->flags & flag)
			BSET(current_set, current_idx);
		p->flags &= ~flag;
	}

	vp = value;
	do {
		if ((vp1 = strchr(vp, ',')) != 0)
			*vp1++ = '\0';
		if (chario_mark_one(vp, flag) == 0)  {
			/* Failed to find device in current list, create it */
			if ((strncmp(vp, "ttya", 4) == 0) ||
			    (strncmp(vp, "com1", 4) == 0)) {
				serial_init(vp, 0,
					S9600|DATA_8|STOP_1|PARITY_NONE,
					flag);
				current_valid = 1;
			} else if ((strncmp(vp, "ttyb", 4) == 0) ||
			    (strncmp(vp, "com2", 4) == 0)) {
				serial_init(vp, 1,
					S9600|DATA_8|STOP_1|PARITY_NONE,
					flag);
				current_valid = 1;
			}
		} else {
			current_valid = 1;
		}
		vp = vp1;
	} while (vp);

	/*
	 * If we didn't find a valid device reset the structures to their
	 * orignal state
	 */
	if (!current_valid) {
		for (current_idx = 0, p = &console; p;
		    p = p->next, current_idx++)
			if (BISSET(current_set, current_idx))
				p->flags |= flag;
	}

	prop_free((struct dprop *)value, len + 1);
	return (BOOT_SUCCESS);
}

int
chario_mark_one(char *name, int flag)
{
	_char_io_p p;
	extern _char_io_t console;

	for (p = &console; p; p = p->next)  {
		if (strcmp(name, p->name) == 0) {
			p->flags |= flag;
			return (1);
		}
	}
	return (0);
}

/*
 * Value of this string is in the form of "9600,8,n,1,-"
 * 1) speed: 9600, 4800, ...
 * 2) data bits
 * 3) parity: n(none), e(even), o(odd)
 * 4) stop bits
 * 5) handshake: -(none), h(hardware: rts/cts), s(software: xon/off)
 *    we don't support handshaking
 *
 * This parsing came from a SPARCstation eeprom.
 */
int
chario_parse_mode(int port, char *buf, int len)
{
	short port_vals;
	char *p, *p1, *values;
	extern struct int_pb	ic;

	if ((values = (char *)prop_alloc(len + 1)) == 0) {
		printf("Failed to alloc space for %s\n", buf);
		return (BOOT_FAILURE);
	} else {
		bcopy(buf, values, len);
	}
	p = values;

	/* ---- baud rate ---- */
	if ((p1 = strchr(p, ',')) != 0) {
		*p1++ = '\0';
	} else {
		goto error_exit;
	}
	switch (*p) {
		case '1':
			if (*(p + 1) == '1')
				port_vals = S110;
			else if (*(p + 1) == '5')
				port_vals = S150;
			else if (*(p + 1) == '2')
				port_vals = S1200;
			else
				goto error_exit;
			break;

		case '2':
			port_vals = S2400;
			break;

		case '3':
			port_vals = S300;
			break;

		case '4':
			port_vals = S4800;
			break;

		case '6':
			port_vals = S600;
			break;

		case '9':
			port_vals = S9600;
			break;
	}

	/* ---- Next item is data bits ---- */
	p = p1;
	if ((p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		goto error_exit;
	}
	switch (*p) {
		case '8': port_vals |= DATA_8; break;
		case '7': port_vals |= DATA_7; break;
		case '6': port_vals |= DATA_6; break;
		case '5': port_vals |= DATA_5; break;
		default:
			goto error_exit;
	}

	/* ---- Parity info ---- */
	p = p1;
	if ((p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		goto error_exit;
	}
	switch (*p)  {
		case 'n': port_vals |= PARITY_NONE; break;
		case 'e': port_vals |= PARITY_EVEN; break;
		case 'o': port_vals |= PARITY_ODD; break;
		default:
			goto error_exit;
	}

	/* ---- Find stop bits ---- */
	p = p1;
	if ((p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		goto error_exit;
	}
	if (*p == '1')
		port_vals |= STOP_1;
	else
		port_vals |= STOP_2;

	/* ---- handshake is next but we ignore it ---- */

	/* ---- Now setup the serial port with our values ---- */
	ic.ax = port_vals;
	ic.dx = (short)port;
	ic.intval = 0x14;

	(void) doint();

	prop_free((struct dprop *)values, len + 1);
	return (BOOT_SUCCESS);

error_exit:
	printf("Bad mode setting\n");
	prop_free((struct dprop *)values, len + 1);
	return (BOOT_FAILURE);
}
