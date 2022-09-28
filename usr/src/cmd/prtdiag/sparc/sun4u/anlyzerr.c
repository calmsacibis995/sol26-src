/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)anlyzerr.c 1.8	 96/02/20 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "anlyzerr.h"

#define	RESERVED_STR	"Reserved"

#define	MAX_PARTS	5
#define	MAX_FRUS	5

#define	MAXSTRLEN	256

/* Define special bits */
#define	UPA_PORT_A	0x1
#define	UPA_PORT_B	0x2

static int disp_parts(char **, u_longlong_t, int);

/*
 * These defines comne from async.h, but it does not get exported from
 * uts/sun4u/sys, so they must be redefined.
 */
#define	P_AFSR_ISAP	0x0000000040000000ULL /* incoming addr. parity err */
#define	P_AFSR_ETP	0x0000000020000000ULL /* ecache tag parity */
#define	P_AFSR_ETS	0x00000000000F0000ULL /* cache tag parity syndrome */
#define	ETS_SHIFT	16

/* List of parts possible */
#define	RSVD_PART	1
#define	UPA_PART	2
#define	UPA_A_PART	3
#define	UPA_B_PART	4
#define	SOFTWARE_PART	5
#define	AC_PART		6
#define	AC_ANY_PART	7
#define	DTAG_PART	8
#define	DTAG_A_PART	9
#define	DTAG_B_PART	10
#define	FHC_PART	11
#define	BOARD_PART	12
#define	BOARD_ANY_PART	13
#define	BOARD_CONN_PART	14
#define	BACK_PIN_PART	15
#define	BACK_TERM_PART	16
#define	CPU_PART	17

/* List of possible parts */
static char *part_str[] = {
	{ "" },			/* 0, a placeholder for indexing */
	{ "" },			/* 1, reserved strings shouldn't be printed */
	{ "UPA devices" },					/* 2 */
	{ "UPA Port A device" },				/* 3 */
	{ "UPA Port B device" },				/* 4 */
	{ "Software error" },					/* 5 */
	{ "Address Controller" },				/* 6 */
	{ "Undetermined Address Controller in system" },	/* 7 */
	{ "Data Tags" },					/* 8 */
	{ "Data Tags for UPA Port A" },				/* 9 */
	{ "Data Tags for UPA Port B" },				/* 10 */
	{ "Firehose Controller" },				/* 11 */
	{ "This Board" },					/* 12 */
	{ "Undetermined Board in system" },			/* 13 */
	{ "Board Connector" },					/* 14 */
	{ "Centerplane pins " },				/* 15 */
	{ "Centerplane terminators" },				/* 16 */
	{ "CPU" },						/* 17 */
};

/* Ecache parity error messages. Tells which bits are bad. */
static char *ecache_parity[] = {
	{ "Bits 7:0 " },
	{ "Bits 15:8 " },
	{ "Bits 21:16 " },
	{ "Bits 24:22 " }
};


struct ac_error {
	char *error;
	int part[MAX_PARTS];
};

typedef struct ac_error ac_err;

/*
 * Hardware error register meanings, failed parts and FRUs. The
 * following strings are indexed for the bit positions of the
 * corresponding bits in the hardware. The code checks bit x of
 * the hardware error register and prints out string[x] if the bit
 * is turned on.
 *
 * This database of parts which are probably failed and which FRU's
 * to replace was based on knowledge of the Sunfire Programmers Spec.
 * and discussions with the hardware designers. The order of the part
 * lists and consequently the FRU lists are in the order of most
 * likely cause first.
 */
