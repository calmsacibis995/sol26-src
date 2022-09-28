/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#ident	"@(#)loadtab.c	1.1	96/08/16 SMI"

#include "synonyms.h"
#include <stdlib.h>
#include <limits.h>
#include <sys/localedef.h>
#include <thread.h>
#include <synch.h>

#ifdef	_REAL_FIX_FOR_SUNTEA
#define	_WCHAR_CSMASK	0x30000000
#define	_WCHAR_S_MASK	0x7f
#define	_WCHAR_SHIFT	7
#define	_WCHAR_SHIFT2	1
#define	_WCHAR_SHIFT3	2
#define	_WCHAR_SHIFT4	3
#define	_WCHAR_BITMASK1	0x00000080
#define	_WCHAR_BITMASK2	0x00008080
#define	_WCHAR_BITMASK3	0x00808080
#define	_WCHAR_BITMASK4	0x80808080
#define	_WCHAR_MASK1	(_WCHAR_S_MASK)
#define	_WCHAR_MASK2	(_WCHAR_S_MASK << _WCHAR_SHIFT)
#define	_WCHAR_MASK3	(_WCHAR_S_MASK << (_WCHAR_SHIFT * 2))
#define	_WCHAR_MASK4	(_WCHAR_S_MASK << (_WCHAR_SHIFT * 3))
#define	_LEN1_BASE	0x00000020
#define	_LEN1_END	0x0000007f
#define	_LEN2_BASE	0x00001020
#define	_LEN2_END	0x00003fff
#define	_LEN3_BASE	0x00081020
#define	_LEN3_END	0x001fffff
#define	_LEN4_BASE	0x04081020
#define	_LEN4_END	0x0fffffff
#define	_SS2	0x8e
#define	_SS3	0x8f
#endif	/* _REAL_FIX_FOR_SUNTEA */

/* For the application that has been linked with libw.a */
/* _lflag = 1 if multi-byte character table is updated */
int	_lflag = 0;

struct _wctype	*_wcptr[3] = {
	0, 0, 0
};

#ifdef _REENTRANT
mutex_t	_locale_lock = DEFAULTMUTEX;
#endif

