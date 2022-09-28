/*
 *	setkey.c
 *
 *	Copyright (c) 1988-1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)setkey.c	1.14	96/07/17 SMI"

/*
 * Do the real work of the keyserver.
 * Store secret keys. Compute common keys,
 * and use them to decrypt and encrypt DES keys.
 * Cache the common keys, so the expensive computation is avoided.
 */
#include <stdio.h>
#include <mp.h>
#include <rpc/rpc.h>
#include <rpc/key_prot.h>
#include <rpc/des_crypt.h>
#include <sys/errno.h>
#include <string.h>
#include <thread.h>

extern char *malloc();
extern char ROOTKEY[];

static MINT *MODULUS;
static int hash_keys();
static keystatus pk_crypt();
static int nodefaultkeys = 0;

/*
 * Exponential caching management
 */
struct cachekey_list {
	keybuf secret;
	keybuf public;
	des_block deskey;
	struct cachekey_list *next;
};
#define	KEY_HASH_SIZE	256
static struct cachekey_list *g_cachedkeys[KEY_HASH_SIZE];
static rwlock_t g_cachedkeys_lock = DEFAULTRWLOCK;

/*
 * prohibit the nobody key on this machine k (the -d flag)
 */
pk_nodefaultkeys()
{
	nodefaultkeys = 1;
	return (0);
}

/*
 * Set the modulus for all our Diffie-Hellman operations
 */
setmodulus(modx)
	char *modx;
{
	MODULUS = mp_xtom(modx);
	return (0);
}

/*
 * Set the secretkey key for this uid
 */
keystatus
pk_setkey(uid, skey)
	uid_t uid;
	keybuf skey;
{
	if (!storesecretkey(uid, skey)) {
		return (KEY_SYSTEMERR);
	}
	return (KEY_SUCCESS);
}

/*
 * Encrypt the key using the public key associated with remote_name and the
 * secret key associated with uid.
 */
keystatus
pk_encrypt(uid, remote_name, remote_key, key)
	uid_t uid;
	char *remote_name;
	netobj	*remote_key;
	des_block *key;
{
	return (pk_crypt(uid, remote_name, remote_key, key, DES_ENCRYPT));
}

/*
 * Decrypt the key using the public key associated with remote_name and the
 * secret key associated with uid.
 */
keystatus
pk_decrypt(uid, remote_name, remote_key, key)
	uid_t uid;
	char *remote_name;
	netobj *remote_key;
	des_block *key;
{
	return (pk_crypt(uid, remote_name, remote_key, key, DES_DECRYPT));
}

/*
 * Key storage management
 */

#define	KEY_ONLY 0
#define	KEY_NAME 1
struct secretkey_netname_list {
	uid_t uid;
	key_netstarg keynetdata;
	u_char sc_flag;
	struct secretkey_netname_list *next;
};

#define	HASH_UID(x)	(x & 0xff)
static struct secretkey_netname_list *g_secretkey_netname[KEY_HASH_SIZE];
static rwlock_t g_secretkey_netname_lock = DEFAULTRWLOCK;

/*
 * Store the keys and netname for this uid
 */
static int
store_netname(uid, netstore)
	uid_t uid;
	key_netstarg *netstore;
{
	struct secretkey_netname_list *new;
	struct secretkey_netname_list **l;
	int hash = HASH_UID(uid);

	(void) rw_wrlock(&g_secretkey_netname_lock);
	for (l = &g_secretkey_netname[hash]; *l != NULL && (*l)->uid != uid;
			l = &(*l)->next) {
	}
	if (*l == NULL) {
/* LINTED pointer alignment */
		new = (struct secretkey_netname_list *)malloc(sizeof (*new));
		if (new == NULL) {
			(void) rw_unlock(&g_secretkey_netname_lock);
			return (0);
		}
		new->uid = uid;
		new->next = NULL;
		*l = new;
	} else {
		new = *l;
		if (new->keynetdata.st_netname)
			(void) free(new->keynetdata.st_netname);
	}
	memcpy(new->keynetdata.st_priv_key, netstore->st_priv_key,
		HEXKEYBYTES);
	memcpy(new->keynetdata.st_pub_key, netstore->st_pub_key, HEXKEYBYTES);

	if (netstore->st_netname)
		new->keynetdata.st_netname = strdup(netstore->st_netname);
	else
		new->keynetdata.st_netname = (char *)NULL;
	new->sc_flag = KEY_NAME;
	(void) rw_unlock(&g_secretkey_netname_lock);
	return (1);

}

