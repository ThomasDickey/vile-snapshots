/*
 * $Id: vl_ctype.c,v 1.23 2025/01/26 14:35:39 tom Exp $
 *
 * On Linux, the normal/wide ctypes give comparable results in the range 0-255,
 * reflecting the fact that codes 128-255 in Unicode are the "same" as
 * Latin-1.  However, Solaris' wide ctypes give only "non-space" results for
 * 128-255.  Since we're using these functions in vile 9.6 only for the normal
 * ctypes (the narrow 8-bit locale), just use the normal ctype functions.
 */

#include <estruct.h>
#include <edef.h>

#if OPT_LOCALE
#include <locale.h>
#endif /* OPT_LOCALE */

static CHARTYPE *ctype_sets;
static CHARTYPE *ctype_clrs;

/* initialize our version of the "chartypes" stuff normally in ctypes.h */
/* also called later, if charset-affecting modes change, for instance */
void
vl_ctype_init(int print_lo, int print_hi)
{
#if OPT_LOCALE
    char *save_ctype = setlocale(LC_CTYPE, NULL);
#endif
    int c;

    TRACE((T_CALLED "vl_ctype_init() lo=%d, hi=%d\n",
	   print_lo,
	   print_hi));

    /* If we're using the locale functions, set our flags based on its
     * tables.  Note that just because you have 'setlocale()' doesn't mean
     * that the tables are present or correct.  But this is a start.
     *
     * NOTE:  Solaris8 and some versions of M$ incorrectly classify tab as a
     * printable character (ANSI C says control characters are not printable).
     * Ignore that (the former fixes it in Solaris9).
     */
#if OPT_LOCALE
    TRACE(("wide_locale:%s\n", NonNull(vl_wide_enc.locale)));
    TRACE(("narrow_locale:%s\n", NonNull(vl_narrow_enc.locale)));
    TRACE(("current_locale:%s\n", NonNull(save_ctype)));

    if (okCTYPE2(vl_narrow_enc))
	setlocale(LC_CTYPE, vl_narrow_enc.locale);
    else if (okCTYPE2(vl_wide_enc))
	setlocale(LC_CTYPE, vl_wide_enc.locale);

    for (c = 0; c < N_chars; c++) {
	if (print_hi > 0 && c > print_hi) {
	    setVlCTYPE(c, 0);
	} else if (!vl_8bit_builtin() && okCTYPE2(vl_narrow_enc)) {
	    setVlCTYPE(c, vl_ctype_bits(c, -TRUE));
	    vl_uppercase[c + 1] = (char) toupper(c);
	    vl_lowercase[c + 1] = (char) tolower(c);
	} else {
	    /* fallback to built-in character tables */
	    vl_8bit_ctype_init(okCTYPE2(vl_wide_enc), c);
	}
    }
#else /* ! OPT_LOCALE */
    (void) memset((char *) vl_chartypes_, 0, sizeof(vl_chartypes_));

    /* control characters */
    for (c = 0; c < ' '; c++)
	addVlCTYPE(c, vl_cntrl);
    addVlCTYPE(127, vl_cntrl);

    /* lowercase */
    for (c = 'a'; c <= 'z'; c++)
	addVlCTYPE(c, vl_lower);
#if OPT_ISO_8859
    for (c = 0xc0; c <= 0xd6; c++)
	addVlCTYPE(c, vl_lower);
    for (c = 0xd8; c <= 0xde; c++)
	addVlCTYPE(c, vl_lower);
#endif
    /* uppercase */
    for (c = 'A'; c <= 'Z'; c++)
	addVlCTYPE(c, vl_upper);
#if OPT_ISO_8859
    for (c = 0xdf; c <= 0xf6; c++)
	addVlCTYPE(c, vl_upper);
    for (c = 0xf8; c <= 0xff; c++)
	addVlCTYPE(c, vl_upper);
#endif

    /*
     * If you want to do this properly, compile-in locale support.
     */
    for (c = 0; c < N_chars; c++) {
	vl_uppercase[c + 1] = (char) c;
	vl_lowercase[c + 1] = (char) c;
	if (isAlpha(c)) {
	    if (isUpper(c)) {
		vl_lowercase[c + 1] = (char) (c ^ DIFCASE);
	    } else {
		vl_uppercase[c + 1] = (char) (c ^ DIFCASE);
	    }
	}
    }

    /* digits */
    for (c = '0'; c <= '9'; c++)
	addVlCTYPE(c, vl_digit);
#ifdef vl_xdigit
    /* hex digits */
    for (c = '0'; c <= '9'; c++)
	addVlCTYPE(c, vl_xdigit);
    for (c = 'a'; c <= 'f'; c++)
	addVlCTYPE(c, vl_xdigit);
    for (c = 'A'; c <= 'F'; c++)
	addVlCTYPE(c, vl_xdigit);
#endif

    /* punctuation */
    for (c = '!'; c <= '/'; c++)
	addVlCTYPE(c, vl_punct);
    for (c = ':'; c <= '@'; c++)
	addVlCTYPE(c, vl_punct);
    for (c = '['; c <= '`'; c++)
	addVlCTYPE(c, vl_punct);
    for (c = L_CURLY; c <= '~'; c++)
	addVlCTYPE(c, vl_punct);
#if OPT_ISO_8859
    for (c = 0xa1; c <= 0xbf; c++)
	addVlCTYPE(c, vl_punct);
#endif

    /* printable */
    for (c = ' '; c <= '~'; c++)
	addVlCTYPE(c, vl_print);

    /* whitespace */
    addVlCTYPE(' ', vl_space);
#if OPT_ISO_8859
    addVlCTYPE(0xa0, vl_space);
#endif
    addVlCTYPE('\t', vl_space);
    addVlCTYPE('\r', vl_space);
    addVlCTYPE('\n', vl_space);
    addVlCTYPE('\f', vl_space);

#endif /* OPT_LOCALE */

    /* legal in pathnames */
    addVlCTYPE('.', vl_pathn);
    addVlCTYPE('_', vl_pathn);
    addVlCTYPE('~', vl_pathn);
    addVlCTYPE('-', vl_pathn);
    addVlCTYPE('/', vl_pathn);

    /* legal in "identifiers" */
    addVlCTYPE('_', vl_ident | vl_qident);
    addVlCTYPE(':', vl_qident);
#if SYS_VMS
    addVlCTYPE('$', vl_ident | vl_qident);
#endif

    c = print_lo;

    /*
     * Guard against setting printing-high before printing-low while we have a
     * buffer which may be repainted and possibly trashing the display.
     */
    if (c == 0
	&& print_hi >= 254)
	c = 160;

    if (c < HIGHBIT)
	c = HIGHBIT;
    TRACE(("Forcing printable for [%d..min(%d,%d)]\n",
	   c, print_hi - 1, N_chars - 1));
    while (c <= print_hi && c < N_chars)
	addVlCTYPE(c++, vl_print);

#if DISP_X11
    for (c = 0; c < N_chars; c++) {
	if (isPrint(c) && !gui_isprint(c)) {
	    clrVlCTYPE(c, vl_print);
	}
    }
#endif
    /* backspacers: ^H, rubout */
    addVlCTYPE('\b', vl_bspace);
    addVlCTYPE(127, vl_bspace);

    /* wildcard chars for most shells */
    addVlCTYPE('*', vl_wild);
    addVlCTYPE('?', vl_wild);
#if !OPT_VMS_PATH
#if SYS_UNIX
    addVlCTYPE('~', vl_wild);
#endif
    addVlCTYPE(L_BLOCK, vl_wild);
    addVlCTYPE(R_BLOCK, vl_wild);
    addVlCTYPE(L_CURLY, vl_wild);
    addVlCTYPE(R_CURLY, vl_wild);
    addVlCTYPE('$', vl_wild);
    addVlCTYPE('`', vl_wild);
#endif

    /* ex mode line specifiers */
    addVlCTYPE(',', vl_linespec);
    addVlCTYPE('%', vl_linespec);
    addVlCTYPE('-', vl_linespec);
    addVlCTYPE('+', vl_linespec);
    addVlCTYPE(';', vl_linespec);
    addVlCTYPE('.', vl_linespec);
    addVlCTYPE('$', vl_linespec);
    addVlCTYPE('\'', vl_linespec);

    /* fences */
    addVlCTYPE(L_CURLY, vl_fence);
    addVlCTYPE(R_CURLY, vl_fence);
    addVlCTYPE(L_PAREN, vl_fence);
    addVlCTYPE(R_PAREN, vl_fence);
    addVlCTYPE(L_BLOCK, vl_fence);
    addVlCTYPE(R_BLOCK, vl_fence);

#if OPT_VMS_PATH
    addVlCTYPE(L_BLOCK, vl_pathn);
    addVlCTYPE(R_BLOCK, vl_pathn);
    addVlCTYPE(L_ANGLE, vl_pathn);
    addVlCTYPE(R_ANGLE, vl_pathn);
    addVlCTYPE('$', vl_pathn);
    addVlCTYPE(':', vl_pathn);
    addVlCTYPE(';', vl_pathn);
#endif

#if OPT_MSDOS_PATH
    addVlCTYPE(BACKSLASH, vl_pathn);
    addVlCTYPE(':', vl_pathn);
#endif

#if OPT_WIDE_CTYPES
    /* scratch-buffer-names (usually superset of vl_pathn) */
    addVlCTYPE(SCRTCH_LEFT[0], vl_scrtch);
    addVlCTYPE(SCRTCH_RIGHT[0], vl_scrtch);
    addVlCTYPE(' ', vl_scrtch);	/* ...to handle "[Buffer List]" */
#endif

    for (c = 0; c < N_chars; c++) {
	if (!(isSpace(c)))
	    addVlCTYPE(c, vl_nonspace);
	if (isDigit(c))
	    addVlCTYPE(c, vl_linespec);
	if (isAlpha(c) || isDigit(c))
	    addVlCTYPE(c, vl_ident | vl_pathn | vl_qident);
#if OPT_WIDE_CTYPES
	if (isSpace(c) || isPrint(c))
	    addVlCTYPE(c, vl_shpipe);
	if (ispath(c))
	    addVlCTYPE(c, vl_scrtch);
#endif
    }

#if OPT_LOCALE
    if (save_ctype != NULL)
	(void) setlocale(LC_CTYPE, save_ctype);
#endif

    returnVoid();
}

