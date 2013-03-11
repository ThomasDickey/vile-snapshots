/*
 * $Id: eightbit.c,v 1.103 2013/03/10 20:27:22 tom Exp $
 *
 * Maintain "8bit" file-encoding mode by converting incoming UTF-8 to single
 * bytes, and providing a function that tells vile whether a given Unicode
 * value will map to the eight-bit value in the "narrow" locale.
 */

#include <estruct.h>
#include <chgdfunc.h>
#include <edef.h>
#include <nefsms.h>

#if OPT_LOCALE
#include <locale.h>
#endif

#if OPT_ICONV_FUNCS
#include <iconv.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#include <gnreight.h>

#define StrMalloc(s) ((s) ? strmalloc(s) : 0)

#if DISP_TERMCAP || DISP_CURSES || DISP_BORLAND
#define USE_MBTERM 1
static int (*save_getch) (void);
static OUTC_DCL(*save_putch) (int c);
#else
#define USE_MBTERM 0
#endif

#define LEN_UTF8 6		/* maximum number of bytes in UTF-8 encoding */

/******************************************************************************/
static BLIST blist_encodings = init_blist(all_encodings);
static BLIST blist_locales = init_blist(all_locales);

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
    return (int) p->code - (int) q->code;
}

static const GNREIGHT_ENC *from_encoding = 0;
static const GNREIGHT_ENC *builtin_locale = 0;

#if OPT_ICONV_FUNCS
#define MY_ICONV_TYPE iconv_t
#else
#define MY_ICONV_TYPE  const GNREIGHT_ENC *
#endif /* OPT_ICONV_FUNCS */

#define MY_ICONV  (MY_ICONV_TYPE)(-2)
#define NO_ICONV  (MY_ICONV_TYPE)(-1)

static MY_ICONV_TYPE mb_desc = NO_ICONV;

static void
close_encoding(void)
{
#if OPT_ICONV_FUNCS
    if (mb_desc == MY_ICONV) {
	;
    } else if (mb_desc != NO_ICONV) {
	iconv_close(mb_desc);
    }
#endif /* OPT_ICONV_FUNCS */
    mb_desc = NO_ICONV;
    from_encoding = 0;
}

static int
try_encoding(const char *from, const char *to)
{
    int found;

    close_encoding();

#if OPT_ICONV_FUNCS
    mb_desc = iconv_open(to, from);
#endif

    if (mb_desc == NO_ICONV) {
	if (!vl_stricmp(to, "UTF-8")) {
	    found = blist_match(&blist_encodings, from);
	    if (found >= 0) {
		mb_desc = MY_ICONV;
		from_encoding = all_encodings[found].data;
		TRACE(("using built-in encoding\n"));
	    }
	}
    }

    TRACE(("try_encoding(from=%s, to=%s) %s\n",
	   from, to,
	   ((mb_desc != NO_ICONV)
	    ? "OK"
	    : "FAIL")));

    return (mb_desc != NO_ICONV);
}

static void
open_encoding(char *from, char *to)
{
    if (!try_encoding(from, to)) {
	fprintf(stderr, "Cannot setup translation from %s to %s\n", from, to);
	tidy_exit(BADEXIT);
    }
}

static void
initialize_table_8bit_utf8(void)
{
    int n;

    for (n = 0; n < N_chars; ++n) {
	size_t converted;
	char input[80];
	char output[80];
	size_t in_bytes = 1;
	size_t out_bytes = sizeof(output);

	FreeIfNeeded(table_8bit_utf8[n].text);
	table_8bit_utf8[n].text = 0;

	input[0] = (char) n;
	input[1] = 0;

#if OPT_ICONV_FUNCS
	if (mb_desc == MY_ICONV) {
	    UINT source = ((n >= 128)
			   ? (from_encoding->chmap[n - 128])
			   : (UINT) n);
	    converted = (size_t) vl_conv_to_utf8((UCHAR *) output, source,
						 sizeof(output));
	    out_bytes = sizeof(output) - converted;
	} else {
	    ICONV_CONST char *ip = input;
	    char *op = output;
	    converted = iconv(mb_desc, &ip, &in_bytes, &op, &out_bytes);
	}
#else
	{
	    UINT source = ((n >= 128)
			   ? (from_encoding->chmap[n - 128])
			   : (UINT) n);
	    converted = (size_t) vl_conv_to_utf8((UCHAR *) output, source,
						 sizeof(output));
	    out_bytes = sizeof(output) - converted;
	}
#endif /* OPT_ICONV_FUNCS */

	if (converted == (size_t) (-1)) {
	    TRACE(("err:%d\n", errno));
	    TRACE(("convert(%d) %d %d/%d\n", n,
		   (int) converted, (int) in_bytes, (int) out_bytes));
	} else if (n == 0) {
	    table_8bit_utf8[n].code = 0;
	    table_8bit_utf8[n].text = strmalloc("");
	} else if ((sizeof(output) - out_bytes) != 0) {
	    output[sizeof(output) - out_bytes] = 0;
	    table_8bit_utf8[n].text = strmalloc(output);
	    vl_conv_to_utf32(&(table_8bit_utf8[n].code),
			     table_8bit_utf8[n].text,
			     strlen(table_8bit_utf8[n].text));
#if OPT_TRACE > 1
	    if (n != (int) table_8bit_utf8[n].code)
		TRACE(("8bit_utf8 %d:%#4x\n", n, table_8bit_utf8[n].code));
#endif
	}
    }
}

