/*
 * adb - PowerPC command parser
 */

#pragma ident	"@(#)command_ppc.c	1.2	96/06/18 SMI"

#include "adb.h"
#include <stdio.h>
#include "fio.h"
#include "fpascii.h"

#define	QUOTE	0200
#define	STRIP	0177

/* 
 * This file contains archecture specific extensions to the command parser
 * The following must be defined in this file:
 *
 *-----------------------------------------------------------------------
 * ext_slash:
 *   command extensions for /,?,*,= .
 *   ON ENTRY:
 *	cmd    - is the char format (ie /v would yeld v)
 *	buf    - pointer to the raw command buffer
 *	defcom - is the char of the default command (ie last command)
 *	eqcom  - true if command was =
 *	atcom  - true if command was @
 *	itype  - address space type (see adb.h)
 *	ptype  - symbol space type (see adb.h)
 *   ON EXIT:
 *      Will return non-zero if a command was found.
 *
 *-----------------------------------------------------------------------
 * ext_getstruct:
 *   return the address of the ecmd structure for extended commands (ie ::)
 *
 *-----------------------------------------------------------------------
 * ext_ecmd:
 *  extended command parser (ie :: commands)
 *   ON ENTRY:
 *	buf    - pointer to the extended command
 *   ON EXIT:
 *	Will exec a found command and return non-zero
 *
 *-----------------------------------------------------------------------
 * ext_dol:
 *   command extensions for $ .
 *   ON ENTRY:
 *	modif    - is the dollar command (ie $b would yeld b)
 *   ON EXIT:
 *      Will return non-zero if a command was found.
 *
 */


/* This is archecture specific extensions to the slash cmd.
 * On return this will return non-zero if there was a cmd!
 */
int
ext_slash(cmd, buf, defcom, eqcom, atcom, itype, ptype)
char	cmd;
char	*buf, defcom;
int	eqcom, atcom, itype, ptype;
{
	int	savdot;
	char wformat[1];
	
	switch(cmd) 
	{
				/* write chars to memory */
			case 'v':
				wformat[0] = 'B'; (void) expr(1);
				do {
				    long longvalue;

				    savdot = dot;
				    psymoff(dot, ptype, ":%16t");
				    (void) exform(1,wformat,itype,ptype);
				    errflg = 0; dot = savdot;
				    longvalue = get(dot, itype);
				    errflg = 0;
				    *(char*)&longvalue = (char)(expv);
				    put(dot, itype, longvalue);
				    savdot = dot;
				    printf("=%8t");
				    (void) exform(1,wformat,itype,ptype);
				    newline();
				} while (expr(0) && errflg == 0);
				dot = savdot;
				chkerr();
				break;
			default:
				return (0);
	}
	return (1);
}

int datalen;

/* breakpoints */
struct	bkpt *bkpthead;


/* This is archecture specific extensions to the $ cmd.
 * On return this will return non-zero if there was a cmd!
 */
int
ext_dol(modif)
int	modif;
{
	switch(modif) {
#ifdef KADB
	case 'l':
		if (hadaddress)
		        if (address == 1 || address == 2 || address == 4)
				datalen = address;
			else
				error("bad data length");
		else
			datalen = 1;
		break;
#endif
	case 'b': {
		register struct bkpt *bkptr;

		printf("breakpoints\ncount%8tbkpt%24ttype%34tlen%40tcommand\n");
		for (bkptr = bkpthead; bkptr; bkptr = bkptr->nxtbkpt)
			if (bkptr->flag) {
		   		printf("%-8.8d", bkptr->count);
				psymoff(bkptr->loc, ISYM, "%24t");
				switch (bkptr->type) {
				case BPINST:
					printf(":b instr");
					break;
				case BPDBINS:
					printf(":p exec");
					break;
				case BPACCESS:
					printf(":a rd/wr");
					break;
				case BPWRITE:
					printf(":w write");
					break;
		    		}
				printf("%34t%-5D %s", bkptr->len, bkptr->comm);
			}
		}
		break;
	default:
		return (0);
	}
	return (1);
}

/*
 * extended commands
 */

static
ecmd_call()
{
#ifdef KADB
	unsigned long	args[20],retval;
	int	i;
	int	(*func)();

	if (hadaddress)
		func = (int(*)())address;
	else
		func = (int(*)())dot;

	for(i=0; i<20; i++)
	{
		if(expr(0))
			args[i] = expv;
		else
			break;
		if (rdc() != ',')
			break;
	}
	retval = (*func)(args[0],
		       args[1],
		       args[2],
		       args[3],
		       args[4],
		       args[5],
		       args[6],
		       args[7],
		       args[8],
		       args[9],
		       args[10],
		       args[11],
		       args[12],
		       args[13],
		       args[14],
		       args[15],
		       args[16],
		       args[17],
		       args[18],
		       args[19]
		);
	printf("%X=%X(%X,%X,%X...)\n",retval,func,args[0],args[1],args[2]);
#else
	printf("Not supported on adb\n");
#endif
}

static struct ecmd ppc_ecmd[];

/*
 * This returns the address of the ext_extended command structure
 */
struct ecmd *
ext_getstruct()
{
	return (ppc_ecmd);
}

/* This is archecture specific extensions to extended cmds.
 * On return this will return non-zero if there was a cmd!
 */
int
ext_ecmd(buf)
char *buf;
{
	int i;

	i = extend_scan(buf, ppc_ecmd);
	if (i >= 0) 
	{
		(*ppc_ecmd[i].func)();
		return (1);
	}

	return (0);
}	

/*
 * all avail extended commands should go here
 */
static struct ecmd ppc_ecmd[] = {
{ "call", ecmd_call, "call the named function\n"},
{ 0 }
};