static ac_err ac_errors[] = {
	{							/* 0 */
		{ "UPA Port A Error" },
		{ UPA_A_PART, 0, 0, 0, 0 },
	},
	{							/* 1 */
		{ "UPA Port B Error" },
		{ UPA_B_PART, 0, 0, 0, 0 },
	},
	{							/* 2 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 3 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 4 */
		{ "UPA Interrupt to unmapped destination" },
		{ BOARD_PART, 0, 0, 0, 0 },
	},
	{							/* 5 */
		{ "UPA Non-cacheable write to unmapped destination" },
		{ BOARD_PART, 0, 0, 0, 0 },
	},
	{							/* 6 */
		{ "UPA Cacheable write to unmapped destination" },
		{ BOARD_PART, 0, 0, 0, 0 },
	},
	{							/* 7 */
		{ "Illegal Write Received" },
		{ BOARD_PART, 0, 0, 0, 0 },
	},
	{							/* 8 */
		{ "Local Writeback match with line in state S" },
		{ AC_PART, DTAG_PART, 0, 0, 0 },
	},
	{							/* 9 */
		{ "Local Read match with valid line in Tags" },
		{ AC_PART, DTAG_PART, 0, 0, 0 },
	},
	{							/* 10 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 11 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 12 */
		{ "Tag and Victim were valid during lookup" },
		{ AC_PART, DTAG_PART, 0, 0, 0 },
	},
	{							/* 13 */
		{ "Local Writeback matches a victim in state S" },
		{ AC_PART, CPU_PART, 0, 0, 0 },
	},
	{							/* 14 */
		{ "Local Read matches valid line in victim buffer" },
		{ AC_PART, CPU_PART, 0, 0, 0 },
	},
	{							/* 15 */
		{ "Local Read victim bit set and victim is S state" },
		{ AC_PART, CPU_PART, 0, 0, 0 },
	},
	{							/* 16 */
		{ "Local Read Victim bit set and Valid Victim Buffer" },
		{ AC_PART, CPU_PART, 0, 0, 0 },
	},
	{							/* 17 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 18 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 19 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 20 */
		{ "UPA Transaction received in Sleep mode" },
		{ AC_PART, 0, 0, 0, 0 },
	},
	{							/* 21 */
		{ "P_FERR error P_REPLY received from UPA Port" },
		{ CPU_PART, AC_PART, 0, 0, 0 },
	},
	{							/* 22 */
		{ "Illegal P_REPLY received from UPA Port" },
		{ CPU_PART, AC_PART, 0, 0, 0 },
	},
	{							/* 23 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 24 */
		{ "Timeout on a UPA Master Port" },
		{ AC_ANY_PART, BOARD_ANY_PART, 0, 0, 0 },
	},
	{							/* 25 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 26 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 27 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 28 */
		{ "Coherent Transactions Queue Overflow Error" },
		{ BACK_PIN_PART, BOARD_CONN_PART, AC_PART, AC_ANY_PART, 0 },
	},
	{							/* 29 */
		{ "Non-cacheable Request Queue Overflow Error" },
		{ AC_PART, AC_ANY_PART, 0, 0, 0 },
	},
	{							/* 30 */
		{ "Non-cacheable Reply Queue Overflow Error" },
		{ AC_PART, 0, 0, 0, 0 },
	},
	{							/* 31 */
		{ "PREQ Queue Overflow Error" },
		{ CPU_PART, AC_PART, 0, 0, 0 },
	},
	{							/* 32 */
		{ "Foreign DID CAM Overflow Error" },
		{ AC_PART, AC_ANY_PART, 0, 0, 0 },
	},
	{							/* 33 */
		{ "FT->UPA Queue Overflow Error" },
		{ BACK_PIN_PART, BOARD_CONN_PART, AC_PART, AC_ANY_PART, 0 },
	},
	{							/* 34 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 35 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 36 */
		{ "UPA Port B Dtag Parity Error" },
		{ DTAG_B_PART, AC_PART, 0, 0, 0 },
	},
	{							/* 37 */
		{ "UPA Port A Dtag Parity Error" },
		{ DTAG_A_PART, AC_PART, 0, 0, 0 },
	},
	{							/* 38 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 39 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 40 */
		{ "UPA Bus Parity Error" },
		{ UPA_PART, AC_PART, 0, 0, 0 },
	},
	{							/* 41 */
		{ "Data ID Line Mismatch" },
		{ BACK_PIN_PART, BOARD_CONN_PART, AC_PART, 0, 0 },
	},
	{							/* 42 */
		{ "Arbitration Line Mismatch" },
		{ BACK_PIN_PART, BOARD_CONN_PART, AC_PART, 0, 0 },
	},
	{							/* 43 */
		{ "Shared Line Parity Mismatch" },
		{ BACK_PIN_PART, BOARD_CONN_PART, AC_PART, 0, 0 },
	},
	{							/* 44 */
		{ "FireTruck Control Line Parity Error" },
		{ AC_PART, BACK_PIN_PART, 0, 0, 0 },
	},
	{							/* 45 */
		{ "FireTruck Address Bus Parity Error" },
		{ AC_PART, BACK_PIN_PART, 0, 0, 0 },
	},
	{							/* 46 */
		{ "Internal RAM Parity Error" },
		{ AC_PART, 0, 0, 0, 0 },
	},
	{							/* 47 */
		{ NULL },
		{ RSVD_PART, 0, 0, 0, 0 },
	},
	{							/* 48 */
		{ "Internal Hardware Error" },
		{ AC_PART, 0, 0, 0, 0 },
	},
	{							/* 49 */
		{ "FHC Communications Error" },
		{ FHC_PART, AC_PART, 0, 0, 0 },
	},
	/* Bits 50-63 are reserved in this implementation. */
};


#define	MAX_BITS (sizeof (ac_errors)/ sizeof (ac_err))

/*
 * There are only two error bits in the DC shadow chain that are
 * important. They indicate an overflow error and a parity error,
 * respectively. The other bits are not error bits and should not
 * be checked for.
 */
#define	DC_OVERFLOW	0x2
#define	DC_PARITY	0x4

static char dc_overflow_txt[] = "Board %d DC %d Overflow Error";
static char dc_parity_txt[] = "Board %d DC %d Parity Error";