/*
 * Fill-in character type information from built-in tables, i.e., when the
 * narrow locale is missing.
 */
void
vl_8bit_ctype_init(int wide, int ch)
{
    const GNREIGHT_ENC *encode;

    if (builtin_locale) {
	TRACE(("using previously found built-in encoding\n"));
	from_encoding = builtin_locale;
    }
#if !OPT_ICONV_FUNCS
    else {
	int found;

	from_encoding = 0;
	found = blist_match(&blist_encodings, wide ? "ISO-8859-1" : "POSIX");
	if (found >= 0) {
	    from_encoding = all_encodings[found].data;
	    TRACE(("using built-in encoding\n"));
	}
    }
#endif

    encode = (from_encoding
	      ? from_encoding
	      : (wide
		 ? &encode_ISO_8859_1
		 : &encode_ANSI_X3_4_1968));

    if ((ch < 128) || encode->chmap[ch - 128]) {
	CHARTYPE value = (CHARTYPE) ((ch < 128)
				     ? encode_POSIX.ctype[ch]
				     : encode->ctype[ch - 128]);

	/* xdigit is specific to ASCII */
	if (value & vl_digit)
	    value |= vl_xdigit;
	if ((value & vl_alpha) && (strchr("abcdefABCDEF", ch) != 0))
	    value |= vl_xdigit;

	value &= ~vl_alpha;	/* unused in vile */

	setVlCTYPE(ch, value);

	vl_lowercase[ch + 1] = (char) ((ch < 128)
				       ? encode_POSIX.lower[ch]
				       : encode->lower[ch - 128]);

	vl_uppercase[ch + 1] = (char) ((ch < 128)
				       ? encode_POSIX.upper[ch]
				       : encode->upper[ch - 128]);
    }
}

int
vl_8bit_builtin(void)
{
    return (builtin_locale != 0);
}

char *
vl_narrowed(const char *wide)
{
    char *result = 0;

    TRACE((T_CALLED "vl_narrowed(%s)\n", NonNull(wide)));
    if (wide != 0) {
	const char *mapping = getenv("VILE_LOCALE_MAPPING");
	const char *parsed;
	char *on_left;
	char *on_right;
	regexp *exp;
	int n;

	/*
	 * The default mapping strips ".UTF-8", ".utf-8", ".UTF8" or ".utf8",
	 * and assumes that the default locale alias corresponds to the 8-bit
	 * encoding.  That is true on many systems.
	 */
	if (mapping == 0)
	    mapping = "/\\.\\(UTF\\|utf\\)[-]\\?8$//";

	parsed = mapping;
	on_left = regparser(&parsed);
	on_right = regparser(&parsed);
	if (on_left != 0
	    && on_right != 0
	    && parsed[1] == '\0'
	    && (exp = regcomp(on_left, strlen(on_left), TRUE)) != 0) {
	    int len = (int) strlen(wide);
	    int found = 0;

	    if ((result = malloc(strlen(wide) + 2 + strlen(on_right))) != 0) {
		strcpy(result, wide);
		for (n = 0; n < len; ++n) {
		    found = regexec(exp, result, result + len, n, len);
		    if (found)
			break;
		}

		/*
		 * Substitute the result (back-substitutions not supported).
		 */
		if (found) {
		    char *save = strmalloc(exp->endp[0]);

		    TRACE(("vl_narrowed match \"%.*s\", replace with \"%s\"\n",
			   (int) (exp->endp[0] - exp->startp[0]),
			   exp->startp[0],
			   on_right));

		    strcpy(exp->startp[0], on_right);
		    strcat(exp->startp[0], save);
		    free(save);
		} else {
		    free(result);
		    result = 0;
		}
	    }
	    regfree(exp);
	} else {
	    fprintf(stderr, "VILE_LOCAL_MAPPING error:%s\n", mapping);
	}
	FreeIfNeeded(on_left);
	FreeIfNeeded(on_right);
    }
    returnString(result);
}

