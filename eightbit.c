/*
 * $Id: eightbit.c,v 1.24 2008/04/02 00:05:58 tom Exp $
 *
 * Maintain "8bit" file-encoding mode by converting incoming UTF-8 to single
 * bytes, and providing a function that tells vile whether a given Unicode
 * value will map to the eight-bit value in the "narrow" locale.
 */

#include <estruct.h>
#include <chgdfunc.h>
#include <edef.h>
#include <nefsms.h>

#if OPT_ICONV_FUNCS
#include <iconv.h>
#include <locale.h>
#include <langinfo.h>
#endif

#if DISP_TERMCAP || DISP_CURSES
static int (*save_getch) (void);
static OUTC_DCL(*save_putch) (int c);
#endif

/******************************************************************************/

typedef struct {
    UINT code;			/* Unicode value */
    char *text;			/* UTF-8 string */
} TABLE_8BIT;

static TABLE_8BIT table_8bit_utf8[N_chars];

typedef struct {
    UINT code;			/* Unicode value */
    int rinx;			/* actual index in table_8bit_utf8[] */
} RINDEX_8BIT;

static RINDEX_8BIT rindex_8bit[N_chars];

static int
cmp_rindex(const void *a, const void *b)
{
    const RINDEX_8BIT *p = (const RINDEX_8BIT *) a;
    const RINDEX_8BIT *q = (const RINDEX_8BIT *) b;
    return p->code - q->code;
}

#if OPT_ICONV_FUNCS
static iconv_t mb_desc;

static void
open_encoding(char *from, char *to)
{
    TRACE(("open_encoding(from=%s, to=%s)\n", from, to));
    mb_desc = iconv_open(to, from);
    if (mb_desc == (iconv_t) (-1)) {
	fprintf(stderr, "Cannot setup translation from %s to %s\n", from, to);
	tidy_exit(BADEXIT);
    }
}
#endif /* OPT_ICONV_FUNCS */

void
vl_init_8bit(const char *wide, const char *narrow)
{
    int n;

    TRACE((T_CALLED "vb_init_8bit(%s, %s)\n", wide, narrow));
#if OPT_ICONV_FUNCS
    if (strcmp(wide, narrow)) {
	char *wide_enc = 0;
	char *narrow_enc = 0;

	TRACE(("setup_locale(%s, %s)\n", wide, narrow));
	utf8_locale = wide;
	vl_get_encoding(&wide_enc, wide);
	vl_get_encoding(&narrow_enc, narrow);
	TRACE(("...setup_locale(%s, %s)\n",
	       NONNULL(wide_enc),
	       NONNULL(narrow_enc)));

	/*
	 * If the wide/narrow encodings do not differ, that is probably because
	 * the narrow encoding is really a wide-encoding.
	 */
	if (narrow_enc != 0
	    && wide_enc != 0
	    && strcmp(narrow_enc, wide_enc)) {
	    open_encoding(narrow_enc, wide_enc);

	    for (n = 0; n < N_chars; ++n) {
		size_t converted;
		char input[80];
		ICONV_CONST char *ip = input;
		char output[80];
		char *op = output;
		size_t in_bytes = 1;
		size_t out_bytes = sizeof(output);
		input[0] = n;
		input[1] = 0;
		converted = iconv(mb_desc, &ip, &in_bytes, &op, &out_bytes);
		if (converted == (size_t) (-1)) {
		    TRACE(("err:%d\n", errno));
		    TRACE(("convert(%d) %d %d/%d\n", n,
			   (int) converted, (int) in_bytes, (int) out_bytes));
		} else {
		    output[sizeof(output) - out_bytes] = 0;
		    table_8bit_utf8[n].text = strmalloc(output);
		    vl_conv_to_utf32(&(table_8bit_utf8[n].code),
				     table_8bit_utf8[n].text,
				     strlen(table_8bit_utf8[n].text));
		}
	    }
	    iconv_close(mb_desc);

	    /*
	     * If we were able to convert in one direction, the other should
	     * succeed in vl_mb_getch().
	     */
	    open_encoding(wide_enc, narrow_enc);
	}
    }
#endif /* OPT_ICONV_FUNCS */

    for (n = 0; n < N_chars; ++n) {
	if (table_8bit_utf8[n].text == 0) {
	    char temp[10];
	    int len = vl_conv_to_utf8((UCHAR *) temp, n, sizeof(temp));

	    temp[len] = EOS;
	    table_8bit_utf8[n].code = n;
	    table_8bit_utf8[n].text = strmalloc(temp);
	}
    }

    /*
     * Build reverse-index.
     */
    for (n = 0; n < N_chars; ++n) {
	TRACE2(("code %d is \\u%04X:%s\n", n,
		table_8bit_utf8[n].code,
		NonNull(table_8bit_utf8[n].text)));
	rindex_8bit[n].code = n;
	rindex_8bit[n].rinx = n;
    }
    qsort(rindex_8bit, N_chars, sizeof(RINDEX_8BIT), cmp_rindex);

    returnVoid();
}

