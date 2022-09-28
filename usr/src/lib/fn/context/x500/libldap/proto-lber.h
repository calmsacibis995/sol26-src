/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PROTO_LBER_H
#define	_PROTO_LBER_H

#pragma ident	"@(#)proto-lber.h	1.1	96/03/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * lber-proto.h
 * function prototypes for lber library
 */

#ifdef LDAP_DEBUG
extern int lber_debug;
#endif

#ifndef LIBIMPORT
#ifdef _WIN32
#define	LIBIMPORT	__declspec(dllimport)
#else /* _WIN32 */
#define	LIBIMPORT
#endif /* _WIN32 */
#endif /* LIBIMPORT */

/*
 * in bprint.c:
 */
LIBIMPORT void lber_bprint(char *data, int len);

/*
 * in decode.c:
 */
LIBIMPORT unsigned long ber_get_tag(BerElement *ber);
LIBIMPORT unsigned long ber_skip_tag(BerElement *ber, unsigned long *len);
LIBIMPORT unsigned long ber_peek_tag(BerElement *ber, unsigned long *len);
LIBIMPORT unsigned long ber_get_int(BerElement *ber, long *num);
LIBIMPORT unsigned long ber_get_stringb(BerElement *ber, char *buf,
	unsigned long *len);
LIBIMPORT unsigned long ber_get_stringa(BerElement *ber, char **buf);
LIBIMPORT unsigned long ber_get_stringal(BerElement *ber, struct berval **bv);
LIBIMPORT unsigned long ber_get_bitstringa(BerElement *ber, char **buf,
	unsigned long *len);
LIBIMPORT unsigned long ber_get_null(BerElement *ber);
LIBIMPORT unsigned long ber_get_boolean(BerElement *ber, int *bool);
LIBIMPORT unsigned long ber_first_element(BerElement *ber, unsigned long *len,
	char **last);
LIBIMPORT unsigned long ber_next_element(BerElement *ber, unsigned long *len,
	char *last);
#if defined(MACOS) || defined(BC31)
LIBIMPORT unsigned long ber_scanf(BerElement *ber, char *fmt, ...);
#else
LIBIMPORT unsigned long ber_scanf();
#endif
LIBIMPORT void ber_bvfree(struct berval *bv);
LIBIMPORT void ber_bvecfree(struct berval **bv);
LIBIMPORT struct berval *ber_bvdup(struct berval *bv);
#ifdef STR_TRANSLATION
LIBIMPORT void ber_set_string_translators(BerElement *ber,
	BERTranslateProc encode_proc, BERTranslateProc decode_proc);
#endif /* STR_TRANSLATION */

/*
 * in encode.c
 */
LIBIMPORT int ber_put_enum(BerElement *ber, long num, unsigned long tag);
LIBIMPORT int ber_put_int(BerElement *ber, long num, unsigned long tag);
LIBIMPORT int ber_put_ostring(BerElement *ber, char *str, unsigned long len,
	unsigned long tag);
LIBIMPORT int ber_put_string(BerElement *ber, char *str, unsigned long tag);
LIBIMPORT int ber_put_bitstring(BerElement *ber, char *str,
	unsigned long bitlen, unsigned long tag);
LIBIMPORT int ber_put_null(BerElement *ber, unsigned long tag);
LIBIMPORT int ber_put_boolean(BerElement *ber, int bool, unsigned long tag);
LIBIMPORT int ber_start_seq(BerElement *ber, unsigned long tag);
LIBIMPORT int ber_start_set(BerElement *ber, unsigned long tag);
LIBIMPORT int ber_put_seq(BerElement *ber);
LIBIMPORT int ber_put_set(BerElement *ber);
#if defined(MACOS) || defined(BC31)
LIBIMPORT int ber_printf(BerElement *ber, char *fmt, ...);
#else
LIBIMPORT int ber_printf();
#endif

/*
 * in io.c:
 */
LIBIMPORT long ber_read(BerElement *ber, char *buf, unsigned long len);
LIBIMPORT long ber_write(BerElement *ber, char *buf, unsigned long len,
	int nosos);
LIBIMPORT void ber_free(BerElement *ber, int freebuf);
LIBIMPORT int ber_flush(Sockbuf *sb, BerElement *ber, int freeit);
LIBIMPORT BerElement *ber_alloc(void);
LIBIMPORT BerElement *der_alloc(void);
LIBIMPORT BerElement *ber_alloc_t(int options);
LIBIMPORT BerElement *ber_dup(BerElement *ber);
LIBIMPORT void ber_dump(BerElement *ber, int inout);
LIBIMPORT void ber_sos_dump(Seqorset *sos);
LIBIMPORT unsigned long ber_get_next(Sockbuf *sb, unsigned long *len,
	BerElement *ber);
LIBIMPORT void ber_init(BerElement *ber, int options);
LIBIMPORT void ber_reset(BerElement *ber, int was_writing);

#ifdef NEEDGETOPT
/*
 * in getopt.c
 */
int getopt(int nargc, char **nargv, char *ostr);
#endif /* NEEDGETOPT */


#ifdef	__cplusplus
}
#endif

#endif	/* _PROTO_LBER_H */