/*
 * Do the rest of the work for setting up the wide- and narrow-locales.
 *
 * Set vl_encoding to the internal code used for resolving encoding of
 * buffers whose file-encoding is "locale" (enc_LOCALE).
 */
void
vl_init_8bit(const char *wide, const char *narrow)
{
    int fixup, fixed_up;
    int n;

    TRACE((T_CALLED "vl_init_8bit(%s, %s)\n", NonNull(wide), NonNull(narrow)));
    if (wide == 0 || narrow == 0) {
	TRACE(("setup POSIX-locale\n"));
	vl_encoding = enc_POSIX;
	vl_wide_enc.locale = 0;
	vl_narrow_enc.locale = 0;
	vl_get_encoding(&vl_narrow_enc.encoding, narrow);
    } else if (vl_stricmp(wide, narrow)) {
	TRACE(("setup mixed-locale(%s, %s)\n", wide, narrow));
	vl_wide_enc.locale = StrMalloc(wide);
	vl_narrow_enc.locale = StrMalloc(narrow);
	vl_get_encoding(&vl_wide_enc.encoding, wide);
	vl_get_encoding(&vl_narrow_enc.encoding, narrow);
	TRACE(("...locale encodings(%s, %s)\n",
	       NONNULL(vl_wide_enc.encoding),
	       NONNULL(vl_narrow_enc.encoding)));

	if (vl_is_utf8_encoding(vl_wide_enc.encoding)) {
	    vl_encoding = enc_UTF8;
	} else {
	    vl_encoding = enc_8BIT;
	}

	/*
	 * If the wide/narrow encodings do not differ, that is probably because
	 * the narrow encoding is really a wide-encoding.
	 */
	if (vl_narrow_enc.encoding != 0
	    && vl_wide_enc.encoding != 0
	    && vl_stricmp(vl_narrow_enc.encoding, vl_wide_enc.encoding)) {
	    open_encoding(vl_narrow_enc.encoding, vl_wide_enc.encoding);
	    initialize_table_8bit_utf8();
	    close_encoding();
	}
    } else {
	TRACE(("setup narrow-locale(%s)\n", narrow));
	vl_encoding = enc_8BIT;
	vl_wide_enc.locale = 0;
	vl_narrow_enc.locale = StrMalloc(narrow);
	vl_get_encoding(&vl_narrow_enc.encoding, narrow);
	if (try_encoding(vl_narrow_enc.encoding, "UTF-8")) {
	    initialize_table_8bit_utf8();
	    close_encoding();
	}
    }

    /*
     * Even if we do not have iconv, we can still convert between the narrow
     * encoding (if it happens to be ISO-8859-1) and UTF-8.
     */
    if (vl_is_latin1_encoding(vl_narrow_enc.encoding)) {
	fixup = N_chars;
    } else {
	/* if nothing else, still accept POSIX characters */
	fixup = 128;
    }
    for (n = 0, latin1_codes = 128, fixed_up = 0; n < fixup; ++n) {
	if (table_8bit_utf8[n].text == 0) {
	    char temp[10];
	    int len = vl_conv_to_utf8((UCHAR *) temp, (UINT) n, sizeof(temp));

	    temp[len] = EOS;
	    table_8bit_utf8[n].code = (UINT) n;
	    FreeIfNeeded(table_8bit_utf8[n].text);
	    table_8bit_utf8[n].text = strmalloc(temp);
	    if (len)
		++fixed_up;
	}
    }
    TRACE(("fixed-up %d of %d 8bit/utf8 mappings\n", fixed_up, fixup));

    /*
     * Build reverse-index.
     */
    TRACE2(("build rindex_8bit\n"));
    for (n = 0; n < N_chars; ++n) {
	int code = (int) table_8bit_utf8[n].code;

	if (n == 0 || code > 0) {
	    rindex_8bit[n].code = (UINT) code;
	    rindex_8bit[n].rinx = n;
	} else {
	    table_8bit_utf8[n].code = (UINT) - 1;
	    rindex_8bit[n].code = (UINT) - 1;
	    rindex_8bit[n].rinx = -1;
	}

	TRACE2(("code %d (0x%04X) is \\u%04X:%s\n", n, n,
		table_8bit_utf8[n].code,
		NonNull(table_8bit_utf8[n].text)));
    }
    qsort(rindex_8bit, (size_t) N_chars, sizeof(RINDEX_8BIT), cmp_rindex);

    /*
     * As long as the narrow/wide encodings match, we can treat those as
     * Latin-1.
     */
    for (n = 0, latin1_codes = 0; n < N_chars; ++n) {
	int check;
	if (vl_ucs_to_8bit(&check, n)) {
	    latin1_codes = n + 1;
	} else {
	    break;
	}
    }
    TRACE(("assume %d Latin1 codes\n", latin1_codes));

    returnVoid();
}