keystatus
pk_netput(uid, netstore)
	uid_t uid;
	key_netstarg *netstore;
{

	if (!store_netname(uid, netstore)) {
		return (KEY_SYSTEMERR);
	}
	return (KEY_SUCCESS);
}

/*
 * Fetch the keys and netname for this uid
 */
static int
fetch_netname(uid, key_netst)
	uid_t uid;
	struct key_netstarg *key_netst;
{
	struct secretkey_netname_list *l;
	int hash = HASH_UID(uid);

	(void) rw_rdlock(&g_secretkey_netname_lock);
	for (l = g_secretkey_netname[hash]; l != NULL; l = l->next) {
		if ((l->uid == uid) && (l->sc_flag == KEY_NAME)) {

			memcpy(key_netst->st_priv_key,
				l->keynetdata.st_priv_key, HEXKEYBYTES);

			memcpy(key_netst->st_pub_key,
				l->keynetdata.st_pub_key, HEXKEYBYTES);

			if (l->keynetdata.st_netname)
				strcpy(key_netst->st_netname,
						l->keynetdata.st_netname);
			else
				key_netst->st_netname = NULL;
			(void) rw_unlock(&g_secretkey_netname_lock);
			return (1);
		}
	}
	(void) rw_unlock(&g_secretkey_netname_lock);
	return (0);
}

keystatus
pk_netget(uid, netstore)
	uid_t uid;
	key_netstarg *netstore;
{
	if (!fetch_netname(uid, netstore)) {
		return (KEY_SYSTEMERR);
	}
	return (KEY_SUCCESS);
}


#define	cachehit(pub, sec, list)	\
		(memcmp(pub, (list)->public, sizeof (keybuf)) == 0 && \
		memcmp(sec, (list)->secret, sizeof (keybuf)) == 0)

/*
 * Try to find the common key in the cache
 */
static
readcache(pub, sec, deskey, hash)
	char *pub;
	char *sec;
	des_block *deskey;
	int hash;
{
	register struct cachekey_list **l;

	for (l = &g_cachedkeys[hash]; (*l) != NULL && !cachehit(pub, sec, *l);
		l = &(*l)->next)
		;
	if ((*l) == NULL)
		return (0);
	*deskey = (*l)->deskey;
	return (1);
}

/*
 * cache result of expensive multiple precision exponential operation
 */
static
writecache(pub, sec, deskey, hash)
	char *pub;
	char *sec;
	des_block *deskey;
	int hash;
{
	struct cachekey_list *new;

/* LINTED pointer alignment */
	new = (struct cachekey_list *) malloc(sizeof (struct cachekey_list));
	if (new == NULL) {
		return (0);
	}
	memcpy(new->public, pub, sizeof (keybuf));
	memcpy(new->secret, sec, sizeof (keybuf));
	new->deskey = *deskey;

	new->next = g_cachedkeys[hash];
	g_cachedkeys[hash] = new;
	return (1);
}

/*
 * Choose middle 64 bits of the common key to use as our des key, possibly
 * overwriting the lower order bits by setting parity.
 */
static
extractdeskey(ck, deskey)
	MINT *ck;
	des_block *deskey;
{
	MINT *a;
	short r;
	int i;
	short base = (1 << 8);
	char *k;

	a = mp_itom(0);
	_mp_move(ck, a);
	for (i = 0; i < ((KEYSIZE - 64) / 2) / 8; i++) {
		mp_sdiv(a, base, a, &r);
	}
	k = deskey->c;
	for (i = 0; i < 8; i++) {
		mp_sdiv(a, base, a, &r);
		*k++ = r;
	}
	mp_mfree(a);
	des_setparity((char *)deskey);
	return (0);
}