/*
 * Check if the given Unicode value can be mapped to an "8bit" value.
 */
int
vl_mb_is_8bit(int code)
{
    RINDEX_8BIT *p;
    RINDEX_8BIT key;

    key.code = code;
    p = (RINDEX_8BIT *) bsearch(&key,
				rindex_8bit,
				N_chars,
				sizeof(RINDEX_8BIT),
				cmp_rindex);
    return (p != 0);
}

/*
 * Returns a string representing the current locale.
 * If the target is nonnull, allocate a copy of it.
 */
char *
vl_get_locale(char **target)
{
    char *result = setlocale(LC_ALL, 0);

    if (target != 0) {
	FreeIfNeeded(*target);
	if (result != 0)
	    result = strmalloc(result);
	*target = result;
    }
    return *target;
}

/*
 * Returns a string representing the character encoding.
 * If the target is nonnull, allocate a copy of it.
 */
char *
vl_get_encoding(char **target, const char *locale)
{
    char *result = 0;
    char *actual = setlocale(LC_ALL, locale);

    if (!isEmpty(actual)) {	/* nonempty means legal locale */
#ifdef HAVE_LANGINFO_CODESET
	result = nl_langinfo(CODESET);
#else
	if (isEmpty(locale)
	    || !strcmp(locale, "C")
	    || !strcmp(locale, "POSIX")) {
	    result = "ASCII";
	} else {
	    result = "ISO-8859-1";
	}
#endif
    }
    TRACE(("vl_get_encoding(%s) -> %s\n", NONNULL(locale), NONNULL(result)));
    if (target != 0) {
	FreeIfNeeded(*target);
	if (result != 0)
	    result = strmalloc(result);
	*target = result;
    }
    return result;
}

#if DISP_TERMCAP || DISP_CURSES
/*
 * Use the lookup table created in vl_init_8bit() to convert an "8bit"
 * value to the corresponding UTF-8 string.  If the current locale
 * encoding is ISO-8859-1 (the default), this is a 1-1 mapping.
 */
static const char *
vl_mb_to_utf8(int code)
{
    const char *result = 0;
    RINDEX_8BIT *p;
    RINDEX_8BIT key;

    key.code = code;
    p = (RINDEX_8BIT *) bsearch(&key,
				rindex_8bit,
				N_chars,
				sizeof(RINDEX_8BIT),
				cmp_rindex);
    if (p != 0)
	result = table_8bit_utf8[p->rinx].text;

    return result;
}

/*
 * Decode a buffer as UTF-8, returning the character value if successful.
 * If unsuccessful, return -1.
 */
static int
decode_utf8(char *input, int used)
{
    UINT check = 0;
    int rc = 0;
    int ch;

    /*
     * If iconv gave up - because a character was not representable
     * in the narrow encoding - just convert it from UTF-8.
     *
     * FIXME: perhaps a better solution would be to use iconv for
     * converting from the wide encoding to UTF-32. 
     */
    rc = vl_conv_to_utf32(&check, input, used);
    if ((rc == used) && (check != 0) && !isSpecial(check))
	ch = check;
    else
	ch = -1;
    return ch;
}

