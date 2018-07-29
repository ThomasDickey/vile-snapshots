/*
 * Unix crypt(1)-style interface.
 * Written by T.E.Dickey for vile (March 1999).
 *
 * $Id: vl_crypt.h,v 1.6 2018/07/29 21:32:21 tom Exp $
 */

#if	OPT_ENCRYPT

#ifndef N_chars
#define N_chars 256
#endif

#define MASKED(n) ((n) & (N_chars - 1))

#define LEN_CLEAR 8
#define LEN_CRYPT 13

extern int vl_encrypt_char(int);
extern void vl_encrypt_blok(char *, UINT);
extern void vl_make_encrypt_key(char *, const char *);
extern void vl_setup_encrypt(char *, int);

static unsigned index_1;
static unsigned index_2;

static UCHAR table1[N_chars];
static UCHAR table2[N_chars];
static UCHAR table3[N_chars];

void
vl_encrypt_blok(char *buf, UINT len)
{
    unsigned c1, c2;

    for (c2 = 0; c2 < len; c2++) {
	c1 = (UCHAR) buf[c2];
	buf[c2] =
	    (char) (table2[MASKED(table3[MASKED(table1[MASKED(c1 + index_1)]
						+ index_2)]
				  - index_2)]
		    - index_1);
	if (++index_1 >= N_chars) {
	    index_1 = 0;
	    if (++index_2 >= N_chars)
		index_2 = 0;
	}
    }
}

int
vl_encrypt_char(int ch)
{
    char buffer[2];
    buffer[0] = (char) ch;
    vl_encrypt_blok(buffer, 1);
    return buffer[0];
}

/*
 * Calls to 'crypt()' are slow; do this only after we have gotten a new
 * key from the user.
 */
void
vl_make_encrypt_key(char *dst, const char *src)
{
    char key[LEN_CLEAR];
    char salt[2];

    memcpy(key, src, sizeof(key));
    memcpy(salt, src, sizeof(salt));
    memset(dst, 0, LEN_CRYPT);
    strncpy(dst, crypt(key, salt), (size_t) LEN_CRYPT);

    TRACE(("made encryption key(%s)\n", dst));
}

/*
 * Call this function at the beginning of encrypting/decrypting a file, i.e.,
 * while writing or reading it.
 */
void
vl_setup_encrypt(char *encrypted_password, int myseed)
{
    int j;
    unsigned c1, c2;
    UCHAR temp;
    unsigned mixs;

    TRACE(("setup_encrypt(%s)\n", encrypted_password));
    index_1 = 0;
    index_2 = 0;

    for (j = 0; j < LEN_CRYPT; j++)
	myseed = myseed * encrypted_password[j] + j;

    for (j = 0; j < N_chars; j++)
	table1[j] = (UCHAR) j;
    memset(table3, 0, sizeof(table3));

    for (j = 0; j < N_chars; j++) {
	myseed = 5 * myseed + encrypted_password[j % LEN_CRYPT];
	mixs = (unsigned) (myseed % 65521);
	c2 = (unsigned) (N_chars - 1 - j);
	c1 = MASKED(mixs) % (c2 + 1);
	mixs >>= 8;
	temp = table1[c2];
	table1[c2] = table1[c1];
	table1[c1] = temp;
	if (table3[c2] == 0) {
	    c1 = MASKED(mixs) % c2;
	    while (table3[c1] != 0)
		c1 = (c1 + 1) % c2;
	    table3[c2] = (UCHAR) c1;
	    table3[c1] = (UCHAR) c2;
	}
    }

    for (j = 0; j < N_chars; j++)
	table2[MASKED(table1[j])] = (UCHAR) j;

    index_1 = 0;
    index_2 = 0;
}

#endif