static bool_t
fetchsecretkey(uid, buf)
	uid_t uid;
	char *buf;
{
	struct secretkey_netname_list *l;
	int hash = HASH_UID(uid);

	(void) rw_rdlock(&g_secretkey_netname_lock);
	for (l = g_secretkey_netname[hash]; l != NULL; l = l->next) {
		if (l->uid == uid) {
			memcpy(buf, l->keynetdata.st_priv_key,
				sizeof (keybuf));
			(void) rw_unlock(&g_secretkey_netname_lock);
			return (TRUE);
		}
	}
	(void) rw_unlock(&g_secretkey_netname_lock);
	return (FALSE);
}

/*
 * Do the work of pk_encrypt && pk_decrypt
 */
static keystatus
pk_crypt(uid, remote_name, remote_key, key, mode)
	uid_t uid;
	char *remote_name;
	netobj *remote_key;
	des_block *key;
	int mode;
{
	char xsecret[1024];
	char xpublic[1024];
	des_block deskey;
	int err;
	MINT *public;
	MINT *secret;
	MINT *common;
	char zero[8];
	int hash;

	if (!fetchsecretkey(uid, xsecret) || xsecret[0] == 0) {
		memset(zero, 0, sizeof (zero));
		if (nodefaultkeys)
			return (KEY_NOSECRET);

		if (!getsecretkey("nobody", xsecret, zero) || xsecret[0] == 0) {
			return (KEY_NOSECRET);
		}
	}
	if (remote_key) {
		memcpy(xpublic, remote_key->n_bytes, remote_key->n_len);
	} else {
		if (!getpublickey(remote_name, xpublic)) {
			if (nodefaultkeys || !getpublickey("nobody", xpublic))
				return (KEY_UNKNOWN);
		}
	}

	xsecret[HEXKEYBYTES] = '\0';
	xpublic[HEXKEYBYTES] = '\0';

	hash = hash_keys(xpublic, xsecret);
	(void) rw_rdlock(&g_cachedkeys_lock);
	if (!readcache(xpublic, xsecret, &deskey, hash)) {
		(void) rw_unlock(&g_cachedkeys_lock);
		(void) rw_wrlock(&g_cachedkeys_lock);
		if (!readcache(xpublic, xsecret, &deskey, hash)) {
			public = mp_xtom(xpublic);
			secret = mp_xtom(xsecret);
			/* Sanity Check on public and private keys */
			if (public == NULL || secret == NULL) {
				(void) rw_unlock(&g_cachedkeys_lock);
				return (KEY_SYSTEMERR);
			}
			common = mp_itom(0);
			mp_pow(public, secret, MODULUS, common);
			extractdeskey(common, &deskey);
			writecache(xpublic, xsecret, &deskey, hash);
			mp_mfree(secret);
			mp_mfree(public);
			mp_mfree(common);
		}
	}
	(void) rw_unlock(&g_cachedkeys_lock);

	err = ecb_crypt((char *)&deskey, (char *)key, sizeof (des_block),
		DES_HW | mode);
	if (DES_FAILED(err)) {
		return (KEY_SYSTEMERR);
	}
	return (KEY_SUCCESS);
}

keystatus
pk_get_conv_key(uid, xpublic, result)
	uid_t uid;
	keybuf xpublic;
	cryptkeyres *result;
{
	char xsecret[1024];
	MINT *public;
	MINT *secret;
	MINT *common;
	char zero[8];
	int hash;

	if (!fetchsecretkey(uid, xsecret) || xsecret[0] == 0) {
		memset(zero, 0, sizeof (zero));
		if (nodefaultkeys)
			return (KEY_NOSECRET);

		if (!getsecretkey("nobody", xsecret, zero) ||
			xsecret[0] == 0)
			return (KEY_NOSECRET);
	}

	hash = hash_keys(xpublic, xsecret);
	(void) rw_rdlock(&g_cachedkeys_lock);
	if (!readcache(xpublic, xsecret, &result->cryptkeyres_u.deskey, hash)) {
		(void) rw_unlock(&g_cachedkeys_lock);
		(void) rw_wrlock(&g_cachedkeys_lock);
		if (!readcache(xpublic, xsecret, &result->cryptkeyres_u.deskey,
									hash)) {
			public = mp_xtom(xpublic);
			secret = mp_xtom(xsecret);
			/* Sanity Check on public and private keys */
			if (public == NULL || secret == NULL) {
				(void) rw_unlock(&g_cachedkeys_lock);
				return (KEY_SYSTEMERR);
			}
			common = mp_itom(0);
			mp_pow(public, secret, MODULUS, common);
			extractdeskey(common, &result->cryptkeyres_u.deskey);
			writecache(xpublic, xsecret,
					&result->cryptkeyres_u.deskey, hash);
			mp_mfree(secret);
			mp_mfree(public);
			mp_mfree(common);
		}
	}
	(void) rw_unlock(&g_cachedkeys_lock);

	return (KEY_SUCCESS);
}