int
vl_is_8bit_encoding(const char *value)
{
    int rc = vl_is_latin1_encoding(value);
    if (!rc) {
	rc = (isEmpty(value)
	      || strstr(value, "ASCII") != 0
	      || strstr(value, "ANSI") != 0
	      || strncmp(value, "KOI8-R", (size_t) 6) == 0);
    }
    return rc;
}

int
vl_is_latin1_encoding(const char *value)
{
    int rc = FALSE;

    if (!isEmpty(value)) {
	if (strncmp(value, "ISO-8859", (size_t) 8) == 0
	    || strncmp(value, "ISO 8859", (size_t) 8) == 0
	    || strncmp(value, "ISO_8859", (size_t) 8) == 0
	    || strncmp(value, "ISO8859", (size_t) 7) == 0
	    || strncmp(value, "8859", (size_t) 4) == 0) {
	    rc = TRUE;
	}
    }
    return rc;
}

int
vl_is_utf8_encoding(const char *value)
{
    int rc = FALSE;

    if (!isEmpty(value)) {
	char *suffix = strchr(value, '.');
	if (suffix != 0) {
#ifdef WIN32
	    if (!strcmp(suffix, ".65000")) {
		rc = TRUE;
	    } else
#endif
		if (strchr(value + 1, '.') == 0
		    && vl_is_utf8_encoding(value + 1)) {
		rc = TRUE;
	    }
	} else if (!vl_stricmp(value, "UTF8")
		   || !vl_stricmp(value, "UTF-8")) {
	    rc = TRUE;
	}
    }
    return rc;
}

/*
 * Check if the given Unicode value can be mapped to an "8bit" value.
 */
int
vl_mb_is_8bit(int code)
{
    int result = 0;

    RINDEX_8BIT *p;
    RINDEX_8BIT key;

    key.code = (UINT) code;
    p = (RINDEX_8BIT *) bsearch(&key,
				rindex_8bit,
				(size_t) N_chars,
				sizeof(RINDEX_8BIT),
				cmp_rindex);

    if (p != 0)
	result = ((int) p->code >= 0 && p->code < 256);

    return result;
}

/*
 * Returns a string representing the current locale.
 * If the target is nonnull, allocate a copy of it.
 */
char *
vl_get_locale(char **target)
{
    char *result = setlocale(LC_CTYPE, 0);

    if (target != 0) {
	FreeIfNeeded(*target);
	if (result != 0)
	    result = strmalloc(result);
	*target = result;
    }
    return result;
}

/*
 * Returns a string representing the character encoding.
 * If the target is nonnull, allocate a copy of it.
 */