/* defines for the sysio */
#define	UPA_APERR	0x4

/*
 * Analysis functions:
 *
 * Most of the Fatal error data analyzed from error registers is not
 * very complicated. This is because the FRUs for errors detected by
 * most parts is either a CPU module, a FFB, or the system board
 * itself.
 * The analysis of the Address Controller errors is the most complicated.
 * These errors can be caused by other boards as well as the local board.
 */

/*
 * analyze_cpu
 *
 * Analyze the CPU MFSR passed in and determine what type of fatal
 * hardware errors occurred at the time of the crash. This function
 * returns a pointer to a string to the calling routine.
 */
int
analyze_cpu(char **msgs, int cpu_id, u_longlong_t afsr)
{
	int count = 0;
	int i;
	int syndrome;
	char msgbuf[MAXSTRLEN];

	if (msgs == NULL) {
		return (count);
	}

	if (afsr & P_AFSR_ETP) {
		(void) sprintf(msgbuf, "CPU %d Ecache Tag Parity Error, ",
			cpu_id);

		/* extract syndrome for afsr */
		syndrome = (afsr & P_AFSR_ETS) >> ETS_SHIFT;

		/* now concat the parity syndrome msg */
		for (i = 0; i < 4; i++) {
			if ((0x1 << i)  & syndrome) {
				(void) strcat(msgbuf, ecache_parity[i]);
			}
		}
		(void) strcat(msgbuf, "\n");
		*msgs++ = strdup(msgbuf);
		count++;
	}

	if (afsr & P_AFSR_ISAP) {
		(void) sprintf(msgbuf,
			"CPU %d Incoming System Address Parity Error\n",
			cpu_id);
		*msgs++ = strdup(msgbuf);
		count++;
	}

	return (count);
}

/*
 * analyze_ac
 *
 * This function checks the AC error register passed in and checks
 * for any errors that occured during the fatal hardware reset.
 */
int
analyze_ac(char **msgs, u_longlong_t ac_error)
{
	int i;
	int count = 0;
	char msgbuf[MAXSTRLEN];
	int tmp_cnt;

	if (msgs == NULL) {
		return (count);
	}

	for (i = 2; i < MAX_BITS; i++) {
		if ((((u_longlong_t) 0x1 << i) & ac_error) != 0) {
			if (ac_errors[i].error != NULL) {
				(void) sprintf(msgbuf, "AC: %s\n",
					ac_errors[i].error);
				*msgs++ = strdup(msgbuf);
				count++;

				/* display the part that might cause this */
				tmp_cnt = disp_parts(msgs, ac_error, i);
				count += tmp_cnt;
				msgs += tmp_cnt;
			}
		}
	}

	return (count);
}

/*
 * analyze_dc
 *
 * This routine checks the DC shdow chain and tries to determine
 * what type of error might have caused the fatal hardware reset
 * error.
 */
int
analyze_dc(int board, char **msgs, u_longlong_t dc_error)
{
	int i;
	int count = 0;
	char msgbuf[MAXSTRLEN];

	if (msgs == NULL) {
		return (count);
	}

	/*
	 * The DC scan data is contained in 8 bytes, one byte per
	 * DC. There are 8 DCs on a system board.
	 */

	for (i = 0; i < 8; i++) {
		if (dc_error & DC_OVERFLOW) {
			(void) sprintf(msgbuf, dc_overflow_txt, board, i);
			*msgs++ = strdup(msgbuf);
			count++;
		}

		if (dc_error & DC_PARITY) {
			(void) sprintf(msgbuf, dc_parity_txt, board, i);
			*msgs++ = strdup(msgbuf);
			count++;
		}
		dc_error = dc_error >> 8;	/* shift over to next byte */
	}

	return (count);
}

static int
disp_parts(char **msgs, u_longlong_t ac_error, int type)
{
	int count = 0;
	int part;
	char msgbuf[MAXSTRLEN];
	int i;

	if (msgs == NULL) {
		return (count);
	}

	(void) sprintf(msgbuf, "\tThe error could be caused by:\n");
	*msgs++ = strdup(msgbuf);
	count++;

	for (i = 0; (i < MAX_FRUS) && ac_errors[type].part[i]; i++) {
		part = ac_errors[type].part[i];

		if (part == UPA_PART) {
			if (ac_error & UPA_PORT_A) {
				part = UPA_A_PART;
			} else if (ac_error & UPA_PORT_B) {
				part = UPA_B_PART;
			}
		}

		if (part == DTAG_PART) {
			if (ac_error & UPA_PORT_A) {
				part = DTAG_A_PART;
			} else if (ac_error & UPA_PORT_B) {
				part = DTAG_B_PART;
			}
		}

		(void) sprintf(msgbuf, "\t\t%s\n", part_str[part]);

		*msgs++ = strdup(msgbuf);
		count++;
	}

	return (count);
}