/*
 * Return the character-type bits for the given character.  There are several
 * cases.
 *
 * vile supports a 256-entry table for "character classes", which are used
 * mainly to support systems with single-byte encodings.  Some of those (no all
 * older systems) may have incorrect character types; that is the reason for
 * having the ability to change classes at runtime.
 *
 * If use_locale is TRUE, this uses the system's character type functions,
 * (wide if available) e.g., for Unicode.  However, we still allow the
 * character-classes to override.  The simple case is where the wide/narrow
 * encodings coincide (up to latin1_codes).
 *
 * A more complicated case is for narrow encodings such as ISO-8859-2, where
 * latin_codes is less than 256.  Then we have to check first if it corresponds
 * to the narrow encoding before using the system's character type functions.
 *
 * If use_locale is -TRUE (negative), then use the system's 8-bit character
 * tests to get the narrow locale information used as a starting point for the
 * character classes.  On some systems, this may give odd results, but that is
 * why it is configurable.
 *
 * If use_locale is FALSE, then use the 256-entry table of character classes.
 */
CHARTYPE
vl_ctype_bits(int ch, int use_locale GCC_UNUSED)
{
    CHARTYPE result = 0;

    if (ch < 0) {
	;
    }
#if OPT_LOCALE
    else if (use_locale > 0) {
	int check;

	/* handle case where character-classes can be overridden */
	if (ch < latin1_codes) {
	    result = vlCTYPE(ch);
	    ch = -1;
	} else if (vl_ucs_to_8bit(&check, ch)) {
	    result = vlCTYPE(check);
	    ch = -1;
	}

	if (ch >= 0) {
	    if (sys_isalpha(ch))
		result |= (vl_ident | vl_pathn | vl_qident);
	    if (sys_iscntrl(ch))
		result |= (vl_cntrl);
	    if (sys_isdigit(ch))
		result |= (vl_digit | vl_ident | vl_pathn | vl_qident);
	    if (sys_islower(ch))
		result |= vl_lower;
	    if (sys_isprint(ch) && ch != '\t')
		result |= vl_print;
	    if (sys_ispunct(ch))
		result |= vl_punct;
	    if (sys_isspace(ch))
		result |= vl_space;
	    else
		result |= vl_nonspace;
	    if (sys_isupper(ch))
		result |= vl_upper;
#ifdef vl_xdigit
	    if (sys_isxdigit(ch))
		result |= vl_xdigit;
#endif
	}
    } else if (use_locale < 0) {
	if (isalpha(ch))
	    result |= (vl_ident | vl_pathn | vl_qident);
	if (iscntrl(ch))
	    result |= (vl_cntrl);
	if (isdigit(ch))
	    result |= (vl_digit | vl_ident | vl_pathn | vl_qident);
	if (islower(ch))
	    result |= vl_lower;
	if (isprint(ch) && ch != '\t')
	    result |= vl_print;
	if (ispunct(ch))
	    result |= vl_punct;
	if (isspace(ch))
	    result |= vl_space;
	else
	    result |= vl_nonspace;
	if (isupper(ch))
	    result |= vl_upper;
#ifdef vl_xdigit
	if (isxdigit(ch))
	    result |= vl_xdigit;
#endif
    } else
#endif /* OPT_LOCALE */
    if (ch < N_chars)
	result = vlCTYPE(ch);
    return result;
}