char *
vl_get_encoding(char **target, const char *locale)
{
    static char iso_latin1[] = "ISO-8859-1";
    static char utf_eight[] = "UTF-8";

#ifdef WIN32
    char cp_value[80];
#endif
    char *result = 0;
    char *actual = setlocale(LC_CTYPE, locale);
    int can_free = 0;

    TRACE((T_CALLED "vl_get_encoding(%s)%s\n",
	   NONNULL(locale),
	   isEmpty(actual) ? " illegal" : ""));

    if (isEmpty(actual)) {	/* nonempty means legal locale */
	char *mylocale;

	/*
	 * If it failed, this may be because we asked for a narrow locale
	 * that corresponds to the given (wide) locale.
	 */
	beginDisplay();
	if (vl_is_utf8_encoding(locale)) {
	    result = utf_eight;
	} else if (!isEmpty(locale) && (mylocale = strmalloc(locale)) != 0) {
	    regexp *exp;

	    exp = regcomp(tb_values(latin1_expr),
			  (size_t) tb_length0(latin1_expr), TRUE);
	    if (exp != 0) {
		if (nregexec(exp, mylocale, (char *) 0, 0, -1)) {
		    TRACE(("... found match in $latin1-expr\n"));
		    result = iso_latin1;
		}
		free(exp);
	    }
	    free(mylocale);

	    if (result == 0) {
		int found = blist_match(&blist_locales, locale);
		if (found >= 0) {
		    int find;
		    TRACE(("... found built-in locale data\n"));
		    for (find = 0; all_encodings[find].name; ++find) {
			if (all_encodings[find].data == all_locales[found].data) {
			    TRACE(("... found built-in encoding data\n"));
			    result = strmalloc(all_encodings[find].name);
			    can_free = 1;
			    builtin_locale = all_encodings[find].data;
			    break;
			}
		    }
		}
	    }
	}
	endofDisplay();
    } else {
#ifdef HAVE_LANGINFO_CODESET
	result = nl_langinfo(CODESET);
#else
	if (vl_is_utf8_encoding(locale)) {
	    result = utf_eight;
	} else if (isEmpty(locale)
		   || !vl_stricmp(locale, "C")
		   || !vl_stricmp(locale, "POSIX")) {
	    result = "ASCII";
	} else {
#ifdef WIN32
	    char *suffix = strrchr(locale, '.');
	    char *next = 0;

	    if (suffix != 0
		&& strtol(++suffix, &next, 10) != 0
		&& (next == 0 || *next == 0)) {
		result = cp_value;
		sprintf(result, "CP%s", suffix);
	    } else
#endif
		result = iso_latin1;
	}
#endif
    }
    if (target != 0) {
	FreeIfNeeded(*target);
	if (result != 0 && !can_free)
	    result = strmalloc(result);
	*target = result;
    }
    returnString(result);
}

/*
 * Use the lookup table created in vl_init_8bit() to convert an "8bit"
 * value to the corresponding UTF-8 string.  If the current locale
 * encoding is ISO-8859-1 (the default), this is a 1-1 mapping.
 */
const char *
vl_mb_to_utf8(int code)
{
    const char *result = 0;
    RINDEX_8BIT *p;
    RINDEX_8BIT key;

    key.code = (UINT) code;
    p = (RINDEX_8BIT *) bsearch(&key,
				rindex_8bit,
				(size_t) N_chars,
				sizeof(RINDEX_8BIT),
				cmp_rindex);
    if (p != 0 && p->rinx >= 0)
	result = table_8bit_utf8[p->rinx].text;

    return result;
}

/*
 * Use the lookup table created in vl_init_8bit() to convert an "8bit"
 * value to the corresponding Unicode value.  If the current locale
 * encoding is ISO-8859-1 (the default), this is a 1-1 mapping.
 */
int
vl_8bit_to_ucs(int *result, int code)
{
    int status = FALSE;

    if (code >= 0 && code < 256 && (int) table_8bit_utf8[code].code >= 0) {
	*result = (int) table_8bit_utf8[code].code;
	status = TRUE;
    }
    return status;
}

/*
 * Use the lookup table to find if a Unicode value exists in the 8bit/utf-8
 * mapping, and if so, set the 8bit result, returning true.
 */
int
vl_ucs_to_8bit(int *result, int code)
{
    int status = FALSE;
    RINDEX_8BIT *p;
    RINDEX_8BIT key;

    key.code = (UINT) code;
    p = (RINDEX_8BIT *) bsearch(&key,
				rindex_8bit,
				(size_t) N_chars,
				sizeof(RINDEX_8BIT),
				cmp_rindex);
    if (p != 0 && p->rinx >= 0) {
	*result = p->rinx;
	status = TRUE;
    }
    return status;
}

#if USE_MBTERM
/*
 * Decode a buffer as UTF-8, returning the character value if successful.
 * If unsuccessful, return -1.
 */
static int
decode_utf8(char *input, int used)
{
    UINT check = 0;
    int rc;
    int ch;

    rc = vl_conv_to_utf32(&check, input, (B_COUNT) used);
    if ((rc == used) && (check != 0) && !isSpecial(check))
	ch = (int) check;
    else
	ch = -1;
    return ch;
}