#ifdef	_REAL_FIX_FOR_SUNTEA
static wchar_t *
create_conv_table(
	int	codeset,
	int	bytelen,
	wchar_t	base_dense,
	wchar_t	end_dense,
	int	ntrans,
	wchar_t	*min_conv,
	wchar_t	*max_conv)
{
	int	i, rc, len, count, cflag;
	int	*idx;
	wchar_t	*conv_table;
	wchar_t	densepc, eucpc, packs;
	wchar_t	min_conv_euc, max_conv_euc;
	wchar_t	wc, conv;
	wchar_t	*tmin, *tmax, *cmin, *cmax;
	const wchar_t	**transtabs;
	static const	unsigned int	bitmask[] = {
		_WCHAR_BITMASK1,	_WCHAR_BITMASK2,
		_WCHAR_BITMASK3,	_WCHAR_BITMASK4,
		0
	};
	_LC_transnm_t	*transname;

	transname = __lc_ctype->transname;
	transtabs = __lc_ctype->transtabs;

	if ((tmin = (wchar_t *)malloc(sizeof (wchar_t) * ntrans))
		== NULL) {
		return ((wchar_t *)NULL);
	}
	if ((tmax = (wchar_t *)malloc(sizeof (wchar_t) * ntrans))
		== NULL) {
		free(tmin);
		return ((wchar_t *)NULL);
	}
	if ((cmin = (wchar_t *)malloc(sizeof (wchar_t) * ntrans))
		== NULL) {
		free(tmin);
		free(tmax);
		return ((wchar_t *)NULL);
	}
	if ((cmax = (wchar_t *)malloc(sizeof (wchar_t) * ntrans))
		== NULL) {
		free(tmin);
		free(tmax);
		free(cmin);
		return ((wchar_t *)NULL);
	}

	if ((idx = (int *)malloc(sizeof (int) * ntrans))
		== NULL) {
		free(tmin);
		free(tmax);
		free(cmin);
		free(cmax);
		return ((wchar_t *)NULL);
	}
	for (i = 1; i <= ntrans; i++) {
		idx[i] = transname[i].index;
		tmin[i] = transname[i].tmin;
		tmax[i] = transname[i].tmax;
	}

	*min_conv = end_dense;
	*max_conv = base_dense;
	cflag = 0;
	for (i = 1; i <= ntrans; i++) {
		if ((tmax[i] < base_dense) ||
			(tmin[i] > end_dense)) {	/* out of range */
			idx[i] = 0;
			continue;
		}
		cflag = 1;
		if (tmin[i] < base_dense) {
			cmin[i] = base_dense;
		} else {
			cmin[i] = tmin[i];
		}
		if (tmax[i] > end_dense) {
			cmax[i] = end_dense;
		} else {
			cmax[i] = tmax[i];
		}
		if (cmin[i] < *min_conv) {
			*min_conv = cmin[i];
		}
		if (cmax[i] > *max_conv) {
			*max_conv = cmax[i];
		}
	}
	if (cflag == 0) {
		free(tmin);
		free(tmax);
		free(cmin);
		free(cmax);
		free(idx);
		return ((wchar_t *)NULL);
	}

	min_conv_euc = _wctoeucpc(__lc_charmap, *min_conv);
	if (min_conv_euc == -1) {
		free(tmin);
		free(tmax);
		free(cmin);
		free(cmax);
		free(idx);
		return ((wchar_t *)NULL);
	}
	max_conv_euc = _wctoeucpc(__lc_charmap, *max_conv);
	if (max_conv_euc == -1) {
		free(tmin);
		free(tmax);
		free(cmin);
		free(cmax);
		free(idx);
		return ((wchar_t *)NULL);
	}
	len = max_conv_euc - min_conv_euc + 1;

	if ((conv_table = (wchar_t *)malloc(sizeof (wchar_t) * len)) ==
		NULL) {
		free(tmin);
		free(tmax);
		free(cmin);
		free(cmax);
		free(idx);
		return ((wchar_t *)NULL);
	}
	memset((void *)conv_table, 0, sizeof (wchar_t) * len);

	count = 0;
	for (eucpc = min_conv_euc; eucpc <= max_conv_euc; eucpc++) {
		wc = eucpc & ~_WCHAR_CSMASK;
		packs = ((_WCHAR_MASK4 & wc) << _WCHAR_SHIFT4) |
			((_WCHAR_MASK3 & wc) << _WCHAR_SHIFT3) |
			((_WCHAR_MASK2 & wc) << _WCHAR_SHIFT2) |
			(_WCHAR_MASK1 & wc);
		packs |= bitmask[bytelen - 1];
		*(conv_table + count) = (wchar_t)packs;
		count++;
	}

	for (i = 1; i <= ntrans; i++) {
		if (idx[i] == 0) {		/* no valid transtable */
			continue;
		}
		for (densepc = cmin[i]; densepc <= cmax[i]; densepc++) {
			conv = transtabs[idx[i]][densepc - tmin[i]];
			if (conv == densepc) {
				continue;
			}
			eucpc = _wctoeucpc(__lc_charmap, conv);
			if (eucpc == -1) {
				free(conv_table);
				free(tmin);
				free(tmax);
				free(cmin);
				free(cmax);
				free(idx);
				return ((wchar_t *)NULL);
			}
			wc = eucpc & ~_WCHAR_CSMASK;
			packs = ((_WCHAR_MASK4 & wc) << _WCHAR_SHIFT4) |
				((_WCHAR_MASK3 & wc) << _WCHAR_SHIFT3) |
				((_WCHAR_MASK2 & wc) << _WCHAR_SHIFT2) |
				(_WCHAR_MASK1 & wc);
			packs |= bitmask[bytelen - 1];
			*(conv_table + (densepc - *min_conv)) = packs;
		}
	}
	free(tmin);
	free(tmax);
	free(cmin);
	free(cmax);
	free(idx);
	return (conv_table);
}