/*
 * Reapply set/clr customizations to the character class data, e.g., after
 * calling vl_ctype_init().
 */
void
vl_ctype_apply(void)
{
    unsigned n;

    TRACE(("vl_ctype_apply\n"));
    if (ctype_sets) {
	for (n = 0; n < N_chars; n++) {
	    addVlCTYPE(n, ctype_sets[n]);
	    TRACE(("...set %d:%#lx\n", n, (ULONG) vlCTYPE(n)));
	}
    }
    if (ctype_clrs) {
	for (n = 0; n < N_chars; n++) {
	    clrVlCTYPE(n, ctype_clrs[n]);
	    TRACE(("...clr %d:%#lx\n", n, (ULONG) vlCTYPE(n)));
	}
    }
}

/*
 * Discard all set/clr customizations.
 */
void
vl_ctype_discard(void)
{
    FreeAndNull(ctype_sets);
    FreeAndNull(ctype_clrs);
}

/*
 * Set the given character class for the given character.
 */
void
vl_ctype_set(int ch, CHARTYPE cclass)
{
    TRACE(("vl_ctype_set %d:%#lx\n", ch, (ULONG) cclass));

    if (ctype_sets == NULL) {
	ctype_sets = typecallocn(CHARTYPE, (size_t) N_chars);
    }
    if (ctype_sets != NULL) {
	ctype_sets[ch] |= cclass;
	addVlCTYPE(ch, cclass);
    }
    if (ctype_clrs != NULL) {
	ctype_clrs[ch] &= ~cclass;
    }
}

void
vl_ctype_clr(int ch, CHARTYPE cclass)
{
    TRACE(("vl_ctype_clr %d:%#lx\n", ch, (ULONG) cclass));

    if (ctype_clrs == NULL) {
	ctype_clrs = typecallocn(CHARTYPE, (size_t) N_chars);
    }
    if (ctype_clrs != NULL) {
	ctype_clrs[ch] |= cclass;
	clrVlCTYPE(ch, cclass);
    }
    if (ctype_sets != NULL) {
	ctype_sets[ch] &= ~cclass;
    }
}

#if NO_LEAKS
void
vl_ctype_leaks(void)
{
    FreeAndNull(ctype_sets);
    FreeAndNull(ctype_clrs);
}
#endif
