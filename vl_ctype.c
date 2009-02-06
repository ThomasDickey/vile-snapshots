/*
 * $Header: /users/source/archives/vile.vcs/RCS/vl_ctype.c,v 1.1 2009/02/06 00:44:39 tom Exp $
 */
#include <estruct.h>
#include <edef.h>

#if OPT_LOCALE
#include	<locale.h>

/*
 * On Linux, the normal/wide ctypes give comparable results in the range 0-255,
 * reflecting the fact that codes 128-255 in Unicode are the "same" as
 * Latin-1.  However, Solaris' wide ctypes give only "non-space" results for
 * 128-255.  Since we're using these functions in vile 9.6 only for the normal
 * ctypes (the narrow 8-bit locale), just use the normal ctype functions.
 */
#if 0				/* def HAVE_WCTYPE */
#include	<wctype.h>
#define sys_iscntrl(n)  iswcntrl(n)
#define sys_isdigit(n)  iswdigit(n)
#define sys_islower(n)  iswlower(n)
#define sys_isprint(n)  iswprint(n)
#define sys_ispunct(n)  iswpunct(n)
#define sys_isspace(n)  iswspace(n)
#define sys_isupper(n)  iswupper(n)
#define sys_isxdigit(n) iswxdigit(n)
#else
#define sys_iscntrl(n)  iscntrl(n)
#define sys_isdigit(n)  isdigit(n)
#define sys_islower(n)  islower(n)
#define sys_isprint(n)  isprint(n)
#define sys_ispunct(n)  ispunct(n)
#define sys_isspace(n)  isspace(n)
#define sys_isupper(n)  isupper(n)
#define sys_isxdigit(n) isxdigit(n)
#endif

#endif /* OPT_LOCALE */

