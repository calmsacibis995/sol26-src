/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)dis.c	1.3	96/09/10 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/auxv.h>
#include <libelf.h>
#include <sys/param.h>
#include <stdarg.h>

#include "rdb.h"
#include "disasm.h"


/*
 * I don't link this global but it's a work-around for the
 * poor disassemble interface for now.
 */
static struct ps_prochandle *	cur_ph;

/*
 * This routine converts 'address' into it's closest symbol
 * representation.
 *
 * The following flags are used to effect the output:
 *
 *	FLG_PAP_SONAME
 *		embed the SONAME in the symbol name
 *	FLG_PAP_NOHEXNAME
 *		if no symbol found return a null string
 *		If this flag is not set return a string displaying
 *		the 'hex' value of address.
 */
char *
print_address_ps(struct ps_prochandle * ph, unsigned address, unsigned flags)
{
	static char	buf[256];
	Elf32_Sym	sym;
	if (addr_to_sym(ph, (ulong_t)address, &sym) == RET_OK) {
		map_info_t *	mip;
		if (flags & FLG_PAP_SONAME) {
			/*
			 * Embed SOName in symbol name
			 */
			if (mip = addr_to_map(ph, address)) {
				strcpy(buf, mip->mi_name);
				strcat(buf, ":");
			} else
				sprintf(buf, "0x%08x:", address);
		} else
			buf[0] = '\0';

		if (sym.st_value < address)
			sprintf(buf, "%s%s+0x%x", buf, sym.st_name,
				address - sym.st_value);
		else
			sprintf(buf, "%s%s", buf, sym.st_name);
	} else {
		if (flags & FLG_PAP_NOHEXNAME)
			buf[0] = '\0';
		else
			sprintf(buf, "0x%x", address);
	}
	return (buf);
}


char *
print_address(unsigned int address)
{
	return (print_address_ps(cur_ph, address, FLG_PAP_SONAME));
}

retc_t
disasm_addr(struct ps_prochandle * ph, ulong_t addr, int num_inst)
{
	ulong_t 	offset;
	ulong_t 	end;

	for (offset = (unsigned int)addr,
	    end = (unsigned int)addr + num_inst * 4;
	    offset < end; offset += 4) {
		char *		instr_str;
		unsigned int	instr;

		if (ps_pread(ph, offset, (char *)&instr,
		    sizeof (unsigned)) != PS_OK)
			perror("da: ps_pread");

		cur_ph = ph;
		instr_str = disassemble(instr, offset, print_address,
			0, 0, V8_MODE);

		printf("%-30s: %s\n", print_address(offset), instr_str);
	}
	return (RET_OK);
}

void
disasm(struct ps_prochandle * ph, int num_inst)
{
	prstatus_t	pstat;

	if (ioctl(ph->pp_fd, PIOCSTATUS, &pstat) != 0)
		perr("disasm: PIOCSTATUS");

	disasm_addr(ph, (ulong_t)pstat.pr_reg[R_PC], num_inst);
}
