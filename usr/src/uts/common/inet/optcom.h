/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef	_INET_OPTCOM_H
#define	_INET_OPTCOM_H

#pragma ident	"@(#)optcom.h	1.10	96/10/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && defined(__STDC__)

/* Options Description Structure */
typedef struct opdes_s {
	int	opdes_name;	/* option name */
	int	opdes_level;	/* option "level" */
	int	opdes_access_nopriv; /* permissions for non-privileged */
	int	opdes_access_priv; /* permissions for privilged */
	int	opdes_props;	/* properties of associated with option */
	int	opdes_size;	/* length of option  [ or maxlen if variable */
			/* length(OP_VARLEN) property set for option] */
	union {
		/*
		 *
		 * Note: C semantics:
		 * static initializer of "union" type assume
		 * the constant on RHS is of the type of the
		 * first member of the union. So what comes first
		 * is important.
		 */
#define	OPDES_DEFSZ_MAX		64
		int64_t  opdes_def_int64;
		char	opdes_def_charbuf[OPDES_DEFSZ_MAX];
	} opdes_def;
} opdes_t;

#define	opdes_default	opdes_def.opdes_def_int64
#define	opdes_defbuf	opdes_def.opdes_def_charbuf
/*
 * Flags to set in opdes_acces_{all,priv} fields in opdes_t
 *
 *	OA_R	read access
 *	OA_W	write access
 *	OA_RW	read-write access
 *	OA_X	execute access
 *
 * Note: - semantics "execute" access used for operations excuted using
 *		option management interface
 *	- no bits set means this option is not visible. Some options may not
 *	  even be visible to all but priviliged users.
 */
#define	OA_R	0x1
#define	OA_W	0x2
#define	OA_X	0x4

/*
 * Utility macros to test permissions needed to compose more
 * complex ones. (Only a few really used directly in code).
 */
#define	OA_RW	(OA_R|OA_W)
#define	OA_WX	(OA_W|OA_X)
#define	OA_RX	(OA_R|OA_X)
#define	OA_RWX	(OA_R|OA_W|OA_X)

#define	OA_ANY_ACCESS(x) ((x)->opdes_access_nopriv|(x)->opdes_access_priv)
#define	OA_R_NOPRIV(x)	((x)->opdes_access_nopriv & OA_R)
#define	OA_R_ANYPRIV(x)	(OA_ANY_ACCESS(x) & OA_R)
#define	OA_W_NOPRIV(x)	((x)->opdes_access_nopriv & OA_W)
#define	OA_X_ANYPRIV(x)	(OA_ANY_ACCESS(x) & OA_X)
#define	OA_X_NOPRIV(x)	((x)->opdes_access_nopriv & OA_X)
#define	OA_W_ANYPRIV(x)	(OA_ANY_ACCESS(x) & OA_W)
#define	OA_WX_NOPRIV(x)	((x)->opdes_access_nopriv & OA_WX)
#define	OA_WX_ANYPRIV(x)	(OA_ANY_ACCESS(x) & OA_WX)
#define	OA_RWX_ANYPRIV(x)	(OA_ANY_ACCESS(x) & OA_RWX)
#define	OA_RONLY_NOPRIV(x)	(((x)->opdes_access_nopriv & OA_RWX) == OA_R)
#define	OA_RONLY_ANYPRIV(x)	((OA_ANY_ACCESS(x) & OA_RWX) == OA_R)

/*
 * Following macros supply the option and their privelege and
 * are used to determine permissions.
 */
#define	OA_READ_PERMISSION(x, p)	((!(p) && OA_R_NOPRIV(x)) ||\
						((p) && OA_R_ANYPRIV(x)))
#define	OA_WRITE_OR_EXECUTE(x, p)	((!(p) && OA_WX_NOPRIV(x)) ||\
						((p) && OA_WX_ANYPRIV(x)))
#define	OA_READONLY_PERMISSION(x, p)	((!(p) && OA_RONLY_NOPRIV(x)) ||\
						((p) && OA_RONLY_ANYPRIV(x)))
#define	OA_WRITE_PERMISSION(x, p)	((!(p) && OA_W_NOPRIV(x)) ||\
					    (p && OA_W_ANYPRIV(x)))
#define	OA_EXECUTE_PERMISSION(x, p)	((!(p) && OA_X_NOPRIV(x)) ||\
					    ((p) && OA_X_ANYPRIV(x)))

/*
 * Other properties set in opdes_props field.
 */
#define	OP_PASSNEXT	0x1
#define	OP_VARLEN	0x2
#define	OP_NOT_ABSREQ	0x4
#define	OP_NODEFAULT	0x8
#define	OP_DEF_FN	0x10


/*
 * Structure to represent attributed of option management specific
 * to one particular layer of "transport".
 */

typedef opdes_t	opdes_arr_t[];

typedef struct optdb_obj {
	pfi_t		odb_deffn;	/* default value function */
	pfi_t		odb_getfn;	/* get function */
	pfi_t		odb_setfn;	/* set function */
	boolean_t	odb_topmost_tpiprovider; /* whether topmost tpi */
						/* provider or downstream */
	u_int		odb_opt_arr_cnt; /* count of number of options in db */
	opdes_t		*odb_opt_des_arr; /* option descriptors in db */
} optdb_obj_t;

/*
 * Macros for proper alignment
 */
#define	ALIGN_TPIOPT_size	(sizeof (int32_t))
#define	ROUNDUP_TPIOPT(p)	(((p) + (ALIGN_TPIOPT_size - 1))\
					& ~(ALIGN_TPIOPT_size - 1))
#define	ISALIGNED_TPIOPT(addr)	\
		(((u_long)(addr) & (ALIGN_TPIOPT_size - 1)) == 0)

/*
 * Return values for tpi_optcom_buf() private function.
 */
#define	OB_SUCCESS	0
#define	OB_BADOPT	-1
#define	OB_NOMEM	-2
#define	OB_NOACCES	-3
#define	OB_ABSREQ_FAIL	-4
#define	OB_INVAL	-5


/*
 * Function prototypes
 */
void	optcom_err_ack(queue_t *q, mblk_t *mp, int t_error,
			int sys_error);

void	svr4_optcom_req(queue_t *q, mblk_t *mp, int priv,
				optdb_obj_t *dbobjp);

void	tpi_optcom_req(queue_t *q, mblk_t *mp, int priv,
				optdb_obj_t *dbobjp);

int	tpi_optcom_buf(queue_t *q, mblk_t *mp, int32_t *opt_lenp,
			int32_t opt_offset, int priv, optdb_obj_t *dbobjp);
u_int	optcom_max_optbuf_len(opdes_t *opt_arr, u_int opt_arr_cnt);

#endif	/* defined(_KERNEL) && defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_OPTCOM_H */