static int
build_wcptr(
	int	codeset,
	int	bytelen,
	wchar_t	base_dense,
	wchar_t	end_dense)
{
	wchar_t	*tbl = NULL;
	wchar_t	base_euc, end_euc;
	unsigned char	*qidx;
	unsigned int	*qmask;
	struct _wctype	*p = NULL;
	int	ntrans;
	wchar_t	min_conv_dense = 0;
	wchar_t	max_conv_dense = 0;
	wchar_t	min_conv_euc = 0;
	wchar_t	max_conv_euc = 0;

	ntrans = __lc_ctype->ntrans;

	if ((p = (struct _wctype *)malloc(sizeof (struct _wctype))) ==
		NULL) {
		return (-1);
	}

	if (ntrans != 0) {
		tbl = create_conv_table(codeset, bytelen,
			base_dense, end_dense, ntrans,
			&min_conv_dense, &max_conv_dense);
		if (tbl == (wchar_t *)NULL) {
			free(p);
			return (-1);
		}
		min_conv_euc = _wctoeucpc(__lc_charmap, min_conv_dense);
		max_conv_euc = _wctoeucpc(__lc_charmap, max_conv_dense);
		if ((min_conv_euc == -1) || (max_conv_euc == -1)) {
			free(tbl);
			free(p);
			return (-1);
		}
		min_conv_euc &= ~_WCHAR_CSMASK;
		max_conv_euc &= ~_WCHAR_CSMASK;
	}

	base_euc = _wctoeucpc(__lc_charmap, base_dense);
	end_euc  = _wctoeucpc(__lc_charmap,  end_dense);
	if ((base_euc == -1) || (end_euc == -1)) {
		if (tbl) {
			free(tbl);
		}
		free(p);
		return (-1);
	}
	base_euc &= ~_WCHAR_CSMASK;
	end_euc  &= ~_WCHAR_CSMASK;
	qidx = (unsigned char *)__lc_ctype->qidx + (base_dense - 256);
	qmask = (unsigned int *)__lc_ctype->qmask;

	p->tmin = base_euc;
	p->tmax = end_euc;
	p->index = qidx;
	p->type = qmask;
	p->cmin = min_conv_euc;
	p->cmax = max_conv_euc;
	p->code = tbl;
	_wcptr[codeset] = p;

	return (0);
}

static wchar_t
findbase(int codeset, int bytelen)
{
	wchar_t	base_euc, end_euc;
	wchar_t	base_dense;
	wchar_t	wc;
	wchar_t	mask[3] = {
		0x30000000,
		0x10000000,
		0x20000000
	};
	unsigned char	str[MB_LEN_MAX];
	int	rc;

	if ((codeset < 0) || (codeset > 2)) {
		return ((wchar_t)WEOF);
	}
	if (bytelen == 1) {
		base_euc = mask[codeset] | _LEN1_BASE;
		end_euc = mask[codeset] | _LEN1_END;
	} else if (bytelen == 2) {
		base_euc = mask[codeset] | _LEN2_BASE;
		end_euc = mask[codeset] | _LEN2_END;
	} else if (bytelen == 3) {
		base_euc = mask[codeset] | _LEN3_BASE;
		end_euc = mask[codeset] | _LEN3_END;
	} else if (bytelen == 4) {
		base_euc = mask[codeset] | _LEN4_BASE;
		end_euc = mask[codeset] | _LEN4_END;
	}

	for (wc = base_euc; wc <= end_euc; wc++) {
		rc = wctomb((char *)str, wc);
		if (rc == -1) {
			continue;
		} else {
			base_dense = _eucpctowc(__lc_charmap, wc);
			if (base_dense == -1) {
				return ((wchar_t)WEOF);
			}
			return (base_dense);
		}
	}
	return ((wchar_t)WEOF);
}