#define	findsec(sec, list)	\
		(memcmp(sec, (list)->secret, sizeof (keybuf)) == 0)

/*
 * Remove common keys from the cache.
 */
static
removecache(sec)
	char *sec;
{
	struct cachekey_list *found;
	register struct cachekey_list **l;
	int i;

	(void) rw_wrlock(&g_cachedkeys_lock);
	for (i = 0; i < KEY_HASH_SIZE; i++) {
		for (l = &g_cachedkeys[i]; (*l) != NULL; ) {
			if (findsec(sec, *l)) {
				found = *l;
				*l = (*l)->next;
				memset((char *) found, 0,
					sizeof (struct cachekey_list));
				free(found);
			} else {
				l = &(*l)->next;
			}
		}
	}
	(void) rw_unlock(&g_cachedkeys_lock);
	return (1);
}

/*
 * Store the secretkey for this uid
 */
storesecretkey(uid, key)
	uid_t uid;
	keybuf key;
{
	struct secretkey_netname_list *new;
	struct secretkey_netname_list **l;
	int hash = HASH_UID(uid);

	(void) rw_wrlock(&g_secretkey_netname_lock);
	for (l = &g_secretkey_netname[hash]; *l != NULL && (*l)->uid != uid;
			l = &(*l)->next) {
	}
	if (*l == NULL) {
		if (key[0] == '\0') {
			(void) rw_unlock(&g_secretkey_netname_lock);
			return (0);
		}
/* LINTED pointer alignment */
		new = (struct secretkey_netname_list *) malloc(sizeof (*new));
		if (new == NULL) {
			(void) rw_unlock(&g_secretkey_netname_lock);
			return (0);
		}
		new->uid = uid;
		new->sc_flag = KEY_ONLY;
		memset(new->keynetdata.st_pub_key, 0, HEXKEYBYTES);
		new->keynetdata.st_netname = NULL;
		new->next = NULL;
		*l = new;
	} else {
		new = *l;
		if (key[0] == '\0')
			removecache(new->keynetdata.st_priv_key);
	}

	memcpy(new->keynetdata.st_priv_key, key,
		HEXKEYBYTES);
	(void) rw_unlock(&g_secretkey_netname_lock);
	return (1);
}

static
hexdigit(val)
	int val;
{
	return ("0123456789abcdef"[val]);
}

bin2hex(bin, hex, size)
	unsigned char *bin;
	unsigned char *hex;
	int size;
{
	int i;

	for (i = 0; i < size; i++) {
		*hex++ = hexdigit(*bin >> 4);
		*hex++ = hexdigit(*bin++ & 0xf);
	}
	return (0);
}

static
hexval(dig)
	char dig;
{
	if ('0' <= dig && dig <= '9') {
		return (dig - '0');
	} else if ('a' <= dig && dig <= 'f') {
		return (dig - 'a' + 10);
	} else if ('A' <= dig && dig <= 'F') {
		return (dig - 'A' + 10);
	} else {
		return (-1);
	}
}

hex2bin(hex, bin, size)
	unsigned char *hex;
	unsigned char *bin;
	int size;
{
	int i;

	for (i = 0; i < size; i++) {
		*bin = hexval(*hex++) << 4;
		*bin++ |= hexval(*hex++);
	}
	return (0);
}

static int
hash_keys(pub, sec)
	char *pub;
	char *sec;
{
	int i;
	int hash = 0;

	for (i = 0; i < HEXKEYBYTES; i += 6, pub += 6, sec += 6) {
		hash ^= *pub;
		hash ^= *sec;
	}
	return (hash & 0xff);
}