/* initialize our version of the "chartypes" stuff normally in ctypes.h */
/* also called later, if charset-affecting modes change, for instance */
void
vl_ctype_init(int print_lo, int print_hi)
{
    int c;

    TRACE((T_CALLED "vl_charinit() lo=%d, hi=%d\n",
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
    for (c = 0; c < N_chars; c++) {
	if (okCTYPE2(vl_narrow_enc)) {
	    vlCTYPE(c) = 0;
	    if (sys_iscntrl(c))
		vlCTYPE(c) |= vl_cntrl;
	    if (sys_isdigit(c))
		vlCTYPE(c) |= vl_digit;
	    if (sys_islower(c))
		vlCTYPE(c) |= vl_lower;
	    if (sys_isprint(c) && c != '\t')
		vlCTYPE(c) |= vl_print;
	    if (sys_ispunct(c))
		vlCTYPE(c) |= vl_punct;
	    if (sys_isspace(c))
		vlCTYPE(c) |= vl_space;
	    if (sys_isupper(c))
		vlCTYPE(c) |= vl_upper;
#ifdef vl_xdigit
	    if (sys_isxdigit(c))
		vlCTYPE(c) |= vl_xdigit;
#endif
	    vl_uppercase[c] = (char) toupper(c);
	    vl_lowercase[c] = (char) tolower(c);
	} else {
	    /* fallback to built-in character tables */
	    if (okCTYPE2(vl_wide_enc)) {
		vlCTYPE(c) = vl_ctype_latin1[c];
	    } else {
		vlCTYPE(c) = vl_ctype_ascii[c];
	    }
	    vl_uppercase[c] = (char) c;
	    vl_lowercase[c] = (char) c;
	    if (isAlpha(c)) {
		if (isUpper(c)) {
		    vl_lowercase[c] = (char) (c ^ DIFCASE);
		} else {
		    vl_uppercase[c] = (char) (c ^ DIFCASE);
		}
	    }
	}
    }
#else /* ! OPT_LOCALE */
    (void) memset((char *) vl_chartypes_, 0, sizeof(vl_chartypes_));

    /* control characters */
    for (c = 0; c < ' '; c++)
	vlCTYPE(c) |= vl_cntrl;
    vlCTYPE(127) |= vl_cntrl;

    /* lowercase */
    for (c = 'a'; c <= 'z'; c++)
	vlCTYPE(c) |= vl_lower;
#if OPT_ISO_8859
    for (c = 0xc0; c <= 0xd6; c++)
	vlCTYPE(c) |= vl_lower;
    for (c = 0xd8; c <= 0xde; c++)
	vlCTYPE(c) |= vl_lower;
#endif
    /* uppercase */
    for (c = 'A'; c <= 'Z'; c++)
	vlCTYPE(c) |= vl_upper;
#if OPT_ISO_8859
    for (c = 0xdf; c <= 0xf6; c++)
	vlCTYPE(c) |= vl_upper;
    for (c = 0xf8; c <= 0xff; c++)
	vlCTYPE(c) |= vl_upper;
#endif

    /*
     * If you want to do this properly, compile-in locale support.
     */
    for (c = 0; c < N_chars; c++) {
	vl_uppercase[c] = (char) c;
	vl_lowercase[c] = (char) c;
	if (isAlpha(c)) {
	    if (isUpper(c)) {
		vl_lowercase[c] = (char) (c ^ DIFCASE);
	    } else {
		vl_uppercase[c] = (char) (c ^ DIFCASE);
	    }
	}
    }

    /* digits */
    for (c = '0'; c <= '9'; c++)
	vlCTYPE(c) |= vl_digit;
#ifdef vl_xdigit
    /* hex digits */
    for (c = '0'; c <= '9'; c++)
	vlCTYPE(c) |= vl_xdigit;
    for (c = 'a'; c <= 'f'; c++)
	vlCTYPE(c) |= vl_xdigit;
    for (c = 'A'; c <= 'F'; c++)
	vlCTYPE(c) |= vl_xdigit;
#endif

    /* punctuation */
    for (c = '!'; c <= '/'; c++)
	vlCTYPE(c) |= vl_punct;
    for (c = ':'; c <= '@'; c++)
	vlCTYPE(c) |= vl_punct;
    for (c = '['; c <= '`'; c++)
	vlCTYPE(c) |= vl_punct;
    for (c = L_CURLY; c <= '~'; c++)
	vlCTYPE(c) |= vl_punct;
#if OPT_ISO_8859
    for (c = 0xa1; c <= 0xbf; c++)
	vlCTYPE(c) |= vl_punct;
#endif

    /* printable */
    for (c = ' '; c <= '~'; c++)
	vlCTYPE(c) |= vl_print;

    /* whitespace */
    vlCTYPE(' ') |= vl_space;
#if OPT_ISO_8859
    vlCTYPE(0xa0) |= vl_space;
#endif
    vlCTYPE('\t') |= vl_space;
    vlCTYPE('\r') |= vl_space;
    vlCTYPE('\n') |= vl_space;
    vlCTYPE('\f') |= vl_space;

#endif /* OPT_LOCALE */

    /* legal in pathnames */
    vlCTYPE('.') |= vl_pathn;
    vlCTYPE('_') |= vl_pathn;
    vlCTYPE('~') |= vl_pathn;
    vlCTYPE('-') |= vl_pathn;
    vlCTYPE('/') |= vl_pathn;

    /* legal in "identifiers" */
    vlCTYPE('_') |= vl_ident | vl_qident;
    vlCTYPE(':') |= vl_qident;
#if SYS_VMS
    vlCTYPE('$') |= vl_ident | vl_qident;
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
	vlCTYPE(c++) |= vl_print;

#if DISP_X11
    for (c = 0; c < N_chars; c++) {
	if (isPrint(c) && !gui_isprint(c)) {
	    vlCTYPE(c) &= ~vl_print;
	}
    }
#endif
    /* backspacers: ^H, rubout */
    vlCTYPE('\b') |= vl_bspace;
    vlCTYPE(127) |= vl_bspace;

    /* wildcard chars for most shells */
    vlCTYPE('*') |= vl_wild;
    vlCTYPE('?') |= vl_wild;
#if !OPT_VMS_PATH
#if SYS_UNIX
    vlCTYPE('~') |= vl_wild;
#endif
    vlCTYPE(L_BLOCK) |= vl_wild;
    vlCTYPE(R_BLOCK) |= vl_wild;
    vlCTYPE(L_CURLY) |= vl_wild;
    vlCTYPE(R_CURLY) |= vl_wild;
    vlCTYPE('$') |= vl_wild;
    vlCTYPE('`') |= vl_wild;
#endif

    /* ex mode line specifiers */
    vlCTYPE(',') |= vl_linespec;
    vlCTYPE('%') |= vl_linespec;
    vlCTYPE('-') |= vl_linespec;
    vlCTYPE('+') |= vl_linespec;
    vlCTYPE(';') |= vl_linespec;
    vlCTYPE('.') |= vl_linespec;
    vlCTYPE('$') |= vl_linespec;
    vlCTYPE('\'') |= vl_linespec;

    /* fences */
    vlCTYPE(L_CURLY) |= vl_fence;
    vlCTYPE(R_CURLY) |= vl_fence;
    vlCTYPE(L_PAREN) |= vl_fence;
    vlCTYPE(R_PAREN) |= vl_fence;
    vlCTYPE(L_BLOCK) |= vl_fence;
    vlCTYPE(R_BLOCK) |= vl_fence;

#if OPT_VMS_PATH
    vlCTYPE(L_BLOCK) |= vl_pathn;
    vlCTYPE(R_BLOCK) |= vl_pathn;
    vlCTYPE(L_ANGLE) |= vl_pathn;
    vlCTYPE(R_ANGLE) |= vl_pathn;
    vlCTYPE('$') |= vl_pathn;
    vlCTYPE(':') |= vl_pathn;
    vlCTYPE(';') |= vl_pathn;
#endif

#if OPT_MSDOS_PATH
    vlCTYPE(BACKSLASH) |= vl_pathn;
    vlCTYPE(':') |= vl_pathn;
#endif

#if OPT_WIDE_CTYPES
    /* scratch-buffer-names (usually superset of vl_pathn) */
    vlCTYPE(SCRTCH_LEFT[0]) |= vl_scrtch;
    vlCTYPE(SCRTCH_RIGHT[0]) |= vl_scrtch;
    vlCTYPE(' ') |= vl_scrtch;	/* ...to handle "[Buffer List]" */
#endif

    for (c = 0; c < N_chars; c++) {
	if (!(isSpace(c)))
	    vlCTYPE(c) |= vl_nonspace;
	if (isDigit(c))
	    vlCTYPE(c) |= vl_linespec;
	if (isAlpha(c) || isDigit(c))
	    vlCTYPE(c) |= vl_ident | vl_pathn | vl_qident;
#if OPT_WIDE_CTYPES
	if (isSpace(c) || isPrint(c))
	    vlCTYPE(c) |= vl_shpipe;
	if (ispath(c))
	    vlCTYPE(c) |= vl_scrtch;
#endif
    }
    returnVoid();
}