int
_loadtab(void)
{
	wchar_t	cs1_base, cs2_base, cs3_base;
	wchar_t	cs1_end, cs2_end, cs3_end;
	unsigned char	*qidx;
	unsigned int	*qmask;
	int	bytelen1, bytelen2, bytelen3;
	int	i, rc, codeset;

	_lflag = 1;

	for (i = 0; i < 2; i++) {
		if (_wcptr[i]) {
			if (_wcptr[i]->code) {
				free(_wcptr[i]->code);
			}
			free(_wcptr[i]);
			_wcptr[i] = 0;
		}
	}

	if (!__lc_charmap->cm_eucinfo) {
		return (0);
	}

	bytelen1 = (int) __lc_charmap->cm_eucinfo->euc_bytelen1;
	bytelen2 = (int) __lc_charmap->cm_eucinfo->euc_bytelen2;
	bytelen3 = (int) __lc_charmap->cm_eucinfo->euc_bytelen3;
	cs1_end = __lc_charmap->cm_eucinfo->dense_end;
	cs2_end = cs1_end;
	cs3_end = cs1_end;


	if (bytelen1 != 0) {	/* Codeset1 exists. */
		codeset = 0;
		cs1_base = findbase(codeset, bytelen1);
		if (cs1_base == WEOF) {
			return (-1);
		}
		cs3_end = cs1_base - 1;
		cs2_end = cs1_base - 1;
		rc = build_wcptr(codeset, bytelen1, cs1_base, cs1_end);
		if (rc == -1) {
			if (_wcptr[0]) {
				if (_wcptr[0]->code) {
					free(_wcptr[0]->code);
				}
				free(_wcptr[0]);
				_wcptr[0] = 0;
			}
			return (-1);
		}
	} else {
		return (0);
	}
	if (bytelen3 != 0) {
		codeset = 2;
		cs3_base = findbase(codeset, bytelen3);
		if (cs3_base == WEOF) {
			if (_wcptr[0]) {
				if (_wcptr[0]->code) {
					free(_wcptr[0]->code);
				}
				free(_wcptr[0]);
				_wcptr[0] = 0;
			}
			return (-1);
		}
		cs2_end = cs3_base - 1;
		rc = build_wcptr(codeset, bytelen3, cs3_base, cs3_end);
		if (rc == -1) {
			if (_wcptr[0]) {
				if (_wcptr[0]->code) {
					free(_wcptr[0]->code);
				}
				free(_wcptr[0]);
				_wcptr[0] = 0;
			}
			if (_wcptr[2]) {
				if (_wcptr[2]->code) {
					free(_wcptr[2]->code);
				}
				free(_wcptr[2]);
				_wcptr[2] = 0;
			}
			return (-1);
		}
	}
	if (bytelen2 != 0) {
		codeset = 1;
		cs2_base = findbase(codeset, bytelen2);
		if (cs2_base == WEOF) {
			if (_wcptr[0]) {
				if (_wcptr[0]->code) {
					free(_wcptr[0]->code);
				}
				free(_wcptr[0]);
				_wcptr[0] = 0;
			}
			if (_wcptr[2]) {
				if (_wcptr[2]->code) {
					free(_wcptr[2]->code);
				}
				free(_wcptr[2]);
				_wcptr[2] = 0;
			}
			return (-1);
		}
		rc = build_wcptr(codeset, bytelen2, cs2_base, cs2_end);
		if (rc == -1) {
			if (_wcptr[0]) {
				if (_wcptr[0]->code) {
					free(_wcptr[0]->code);
				}
				free(_wcptr[0]);
				_wcptr[0] = 0;
			}
			if (_wcptr[1]) {
				if (_wcptr[1]->code) {
					free(_wcptr[1]->code);
				}
				free(_wcptr[1]);
				_wcptr[1] = 0;
			}
			if (_wcptr[2]) {
				if (_wcptr[2]->code) {
					free(_wcptr[2]->code);
				}
				free(_wcptr[2]);
				_wcptr[2] = 0;
			}
			return (-1);
		}
	}

#ifdef	DEBUG
	fprintf(stdout, "{%x, %x, %x, %x, %x, %x, %x}\n",
		_wcptr[0]->tmin, _wcptr[0]->tmax, _wcptr[0]->index,
		_wcptr[0]->type,
		_wcptr[0]->cmin, _wcptr[0]->cmax, _wcptr[0]->code);
	fprintf(stdout, "{%x, %x, %x, %x, %x, %x, %x}\n",
		_wcptr[1]->tmin, _wcptr[1]->tmax, _wcptr[1]->index,
		_wcptr[1]->type,
		_wcptr[1]->cmin, _wcptr[1]->cmax, _wcptr[1]->code);
	fprintf(stdout, "{%x, %x, %x, %x, %x, %x, %x}\n",
		_wcptr[2]->tmin, _wcptr[2]->tmax, _wcptr[2]->index,
		_wcptr[2]->type,
		_wcptr[2]->cmin, _wcptr[2]->cmax, _wcptr[2]->code);
#endif
	return (0);
}
#else	/* !_REAL_FIX_FOR_SUNTEA */
int
_loadtab(void)
{
	return (0);
}
#endif	/* _REAL_FIX_FOR_SUNTEA */