/*
 * Read a whole character, according to the locale.  If an error is detected,
 * return -1.
 */
static int
vl_mb_getch(void)
{
    static char input[80];
    static int used = 0;
    static int have = 0;

    UINT tempch;
    int ch;
    int need, test;
    int actual_encoding;

    if (used < have) {
	ch = CharOf(input[used++]);
    } else {
	used = have = 0;

	if ((actual_encoding = kbd_encoding) == enc_LOCALE) {
	    actual_encoding = (okCTYPE2(vl_wide_enc)
			       ? enc_UTF8
			       : enc_8BIT);
	}
	switch (actual_encoding) {
	case enc_AUTO:
	    input[0] = (char) (ch = save_getch());
	    if ((need = vl_check_utf8(input, (B_COUNT) 1)) > 1) {
		have = 1;
		for (;;) {
		    input[have++] = (char) save_getch();
		    test = vl_check_utf8(input, (B_COUNT) have);
		    if (test == 0) {
			kbd_encoding = enc_8BIT;
			ch = vl_mb_getch();
			break;
		    } else if (test == need) {
			kbd_encoding = enc_UTF8;
			vl_conv_to_utf32(&tempch, input, (B_COUNT) have);
			ch = (int) tempch;
			used = have = 0;
			break;
		    }
		}
	    }
	    break;
	case enc_POSIX:
	    do {
		ch = save_getch();
	    } while (ch >= 128);
	    break;
	default:
	case enc_8BIT:
	    ch = save_getch();
	    if (ch >= 128 && okCTYPE2(vl_wide_enc)) {
		if (ch >= 256) {
		    ch = -1;
		} else if ((ch = (int) table_8bit_utf8[ch].code) == 0) {
		    ch = -1;
		}
	    }
	    break;
	case enc_UTF8:
	    for (;;) {
		ch = save_getch();
		input[have++] = (char) ch;
		input[have] = 0;

		ch = decode_utf8(input, have);
		if (ch >= 0) {
		    break;
		} else if (have > LEN_UTF8) {
		    ch = -1;
		    break;
		}
	    }
	    have = used = 0;
	    if (ch >= 256 && !okCTYPE2(vl_wide_enc)) {
		ch = -1;
	    }
	    break;
	}
    }
    TRACE(("vl_mb_getch:%#x\n", ch));
    return ch;
}

static OUTC_DCL
vl_mb_putch(int c)
{
    if (c > 0) {
	int rc;
	UCHAR temp[LEN_UTF8 + 1];
	const char *s = vl_mb_to_utf8(c);

	/*
	 * If we got no result, then it was not in the cached mapping to 8bit.
	 * But we can still convert it to UTF-8.
	 */
	if (s == 0) {
	    rc = vl_conv_to_utf8(temp, (UINT) c, sizeof(temp));
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
    TRACE(("vl_open_mbterm\n"));

    if (save_getch == 0) {
	TRACE(("...using vl_mb_getch(%s)\n", encoding2s(kbd_encoding)));
	save_getch = term.getch;
	term.getch = vl_mb_getch;
    }

    if (okCTYPE2(vl_wide_enc)) {
	TRACE(("...using vl_mb_putch\n"));

	if (term.putch != vl_mb_putch) {
	    save_putch = term.putch;
	    term.putch = vl_mb_putch;
	}
	term.set_enc(enc_UTF8);
    }
}

void
vl_close_mbterm(void)
{
    if (save_getch != 0) {
	term.getch = save_getch;
	save_getch = 0;
    }

    if (okCTYPE2(vl_wide_enc)) {
	if (save_putch != 0) {
	    TRACE(("vl_close_mbterm\n"));
	    term.putch = save_putch;

	    term.set_enc(enc_POSIX);

	    save_putch = 0;
	}
    }
}
#endif /* USE_MBTERM */

#if NO_LEAKS
void
eightbit_leaks(void)
{
    int n;

#if OPT_ICONV_FUNCS
    close_encoding();
#endif

    for (n = 0; n < N_chars; ++n) {
	if (table_8bit_utf8[n].text != 0) {
	    free(table_8bit_utf8[n].text);
	    table_8bit_utf8[n].text = 0;
	}
    }
    FreeIfNeeded(vl_wide_enc.locale);
    FreeIfNeeded(vl_narrow_enc.locale);
}
#endif