static int
vl_mb_getch(void)
{
    int ch;
    char input[80];
    ICONV_CONST char *ip;
#if OPT_ICONV_FUNCS
    char output[80];
    char *op;
    int used = 0;
    size_t converted, in_bytes, out_bytes;

    for (;;) {
	ch = save_getch();
	input[used++] = ch;
	input[used] = 0;
	ip = input;
	in_bytes = used;
	op = output;
	out_bytes = sizeof(output);
	*output = 0;
	if (mb_desc != 0) {
	    /*
	     * First, try with iconv, assuming it does a better job.
	     */
	    converted = iconv(mb_desc, &ip, &in_bytes, &op, &out_bytes);
	    TRACE(("converted %d '%s' -> %d:%#x\n",
		   (int) converted, input, (int) out_bytes, *output));
	    if (converted == (size_t) (-1)) {
		if (errno == EILSEQ) {
		    /*
		     * If iconv gave up - because a character was not
		     * representable in the narrow encoding - just convert it
		     * from UTF-8.
		     *
		     * FIXME:  perhaps a better solution would be to use iconv
		     * for converting from the wide encoding to UTF-32. 
		     */
		    ch = decode_utf8(input, used);
		    break;
		}
	    } else {
		/* assume it is 8-bits */
		ch = CharOf(output[0]);
		break;
	    }
	} else {
	    ch = decode_utf8(input, used);
	    if (ch >= 0 || used > 5)
		break;
	}
    }
#else
    for (ip = input;;) {
	UINT result;

	if ((ch = save_getch()) < 0)
	    break;
	*ip++ = ch;
	ch = vl_conv_to_utf32(&result, input, ip - input);
	if (ch == 0) {
	    ch = -1;
	} else if (ch == (ip - input)) {
	    /* ensure result is 8-bits */
	    if (vl_mb_to_utf8(result) != 0)
		ch = result;
	    else
		ch = -1;
	    break;
	}
    }
#endif
    TRACE(("vl_mb_getch:%#x\n", ch));
    return ch;
}

static OUTC_DCL
vl_mb_putch(int c)
{
    if (c > 0) {
	int rc;
	UCHAR temp[10];
	const char *s = vl_mb_to_utf8(c);

	/*
	 * If we got no result, then it was not in the cached mapping to 8bit.
	 * But we can still convert it to UTF-8.
	 */
	if (s == 0) {
	    rc = vl_conv_to_utf8(temp, c, sizeof(temp));
	    if (rc > 0) {
		temp[rc] = EOS;
		s = (const char *) temp;
	    }
	}
	if (s != 0) {
	    while (*s != 0) {
		save_putch(*s++);
	    }
	}
    }
    OUTC_RET 0;
}

/*
 * If the display driver (termcap/terminfo/curses) is capable of
 * reading/writing UTF-8 (or other iconv-supported multibyte sequences),
 * save/intercept the put/get pointers from 'term'.
 */
void
vl_open_mbterm(void)
{
    if (utf8_locale) {
	TRACE(("vl_open_mbterm\n"));
	save_putch = term.putch;
	save_getch = term.getch;

	term.putch = vl_mb_putch;
	term.getch = vl_mb_getch;

	term.encoding = enc_UTF8;
    }
}

void
vl_close_mbterm(void)
{
    if (utf8_locale) {
	if (save_putch && save_getch) {
	    TRACE(("vl_close_mbterm\n"));
	    term.putch = save_putch;
	    term.getch = save_getch;

	    term.encoding = enc_POSIX;

	    save_putch = 0;
	    save_getch = 0;
	}
    }
}
#endif /* DISP_TERMCAP || DISP_CURSES */

#if NO_LEAKS
void
eightbit_leaks(void)
{
    int n;

#if OPT_ICONV_FUNCS
    if (mb_desc) {
	iconv_close(mb_desc);
	mb_desc = 0;
    }
#endif

    for (n = 0; n < N_chars; ++n) {
	if (table_8bit_utf8[n].text != 0) {
	    free(table_8bit_utf8[n].text);
	    table_8bit_utf8[n].text = 0;
	}
    }
}
#endif
