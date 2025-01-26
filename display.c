/*
 * The functions in this file handle redisplay. There are two halves, the
 * ones that update the virtual display screen, and the ones that make the
 * physical display screen the same as the virtual display screen. These
 * functions use hints that are left in the windows by the commands.
 *
 * $Id: display.c,v 1.585 2025/01/26 17:07:24 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"
#include	"pscreen.h"
#include	"nefsms.h"

#define vMAXINT ((int)((unsigned)(~0)>>1))	/* 0x7fffffff */
#define vMAXNEG (-vMAXINT)	/* 0x80000001 */

#define	NU_WIDTH 8
#define NU_GUTTER 1

#if DISP_X11
#define FontHasGlyph(ch) gui_isprint(ch)
#define UseCellWidth(wp, ch) \
	(!w_val(wp, WMDUNICODE_AS_HEX) && FontHasGlyph(ch))
#else
#define FontHasGlyph(ch) TRUE
#define UseCellWidth(wp, ch) \
	(!w_val(wp, WMDUNICODE_AS_HEX))
#endif

#define UseCellWidth2(wp, lp, off) \
	UseCellWidth(wp, get_char2(lp, off))

#define reset_term_attrs() term.rev(0)

#ifdef GVAL_VIDEO
#define VIDEOATTRS (VABOLD|VAUL|VAITAL)
#define set_term_attrs(a)  term.rev(a ^ ((VIDEO_ATTR) global_g_val(GVAL_VIDEO) & VIDEOATTRS))
#else
#define set_term_attrs(a)  term.rev(a)
#endif

#ifdef WMDLINEWRAP		/* overrides left/right scrolling */
#define if_LINEWRAP(wp, t, f) w_val(wp, WMDLINEWRAP) ? (t) : (f)
#else
#define if_LINEWRAP(wp, t, f) (f)
#endif

#define line_has_newline(lp, bp) \
	(b_val(bp, MDNEWLINE) || (lforw(lp) != buf_head(bp)))

#define	MRK_EMPTY        "~"
#define	MRK_EXTEND_LEFT  "<"
#define	MRK_EXTEND_RIGHT ">"

VIDEO **vscreen;		/* Virtual screen. */
VIDEO **pscreen;		/* Physical screen. */

typedef struct {
    LINE *lp;			/* the line itself */
    int map;			/* map to line-number in window */
    int left;			/* offset of leftmost character on line */
    int right;			/* offset of rightmost character on line */
} LMAP;
static LMAP *lmap0;		/* where to _not_ alter attributes */
static LMAP *lmap;		/* workspace for computing attributes */

#if OPT_SCROLLCODE
#define CAN_SCROLL 1
#else
#define CAN_SCROLL 0
#endif

static int i_displayed;		/* false until we're in screen-mode */
static int mpresf;		/* zero if message-line empty */
#if OPT_WORKING
static int im_timing;
#endif

/*
 * MARK2COL may be greater than mark2col if the mark does not point to the end
 * of the line, and if it points to a nonprinting character.  We use that value
 * when setting visible attributes, to keep tabs and other nonprinting
 * characters looking 'right'.
 */
#define MARK2COL(wp, mk)  offs2col(wp, mk.l, mk.o + BytesAt(mk.l, mk.o)) - 1
#define mark2col(wp, mk)  offs2col(wp, mk.l, mk.o)

#ifdef WMDLINEWRAP
#define TopRow(wp) (wp)->w_toprow + (wp)->w_line.o
static int allow_wrap;
#else
#define TopRow(wp) (wp)->w_toprow
#endif

/* for window size changes */
static int chg_width, chg_height;

/******************************************************************************/

typedef int (*OutFunc) (int c);

static OutFunc dfoutfn;

static int vtlistc(WINDOW *wp, const char *src, unsigned limit);

/*--------------------------------------------------------------------------*/

/*
 * Format a number, right-justified, returning a pointer to the formatted
 * buffer.
 */
static char *
right_num(char *buffer, int len, long value)
{
    char temp[NSTRING];
    char *p = lsprintf(temp, "%ld", value);
    char *q = buffer + len;

    *q = EOS;
    while (q != buffer)
	*(--q) = (char) ((p != temp) ? *(--p) : ' ');
    return buffer;
}

/*
 * Do format a string.  Return the number of bytes by which 'width' exceeds
 * that actually written to the output.
 */
static int
dfputsn(OutFunc outfunc, const char *s, int width, int limit)
{
    int l = 0;

    if (s != NULL) {
	int length = (int) strlen(s);

	if (width < 0)
	    width = length;
	if (limit > 0 && width > limit)
	    width = limit;
	if (width > length)
	    width = length;

	TRACE2(("...str=%s\n", visible_buff(s, width, TRUE)));
	while (width-- > 0) {
	    (*outfunc) (*s++);
	    l++;
	}
    }
    return l;
}

/* as above, but uses null-terminated string's length */
static int
dfputs(OutFunc outfunc, const char *s)
{
    return dfputsn(outfunc, s, -1, -1);
}

/*
 * Do format an integer, in the specified radix.
 */
static int
dfputi(OutFunc outfunc, UINT i, UINT r)
{
    int q;

    TRACE2(("...int=%d\n", i));

    q = (i >= r) ? dfputi(outfunc, i / r, r) : 0;

    (*outfunc) (hexdigits[i % r]);
    return q + 1;		/* number of digits printed */
}

/*
 * do the same except as a long integer.
 */
static int
dfputli(OutFunc outfunc, ULONG l, UINT r)
{
    int q;

    TRACE2(("...long=%ld\n", l));

    q = (l >= r) ? dfputli(outfunc, (l / r), r) : 0;

    return q + dfputi(outfunc, (UINT) (l % r), r);
}

/*
 * format a floating value with two decimal places
 */
static int
dfputf(OutFunc outfunc, double s)
{
    int n = 3;
    UINT i;			/* integer portion of number */
    UINT f;			/* fractional portion of number */

    if (s < 0) {
	(*outfunc) ('.');
	s = -s;
	n++;
    }

    /* break it up */
    i = (UINT) s;
    f = (UINT) ((s - (double) i) * 100);

    /* send out the integer portion */
    n += dfputi(outfunc, i, 10);
    (*outfunc) ('.');
    (*outfunc) ((int) (f / 10) + '0');
    (*outfunc) ((int) (f % 10) + '0');
    return n;
}

/*
 * On entry, *fmt may point to either '*' or a digit.  If either, decode the
 * appropriate width or limit.
 */
static void
decode_length(const char **fmt, int *result)
{
    int c = **fmt;
    int value = 0;
    int found = FALSE;

    while (isDigit(c)) {
	value = (value * 10) + c - '0';
	found = TRUE;
	(*fmt)++;
	c = **fmt;
    }
    if (found)
	*result = value;
}

#define DecodeLength(fmt, app, result) \
	if (*fmt == '*') { \
	    result = va_arg(app, int); \
	    ++fmt; \
	} else { \
	    decode_length(&fmt, &result); \
	}

/*
 * Generic string formatter.  Takes printf-like args, and calls
 * the global function (*dfoutfn)(c) for each c
 */
static void
dofmt(const char *fmt, va_list app2)
{
    int c;			/* current char in format string */
    int the_width;
    int the_limit = 0;
    int n;
    int islong;
    UINT uint_value;
    ULONG ulong_value;
    int int_value;
    long long_value;
    va_list app;
    OutFunc outfunc = dfoutfn;	/* local copy, for recursion */

    TRACE2(("dofmt fmt='%s'\n", visible_buff(fmt, strlen(fmt), FALSE)));
    begin_va_copy(app, app2);
    while ((c = *fmt++) != 0) {
	if (c != '%') {
	    (*outfunc) (c);
	    continue;
	}
	the_width = -1;
	islong = FALSE;
	DecodeLength(fmt, app, the_width);
	if (*fmt == '.') {
	    ++fmt;
	    DecodeLength(fmt, app, the_limit);
	    if (the_width < 0)
		the_width = the_limit;
	} else {
	    the_limit = the_width;
	}
	c = *fmt++;
	if (c == 'l') {
	    islong = TRUE;
	    c = *fmt++;
	}
	TRACE2(("... fmt='%%%d.%d%c%s'\n",
		the_width, the_limit,
		c, islong ? "L" : ""));
	switch (c) {
	case EOS:
	    n = 0;
	    break;
	case 'c':
	    (*outfunc) (va_arg(app, int));
	    n = 1;
	    break;

	case 'd':
	    if (!islong) {
		int_value = va_arg(app, int);
		if (int_value < 0) {
		    if (int_value < vMAXNEG) {
			n = dfputs(outfunc, "OVFL");
			break;
		    }
		    int_value = -int_value;
		    (*outfunc) ('-');
		}
		n = dfputi(outfunc, (UINT) int_value, 10);
		break;
	    }
	    long_value = va_arg(app, long);
	    if (long_value < 0) {
		long_value = -long_value;
		(*outfunc) ('-');
	    }
	    n = dfputli(outfunc, (ULONG) long_value, 10);
	    break;

	case 'u':
	    if (!islong) {
		uint_value = va_arg(app, UINT);
		n = dfputi(outfunc, uint_value, 10);
		break;
	    }
	    ulong_value = va_arg(app, ULONG);
	    n = dfputli(outfunc, ulong_value, 10);
	    break;

	case 'o':
	    n = dfputi(outfunc, va_arg(app, UINT), 8);
	    break;

	case 'x':
	    if (!islong) {
		n = dfputi(outfunc, va_arg(app, UINT), 16);
		break;
	    }
	    /* FALLTHROUGH */
	case 'X':
	    n = dfputli(outfunc, va_arg(app, ULONG), 16);
	    break;

	case 's':
	    n = dfputsn(outfunc, va_arg(app, char *), the_width, the_limit);
	    break;

	case 'f':
	    n = dfputf(outfunc, va_arg(app, double));
	    break;

	default:
	    (*outfunc) (c);
	    n = 1;
	    break;
	}
	the_width -= n;
	while (the_width-- > 0) {
	    (*outfunc) (' ');
	}
    }
    end_va_copy(app);
}

/******************************************************************************/

/*
 * Line-number mode
 */
int
nu_width(WINDOW *wp)
{
    return w_val(wp, WMDNUMBER) ? NU_WIDTH : 0;
}

int
col_limit(WINDOW *wp)
{
    return if_LINEWRAP(wp, curcol + 1, term.cols - 1 - nu_width(wp));
}

#if OPT_VIDEO_ATTRS
static void
set_vattrs(int row, int col, unsigned attr, size_t len)
{
    if ((long) len > 0) {
	while (len--)
	    vscreen[row]->v_attrs[col++] = (VIDEO_ATTR) attr;
    }
}

static void
preset_lmap0(void)
{
    int row;
    for (row = 0; row <= term.rows; ++row) {
	lmap0[row].left = 0;
	lmap0[row].right = term.cols;
    }
}

/* use this to markup line-numbers and line-break padding, which conveniently
 * are both on the extreme left/right of the line on the display.
 */
#if OPT_EXTRA_COLOR
static void
preset_vattrs(int row, int col, int attr, size_t len)
{
    set_vattrs(row, col, (unsigned) attr, len);
    if (col == 0) {
	lmap0[row].left = (int) len;
    } else {
	lmap0[row].right = col;
    }
}
#endif

#else

#define preset_lmap0()		/* nothing */
#define set_vattrs(row, col, attr, len)		/*nothing */
#define preset_vattrs(row, col, attr, len)	/*nothing */

#endif

static void
freeVIDEO(VIDEO * vp)
{
    if (vp != NULL) {
#if OPT_VIDEO_ATTRS
	FreeIfNeeded(VideoAttr(vp));
#endif
	free((char *) vp);
    }
}

int
video_alloc(VIDEO ** vpp)
{
    VIDEO *vp;
    size_t have = (size_t) ((term.maxcols > 0) ? term.maxcols : 1);
    size_t need = sizeof(VIDEO_TEXT) * (have - 1);

    if ((vp = typeallocplus(VIDEO, need)) != NULL) {
	(void) memset((char *) vp, 0, sizeof(VIDEO) + need);

#if OPT_VIDEO_ATTRS
	VideoAttr(vp) = typecallocn(VIDEO_ATTR, have);
	if (VideoAttr(vp) == NULL) {
	    FreeAndNull(vp);
	}
#endif
	if (vp != NULL) {
	    freeVIDEO(*vpp);
	    *vpp = vp;
	}
    }
    return (vp != NULL);
}

static int
vtalloc(void)
{
    int i, first;
    static int vcols, vrows;

    if (term.maxrows > vrows) {
	GROW(vscreen, VIDEO *, vrows, term.maxrows);
	GROW(pscreen, VIDEO *, vrows, term.maxrows);
	GROW(lmap0, LMAP, vrows, ((size_t) term.maxrows + 1));
	GROW(lmap, LMAP, vrows, ((size_t) term.maxrows + 1));
    } else {
	for (i = term.maxrows; i < vrows; i++) {
	    freeVIDEO(vscreen[i]);
	    freeVIDEO(pscreen[i]);
	}
    }

    first = (term.maxcols > vcols) ? 0 : vrows;

    for (i = first; i < term.maxrows; ++i) {
	if (!video_alloc(&vscreen[i]))
	    return FALSE;
	if (!video_alloc(&pscreen[i]))
	    return FALSE;
    }
    vcols = term.maxcols;
    vrows = term.maxrows;

    return TRUE;
}

/* free all video memory, in anticipation of a (growing) resize */
#if NO_LEAKS
static void
vtfree(void)
{
    int i;

    if (vscreen) {
	for (i = 0; i < term.maxrows; ++i) {
	    freeVIDEO(vscreen[i]);
	}
	free((char *) vscreen);
	vscreen = NULL;
    }
    if (pscreen) {
	for (i = 0; i < term.maxrows; ++i) {
	    freeVIDEO(pscreen[i]);
	}
	free((char *) pscreen);
	pscreen = NULL;
    }
    FreeIfNeeded(lmap0);
    FreeIfNeeded(lmap);
}
#endif

/*
 * Returns true if the virtual screen has been allocated and initialized,
 * so we're able to create/update windows.
 */
int
is_vtinit(void)
{
    return (vscreen != NULL);
}

/*
 * Initialize the data structures used by the display code. The edge vectors
 * used to access the screens are set up.
 */
int
vtinit(void)
{
    int rc = TRUE;
    int i;
    VIDEO *vp;

    beginDisplay();
    /* allocate new display memory */
    if (vtalloc() == FALSE) {	/* if we fail, only serious if not a realloc */
	rc = is_vtinit();
    } else {
	for (i = 0; i < term.maxrows; ++i) {
	    vp = vscreen[i];
	    vp->v_flag = 0;
#if OPT_COLOR
	    ReqFcolor(vp) = gfcolor;
	    ReqBcolor(vp) = gbcolor;
#endif
	}
#if OPT_WORKING
	if (!i_displayed && !im_displaying)
	    imworking(0);
#endif
    }
    endofDisplay();
    return rc;
}

/*
 * Set the virtual cursor to the specified row and column on the virtual
 * screen. There is no checking for nonsense values.
 */
static void
vtmove(int row, int col)
{
    vtrow = row;
    vtcol = col;
}

static VIDEO *
current_video(void)
{
    VIDEO *vp;

#ifdef WMDLINEWRAP
    if (vtrow < 0) {
	static VIDEO *fake_line;
	static int length;
	if (length != term.maxcols || fake_line == NULL) {
	    if (video_alloc(&fake_line)) {
		length = term.maxcols;
	    } else {
		freeVIDEO(fake_line);
		fake_line = NULL;
	    }
	}
	vp = fake_line;
    } else
#endif
	vp = vscreen[vtrow];

    return vp;
}

static void
mark_extent(int row, int col, int ch)
{
    if (row >= 0 && row < term.maxrows && col >= 0 && col < term.maxcols) {
#if OPT_EXTRA_COLOR && OPT_VIDEO_ATTRS
	int *attrp = lookup_extra_color(XCOLOR_LINEBREAK);
	if (!isEmpty(attrp)) {
	    set_vattrs(row, col, (unsigned) *attrp, 1);
	    if (ch == MRK_EXTEND_LEFT[0]) {
		lmap0[row].left = col + 1;
	    } else {
		lmap0[row].right = col;
	    }
	}
#endif /* OPT_EXTRA_COLOR */
	vscreen[row]->v_text[col] = (VIDEO_TEXT) ch;
    }
}

static int
current_limit(void)
{
    return ((vtrow == term.rows - 1)
	    ? (term.cols - 1)
	    : term.cols);
}

/*
 * Given an offset limit and a string, find the offset of the first blank
 * within that limit.  Compute the corresponding number of columns.
 */
#ifdef WMDLINEWRAP
static unsigned
cols_until(WINDOW *wp, const char *src, unsigned limit)
{
    unsigned n;
    unsigned cols = 0;
    int used;
    int nxt;
    int adj;

    for (n = 0; n < limit; ++n) {
	if (isBlank(src[n])) {
	    break;
	}
	used = 1;
	nxt = (int) (limit - n);
	adj = column_sizes(wp, src + n, (unsigned) nxt, &used);

#if OPT_MULTIBYTE
	if (adj == COLS_UTF8
	    && b_is_utfXX(wp->w_bufp)
	    && !w_val(wp, WMDUNICODE_AS_HEX)) {
	    UINT target;
	    int ch2;

	    if (vl_conv_to_utf32(&target,
				 src + n,
				 (B_COUNT) (limit - n)) > 0) {
		ch2 = (int) target;
	    } else {
		ch2 = CharOf(src[n]);
	    }
	    (void) ch2;
	    if (FontHasGlyph(ch2)) {
		adj = mb_cellwidth(wp, src + n, nxt);
	    }
	}
#endif
	cols += (unsigned) adj;
	n += (unsigned) (used - 1);
    }
    return cols;
}
#endif

/*
 * Write a character to the virtual screen.  The virtual row and column are
 * updated.  Only print characters if they would be "visible".  If the line is
 * too long put a ">" in the last column.  This routine only puts printing
 * characters into the virtual terminal buffers.  Only column overflow is
 * checked.
 *
 * Returns the number of bytes eaten from the source.
 */
static int
vtputc(WINDOW *wp, const char *src, unsigned limit)
{
    /* since we don't allow wrapping on the message line, we only need
     * to evaluate this once.  */
    int lastcol = current_limit();
    int rc = 1;
    UINT ch = (UCHAR) (*src);
    VIDEO *vp = current_video();

    if (vp != NULL) {
	if (isPrint(ch) && vtcol >= 0 && vtcol < lastcol) {
#if OPT_MULTIBYTE
	    if (!b_is_utfXX(wp->w_bufp) && (vl_encoding >= enc_UTF8)) {
		int ch2;
		if (vl_8bit_to_ucs(&ch2, (int) ch)) {
		    ch = (UINT) ch2;
		}
	    }
#endif
#ifdef WMDLINEWRAP
	    if ((allow_wrap != 0)
		&& w_val(wp, WMDLINEBREAK)
		&& w_val(wp, WMDLINEWRAP)
		&& vtcol > 0
		&& VideoText(vp)[vtcol - 1] == ' '
		&& !isBlank(ch)) {
		unsigned n = cols_until(wp, src, limit);
		int have = term.cols - vtcol;

		if (n < limit)
		    ++n;
		if ((int) n > have) {
#if OPT_EXTRA_COLOR
		    int *attrp = lookup_extra_color(XCOLOR_LINEBREAK);
		    if (!isEmpty(attrp)) {
			preset_vattrs(vtrow, vtcol, *attrp,
				      (size_t) term.cols - (size_t) vtcol);
		    }
#endif /* OPT_EXTRA_COLOR */
		    while (vtcol < term.cols)
			VideoText(vp)[vtcol++] = ' ';
		    vtcol = 0;
		    ++vtrow;
		    vp = current_video();
		    vp->v_flag |= VFCHG;
		    horscroll += lastcol;
		}
	    }
#endif
	    VideoText(vp)[vtcol++] = (VIDEO_TEXT) ch;
#ifdef WMDLINEWRAP
	    if ((allow_wrap != 0)
		&& (vtcol == lastcol)
		&& (vtrow < allow_wrap)) {
		vtcol = 0;
		if (++vtrow >= 0)
		    vscreen[vtrow]->v_flag |= VFCHG;
		horscroll += lastcol;
	    }
#endif
	} else if (vtcol >= lastcol) {
	    mark_extent(vtrow, lastcol - 1, MRK_EXTEND_RIGHT[0]);
	} else if (isTab(ch)) {
	    do {
		vtputc(wp, " ", 1);
	    } while (((vtcol + horscroll) % wp->w_tabstop) != 0
		     && vtcol < lastcol);
	} else if (!is_record_sep(wp->w_bufp, (int) ch)) {
	    if (isPrint(ch)) {
		++vtcol;
	    } else {
		rc = vtlistc(wp, src, limit);
	    }
	}
    }
    return rc;
}

static void
vtputsn(WINDOW *wp, const char *src, size_t n)
{
    if (src != NULL) {
	while (n != 0 && *src != EOS) {
	    vtputc(wp, src++, (unsigned) (n--));
	}
    }
}

/*
 * Shows non-printing character.  Returns the number of bytes eaten from the
 * source, which will always be one except when decoding UTF-8.
 */
static int
vtlistc(WINDOW *wp, const char *src, unsigned limit)
{
    int rc = 1;
    char temp[10];
    unsigned n = 0;

#if OPT_MULTIBYTE
    if (b_is_utfXX(wp->w_bufp)) {
	UINT value;
	int need = column_sizes(wp, src, limit, &rc);
	int cells;

	switch (need) {
	case COLS_UTF8:
	    rc = vl_conv_to_utf32(&value, src, (B_COUNT) limit);
	    cells = vl_wcwidth((int) value);

	    if (!w_val(wp, WMDUNICODE_AS_HEX)
		&& cells == 1
		&& isPrint(value)
		&& FoldTo8bits((int) value)) {
		temp[n++] = (char) value;
	    } else if (cells > 0
		       && term_is_utfXX()
		       && UseCellWidth(wp, (int) value)) {
		/*
		 * It does not fit into the "8bit" mapping, but is printable
		 * (since the number of cells is nonzero).  Write the Unicode
		 * value directly to the grid (do not try to recur back here
		 * again).
		 */
		VIDEO *vp = current_video();
		int lastcol = current_limit();

		if (vtcol >= 0 && vtcol + cells <= lastcol) {
		    /* FIXME - adapt the wrapping logic and margin marker too */
		    VideoText(vp)[vtcol++] = (VIDEO_TEXT) value;
		    /*
		     * Pad multicell values with 0's which the terminal driver
		     * has to remove.  The padding lets us update 'vtcol' in a
		     * simple way.
		     */
		    while (--cells > 0)
			VideoText(vp)[vtcol++] = 0;
#ifdef WMDLINEWRAP
		    if ((allow_wrap != 0)
			&& (vtcol == lastcol)
			&& (vtrow < allow_wrap)) {
			vtcol = 0;
			if (++vtrow >= 0)
			    vscreen[vtrow]->v_flag |= VFCHG;
			horscroll += lastcol;
		    }
#endif
		} else if (vtcol >= lastcol) {
		    if (VideoText(vp)[lastcol - 1] == 0)
			mark_extent(vtrow, lastcol - 2, MRK_EXTEND_RIGHT[0]);
		    mark_extent(vtrow, lastcol - 1, MRK_EXTEND_RIGHT[0]);
		} else {
		    int nulls = cells;
		    /* have to account for split-characters... */
		    while (cells-- > 0 && vtcol <= lastcol) {
			mark_extent(vtrow, vtcol++, MRK_EXTEND_RIGHT[0]);
		    }
#ifdef WMDLINEWRAP
		    if (allow_wrap != 0
			&& (vtrow < allow_wrap)) {
			vtcol = 0;
			if (++vtrow >= 0) {
			    vscreen[vtrow]->v_flag |= VFCHG;
			    vp = current_video();
			    VideoText(vp)[vtcol++] = (VIDEO_TEXT) value;
			    while (--nulls > 0)
				VideoText(vp)[vtcol++] = 0;
			}
		    }
#endif
		}
		return rc;
	    } else {
		/* FIXME - some of recode's UTF-8/UTF-16 gives 6-digit codes
		 * rather than 4 - but the low 4 digits are the same.
		 */
		sprintf(temp, "\\u%04X", value & ((1 << MaxCBits) - 1));
		n = (unsigned) need;
	    }
	    break;
	case COLS_CTRL:
	    if (isPrint(*src)) {
		temp[n++] = *src;
	    } else {
		temp[n++] = '^';
		temp[n++] = (char) toalpha(*src);
	    }
	    break;
	case COLS_8BIT:
	    /*
	     * Recover from illegal bytes...
	     */
	    TRACE2(("vtlistc illegal:%#x\n", CharOf(*src)));
	    sprintf(temp, "\\?%02X", CharOf(*src));
	    n = (unsigned) need;
	    break;
	default:
	    temp[n++] = *src;
	    break;
	}
    } else
#endif
    if (isPrint(*src)) {
	temp[n++] = *src;
    } else {
	int need = column_sizes(wp, src, limit, &rc);

	switch (need) {
	case COLS_8BIT:
	    temp[n++] = BACKSLASH;
	    if (w_val(wp, WMDNONPRINTOCTAL)) {
		temp[n++] = (char) (((*src >> 6) & 3) + '0');
		temp[n++] = (char) (((*src >> 3) & 7) + '0');
		temp[n++] = (char) (((*src) & 7) + '0');
	    } else {
		temp[n++] = 'x';
		temp[n++] = hexdigits[(*src >> 4) & 0xf];
		temp[n++] = hexdigits[(*src) & 0xf];
	    }
	    break;
	case COLS_CTRL:
	    temp[n++] = '^';
	    temp[n++] = (char) toalpha(*src);
	    break;
	default:
	    temp[n++] = *src;
	    break;
	}
    }
    vtputsn(wp, temp, (size_t) n);
    return rc;
}

#if OPT_MULTIBYTE
static int
vtset_put(WINDOW *wp, const char *src, unsigned limit)
{
    int rc;

    if (w_val(wp, WMDLIST)) {
	rc = vtlistc(wp, src, limit);
    } else if (b_is_utfXX(wp->w_bufp)
	       && column_sizes(wp, src, limit, &rc) >= COLS_8BIT) {
	rc = vtlistc(wp, src, limit);
    } else {
	rc = vtputc(wp, src, limit);
    }
    return rc;
}
#else
#define vtset_put(wp, src, n) (w_val(wp, WMDLIST) \
				? vtlistc(wp, src, n) \
				: vtputc(wp, src, n))
#endif
/*
 * Write a line to the screen at the current video coordinates, allowing for
 * line-wrap or right-shifting.
 */
static void
vtset(LINE *lp, WINDOW *wp)
{
#if OPT_TRACE > 1
    int save_vtrow = vtrow;
#endif
    char *from;
    int n = llength(lp);
    BUFFER *bp = wp->w_bufp;
    int skip = -vtcol;
    int list = w_val(wp, WMDLIST);

    TRACE2(("vtset line %d (top %d.%d), row %d \n",
	    line_no(wp->w_bufp, lp),
	    line_no(wp->w_bufp, wp->w_line.l),
	    wp->w_line.o,
	    vtrow));
    wp->w_tabstop = tabstop_val(wp->w_bufp);

#ifdef WMDLINEWRAP
    /*
     * If the window's offset is negative, we've got a case of linewrap
     * where the line's beginning is forced before the beginning of the
     * window.
     */
    if (wp->w_line.o < 0) {
	vtrow -= wp->w_line.o;
	skip = col2offs(wp, lp, -(wp->w_line.o * term.cols));
	n -= skip;
    } else
#endif
    if (w_val(wp, WMDNUMBER)) {
	int j, k, jk;
	L_NUM line = line_no(bp, lp);
	char temp[NU_WIDTH + 2];

	vtcol = 0;		/* make sure we always see line numbers */
	vtputsn(wp,
		right_num(temp, NU_WIDTH - NU_GUTTER, (long) line),
		(size_t) (NU_WIDTH - NU_GUTTER));
	vtputsn(wp, "  ", (size_t) NU_GUTTER);
#if OPT_EXTRA_COLOR
	{
	    int *attrp = lookup_extra_color(XCOLOR_LINENUMBER);
	    if (!isEmpty(attrp)) {
		preset_vattrs(vtrow, 0, *attrp, (NU_WIDTH - NU_GUTTER));
	    }
	}
#endif /* OPT_EXTRA_COLOR */
	horscroll = skip - vtcol;

	/* account for leading fill; this repeats logic in vtputc so
	 * I don't have to introduce a global variable... */
	from = lvalue(lp);
	for (j = k = jk = 0; (j < n) && (k < skip);) {
	    int c = from[j];
	    int used = 1;
	    if ((list || !isTab(c)) && !isPrint(c)) {
		k += column_sizes(wp, from + j, (UINT) (n - j), &used);
	    } else {
		if (isTab(c)) {
		    k += (wp->w_tabstop - (k % wp->w_tabstop));
		} else if (isPrint(c)) {
		    k++;
		}
	    }
	    j += used;
	    jk = j;
	}
	while (k-- > skip)
	    vtputc(wp, " ", 1);
	if ((skip = jk) < 0)
	    skip = 0;
	n -= skip;
    } else
	skip = 0;

#if OPT_B_LIMITS
    horscroll -= w_left_margin(wp);
#endif
    from = lvalue(lp) + skip;
#ifdef WMDLINEWRAP
    allow_wrap = w_val(wp, WMDLINEWRAP) ? mode_row(wp) - 1 : 0;
#endif

    /*
     * The loops below are split up for a reason - there's a hidden side effect
     * which makes the final increment of vtcol inconsistent, so it won't
     * terminate until the end of the line.  For very long lines, that's a
     * performance hit.
     */
#define VTSET_PUT() \
	int used = vtset_put(wp, from, (UINT) n); \
	from += used; n -= used

#define VTSET_LOOP(condition) \
	while ((vtcol < term.cols) && (condition)) { \
	    VTSET_PUT(); \
	} \
	if ((vtcol <= term.cols) && (condition)) { \
	    VTSET_PUT(); \
	}

#ifdef WMDLINEWRAP
    if (w_val(wp, WMDLINEWRAP)) {
	VTSET_LOOP(((vtrow == term.rows - 1) || (vtrow < mode_row(wp)))
		   && (n > 0));
    } else
#endif
    {
	VTSET_LOOP((n > 0));
    }

    /* Display a "^J" if 'list' mode is active, unless we've suppressed
     * it for some reason.
     */
    if (list && (n >= 0)) {
	if (b_is_scratch(bp) && listrimmed(lp))
	    /*EMPTY */ ;
	else if (line_has_newline(lp, bp)) {
	    const char *s = get_record_sep(bp);
	    UINT len = (UINT) len_record_sep(bp);
	    while (len != 0)
		vtlistc(wp, s++, len--);
	}
    }
#ifdef WMDLINEWRAP
    allow_wrap = 0;
#endif
#if OPT_TRACE > 1
    while (save_vtrow <= vtrow) {
	/*
	 * Display the row(s) which we just wrote to the virtual screen.
	 *
	 * FIXME - need to compute length from the number of columns.
	 * This is only assuming columns==cells.
	 */
	TRACE2(("TEXT %4d:%s\n",
		save_vtrow,
		visible_video_text(vscreen[save_vtrow]->v_text,
				   (save_vtrow == vtrow)
				   ? vtcol
				   : term.cols)));
	TRACE3(("ATTR     :%s\n",
		visible_video_attr(vscreen[save_vtrow]->v_attrs,
				   (save_vtrow == vtrow)
				   ? vtcol
				   : term.cols)));
	++save_vtrow;
    }
#endif
}

/*
 * Erase from the end of the software cursor to the end of the line on which
 * the software cursor is located.
 */
static void
vteeol(void)
{
    if (vtcol < term.cols) {
	int n = (vtcol >= 0) ? vtcol : 0;
#ifdef WMDLINEWRAP
	if (vtrow >= 0)
#endif
	    while (n < term.cols) {
		vscreen[vtrow]->v_text[n++] = ' ';
	    }
	vtcol = term.cols;
    }
}

/* upscreen:	user routine to force a screen update
		always finishes complete update		*/
#if !SMALLER
/* ARGSUSED */
int
upscreen(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return update(TRUE);
}
#endif

static UINT scrflags;

/*
 * Convert MARK's offset to virtual column.  The virtual column counts the left
 * margin, the sideways shift of the window, etc., up to the given MARK.
 *
 * The buffer may have a left-margin, e.g., [Registers].  Use the 'bp'
 * parameter to get that value.
 *
 * To allow this to be used in the OPT_CACHE_VCOL logic, it accepts a 'column'
 * parameter which in that case may be nonzero, e.g., the nominal column of the
 * left margin of the display (the column at which the sideways-shift
 * coincides).
 *
 * Finally, use 'adjust' to adjust for cases where the sideways-shift does
 * not exactly coincide with the 'column', where nonprinting data is displayed
 * in hex/octal/uparrow format and is split across the display's left margin.
 * After adding the left margin, that gives the index into the line for
 * reading bytes whose width is added til we read the mark's offset.
 */
int
mk_to_vcol(WINDOW *wp, MARK mark, int expanded, int column, int adjust)
{
    BUFFER *bp = wp->w_bufp;
    int i = b_left_margin(bp) + adjust;
    int ch, c0;
    int limit;
    int t = tabstop_val(bp);
    LINE *lp;
    int extra = ((!global_g_val(GMDALTTABPOS) && !insertmode) ? 1 : 0);
    const char *text;
    int prev_col = column;
    int my_bytes = BytesAt(mark.l, mark.o);
#ifdef WMDLINEWRAP
    int nu_adj = nu_width(wp);
#endif

    TRACE2((T_CALLED "mk_to_vcol(mark.o=%d, col=%d, adjust=%d) extra %d\n",
	    mark.o, column, adjust, extra));

    if (i < 0) {
	i = 0;
	column = 0;
    }
    lp = mark.l;
    limit = mark.o + (extra ? my_bytes : 0);
    if (limit > llength(lp))
	limit = llength(lp);

    text = lvalue(lp);
    if (column == 0 || !mark.o) {
	ch = EOF;
    } else if (mark.o) {
	ch = text[mark.o - 1];
    } else {
	ch = EOF;
    }
    while (i < limit) {
	int used = 1;

	prev_col = column;
	c0 = ch;
	ch = text[i];
#ifdef WMDLINEWRAP
	if (column != 0
	    && w_val(wp, WMDLINEBREAK)
	    && w_val(wp, WMDLINEWRAP)
	    && isBlank(c0)) {
	    int nu_col = (column + nu_adj);
	    int k = (int) cols_until(wp, text + i, (unsigned) (llength(lp) - i));
	    int wide = term.cols;
	    int have = (nu_col % wide) + k;

	    if (have >= wide) {
		int wrap = (nu_col / wide);
		int need = (wrap + 1) * wide;
		nu_col = need;
		column = nu_col - nu_adj;
		prev_col = column;
	    }
	}
#else
	(void) c0;
#endif
	if (isTab(ch) && !expanded) {
	    column += t - (column % t);
	}
#if OPT_MULTIBYTE
	else if (b_is_utfXX(wp->w_bufp)) {
	    int nxt = llength(mark.l) - i;
	    int adj = column_sizes(wp, text + i, (unsigned) nxt, &used);

	    if (adj == COLS_UTF8 && UseCellWidth2(wp, lp, i)) {
		adj = mb_cellwidth(wp, text + i, nxt);
	    }
	    column += adj;
	}
#endif
	else if (!isPrint(ch)) {
	    column += column_sizes(wp,
				   text + i,
				   (UINT) (llength(mark.l) - i),
				   &used);
	} else {
	    ++column;
	}
	i += used;
    }
    if (extra && (column != 0) && (mark.o < llength(lp))) {
	column = (my_bytes == 1 && b_is_utfXX(bp)) ? (column - 1) : prev_col;
    }
    return2Code(column);
}

/*
 * Convert the window's DOT into a virtual column.  This is used both for the
 * ruler, as well as for updating the cursor position, and for very long lines
 * can be slow.  So we try to cache a good starting point to allow rapid
 * movement along the line.
 */
static int
dot_to_vcol(WINDOW *wp)
{
    BUFFER *bp = wp->w_bufp;
    int result;
#if OPT_CACHE_VCOL
    W_TRAITS *wt = &(wp->w_traits);
    int need_col = FALSE;
    int shift = (w_val(wp, WVAL_SIDEWAYS)
		 ? w_val(wp, WVAL_SIDEWAYS) - 1
		 : 0);
    int use_off = -b_left_margin(bp);
#if OPT_TRACE
    int check;
#endif

    TRACE2((T_CALLED "dot_to_vcol(%s)\n", bp->b_bname));

#if OPT_TRACE && OPT_DEBUG
    TRACE(("HAVE: dot.o %d, left_dot %d+%d, left_col %d\n",
	   wp->w_dot.o,
	   wt->w_left_dot.o, use_off,
	   wt->w_left_col));
    TRACE(("%s\n", lp_visible(wp->w_dot.l)));
    TRACE(("DOT:%s\n", visible_buff(lvalue(wp->w_dot.l) + wp->w_dot.o,
				    llength(wp->w_dot.l) - wp->w_dot.o,
				    FALSE)));
#endif

    /*
     * If we've moved to a new line, or if the 'list' mode is changed,
     * we'll need to recompute the whole state.
     */
    if (wt->w_left_dot.l != wp->w_dot.l
	|| wt->w_list_opt != w_val(wp, WMDLIST)
	|| wt->w_num_cols != term.cols) {
	wt->w_left_dot.l = wp->w_dot.l;
	wt->w_left_dot.o = 0;
	wt->w_left_col = 0;
	wt->w_left_adj = 0;
	wt->w_list_opt = w_val(wp, WMDLIST);
	wt->w_num_cols = term.cols;
	need_col = TRUE;
    }
#ifdef WMDLINEWRAP
    if (w_val(wp, WMDLINEWRAP)) {
	int on_1st = (nu_width(wp) + w_left_margin(wp));
	int lo = wt->w_left_dot.o;
	int hi = lo + term.cols;	/* estimate (may be lower) */
	int inside;
	int row;
	int col = wt->w_left_col;

	/*
	 * Normally 'hi' is a good estimate since each column in the display
	 * corresponds to one or more bytes of the displayed data.  But if we
	 * are in linebreak mode, then we can get some extra columns on each
	 * row.  That can make the estimate too high, causing the chunk of
	 * code tested by 'inside' to not be executed.
	 *
	 * FIXME - it would be nice to cache the offset corresponding to the
	 * right-limit of the row containing dot, e.g., wt->w_right_dot.o
	 */
	if (w_val(wp, WMDLINEBREAK)) {
	    int chk = hi;
	    int tst;
	    char *text = lvalue(wp->w_dot.l);

	    while (chk < llength(wp->w_dot.l) && !isBlank(text[chk])) {
		++chk;
	    }
	    tst = offs2col(wp, wp->w_dot.l, chk);
	    if (tst > hi) {
		chk = hi;
		do {
		    while (chk >= 0 && !isBlank(text[chk])) {
			--chk;
		    }
		    if (chk >= 0) {
			tst = offs2col(wp, wp->w_dot.l, chk);
		    } else {
			break;
		    }
		} while (--chk >= 0 && tst > hi);
	    }
	    hi = tst;
	}

	inside = (wp->w_dot.o >= lo && wp->w_dot.o < hi);
	TRACE(("dot.o %d is %s [%d..%d]\n",
	       wp->w_dot.o,
	       inside ? "inside" : "NOT inside",
	       lo, hi - 1));

	if (!inside) {
	    col = offs2col(wp, wt->w_left_dot.l, wp->w_dot.o);
	    TRACE(("w_dot offs2col(%d) = %d\n", wp->w_dot.o, col));
	    row = col / term.cols;
	    TRACE(("...in row %d\n", row));
	    col = row * term.cols - on_1st;
	    if (col < 0)
		col = 0;
	    TRACE(("...row %d begins with col %d\n", row, col));
	}

	if (wt->w_left_col != col) {
	    TRACE(("left_col %d vs %d\n", wt->w_left_col, col));
	    wt->w_left_col = col;

	    wt->w_left_dot.o = col2offs(wp, wt->w_left_dot.l, col + on_1st);
	    TRACE(("...left_dot col2offs(%d + %d) = %d\n",
		   col, on_1st, wt->w_left_dot.o));
	}
    } else
#endif
	if ((wt->w_left_col + wt->w_left_adj < shift)
	    || (wt->w_left_col > shift)
	    || (wp->w_dot.o < wt->w_left_dot.o)) {
	int base = nu_width(wp) + w_left_margin(wp) - 1;
	int col;

	TRACE(("...change dot %d(%d) w_left_col from [%d..%d] to match shift %d, base %d\n",
	       wp->w_dot.o,
	       wt->w_left_dot.o,
	       wt->w_left_col,
	       wt->w_left_col + wt->w_left_adj,
	       shift, base));

	col = offs2col(wp, wt->w_left_dot.l, wp->w_dot.o) + shift - base;
	TRACE(("...compare col %d to shift %d\n", col, shift));
	if (col < shift) {
	    wt->w_left_col = col;
	} else {
	    wt->w_left_col = shift;
	}
	wt->w_left_adj = 0;

	wt->w_left_dot.o = col2offs(wp,
				    wt->w_left_dot.l,
				    wt->w_left_col + base - shift);
	TRACE(("...col2offs(%d) = %d\n", wt->w_left_col, wt->w_left_dot.o));

	TRACE(("...compare w_left_dot.o %d to w_dot.o %d\n",
	       wt->w_left_dot.o, wp->w_dot.o));
	if (wt->w_left_dot.o > wp->w_dot.o) {
	    need_col = TRUE;	/* dot is inconsistent with shift */
	    wt->w_left_dot.o = wp->w_dot.o;
	} else {
	    col = offs2col(wp, wt->w_left_dot.l, wt->w_left_dot.o) + shift - base;
	    TRACE(("...offs2col(%d) = %d\n", wt->w_left_dot.o, col));

	    TRACE(("...compare col %d to w_left_col %d\n",
		   col, wt->w_left_col));
	    if (col < wt->w_left_col) {
		/* if there was a multi-column character at the left
		 * side of the shifted screen, adjust */
		wt->w_left_adj = wt->w_left_col - col;
		TRACE(("...adjust for multi-column character %d..%d -> %d\n",
		       col,
		       wt->w_left_col,
		       wt->w_left_adj));
		wt->w_left_col = col;
	    }
	    need_col = FALSE;
	}
    }

    if (need_col) {
	wt->w_left_col = 0;
	if (wt->w_left_dot.o > 0) {
	    wt->w_left_col = mk_to_vcol(wp,
					wt->w_left_dot,
					w_val(wp, WMDLIST),
					0,
					0);
	    TRACE(("...cache w_left_col %d\n", wt->w_left_col));
	}
    }
    if (use_off != 0) {
	if (wt->w_left_col == 0)
	    use_off = -wt->w_left_dot.o;
    }
    result = mk_to_vcol(wp,
			wp->w_dot,
			w_val(wp, WMDLIST),
			wt->w_left_col,
			wt->w_left_dot.o + use_off);
#if OPT_TRACE
    check = mk_to_vcol(wp, wp->w_dot, w_val(wp, WMDLIST), 0, 0);
    if (check != result) {
	TRACE(("MISMATCH result %d check %d\n", result, check));
	kbd_alarm();
	TRACE(("-> OOPS: dot.o %d, left_dot %d+%d, left_col %d\n",
	       wp->w_dot.o,
	       wt->w_left_dot.o, use_off,
	       wt->w_left_col));
	result = check;
    }
#endif
#else
    (void) bp;
    result = mk_to_vcol(wp, wp->w_dot, w_val(wp, WMDLIST), 0, 0);
#endif
    return2Code(result);
}

/*
 * Update a single line. This does not know how to use insert or delete
 * character sequences; we are using VT52 functionality. Update the physical
 * row and column variables. It does try an exploit erase to end of line.
 *
 * row     - row of screen to update
 * colfrom - col to start updating from
 * colto   - col to go to
 */

#if OPT_PSCREEN
/*
 * Update a single row on the physical screen.
 */
static void
update_line(int row, int colfrom, int colto)
{
    VIDEO_TEXT *vc, *pc, *evc;
    VIDEO_ATTR *va, *pa, xx;
    int nchanges = 0;

    if ((vscreen[row]->v_flag & VFCHG) == 0)
	return;

    vc = &vscreen[row]->v_text[colfrom];
    evc = &vscreen[row]->v_text[colto];
    va = &vscreen[row]->v_attrs[colfrom];
    pc = &CELL_TEXT(row, colfrom);
    pa = &CELL_ATTR(row, colfrom);

    while (vc < evc) {
	xx = *va;
#ifdef GVAL_VIDEO
	xx ^= (VIDEO_ATTR) global_g_val(GVAL_VIDEO);
#endif
	if (*vc != *pc || VATTRIB(xx) != VATTRIB(*pa)) {
	    *pc = *vc;
	    *pa = xx | VADIRTY;
	    nchanges++;
	}
	vc++;
	pc++;
	va++;
	pa++;
    }

    if (nchanges > 0)
	MARK_LINE_DIRTY(row);
    vscreen[row]->v_flag &= ~(VFCHG | VFCOL);	/* mark virtual line updated */
}

#else /* !OPT_PSCREEN */

/*
 * UPDATELINE code for all other versions
 */
static void
update_line(int row, int colfrom, int colto)
{
    struct VIDEO *vp1 = vscreen[row];	/* virtual screen image */
    struct VIDEO *vp2 = pscreen[row];	/* physical screen image */
    int xl = colfrom;
    int xr = colto;
    int xx;

    VIDEO_TEXT *cp1 = VideoText(vp1);
    VIDEO_TEXT *cp2 = VideoText(vp2);
    int nbflag;			/* non-blanks to the right flag? */

#if OPT_VIDEO_ATTRS
    VIDEO_ATTR *ap1 = VideoAttr(vp1);
    VIDEO_ATTR *ap2 = VideoAttr(vp2);
    VIDEO_ATTR Blank = 0;	/* FIXME: Color? */
#else
    UINT rev;			/* reverse video flag */
    UINT req;			/* reverse video request flag */
#endif

#if !OPT_VIDEO_ATTRS
#if OPT_COLOR
    term.setfore(ReqFcolor(vp1));
    term.setback(ReqBcolor(vp1));
#endif

#if OPT_REVSTA || OPT_COLOR
    /* if we need to change the reverse video status of the
       current line, we need to re-write the entire line     */
    rev = (vp1->v_flag & VFREV) == VFREV;
    req = (vp1->v_flag & VFREQ) == VFREQ;
    if ((rev != req)
#if OPT_COLOR
	|| (CurFcolor(vp1) != ReqFcolor(vp1))
	|| (CurBcolor(vp1) != ReqBcolor(vp1))
#endif
	) {
	movecursor(row, colfrom);	/* Go to start of line. */
	/* set rev video if needed */
	if (req)
	    set_term_attrs(req);

	/* scan through the line and dump it to the screen and
	   the virtual screen array                             */
	for (; xl < colto; xl++) {
	    term.putch(cp1[xl]);
	    ++ttcol;
	    cp2[xl] = cp1[xl];
	}
	/* turn rev video off */
	if (req)
	    reset_term_attrs();

	/* update the needed flags */
	vp1->v_flag &= ~(VFCHG | VFCOL);
	if (req)
	    vp1->v_flag |= VFREV;
	else
	    vp1->v_flag &= ~VFREV;
#if OPT_COLOR
	CurFcolor(vp1) = ReqFcolor(vp1);
	CurBcolor(vp1) = ReqBcolor(vp1);
#endif
	return;
    }
#else
    rev = FALSE;
#endif /* OPT_REVSTA || OPT_COLOR */
#endif /* !OPT_VIDEO_ATTRS */

    /* advance past any common chars at the left */
#if !OPT_VIDEO_ATTRS
    if (!rev)
#endif /* !OPT_VIDEO_ATTRS */
	while (xl != colto
	       && cp1[xl] == cp2[xl]
#if OPT_VIDEO_ATTRS
	       && VATTRIB(ap1[xl]) == VATTRIB(ap2[xl])
#endif /* OPT_VIDEO_ATTRS */
	    ) {
	    ++xl;
	}

    /* This can still happen, even though we only call this routine on changed
     * lines.  A hard update is always done when a line splits, a massive
     * change is done, or a buffer is displayed twice.  This optimizes out most
     * of the excess updating.  A lot of computes are used, but these tend to
     * be hard operations that do a lot of update, so I don't really care.
     */

    /* if both lines are the same, no update needs to be done */
    if (xl == colto) {
	vp1->v_flag &= ~VFCHG;
	return;
    }

    /* find out if there is a match on the right */
    nbflag = FALSE;

#if !OPT_VIDEO_ATTRS
    if (!rev)
#endif
	while (cp1[xr - 1] == cp2[xr - 1]
#if OPT_VIDEO_ATTRS
	       && VATTRIB(ap1[xr - 1]) == VATTRIB(ap2[xr - 1])
#endif
	    ) {
	    --xr;
	    /* Note if any nonblank in right match */
	    if (cp1[xr] != ' '
#if OPT_VIDEO_ATTRS
		|| VATTRIB(ap1[xr] != Blank)
#endif
		)
		nbflag = TRUE;
	}

    xx = xr;

    /* Erase to EOL ? */
    if (nbflag == FALSE
	&& eolexist == TRUE
#if OPT_REVSTA && !OPT_VIDEO_ATTRS
	&& (req != TRUE)
#endif
	) {
	while ((xx != xl)
	       && cp1[xx - 1] == ' '
#if OPT_VIDEO_ATTRS
	       && VATTRIB(ap1[xx - 1]) == Blank
#endif
	    ) {
	    xx--;
	}

	if ((xr - xx) <= 3)	/* Use only if erase is */
	    xx = xr;		/* fewer characters. */
    }

    movecursor(row, xl - colfrom);	/* Go to start of line. */
#if OPT_VIDEO_ATTRS
    while (xl < xx) {
	int j = xl;
	VIDEO_ATTR attr = VATTRIB(ap1[j]);
	while ((j < xx) && (attr == VATTRIB(ap1[j])))
	    j++;
	set_term_attrs(attr);
	for (; xl < j; xl++) {
	    term.putch((int) cp1[xl]);
	    ++ttcol;
	    cp2[xl] = cp1[xl];
	    ap2[xl] = ap1[xl];
	}
    }
    reset_term_attrs();

    if (xx != xr) {		/* Erase. */
	term.eeol();
	for (; xl < xr; xl++) {
	    if (cp2[xl] != cp1[xl]
		|| VATTRIB(ap2[xl]) != VATTRIB(ap1[xl]))
		ap2[xl] = ap1[xl];
	    cp2[xl] = cp1[xl];
	}
    }
#else /* OPT_VIDEO_ATTRS */
#if OPT_REVSTA
    set_term_attrs(rev);
#endif

    for (; xl < xr; xl++) {	/* Ordinary. */
	term.putch(cp1[xl]);
	++ttcol;
	cp2[xl] = cp1[xl];
    }

    if (xx != xr) {		/* Erase. */
	term.eeol();
	for (; xl < xr; xl++) {
	    cp2[xl] = cp1[xl];
	}
    }
#if OPT_REVSTA
    reset_term_attrs();
#endif
#endif /* OPT_VIDEO_ATTRS */
    vp1->v_flag &= ~(VFCHG | VFCOL);
    return;
}
#endif /* OPT_PSCREEN(update_line) */

void
kbd_openup(void)
{
    kbd_flush();
    bottomleft();
    term.putch('\n');
    term.putch('\r');
    term.flush();

#if !OPT_PSCREEN
    if (pscreen != NULL) {
	int i, j;
	for (i = 0; i < term.rows - 1; ++i) {
	    for (j = 0; j < term.cols; ++j) {
		CELL_TEXT(i, j) = CELL_TEXT(i + 1, j);
#if OPT_VIDEO_ATTRS
		CELL_ATTR(i, j) = CELL_ATTR(i + 1, j);
#endif
	    }
	}
	for (j = 0; j < term.cols; ++j) {
	    CELL_TEXT(i, j) = ' ';
#if OPT_VIDEO_ATTRS
	    CELL_ATTR(i, j) = VADIRTY;
#endif
	}
    }
#endif
}

/* cannot be allocated since it's used by OPT_HEAPSIZE */
static char my_overlay[20];

/* save/erase text for the overlay on the message line */
void
kbd_overlay(const char *s)
{
    my_overlay[0] = EOS;
    if ((mpresf = (s != NULL && *s != EOS)) != 0) {
	vl_strncpy(my_overlay, s, sizeof(my_overlay));
    }
}

void
kbd_flush(void)
{
    int ok;

    beginDisplay();
    if (is_vtinit()) {
	int row = term.rows - 1;

	vtmove(row, -w_val(wminip, WVAL_SIDEWAYS));

	ok = (wminip != NULL
	      && wminip->w_dot.l != NULL
	      && lvalue(wminip->w_dot.l) != NULL);
	if (ok) {
	    TRACE(("SHOW:%2d:%s\n",
		   llength(wminip->w_dot.l),
		   lp_visible(wminip->w_dot.l)));
	    lsettrimmed(wminip->w_dot.l);
	    vtset(wminip->w_dot.l, wminip);
	}

	vteeol();
	set_vattrs(row, 0,
		   (VIDEO_ATTR) (miniedit
				 ? global_g_val(GVAL_MINI_HILITE)
				 : 0),
		   (size_t) term.cols);
	if (my_overlay[0] != EOS) {
	    int j;
	    int n = term.cols - (int) strlen(my_overlay) - 1;
	    if (n > 0) {
		for (j = 0; my_overlay[j] != EOS; ++j)
		    vscreen[row]->v_text[n + j] = (VIDEO_TEXT) my_overlay[j];
	    }
	}
	vscreen[row]->v_flag |= VFCHG;
	update_line(row, 0, term.cols);
	if (ok)
	    movecursor(row,
		       offs2col(wminip, wminip->w_dot.l, wminip->w_dot.o));
    }
    term.flush();
    endofDisplay();
}

/*
 * Translate offset (into a line's text) into the display-column, taking into
 * account the tabstop, sideways, number- and list-modes.
 *
 * Note: we intentionally compare against '\n' rather than the record separator,
 * since it is simpler than handling ^M^J in one case, and we really only want
 * to know if we're in the record-separator.
 */
static int
offs2col0(WINDOW *wp,
	  LINE *lp,
	  C_NUM offset,
	  C_NUM * cache_offset,
	  int *cache_column)
{
    int column;

    /* this makes the how-much-to-select calculation easier above */
    if (offset < 0) {
	column = offset;
    } else if ((lp == win_head(wp)) || !lisreal(lp)) {
	column = 0;
    } else {
	BUFFER *bp = wp->w_bufp;
	int rs = use_record_sep(bp);
	int length = llength(lp);
	int nums = nu_width(wp);
	int tabs = tabstop_val(wp->w_bufp);
	int list = w_val(wp, WMDLIST);
	int last = (list ? (N_chars * 2) : rs);
	int left = if_LINEWRAP(wp, 0, w_val(wp, WVAL_SIDEWAYS));
	int ch = EOF, c0;
	C_NUM n;
	C_NUM start_offset;
	const char *text = lvalue(lp);

	if (offset > length + 1)
	    offset = length + 1;

	if (cache_offset && cache_column && *cache_offset < offset) {
	    start_offset = *cache_offset;
	    column = *cache_column;
	} else {
	    start_offset = w_left_margin(wp);
	    column = 0;
	}

	for (n = start_offset; n < offset;) {
	    int used = 1;

	    c0 = ch;
	    ch = (n == length) ? rs : CharOf(text[n]);
#if OPT_MULTIBYTE
	    if ((n < length) && b_is_utfXX(bp) && !isTab(ch)) {
		int nxt = offset - n;
		int adj = column_sizes(wp, text + n, (UINT) nxt, &used);

		if (adj == COLS_UTF8 && UseCellWidth2(wp, lp, n)) {
		    adj = mb_cellwidth(wp, text + n, nxt);
		}
		column += adj;
	    } else
#endif
	    if (isPrint(ch)) {
#ifdef WMDLINEWRAP
		if (column != 0
		    && w_val(wp, WMDLINEBREAK)
		    && w_val(wp, WMDLINEWRAP)
		    && isBlank(c0)) {
		    int k = (int) cols_until(wp,
					     text + n,
					     (unsigned) (length - n));
		    int col2 = column + nums;
		    int wide = term.cols;
		    int have;

		    have = (col2 % wide) + k;
		    if (have >= wide) {
			int wrap = (col2 / wide);
			int need = (wrap + 1) * wide;
			column = need - nums;
		    }
		}
#else
		(void) c0;
#endif
		column++;
	    } else if (ch == last) {
		column++;
	    } else if (list || !isTab(ch)) {
		column += column_sizes(wp,
				       (n >= length) ? "" : (text + n),
				       (UINT) (offset - n), &used);
	    } else if (isTab(ch)) {
		column = ((column / tabs) + 1) * tabs;
	    }
	    n += used;
	}

	if (cache_offset && cache_column) {
	    *cache_offset = offset;
	    *cache_column = column;
	}

	column += (nums + w_left_margin(wp) - left);
    }
    return column;
}

int
offs2col(WINDOW *wp, LINE *lp, C_NUM offset)
{
    return offs2col0(wp, lp, offset, NULL, NULL);
}

/*
 * Translate a display-column (assuming an infinitely-wide display) into the
 * line's offset, taking into account the tabstop, sideways, number and list
 * modes.
 */
#if OPT_MOUSE || defined(WMDLINEWRAP)
int
col2offs(WINDOW *wp, LINE *lp, C_NUM col)
{
    int nums = nu_width(wp);
    int tabs = tabstop_val(wp->w_bufp);
    int list = w_val(wp, WMDLIST);
    int left = if_LINEWRAP(wp, 0, w_val(wp, WVAL_SIDEWAYS));
    int goal = col + left - nums - w_left_margin(wp);

    C_NUM n;
    C_NUM offset;
    C_NUM len = llength(lp);
    char *text = lvalue(lp);

    if (lp == win_head(wp)) {
	offset = 0;
    } else {
	for (offset = w_left_margin(wp), n = 0;
	     (offset < len) && (n < goal);
	    ) {
	    int c = text[offset];
	    int used = 1;

#ifdef WMDLINEWRAP
	    /*
	     * A linebreak can happen after whitespace.  To avoid a confusing
	     * display, we avoid doing the linebreak at the beginning or in the
	     * middle of whitespace.  Also, we disallow linebreak at the
	     * beginning of a line.
	     */
	    if (offset != 0
		&& w_val(wp, WMDLINEBREAK)
		&& w_val(wp, WMDLINEWRAP)
		&& isBlank(text[offset - 1])
		&& !isBlank(text[offset])) {
		int n2 = (int) cols_until(wp,
					  text + offset,
					  (unsigned) (len - offset));
		int col1 = ((n + nums) % term.cols);
		int col2 = col1 + n2;

		if (col2 >= term.cols) {
		    n += (term.cols - col1);
		}
	    }
#endif

#if OPT_MULTIBYTE
	    if (b_is_utfXX(wp->w_bufp) && !isTab(c)) {
		int nxt = len - offset;
		int adj = column_sizes(wp, text + offset, (UINT) nxt, &used);

		if (adj == COLS_UTF8 && UseCellWidth2(wp, lp, offset)) {
		    adj = mb_cellwidth(wp, text + offset, nxt);
		}
		n += adj;
	    } else
#endif
	    if (isPrint(c)) {
		n++;
	    } else if (list || !isTab(c)) {
		n += column_sizes(wp, text + offset, (UINT) (len - offset), &used);
	    } else if (isTab(c)) {
		n = ((n / tabs) + 1) * tabs;
	    }
	    if (n > goal)
		break;
	    offset += used;
	}
    }
    return offset;
}
#endif

/*
 * Compute the number of rows required for displaying a line.
 */
#ifdef WMDLINEWRAP
int
line_height(WINDOW *wp, LINE *lp)
{
    int hi = 1;

    if (w_val(wp, WMDLINEWRAP)) {
	int len = llength(lp);
	if (len > 0) {
	    int col = offs2col(wp, lp, len) - 1;
	    int rsl = ((w_val(wp, WMDLIST))
		       ? ((b_val(wp->w_bufp, VAL_RECORD_SEP) == RS_CRLF) ? 4 : 2)
		       : 1);
	    if (ins_mode(wp) != FALSE
		&& lp == DOT.l
		&& len <= DOT.o) {
		col += rsl - 1;	/* wrapping treats one column specially */
	    } else if (w_val(wp, WMDLIST)) {
		col += rsl;
	    }
	    hi = (col / term.cols) + 1;
	}
    }
    return hi;
}
#endif

/*
 * Given a row on the screen, determines which window it belongs to.  Returns
 * null only for the message line.
 */
#if defined(WMDLINEWRAP) || OPT_MOUSE
WINDOW *
row2window(int row)
{
    WINDOW *wp;

    for_each_visible_window(wp) {
	if (row >= wp->w_toprow && row <= mode_row(wp))
	    return wp;
    }
    return NULL;
}
#endif

#if OPT_UPBUFF
typedef struct {
    WINDOW *wp;
    struct VAL w_vals[MAX_W_VALUES];
    int top;
    int line;
    int col;
} SAVEWIN;

static SAVEWIN *recomp_tbl;
static size_t recomp_len;
static WINDOW *recomp_win;

/*
 * Return true if the current window (or its associated buffer) is being
 * recomputed.
 */
int
is_recomputing(WINDOW *wp)
{
    int result = FALSE;

    if (recomp_win && wp) {
	if (recomp_win == wp) {
	    result = TRUE;
	} else if (recomp_win->w_bufp == wp->w_bufp) {
	    result = TRUE;
	}
    }

    return result;
}

BUFFER *
recomputing_buf(void)
{
    BUFFER *result = NULL;

    if (recomp_win != NULL)
	result = recomp_win->w_bufp;

    return result;
}

WINDOW *
recomputing_win(void)
{
    return recomp_win;
}

/*
 * Recompute the given buffer. Save/restore its modes and position information
 * so that a redisplay will show as little change as possible.
 */
static void
recompute_buffer(BUFFER *bp)
{
    if (b_val(bp, MDUPBUFF)) {
	WINDOW *wp;
	SAVEWIN *tbl;

	struct VAL b_vals[MAX_B_VALUES];
	size_t num = 0;
	BUFFER *savebp = curbp;
	WINDOW *savewp = curwp;
	int mygoal = curgoal;

	recomp_win = curwp;

	if (recomp_len < bp->b_nwnd) {
	    recomp_len = (size_t) bp->b_nwnd + 1;
	    recomp_tbl = (recomp_tbl != NULL)
		? typereallocn(SAVEWIN, recomp_tbl, recomp_len)
		: typeallocn(SAVEWIN, recomp_len);
	    if (recomp_tbl == NULL) {
		recomp_len = 0;
		return;
	    }
	}
	tbl = recomp_tbl;

	/* remember where we are, to reposition */
	/* ...in case line is deleted from buffer-list */
	relisting_b_vals = NULL;
	relisting_w_vals = NULL;
	if (curbp == bp) {
	    relisting_b_vals = b_vals;
	} else {
	    curbp = bp;
	    curwp = bp2any_wp(bp);
	}
	for_each_visible_window(wp) {
	    if (wp->w_bufp == bp) {
		if (wp == savewp)
		    relisting_w_vals = tbl[num].w_vals;
		curwp = wp;	/* to make 'getccol()' work */
		curbp = curwp->w_bufp;
		tbl[num].wp = wp;
		tbl[num].top = line_no(bp, wp->w_line.l);
		tbl[num].line = line_no(bp, wp->w_dot.l);
		tbl[num].col = getccol(FALSE);
		save_vals(NUM_W_VALUES, global_w_values.wv,
			  tbl[num].w_vals, wp->w_values.wv);
		if (++num >= recomp_len)
		    break;
	    }
	}
	curwp = savewp;
	curbp = savebp;

	save_vals(NUM_B_VALUES, global_b_values.bv, b_vals, bp->b_values.bv);
	(bp->b_upbuff) (bp);
	copy_mvals(NUM_B_VALUES, bp->b_values.bv, b_vals);

	/* reposition and restore */
	while (num-- != 0) {
	    curwp = wp = tbl[num].wp;
	    curbp = curwp->w_bufp;
	    (void) vl_gotoline(tbl[num].top);
	    wp->w_line.l = wp->w_dot.l;
	    wp->w_line.o = 0;
	    if (tbl[num].line != tbl[num].top)
		(void) vl_gotoline(tbl[num].line);
	    (void) gocol(tbl[num].col);
	    wp->w_flag |= WFMOVE | WFSBAR;
	    copy_mvals(NUM_W_VALUES, wp->w_values.wv, tbl[num].w_vals);
	}
	curwp = savewp;
	curbp = savebp;
	curgoal = mygoal;
	relisting_b_vals = NULL;
	relisting_w_vals = NULL;

	recomp_win = NULL;
    }
    b_clr_obsolete(bp);

    return;
}
#endif /* OPT_UPBUFF */

static int
TypeAhead(int force)
{
    if (force != TRUE) {
	if ((force == FALSE && !global_g_val(GMDSMOOTH_SCROLL))
	    || (force == SORTOFTRUE))
	    return (keystroke_avail());
    }
    return FALSE;
}

/*
 * Update virtual screen line, given a LINE pointer.
 */
static void
update_screen_line(WINDOW *wp, LINE *lp, int sline)
{
    C_NUM left;

    /*
     * Mark the screen lines changed, resetting the requests for reverse
     * video.  Set the global 'horscroll' to the amount of horizontal
     * scrolling.
     */
#ifdef WMDLINEWRAP
    if (w_val(wp, WMDLINEWRAP)) {
	int top_line = sline - wp->w_line.o;
	int m = (top_line >= 0) ? top_line : 0;
	int n = top_line + line_height(wp, lp);
	while (n > m)
	    if (--n < mode_row(wp)) {
		vscreen[n]->v_flag |= VFCHG;
		vscreen[n]->v_flag &= ~VFREQ;
	    }
	horscroll = 0;
    } else
#endif
    if (sline >= 0) {
	vscreen[sline]->v_flag |= VFCHG;
	vscreen[sline]->v_flag &= ~VFREQ;
	if (w_val(wp, WVAL_SIDEWAYS))
	    horscroll = w_val(wp, WVAL_SIDEWAYS);
    }
    left = horscroll;

    if (lp != win_head(wp)) {
	vtmove(sline, -left);
	vtset(lp, wp);
	if (left && sline >= 0) {
	    int zero = nu_width(wp);
	    mark_extent(sline, zero, MRK_EXTEND_LEFT[0]);
	    if (vtcol <= zero)
		vtcol = zero + 1;
	}
    } else {
	vtmove(sline, 0);
	vtputc(wp, MRK_EMPTY, 1);
    }
    horscroll = 0;
#if OPT_COLOR
    if (sline >= 0) {
	ReqFcolor(vscreen[sline]) = gfcolor;
	ReqBcolor(vscreen[sline]) = gbcolor;
    }
#endif
}

/*
 * Update the current line to the virtual screen.
 *
 * 'wp' points to the window to update current line in.
 */
static void
update_oneline(WINDOW *wp)
{
    LINE *lp;			/* line to update */
    int sline;			/* physical screen line to update */

    /* search down the line we want */
    lp = wp->w_line.l;
    sline = TopRow(wp);
    while (lp != wp->w_dot.l) {
	sline += line_height(wp, lp);
	lp = lforw(lp);
    }

    update_screen_line(wp, lp, sline);
    vteeol();
}

/*
 * Update all the lines in a window on the virtual screen.
 *
 * 'wp' points to the window to update current line in.
 */
static void
update_all(WINDOW *wp)
{
    LINE *lp;			/* line to update */
    int sline;			/* physical screen line to update */

    /* search down the lines, updating them */
    lp = wp->w_line.l;
    sline = TopRow(wp);
    while (sline < mode_row(wp)) {
	update_screen_line(wp, lp, sline);
	vteeol();
	sline += line_height(wp, lp);
	if (lp != win_head(wp))
	    lp = lforw(lp);
    }
}

#if OPT_VIDEO_ATTRS
/* Merge (or combine) the specified attribute into the vscreen structure */
static void
mergeattr(WINDOW *wp, int row, int start_col, int end_col, unsigned attr)
{
    int col;

    if (start_col < 0)
	start_col = 0;

#ifdef WMDLINEWRAP
    if (w_val(wp, WMDLINEWRAP)) {
	for (col = start_col; col <= end_col; col++) {
	    int x = row + col / term.cols;
	    if (x < 0)
		continue;
	    if (x < mode_row(wp)) {
		int y = col % term.cols;
		vscreen[x]->v_attrs[y] =
		    (VIDEO_ATTR) ((vscreen[x]->v_attrs[y] | (attr & ~VAREV))
				  ^ (attr & VAREV));
	    } else {
		break;
	    }
	}
    } else
#endif
    {
	if (end_col >= term.cols)
	    end_col = term.cols - 1;
	for (col = start_col; col <= end_col; col++)
	    vscreen[row]->v_attrs[col] =
		(VIDEO_ATTR) ((vscreen[row]->v_attrs[col] | (attr & ~VAREV))
			      ^ (attr & VAREV));
    }
}

#if OPT_LINE_ATTRS
static void
update_line_attrs(WINDOW *wp)
{
    int row;
    LINE *lp;
    int linewrap;

#ifdef WMDLINEWRAP
    linewrap = w_val(wp, WMDLINEWRAP);
#else
    linewrap = 0;
#endif

    row = TopRow(wp);
    lp = wp->w_line.l;
    while (row < wp->w_toprow + wp->w_ntrows && lp != win_head(wp)) {
	int save_offset = w_left_margin(wp);
	int save_column = 0;
	if (lp->l_attrs) {
	    C_NUM start_col, end_col;
	    int i, a;
	    /* Attempt to set starting point intelligently.  */
	    if (wp->w_line.o < 0)
		i = col2offs(wp, lp, -(wp->w_line.o * term.cols));
	    else {
		if (w_val(wp, WVAL_SIDEWAYS))
		    i = col2offs(wp, lp, 0);
		else
		    i = 0;
	    }
	    if (!linewrap && w_val(wp, WMDNUMBER) && w_val(wp, WVAL_SIDEWAYS)) {
		i += NU_WIDTH;
	    }
	    /* If i > 0, make sure it indexes a valid attribute.  */
	    if (i > 0) {
		int ii;
		for (ii = 0; ii < i; ii++)
		    if (lp->l_attrs[ii] == 0)
			break;
		i = ii;
	    }
	    /* Scan attributes... */
	    for (;;) {
		if (lp->l_attrs[i] == 0)
		    break;	/* get out if at end */
		while (lp->l_attrs[i] == 1)
		    i++;	/* skip normals */
		a = lp->l_attrs[i];
		if (a == 0)
		    break;	/* get out if at end */
		start_col = offs2col0(wp, lp, i, &save_offset, &save_column);
		i++;
		while (lp->l_attrs[i] == a)
		    i++;	/* find run of same attr */
		if (start_col < w_left_margin(wp))
		    start_col = w_left_margin(wp);
		end_col = offs2col0(wp, lp, i, &save_offset, &save_column) - 1;
		if (!linewrap) {
		    if (start_col > term.cols)
			break;
		    mergeattr(wp, row, start_col, end_col,
			      line_attr_tbl[a].vattr);
		    if (end_col > term.cols)
			break;
		} else {
		    /* mergeattr() handles wrapping of attributes for us.  */
		    mergeattr(wp, row, start_col, end_col,
			      line_attr_tbl[a].vattr);
		    if (row + end_col / term.cols >= mode_row(wp))
			break;	/* Get out if no more to do.  */
		}
	    }
	}
	row += line_height(wp, lp);
	lp = lforw(lp);
    }
}
#endif /* OPT_LINE_ATTRS */

static void
update_window_attrs(WINDOW *wp)
{
    BUFFER *bp = wp->w_bufp;
    AREGION **rpp;
    AREGION *ap;
    int i, lmax;

    L_NUM start_wlnum, end_wlnum;
    LINE *lp;
    int rows;
    C_NUM col;
#if OPT_TRACE
    int total_regions = 0;
    int visible_regions = 0;
#endif

    /*
     * Clear portion of virtual screen associated with window pointer
     * of all attributes.
     */
    /* FIXME: color; need to set to value indicating fg and bg for window */
    for (i = wp->w_toprow + wp->w_ntrows - 1; i >= wp->w_toprow; i--) {
	set_vattrs(i,
		   lmap0[i].left, 0,
		   (size_t) lmap0[i].right - (size_t) lmap0[i].left);
    }

#if OPT_LINE_ATTRS
    update_line_attrs(wp);
#endif /* OPT_LINE_ATTRS */

    /*
     * No need to do any more work on this window if there are no
     * attributes.
     */
    if (bp->b_attribs == NULL)
	return;

    /*
     * Compute starting and ending line numbers for the window.  We
     * also fill in lmap which is used for mapping line numbers to
     * screen row numbers.
     */
    lp = wp->w_line.l;
    start_wlnum =
	end_wlnum = line_no(bp, lp);
    rows = wp->w_ntrows;

    lmax = end_wlnum - start_wlnum;
    lmap[lmax].lp = lp;
    lmap[lmax].map = TopRow(wp);
    lmap[lmax].left = -1;
    lmap[lmax].right = -1;

    while ((rows -= line_height(wp, lp)) > 0) {
	lp = lforw(lp);
	end_wlnum++, lmax++;
	lmap[lmax].lp = lp;
	lmap[lmax].map = TopRow(wp) + wp->w_ntrows - rows;
	lmap[lmax].left = -1;
	lmap[lmax].right = -1;
    }

    ++lmax;
    lmap[lmax].lp = NULL;
    lmap[lmax].map = TopRow(wp) + wp->w_ntrows;
    lmap[lmax].left = -1;
    lmap[lmax].right = -1;

    /*
     * Set current attributes in virtual screen associated with window
     * pointer.
     */
    rpp = &(bp->b_attribs);
    while ((ap = *rpp) != NULL) {
	AREGION **next = &(ap->ar_next);
	VIDEO_ATTR attr;
	C_NUM start_col, end_col;
	C_NUM rect_start_col = 0, rect_end_col = 0;
	L_NUM start_rlnum, end_rlnum, lnum, start_lnum, end_lnum;
	int visible = TRUE;

	start_rlnum = line_no(bp, ap->ar_region.r_orig.l);
	end_rlnum = line_no(bp, ap->ar_region.r_end.l);

	/* Garbage collect empty visible regions */
	if (start_rlnum == end_rlnum
	    && VATTRIB(ap->ar_vattr) != 0
	    && ap->ar_region.r_orig.o >= ap->ar_region.r_end.o) {
	    free_attrib2(bp, rpp);
	    continue;
	}

	/* compute starting and ending line-numbers, given the region's */
	if (start_rlnum > start_wlnum) {
	    start_lnum = start_rlnum;
	    lp = ap->ar_region.r_orig.l;
	} else {
	    start_lnum = start_wlnum;
	    lp = wp->w_line.l;
	}
	end_lnum = (end_rlnum < end_wlnum) ? end_rlnum : end_wlnum;

	/*
	 * Filter out attribute regions that will not be on the screen.
	 * Most of the regions generated by syntax filters start/end on
	 * the same line, which allows us to make a simple/fast check.
	 */
	if (start_rlnum == end_rlnum) {
	    i = (start_rlnum - start_wlnum);

	    /*
	     * Skip lines which don't fall into the vertical range.
	     */
	    if (i < 0 || i >= lmax) {
		visible = FALSE;
	    } else {

		/*
		 * Compute the left/right offsets for the visible part of the
		 * line the first time we need to compare it.
		 */
		if (lmap[i].left < 0 && lmap[i].right < 0) {
#ifdef WMDLINEWRAP
		    if (w_val(wp, WMDLINEWRAP)) {
			int lines_remaining = (lmap[i + 1].map - lmap[i].map);

			col = -((wp->w_line.o * term.cols) + nu_width(wp));
			lmap[i].left = col2offs(wp, lmap[i].lp, col);

			col += (lines_remaining * term.cols);
			lmap[i].right = col2offs(wp, lmap[i].lp, col);
		    } else
#endif
		    {
			col = -((wp->w_line.o * term.cols) + nu_width(wp));
			lmap[i].left = col2offs(wp, lmap[i].lp, col);

			col += term.cols;
			lmap[i].right = col2offs(wp, lmap[i].lp, col);
		    }
		    TRACE2(("...update_window_attrs row %d [%d..%d] (%d)\n",
			    lmap[i].map,
			    lmap[i].left,
			    lmap[i].right,
			    lmap[i].right - lmap[i].left));
		}

		if (ap->ar_region.r_orig.o >= lmap[i].right) {
		    visible = FALSE;
		} else if (ap->ar_region.r_end.o < lmap[i].left) {
		    visible = FALSE;
		}
	    }
	}
#if OPT_TRACE
	total_regions++;
#endif
	if (visible) {
#if OPT_TRACE
	    visible_regions++;
#endif

	    attr = ap->ar_vattr;

	    /* if it's a rectangle, precompute the start/end columns */
	    if (ap->ar_shape == rgn_RECTANGLE) {
		int n;
		rect_start_col = mark2col(wp, ap->ar_region.r_orig);
		rect_end_col = mark2col(wp, ap->ar_region.r_end);
		if (rect_end_col < rect_start_col) {
		    col = rect_end_col;
		    rect_end_col = rect_start_col;
		    rect_start_col = col;
		    n = MARK2COL(wp, ap->ar_region.r_orig);
		} else {
		    n = MARK2COL(wp, ap->ar_region.r_end);
		}
		if (rect_end_col < n)
		    rect_end_col = n;
	    }
	    for (lnum = start_lnum; lnum <= end_lnum; lnum++, lp = lforw(lp)) {
		int row;
		if (ap->ar_shape == rgn_RECTANGLE) {
		    start_col = rect_start_col;
		} else if (lnum == start_rlnum) {
		    start_col = mark2col(wp, ap->ar_region.r_orig);
		} else {
		    start_col = w_left_margin(wp) + nu_width(wp);
		}

		if (start_col < w_left_margin(wp))
		    start_col = (lnum == start_rlnum)
			? w_left_margin(wp) + nu_width(wp)
			: w_left_margin(wp);

		if (ap->ar_shape == rgn_RECTANGLE) {
		    end_col = rect_end_col;
		} else if (lnum == end_rlnum) {
		    end_col = mark2col(wp, ap->ar_region.r_end) - 1;
		} else {
		    end_col = offs2col(wp, lp, llength(lp) + 1) - 1;
#ifdef WMDLINEWRAP
		    if (w_val(wp, WMDLINEWRAP)
			&& (end_col % term.cols) == 0)
			end_col--;	/* cannot highlight the newline */
#endif
		}
		row = lmap[lnum - start_wlnum].map;
		mergeattr(wp, row, start_col, end_col, attr);
	    }
	}
	rpp = next;
    }
    TRACE2(("update_window_attrs visible %d / %d\n",
	    visible_regions,
	    total_regions));
}
#endif /* OPT_VIDEO_ATTRS */

/*
 * If the screen is garbage, clear the physical screen and the virtual screen
 * and force a full update.
 */
static void
update_garbaged_screen(void)
{
#if !OPT_PSCREEN
    int j;
#endif
    int i;

    TRACE((T_CALLED "update_garbaged_screen\n"));

    for (i = 0; i < term.rows; ++i) {
	vscreen[i]->v_flag |= VFCHG;
#if OPT_REVSTA
	vscreen[i]->v_flag &= ~VFREV;
#endif
#if OPT_COLOR
	CurFcolor(vscreen[i]) = -1;
	CurBcolor(vscreen[i]) = -1;
#endif
#if !OPT_PSCREEN
	for (j = 0; j < term.cols; ++j) {
	    CELL_TEXT(i, j) = ' ';
#if OPT_VIDEO_ATTRS
	    CELL_ATTR(i, j) = 0;
#endif /* OPT_VIDEO_ATTRS */
	}
#endif
    }
#if !OPT_PSCREEN
#if OPT_COLOR
    term.setfore(gfcolor);	/* Need to set before erasing. */
    term.setback(gbcolor);
#endif
    movecursor(0, 0);		/* Erase the screen. */
    reset_term_attrs();
    term.eeop();
#else
    kbd_erase_to_end(0);
#endif
    sgarbf = FALSE;		/* Erase-page clears */
    need_update = FALSE;
    kbd_flush();

    returnVoid();
}

/*
 * Update the extended line which the cursor is currently on at a column less
 * than the terminal width.  The line will be scrolled right or left to let the
 * user see where the cursor is.
 */
static int
update_extended_line(int col, int excess, int use_excess)
{
    int rcursor;
    int zero;
    int scrollsiz;

    /* calculate what column the real cursor will end up in */
    if (!use_excess) {
	curcol = col;
	rcursor = (col % (term.cols - term.cols / 10));
    } else {
	scrollsiz = term.rows - ((term.rows / 10) * 2);
	rcursor = ((excess - 1) % scrollsiz) + term.cols / 10;
    }
    horscroll = col - rcursor;

    /* Scan through the line outputting characters to the virtual screen
     * once we reach the left edge.  */
    vtmove(currow, -horscroll);	/* start scanning offscreen */
    vtset(DOT.l, curwp);

    /* truncate the virtual line */
    vteeol();

    horscroll = 0;

    if ((use_excess || col != rcursor)
	&& (zero = nu_width(curwp)) < term.cols) {
	/* ... put a marker in column 1 */
	vscreen[currow]->v_text[zero] = (VIDEO_TEXT) MRK_EXTEND_LEFT[0];
    }
    vscreen[currow]->v_flag |= (VFEXT | VFCHG);
    return rcursor;
}

/*
 * Update the position of the hardware cursor and handle extended lines.  This
 * is the only update for simple moves.  Returns the screen location for the
 * cursor, and a boolean indicating if full sideways scroll was necessary
 */
static int
update_cursor_position(int *screenrowp, int *screencolp)
{
    LINE *lp;
#ifdef WMDLINEWRAP
    int i;
#endif
    int col, excess;
    int collimit;
    int moved = FALSE;
    int nuadj;
    int liadj;

    if (curwp == NULL) {
	*screenrowp = 0;
	*screencolp = 0;
	return FALSE;
    }
    nuadj = is_empty_buf(curwp->w_bufp) ? 0 : nu_width(curwp);
    liadj = (w_val(curwp, WMDLIST)) ? 1 : 0;

    TRACE2((T_CALLED "update_cursor_position\n"));
    /* find the current row */
    lp = curwp->w_line.l;
    currow = TopRow(curwp);
    while (lp != DOT.l) {
	currow += line_height(curwp, lp);
	lp = lforw(lp);
	if (lp == curwp->w_line.l
	    || currow > mode_row(curwp)) {
	    mlforce("BUG:  lost dot updpos().  setting at top");
	    DOT.l = lforw(buf_head(curbp));
	    DOT.o = b_left_margin(curbp);
	    lp = curwp->w_line.l = DOT.l;
	    currow = TopRow(curwp);
	}
    }

    /*
     * Find the current column.  Reuse the ruler column-value if we
     * computed it.
     */
#ifdef WMDRULER
    if (w_val(curwp, WMDRULER))
	col = curwp->w_ruler_col + w_left_margin(curwp) - 1;
    else
#endif
	col = dot_to_vcol(curwp) + w_left_margin(curwp);

#ifdef WMDLINEWRAP
    if (w_val(curwp, WMDLINEWRAP)) {
	curcol = col;
	collimit = term.cols - nuadj;
	*screenrowp = currow;
	if (col >= collimit) {
	    col -= collimit;
	    *screenrowp += 1;
	    if (col >= term.cols)
		*screenrowp += (col / term.cols);
	    *screencolp = col % term.cols;
	} else {
	    *screencolp = col + nuadj;
	}
	/* kludge to keep the cursor within the window */
	i = mode_row(curwp) - 1;
	if (*screenrowp > i) {
	    *screenrowp = i;
	    *screencolp = term.cols - 1;
	}
	TRACE2(("... (%d,%d)\n", *screenrowp, *screencolp));
	return2Code(FALSE);
    } else
#endif
	*screenrowp = currow;

    /* ...adjust to offset from shift-margin */
    curcol = col - w_val(curwp, WVAL_SIDEWAYS);
    *screencolp = curcol;

    /* if extended, flag so and update the virtual line image */
    collimit = col_limit(curwp);
    excess = curcol - collimit + liadj;
    if ((excess > 0) || (excess == 0 &&
			 (DOT.o < llength(DOT.l) - 1))) {
	if (w_val(curwp, WMDHORSCROLL)) {
	    (void) mvrightwind(TRUE, excess + collimit / 2);
	    moved = TRUE;
	    preset_lmap0();
	} else {
	    *screencolp = update_extended_line(col, excess, TRUE);
	}
    } else if (w_val(curwp, WVAL_SIDEWAYS) && (curcol < 1)) {
	if (w_val(curwp, WMDHORSCROLL)) {
	    (void) mvleftwind(TRUE, -curcol + collimit / 2 + 1);
	    moved = TRUE;
	    preset_lmap0();
	} else {
	    *screencolp = update_extended_line(col, 0, FALSE);
	}
    } else {
	if (vscreen[currow]->v_flag & VFEXT) {
	    update_screen_line(curwp, lp, currow);
	    vteeol();
	    /* this line no longer is extended */
	    vscreen[currow]->v_flag &= ~VFEXT;
	}
    }
    if (!moved)
	*screencolp += nuadj;
    TRACE2(("... (%d,%d)\n", *screenrowp, *screencolp));
    return2Code(moved);
}

#if CAN_SCROLL

static int
same_video_text(const VIDEO_TEXT * virtual_text, const VIDEO_TEXT *
		physical_text, int ncols)
{
    int rc = TRUE;
    int j;

    for (j = 0; j < ncols; ++j) {
	if (virtual_text[j] != physical_text[j]) {
	    rc = FALSE;
	    break;
	}
    }

    return rc;
}

/*
 * return TRUE on text match
 *
 * vrow - virtual row
 * prow - physical row
 */
static int
same_row_text(int vrow, int prow)
{
    return same_video_text(vscreen[vrow]->v_text,
			   pscreen[prow]->v_text,
			   term.cols);
}

/*
 * return the index of the first trailing whitespace character
 */
static int
endofline(const VIDEO_TEXT * s, int n)
{
    int i;
    for (i = n - 1; i >= 0; i--)
	if (s[i] != ' ')
	    return (i + 1);
    return (0);
}

/* move the "count" lines starting at "from" to "to" */
static void
scroll_screen(int from, int to, int count)
{
    beginDisplay();
    ttrow = ttcol = -1;
    term.scroll(from, to, count);
    endofDisplay();
}

/*
 * Optimize simple scrolled screen region movements.  Argument chooses between
 * looking for inserts or deletes.  Original code by Roger Ove, for an early
 * version of MicroEMACS.  used by permission.
 *
 * inserts - returns true if it does an optimization
 */
static int
simple_scroll(int inserts)
{
    struct VIDEO *vpv;		/* virtual screen image */
    struct VIDEO *vpp;		/* physical screen image */
    int i, j, k;
    int rows, cols;
    int first, match, count, ptarget = 0, vtarget = 0;
    size_t end;
    int longmatch, longcount;
    int longinplace, inplace;	/* count of lines which are already
				   in the right place */
    int from, to;

    if (term.scroll == nullterm_scroll)		/* can't scroll */
	return FALSE;

    rows = term.rows - 1;
    cols = term.cols;

    /* find first line that doesn't match */
    first = -1;
    for (i = 0; i < rows; i++) {
	if (!same_row_text(i, i)) {
	    first = i;
	    break;
	}
    }
    if (first < 0)
	return FALSE;		/* there isn't one */

    vpv = vscreen[first];
    vpp = pscreen[first];

    if (inserts) {
	/* determine types of potential scrolls */
	end = (size_t) endofline(vpv->v_text, cols);
	if (end == 0)
	    ptarget = first;	/* newlines */
	else if (same_video_text(vpp->v_text, vpv->v_text, (int) end))
	    ptarget = first + 1;	/* broken line newlines */
	else
	    ptarget = first;
	from = ptarget;
    } else {
	from = vtarget = first + 1;
    }

    /* can we find that text elsewhere ? */
    longmatch = -1;
    longcount = 0;
    longinplace = 0;
    for (i = from + 1; i < rows; i++) {
	if (inserts ? same_row_text(i, from) : same_row_text(from, i)) {
	    match = i;
	    count = 1;
	    inplace = same_row_text(match, match) ? 1 : 0;
	    for (j = match + 1, k = from + 1; j < rows && k < rows; j++, k++) {
		if (inserts ? same_row_text(j, k) : same_row_text(k, j)) {
		    count++;
		    if (same_row_text(j, j))
			inplace++;
		} else
		    break;
	    }
	    if (longcount - longinplace < count - inplace) {
		longcount = count;
		longmatch = match;
		longinplace = inplace;
	    }
	}
    }
    match = longmatch;
    count = longcount;

    if (!inserts) {
	/* full kill case? */
	if (match > 0 && same_row_text(first, match - 1)) {
	    vtarget--;
	    match--;
	    count++;
	}
    }

    if (match > 0 && count > 2) {	/* got a scroll */
	/* move the count lines starting at ptarget to match */
	/* mlwrite("scrolls: move the %d lines starting at %d to %d",
	   count,ptarget,match);
	 */
	if (inserts) {
	    from = ptarget;
	    to = match;
	} else {
	    from = match;
	    to = vtarget;
	}
#if OPT_PSCREEN
	/*
	 * Update lines _before_ the scroll so that they will be available for
	 * any updates which need to be done (due to a GraphicsExpose event in
	 * X11...these occur when scrolling a partially obscured window).  Note
	 * that in the typical case of scrolling a line or two that very few
	 * memory accesses are performed.  We mostly shuffle pointers around.
	 */
#define SWAP_PLINE(a, b) do { VIDEO *temp = pscreen[a];	\
			      pscreen[a] = pscreen[b];	\
			      pscreen[b] = temp; } one_time
#define CLEAR_PLINE(a)  do {						\
			    MARK_LINE_DIRTY(a);				\
			    for (j = 0; j < term.cols; j++) {		\
				CELL_TEXT(a,j) = ' ';			\
				CELL_ATTR(a,j) = 0;			\
			    }						\
			  } one_time
	if (from < to) {
	    /* FIXME: color */
	    for (i = from; i < to; i++)
		CLEAR_PLINE(i + count);
	    for (i = count - 1; i >= 0; i--)
		SWAP_PLINE(from + i, to + i);
	} else {
	    /* FIXME: color */
	    for (i = to; i < from; i++)
		CLEAR_PLINE(i);
	    for (i = 0; i < count; i++)
		SWAP_PLINE(from + i, to + i);
	}
#endif /* OPT_PSCREEN */
	scroll_screen(from, to, count);
#if !OPT_PSCREEN
	for (i = 0; i < count; i++) {
	    vpp = pscreen[to + i];
	    vpv = vscreen[to + i];
	    for (j = 0; j < cols; ++j)
		vpp->v_text[j] = vpv->v_text[j];
	}
#if OPT_VIDEO_ATTRS
#define SWAP_ATTR_PTR(a, b) do { VIDEO_ATTR *temp = pscreen[a]->v_attrs;  \
				 pscreen[a]->v_attrs = pscreen[b]->v_attrs; \
				 pscreen[b]->v_attrs = temp; } one_time
	if (from < to) {
	    /* FIXME: color */
	    for (i = from; i < to; i++)
		for (j = 0; j < term.cols; j++)
		    CELL_ATTR(i + count, j) = 0;
	    for (i = count - 1; i >= 0; i--)
		SWAP_ATTR_PTR(from + i, to + i);

	} else {
	    /* FIXME: color */
	    for (i = to; i < from; i++)
		for (j = 0; j < term.cols; j++)
		    CELL_ATTR(i, j) = 0;
	    for (i = 0; i < count; i++)
		SWAP_ATTR_PTR(from + i, to + i);
	}
#undef SWAP_ATTR_PTR
#endif /* OPT_VIDEO_ATTRS */
	if (inserts) {
	    from = ptarget;
	    to = match;
	} else {
	    from = vtarget + count;
	    to = match + count;
	}
	for (i = from; i < to; i++) {
	    VIDEO_TEXT *txt;
	    txt = pscreen[i]->v_text;
	    for (j = 0; j < term.cols; ++j)
		txt[j] = ' ';
	    vscreen[i]->v_flag |= VFCHG;
	}
#endif /* !OPT_PSCREEN */
	return (TRUE);
    }
    return (FALSE);
}

#endif /* CAN_SCROLL */

/*
 * update the physical screen from the virtual screen.  The 'force' flag
 * forces an update even if there is type-ahead.
 */
static void
update_physical_screen(int force GCC_UNUSED)
{
    int i;

    TRACE((T_CALLED "update_physical_screen(%d)\n", force));
#if CAN_SCROLL
    if (scrflags & WFKILLS)
	(void) simple_scroll(FALSE);
    if (scrflags & WFINS)
	(void) simple_scroll(TRUE);
    scrflags = 0;
#endif

    for (i = 0; i < term.rows; ++i) {
	/* for each line that needs to be updated */
	if ((vscreen[i]->v_flag & (VFCHG | VFCOL)) != 0) {
#if !DISP_X11
	    if (TypeAhead(force))
		returnVoid();
#endif
	    update_line(i, 0, term.cols);
	}
    }
    returnVoid();
}

#if OPT_MLFORMAT || OPT_POSFORMAT || OPT_TITLE
static void
mlfs_prefix(const char **fsp, char **msp, int lchar)
{
    const char *fs = *fsp;
    char *ms = *msp;

    if (*fs == ':') {
	fs++;
	while (*fs && *fs != ':') {
	    if (*fs != '%')
		*ms++ = *fs++;
	    else {
		fs++;
		switch (*fs++) {
		case EOS:
		    fs--;
		    break;
		case '%':
		    *ms++ = '%';
		    break;
		case ':':
		    *ms++ = ':';
		    break;
		case '-':
		    *ms++ = (char) lchar;
		    break;
		default:
		    *ms++ = '%';
		    *ms++ = *(fs - 1);
		    break;
		}
	    }
	}
    }
    *fsp = fs;
    *msp = ms;
}

static void
mlfs_suffix(const char **fsp, char **msp, int lchar)
{
    mlfs_prefix(fsp, msp, lchar);
    if (**fsp == ':')
	(*fsp)++;
}

static void
mlfs_skipfix(const char **fsp)
{
    const char *fs = *fsp;
    if (*fs == ':') {
	for (fs++; *fs && *fs != ':'; fs++) ;
	if (*fs == ':')
	    fs++;
	for (; *fs && *fs != ':'; fs++) ;
	if (*fs == ':')
	    fs++;
    }
    *fsp = fs;
}
#endif /* OPT_MLFORMAT */

#define PutModename(format, name) { \
		if (ms != NULL) { \
			ms = lsprintf(ms, format, \
				mcnt ? ' ' : '[', \
				name); \
		} \
		mcnt++; \
	}

#define PutMode(mode,name) \
	if (b_val(bp, mode)) PutModename("%c%s", name)

#if OPT_MAJORMODE
#define PutMajormode(bp) \
	if (bp->majr != NULL) \
	    PutModename("%c%s", \
		brief \
		? bp->majr->shortname \
		: bp->majr->longname)
#else
#define PutMajormode(bp)	/*nothing */
#endif

static int
modeline_modes(BUFFER *bp, char **msptr, int brief)
{
    char *ms = msptr ? *msptr : NULL;
    size_t mcnt = 0;

    PutMajormode(bp);
#if COMPLETE_FILES || COMPLETE_DIRS
    if (b_is_directory(bp))
	PutModename("%c%s", brief ? "dir" : "directory");
#endif
#if !OPT_MAJORMODE
    PutMode(MDCMOD, "cmode");
#endif
#if OPT_ENCRYPT
    PutMode(MDCRYPT, "crypt");
#endif
    {
#if OPT_RECORDSEP_CHOICES
	int show = FALSE;

	switch (b_val(bp, VAL_SHOW_FORMAT)) {
	case SF_ALWAYS:
	    show = TRUE;
	    break;
	case SF_NEVER:
	    show = FALSE;
	    break;
	case SF_DIFFERS:
	    show = b_val(bp, VAL_RECORD_SEP) != global_b_val(VAL_RECORD_SEP);
	    break;
	case SF_FOREIGN:
	    show = b_val(bp, VAL_RECORD_SEP) != RS_SYS_DEFAULT;
	    break;
	case SF_LOCAL:
	    show = is_local_b_val(bp, VAL_RECORD_SEP);
	    break;
	}
	if (show)
	    PutModename("%c%s",
			choice_to_name(&fsm_recordsep_blist,
				       b_val(bp, VAL_RECORD_SEP)));
#else
	PutMode(MDDOS, brief ? "dos" : "dos-style");
#endif
    }
    PutMode(MDREADONLY, brief ? "ro" : "read-only");
    PutMode(MDVIEW, brief ? "view" : "view-only");
    PutMode(MDLOADING, "loading");
#if OPT_LCKFILES
    PutMode(MDLOCKED, "locked by");
    if (ms != 0 && b_val(bp, MDLOCKED))
	ms = lsprintf(ms, " %s", b_val_ptr(bp, VAL_LOCKER));
#endif
    if (mcnt && ms)
	*ms++ = ']';
    if (b_is_changed(bp)) {
	if (ms != NULL)
	    ms = lsprintf(ms, "%s[modified]", mcnt ? " " : "");
	mcnt++;
    }
    if (kbd_mac_recording()) {
	if (ms != NULL)
	    ms = lsprintf(ms, "%s[recording]", mcnt ? " " : "");
	mcnt++;
    }
    if (ms != NULL)
	*msptr = ms;
    return (mcnt != 0);
}

static char
modeline_show(WINDOW *wp, int lchar)
{
    char ic = (char) lchar;
    BUFFER *bp = wp->w_bufp;

    if (b_val(bp, MDSHOWMODE)) {
#ifdef insertmode		/* insert mode is a trait for each window */
	if (wp->w_traits.insmode == INSMODE_INS)
	    ic = 'I';
	else if (wp->w_traits.insmode == INSMODE_RPL)
	    ic = 'R';
	else if (wp->w_traits.insmode == INSMODE_OVR)
	    ic = 'O';
#else /* insertmode is a variable global to all windows */
	if (wp == curwp) {
	    if (insertmode == INSMODE_INS)
		ic = 'I';
	    else if (insertmode == INSMODE_RPL)
		ic = 'R';
	    else if (insertmode == INSMODE_OVR)
		ic = 'O';
	}
#endif /* !defined(insertmode) */
    }
    return ic;
}

static const char *
rough_position(WINDOW *wp)
{
    LINE *lp = wp->w_line.l;
    int rows = wp->w_ntrows;
    const char *msg = NULL;

    while (rows-- > 0) {
	lp = lforw(lp);
	if (lp == win_head(wp)) {
	    msg = "bot";
	    break;
	}
    }
    if (lback(wp->w_line.l) == win_head(wp)) {
	if (msg) {
	    if (wp->w_line.l == win_head(wp))
		msg = "emp";
	    else
		msg = "all";
	} else {
	    msg = "top";
	}
    }
    if (!msg)
	msg = "mid";
    return msg;
}

#define MAX_FORMAT ((NFILEN + 1) * 2 * COLS_UTF8)

#define MARK_COL 80

#if OPT_MLFORMAT || OPT_POSFORMAT || OPT_TITLE

#define L_CURL '{'
#define R_CURL '}'

static int
percentage(WINDOW *wp)
{
    BUFFER *bp = wp->w_bufp;
    L_NUM val;
    L_NUM denom = vl_line_count(bp);

#ifdef WMDRULER
    if (w_val(wp, WMDRULER) && !is_empty_buf(bp))
	val = wp->w_ruler_line;
    else
#endif
	val = line_no(bp, wp->w_dot.l);

    return PERCENT(val, denom);
}

#if OPT_MULTIBYTE
static int
str_col2offs(WINDOW *wp, int target_col, const char *source, int length)
{
    const char *base = source;
    int result = 0;
    int have_col = 0;

    while (have_col < target_col) {
	int rc;
	UINT value;
	int need = column_sizes(wp, source, (UINT) length, &rc);
	int cells = need;

	switch (need) {
	case COLS_UTF8:
	    rc = vl_conv_to_utf32(&value, source, (B_COUNT) length);
	    cells = vl_wcwidth((int) value);
	    source += rc;
	    length -= rc;
	    break;
	case COLS_CTRL:
	    source += 1;
	    length -= 1;
	    break;
	case COLS_8BIT:
	    source += 1;
	    length -= 1;
	    break;
	default:
	    source += 1;
	    length -= 1;
	    break;
	}
	have_col += cells;

	if (have_col < target_col)
	    result = (int) (source - base);
    }
    return result;
}
#else
#define str_col2offs(wp,target_col,value,length) target_col
#endif

#define HaveEnough(string) ((ms + strlen(string)) - base < MAX_FORMAT)

/*
 * Format special single-use messages, i.e., the modeline format, which has
 * a number of special variables that we would like to output quickly.
 */
void
special_formatter(TBUFF **result, const char *fs, WINDOW *wp)
{
    BUFFER *bp;
    char *ms;
    char *base;
    const char *want;
    const char *save_fs;
    char left_ms[MAX_FORMAT];
    char right_ms[MAX_FORMAT];
    char temp[MAX_FORMAT / 2];
    int have_cols;
    int col;
    int fc;
    char lchar;
    int need_eighty_column_indicator;
    int right_cols;
    int right_offs;
    int n;
    int skip = TRUE;

    if (fs == NULL)
	return;

    if (wp == wnullp)
	return;

    TRACE((T_CALLED "special_formatter %s\n", fs));
    tb_init(result, EOS);

    left_ms[0] = right_ms[0] = EOS;
    ms = base = left_ms;
    need_eighty_column_indicator = FALSE;

    bp = wp->w_bufp;

    if (wp == curwp) {		/* mark the current buffer */
	lchar = '=';
    } else {
#if OPT_REVSTA
	if (revexist)
	    lchar = ' ';
	else
#endif
	    lchar = '-';
    }

    while (*fs) {
	/* check for single-character buffer overflow */
	if ((ms + (COLS_UTF8 + 2)) - base >= MAX_FORMAT) {
	    if (base == right_ms)
		break;
	    if (strncmp(fs, "%=", (size_t) 2)) {
		++fs;
		continue;
	    }
	}
	if (*fs != '%') {
	    *ms++ = *fs++;
	} else {
	    fs++;
	    switch ((fc = *fs++)) {
	    case EOS:		/* Null character ! */
		fs--;
		break;
	    case '%':
	    case ':':
		*ms++ = *(fs - 1);
		break;
	    case '|':
		need_eighty_column_indicator = TRUE;
		break;
	    case '-':
		*ms++ = lchar;
		break;
	    case '=':
		*ms = EOS;
		ms = base = right_ms;
		break;
	    case 'i':		/* insert mode indicator */
		*ms++ = modeline_show(wp, lchar);
		break;
	    case 'b':
		want = (bp ? bp->b_bname : UNNAMED_BufName);
		if (!HaveEnough(want))
		    continue;
		ms = lsprintf(ms, "%s", want);
		break;
	    case 'M':
	    case 'm':
		if (bp != NULL && modeline_modes(bp, (char **) 0, (fc == 'M'))) {
		    mlfs_prefix(&fs, &ms, lchar);
		    (void) modeline_modes(bp, &ms, (fc == 'M'));
		    mlfs_suffix(&fs, &ms, lchar);
		} else
		    mlfs_skipfix(&fs);
		break;
	    case 'f':
	    case 'F':
		skip = TRUE;	/* assumption */

		if (bp != NULL && bp->b_fname != NULL) {
		    char *p;

		    /*
		     * when b_fname is a pipe cmd, it can be
		     * arbitrarily long
		     */
		    vl_strncpy(temp, bp->b_fname, sizeof(temp));

		    if ((p = shorten_path(temp, FALSE)) != NULL
			&& *(p = skip_space_tab(p)) != '\0'
			&& !eql_bname(bp, p)
			&& ((fc == 'f')
			    ? !is_internalname(p)
			    : is_internalname(p))) {
			mlfs_prefix(&fs, &ms, lchar);
			if (!HaveEnough(p))
			    continue;
			ms = lsprintf(ms, "%s", p);
			mlfs_suffix(&fs, &ms, lchar);
			skip = FALSE;
		    }
		}
		if (skip)
		    mlfs_skipfix(&fs);
		break;
	    case 'r':
	    case 'n':
	    case 'N':
		mlfs_prefix(&fs, &ms, lchar);
		if (bp != NULL) {
		    if (bp->b_fname != NULL && !is_internalname(bp->b_bname)) {

			vl_strncpy(temp, bp->b_fname, sizeof(temp));

			switch (fc) {
			case 'r':
			    want = shorten_path(temp, FALSE);
			    if (want == NULL)
				want = temp;
			    break;
			case 'n':
			    want = pathleaf(temp);
			    break;
			default:
			    want = temp;
			    break;
			}
		    } else {
			want = bp->b_bname;
		    }
		} else {
		    want = UNNAMED_BufName;
		}
		if (!HaveEnough(want))
		    continue;
		ms = lsprintf(ms, "%s", want);
		mlfs_suffix(&fs, &ms, lchar);
		break;
#ifdef WMDRULER
	    case 'l':		/* line number */
	    case 'c':		/* column number */
	    case 'p':		/* percentage */
	    case 'L':		/* number of lines in buffer */

		if (w_val(wp, WMDRULER) && (bp != NULL && !is_empty_buf(bp))) {
		    int val = 0;
		    switch (fc) {
		    case 'l':
			val = wp->w_ruler_line;
			break;
		    case 'L':
			val = vl_line_count(bp);
			break;
		    case 'c':
			val = wp->w_ruler_col;
			break;
		    case 'p':
			val = percentage(wp);
			break;
		    }
		    mlfs_prefix(&fs, &ms, lchar);
		    ms = lsprintf(ms, "%d", val);
		    mlfs_suffix(&fs, &ms, lchar);
		} else
		    mlfs_skipfix(&fs);
		break;

#endif
#ifdef WMDSHOWCHAR
	    case 'C':
		if (w_val(wp, WMDSHOWCHAR)
		    && (bp != NULL && !is_empty_buf(bp))
		    && (wp->w_dot.o < llength(wp->w_dot.l)
			|| line_has_newline(wp->w_dot.l, bp))) {
		    sprintf(temp, "%02X", char_at_mark(wp->w_dot));
		    mlfs_prefix(&fs, &ms, lchar);
		    ms = lsprintf(ms, "%s", temp);
		    mlfs_suffix(&fs, &ms, lchar);
		} else {
		    mlfs_skipfix(&fs);
		}
		break;
#endif
	    case 'P':
		ms = lsprintf(ms, "%d", percentage(wp));
		break;

	    case 'S':
		if (
#ifdef WMDRULER
		       !w_val(wp, WMDRULER) ||
#endif
		       ((bp == NULL) || is_empty_buf(bp))) {
		    mlfs_prefix(&fs, &ms, lchar);
		    ms = lsprintf(ms, " %s ", rough_position(wp));
		    mlfs_suffix(&fs, &ms, lchar);
		} else
		    mlfs_skipfix(&fs);
		break;
	    case L_CURL:
		save_fs = fs;
		while (*fs != EOS && *fs != R_CURL)
		    fs++;

		if (fs != save_fs) {
		    int flag = clexec;
		    char *save_execstr;
		    TBUFF *tok = NULL;

		    save_execstr = execstr;
		    clexec = TRUE;
		    execstr = temp;

		    strncpy0(temp, save_fs, (size_t) (fs + 1 - save_fs));
		    execstr = get_token(execstr,
					&tok,
					eol_null,
					EOS,
					(int *) 0);
		    vl_strncpy(temp, tokval(tb_values(tok)), sizeof(temp));

		    tb_free(&tok);
		    execstr = save_execstr;
		    clexec = flag;
		} else {
		    *temp = EOS;
		}
		if (*fs != EOS)
		    fs++;
		save_fs = fs;
		/*
		 * Allow an optional <number><format> on the end of the
		 * token, to reformat it.  Don't bother reformatting if
		 * it is just a 'd' added to make the string unambiguous.
		 */
		fs = skip_cnumber(fs);
		if (isAlpha(*fs)) {
		    if ((*fs != 'd' || fs != save_fs)) {
			char format[NSTRING];

			format[0] = '%';
			strncpy0(format + 1, save_fs, (size_t) (fs + 2 - save_fs));
			if (strchr("dDxXo", *fs)) {
			    int value = scan_int(temp);
			    ms = lsprintf(ms, format, value);
			} else {
			    ms = lsprintf(ms, format, temp);
			}
		    } else {
			ms = lsprintf(ms, "%s", temp);
		    }
		    fs++;
		} else {
		    ms = lsprintf(ms, "%s", temp);
		}
		break;
	    default:
		*ms++ = '%';
		*ms++ = *(fs - 1);
		break;
	    }
	}
    }
    *ms++ = EOS;

    tb_bappend(result, left_ms, strlen(left_ms));

    if (tb_values(*result) != NULL) {
	have_cols = tb_columns(*result);

	if ((have_cols < term.cols)
	    && (right_cols = str_columns(right_ms)) != 0) {
	    for (n = term.cols - have_cols - right_cols; n > 0; n--)
		tb_append(result, lchar);
	    if ((n = term.cols - right_cols) < 0) {
		col = right_cols + n;
		n = -n;
	    } else {
		col = right_cols;
		n = 0;
	    }
	    if ((col < term.cols)
		&& (n < right_cols)) {
		right_offs = str_col2offs(wp, n, right_ms, (int) strlen(right_ms));
		if ((tb_columns(*result) + col) > term.cols) {
		    tb_setlen(result,
			      str_col2offs(wp, term.cols - col,
					   tb_values(*result),
					   (int) tb_length(*result)));
		    have_cols = tb_columns(*result);
		    for (n = term.cols - have_cols - right_cols; n > 0; n--)
			tb_append(result, lchar);
		}
		tb_bappend(result, right_ms + right_offs, (size_t) col);
	    }
	}
    }

    /* mark column 80 */
    if (tb_values(*result) != NULL) {
	have_cols = tb_columns(*result);

	if (need_eighty_column_indicator) {
	    int left = -nu_width(wp);
	    char *ss;
#ifdef WMDLINEWRAP
	    if (!w_val(wp, WMDLINEWRAP))
#endif
		left += w_val(wp, WVAL_SIDEWAYS);
	    n = term.cols + left;
	    col = MARK_COL - left;

	    if ((n > MARK_COL) && (col >= 0)) {
		for (n = have_cols; n < col; n++)
		    tb_append(result, lchar);
		right_offs = str_col2offs(wp, col,
					  tb_values(*result),
					  (int) tb_length(*result));
		if ((ss = tb_values(*result)) != NULL
		    && ss[right_offs] == lchar) {
		    ss[right_offs] = '|';
		}
	    }
	}
    }
    tb_append(result, EOS);
    returnVoid();
}
#endif

/*
 * Redisplay the mode line for the window pointed to by the "wp".
 * This is the only routine that has any idea of how the modeline is
 * formatted.  You can change the modeline format by hacking at this
 * routine.  Called by "update" any time there is a dirty window.
 */
static void
update_modeline(WINDOW *wp)
{
#if OPT_MLFORMAT
    static TBUFF *result;
#else
    static char lchar_pad[4];
    BUFFER *bp;
    char temp[MAX_FORMAT / 2];
    int col;
    char lchar[2];
    int right_cols;
    char left_ms[MAX_FORMAT];
    char *ms;
    char right_ms[MAX_FORMAT];
    int my_col;
#endif
    int my_row;

    TRACE((T_CALLED "update_modeline\n"));
    if (is_vtinit()) {
	term.cursorvis(FALSE);

	my_row = mode_row(wp);	/* Location. */
#if OPT_VIDEO_ATTRS
	{
	    VIDEO_ATTR attr;
	    if (wp == curwp)
		attr = VAMLFOC;
	    else
		attr = VAML;
#if OPT_REVSTA
#ifdef	GVAL_MCOLOR
	    attr = (VIDEO_ATTR) (attr | (global_g_val(GVAL_MCOLOR) & ~VASPCOL));
#else
	    attr |= VAREV;
#endif
#endif
	    vscreen[my_row]->v_flag |= VFCHG;
	    set_vattrs(my_row, 0, attr, (size_t) term.cols);
	}
#else
	vscreen[my_row]->v_flag |= VFCHG | VFREQ | VFCOL;	/* Redraw next time. */
#endif
#if OPT_COLOR
	ReqFcolor(vscreen[my_row]) = gfcolor;
	ReqBcolor(vscreen[my_row]) = gbcolor;
#endif
	vtmove(my_row, 0);	/* Seek to right line. */

#if OPT_MLFORMAT
	special_formatter(&result, modeline_format, wp);
#if OPT_MULTIBYTE
	{
	    char *from = tb_values(result);
	    int n = (int) strlen(from);
	    VTSET_LOOP(n > 0);
	}
#else
	vtputsn(wp, tb_values(result), tb_length(result));
#endif
#else /* hard-coded format */
	bp = wp->w_bufp;
	if (bp == 0)
	    bp = curbp;
	if (bp == 0)
	    bp = bminip;
	if (wp == curwp) {	/* current buffer shows as = */
	    lchar[0] = '=';
	} else {
#if OPT_REVSTA
	    if (revexist)
		lchar[0] = ' ';
	    else
#endif
		lchar[0] = '-';
	}
	left_ms[0] = right_ms[0] = EOS;
	ms = left_ms;
	ms = lsprintf(ms, "%c%c%c %s ",
		      lchar[0], modeline_show(wp, lchar[0]), lchar[0], bp->b_bname);
	if (modeline_modes(bp, &ms, FALSE))
	    *ms++ = ' ';
	if (bp->b_fname != 0
	    && (shorten_path(vl_strncpy(temp, bp->b_fname, sizeof(temp)), FALSE))
	    && !eql_bname(bp, temp)) {
	    if (is_internalname(temp)) {
		for (my_col = term.cols - (13 + strlen(temp));
		     my_col > 0; my_col--)
		    *ms++ = lchar[0];
	    } else {
		ms = lsprintf(ms, "is");
	    }
	    ms = lsprintf(ms, " %s ", temp);
	}
	memset(lchar_pad, lchar[0], sizeof(lchar_pad) - 1);
#ifdef WMDRULER
	if (w_val(wp, WMDRULER))
	    (void) lsprintf(right_ms, " (%d,%d) %s",
			    wp->w_ruler_line, wp->w_ruler_col, lchar_pad);
	else
#endif
	    (void) lsprintf(right_ms, " %s %s", rough_position(wp), lchar_pad);

	*ms++ = EOS;
	right_cols = str_columns(right_ms);
	vtputsn(wp, left_ms, term.cols);
	for (my_col = term.cols - str_columns(left_ms) - right_cols;
	     my_col > 0;
	     my_col--)
	    vtputc(wp, lchar, 1);

	vtcol = term.cols - right_cols;
	if (vtcol < 0) {
	    my_col = -vtcol;
	    vtcol = 0;
	} else {
	    my_col = 0;
	}

	if (term.cols > vtcol)
	    vtputsn(wp, right_ms + my_col, term.cols - vtcol);

	col = -nu_width(wp);
#ifdef WMDLINEWRAP
	if (!w_val(wp, WMDLINEWRAP))
#endif
	    col += w_val(wp, WVAL_SIDEWAYS);
	my_col = term.cols + col;
	col = MARK_COL - col;

	if ((my_col > MARK_COL)
	    && (col >= 0)
	    && (vscreen[vtrow]->v_text[col] == lchar[0])) {
	    vtcol = col;
	    vtputc(wp, "|", 1);
	}
#endif /* OPT_MLFORMAT */
	term.cursorvis(TRUE);
	TRACE2(("MODE %4d:%s\n",
		vtrow,
		visible_video_text(vscreen[vtrow]->v_text, vtcol)));
    }
    returnVoid();
}

/*
 * Update all the mode lines.
 */
void
upmode(void)
{
    WINDOW *wp;

    for_each_window(wp) {
	wp->w_flag |= WFMODE;
    }
}

/*
 * Check to see if the cursor is on in the window and re-frame it if needed or
 * wanted.
 */
#if OPT_UPBUFF
static void
reframe_cursor_position(WINDOW *wp)
{
    LINE *dlp;
    LINE *lp;
    int i = 0;
    int rows;
    int founddot = FALSE;	/* set to true iff we find dot */
    int tildecount;

    /* if not a requested reframe, check for a needed one */
    if ((wp->w_flag & WFFORCE) == 0) {
	/* initial update in main.c may not set these first... */
	if (!valid_line_wp(wp->w_dot.l, wp)) {
	    wp->w_dot.l = lforw(win_head(wp));
	    wp->w_dot.o = 0;
	}
	if (!valid_line_wp(wp->w_line.l, wp)) {
	    wp->w_line.l = wp->w_dot.l;
	    wp->w_line.o = 0;
	}
#if CAN_SCROLL
	/* loop from one line above the window to one line after */
	lp = lback(wp->w_line.l);
	i = -line_height(wp, lp);
#else
	/* loop through the window */
	lp = wp->w_line.l;
	i = 0;
#endif
	for_ever {
	    /* if the line is in the window, no reframe */
	    if (lp == wp->w_dot.l) {
		founddot = TRUE;
#if CAN_SCROLL
		/* if not _quite_ in, we'll reframe gently */
		if (i < 0 || i >= wp->w_ntrows) {
		    /* if the terminal can't help, then
		       we're simply outside */
		    if (term.scroll == nullterm_scroll)
			i = wp->w_force;
		    break;
		}
#endif
#ifdef WMDLINEWRAP
		if (w_val(wp, WMDLINEWRAP)
		    && i > 0
		    && i + line_height(wp, lp) > wp->w_ntrows) {
		    i = wp->w_ntrows;
		    break;
		}
#endif
		lp = wp->w_line.l;
		goto kill_tildes;
	    }

	    /* if we are at the end of the file, reframe */
	    if (i >= 0 && lp == win_head(wp))
		break;

	    /* on to the next line */
	    if (i >= wp->w_ntrows) {
		i = 0;		/* dot-not-found */
		break;
	    }
	    i += line_height(wp, lp);
	    lp = lforw(lp);
	}
    }
#if CAN_SCROLL
    if (i < 0) {		/* we're just above the window */
	i = 1;			/* put dot at first line */
	scrflags |= WFINS;
    } else if (founddot && (i >= wp->w_ntrows)) {
	/* we're just below the window */
	i = -1;			/* put dot at last line */
	scrflags |= WFKILLS;
    } else			/* put dot where requested */
#endif
	i = wp->w_force;	/* (is 0, unless reposition() was called) */

    wp->w_flag |= WFMODE;
    wp->w_line.o = 0;

    /* w_force specifies which line of the window dot should end up on */
    /*      positive --> lines from the top                         */
    /*      negative --> lines from the bottom                      */
    /*      zero --> middle of window                               */

    lp = wp->w_dot.l;

#ifdef WMDLINEWRAP
    /*
     * Center dot in middle of screen with line-wrapping
     */
    if (i == 0 && w_val(wp, WMDLINEWRAP)) {
	rows = (wp->w_ntrows - line_height(wp, lp) + 2) / 2;
	while (rows > 0) {
	    dlp = lback(lp);
	    if (dlp == win_head(wp))
		break;
	    if ((rows -= line_height(wp, dlp)) < 0)
		break;
	    lp = dlp;
	}
    } else
#endif
    {
	rows = (i != 0)
	    ? wp->w_ntrows
	    : wp->w_ntrows / 2;
	while (rows > 0) {
	    if ((i > 0)
		&& (--i <= 0))
		break;
	    dlp = lback(lp);
	    if (dlp == win_head(wp))
		break;
	    if ((rows -= line_height(wp, lp)) < 0)
		break;
	    lp = dlp;
	}
	if (rows < line_height(wp, lp)
	    && (lp != wp->w_dot.l)) {
	    while (i++ < 0) {
		dlp = lforw(lp);
		if (dlp == win_head(wp))
		    break;
		else
		    lp = dlp;
	    }
	}
    }
  kill_tildes:
    /* Eliminate as many tildes as possible from bottom */
    dlp = lp;
    rows = wp->w_ntrows;
    while (rows > 0 && (dlp != win_head(wp))) {
	rows -= line_height(wp, dlp);
	dlp = lforw(dlp);
    }
    dlp = lback(lp);

    tildecount = (wp->w_ntrows * ntildes) / 100;
    if (tildecount == wp->w_ntrows)
	tildecount--;

    while (dlp != win_head(wp)
	   && (rows -= line_height(wp, dlp)) >= tildecount) {
	lp = dlp;
	dlp = lback(lp);
    }

    /* and reset the current line-at-top-of-window */
    if (lp != win_head(wp)	/* mouse click could be past end */
	&&lp != wp->w_line.l) {	/* no need to set it if already there */
	wp->w_line.l = lp;
	wp->w_flag |= WFHARD;
	clr_typed_flags(wp->w_flag, USHORT, WFFORCE);
    }
#ifdef WMDLINEWRAP
    /*
     * Ensure that dot will be visible, by adjusting the w_line.o value if
     * necessary.  That's used to start the beginning of the first line in
     * a window "before" the start of the window.
     */
    if (w_val(wp, WMDLINEWRAP)
	&& sameline(wp->w_line, wp->w_dot)) {
	int want = mark2col(wp, wp->w_dot) / term.cols;
	if (want + wp->w_line.o >= wp->w_ntrows) {
	    wp->w_line.o = wp->w_ntrows - want - 1;
	    wp->w_flag |= WFHARD;
	    clr_typed_flags(wp->w_flag, USHORT, WFFORCE);
	} else if (want + wp->w_line.o < 0) {
	    wp->w_line.o = -want;
	    wp->w_flag |= WFHARD;
	    clr_typed_flags(wp->w_flag, USHORT, WFFORCE);
	}
    } else if (!w_val(wp, WMDLINEWRAP)) {
	if (wp->w_line.o < 0) {
	    wp->w_line.o = 0;
	    wp->w_flag |= WFHARD;
	    clr_typed_flags(wp->w_flag, USHORT, WFFORCE);
	}
    }
#endif
}
#endif /* OPT_UPBUFF */

/*
 * De-extend any line that deserves it.
 */
static void
de_extend_lines(void)
{
    WINDOW *wp;
    LINE *lp;
    int i;

    for_each_visible_window(wp) {
	if ((lp = wp->w_line.l) == NULL) {
	    /* this should not happen */
	    mlforce("BUG:  lost w_line for %s", wp->w_bufp->b_bname);
	    lp = win_head(wp);
	    wp->w_toprow = 0;
	}
	if (valid_line_wp(lp, wp)) {
	    i = TopRow(wp);

	    while (i < mode_row(wp)) {
		if (i >= 0
		    && (vscreen[i]->v_flag & VFEXT) != 0) {
		    if ((wp != curwp)
			|| (lp != wp->w_dot.l)
			|| ((i != currow)
			    && (curcol < col_limit(wp)))) {
			update_screen_line(wp, lp, i);
			vteeol();
			/* this line no longer is extended */
			vscreen[i]->v_flag &= ~VFEXT;
		    }
		}
		i += line_height(wp, lp);
		lp = lforw(lp);
	    }
	}
    }
}

/*
 * Make sure that the display is right. This is a three part process. First,
 * scan through all of the windows looking for dirty ones. Check the framing,
 * and refresh the screen. Second, make sure that "currow" and "curcol" are
 * correct for the current window. Third, make the virtual and physical
 * screens the same.
 */
int
update(int force /* force update past type ahead? */ )
{
    WINDOW *wp;
    int origrow, origcol;
    int screenrow, screencol;
    int updated = FALSE;

    TRACE((T_CALLED "update(%d)\n", force));

    /* Get row and column prior to doing the update in case we are
     * reading the message line.
     */
    origrow = ttrow;
    origcol = ttcol;

    if (clhide || (valid_window(curwp) && !is_visible_window(curwp)))
	returnCode(TRUE);

    /*
     * If not initialized, just return.
     */
    if (!valid_buffer(curbp)
	|| !vscreen
	|| !valid_window(curwp))
	returnCode(FALSE);

    /*
     * Don't try to update if we got called via a read-hook on a window
     * that isn't complete.
     */
    if (!valid_buffer(curwp->w_bufp)
	|| curwp->w_bufp->b_nwnd == 0
	|| curwp->w_ntrows < 1)
	returnCode(FALSE);
    if (TypeAhead(force))
	returnCode(SORTOFTRUE);

    /* don't display during keystroke replay */
    if (!force && kbd_replaying(TRUE) && (get_recorded_char(FALSE) != -1))
	returnCode(SORTOFTRUE);

    beginDisplay();

    preset_lmap0();
#if OPT_TITLE
    /*
     * Only update the title when we have nothing better to do.  The
     * auto_set_title logic is otherwise likely to set the title frequently if
     * [Buffer List] is visible.
     */
    if (tb_values(request_title) != NULL
	&& (tb_values(current_title) == NULL
	    || strcmp(tb_values(current_title), tb_values(request_title)))) {
	tb_copy(&current_title, request_title);
	term.set_title(tb_values(request_title));
    }
#endif

    /* propagate mode line changes to all instances of
       a buffer displayed in more than one window */
    for_each_visible_window(wp) {
	if (wp->w_flag & WFMODE) {
	    if (wp->w_bufp->b_nwnd > 1) {
		/* make sure all previous windows have this */
		WINDOW *owp;
		for_each_visible_window(owp) {
		    if (owp->w_bufp == wp->w_bufp)
			owp->w_flag |= WFMODE;
		}
	    }
	}
#ifdef WMDLINEWRAP
	/* Make sure that movements in very long wrapped lines
	   get updated properly.  */
	if (w_val(wp, WMDLINEWRAP)
	    && line_height(wp, wp->w_dot.l) > wp->w_ntrows)
	    wp->w_flag |= WFMOVE;
#endif
#if OPT_CACHE_VCOL
	if (wp->w_flag & (WFEDIT | WFHARD | WFMODE | WFKILLS | WFINS))
	    wp->w_traits.w_left_dot = nullmark;
#endif
    }
#ifdef WMDSHOWVARS
    if (curwp != NULL
	&& w_val(curwp, WMDSHOWVARS)) {
	updatelistvariables();
    }
#endif

    /* look for scratch-buffers that should be recomputed.  */
#if OPT_UPBUFF
    for_each_visible_window(wp) {
	if (wp->w_flag) {
	    reframe_cursor_position(wp);	/* check the framing */
	}
	if (b_is_obsolete(wp->w_bufp))
	    recompute_buffer(wp->w_bufp);
    }
#endif

    /* look for windows that need the ruler updated */
#ifdef WMDRULER
    for_each_visible_window(wp) {
#ifdef WMDSHOWCHAR
	if (w_val(wp, WMDSHOWCHAR)) {
	    wp->w_flag |= WFMODE;
	    update_char_classes();
	}
#endif
	if (w_val(wp, WMDRULER)) {
	    int line = line_no(wp->w_bufp, wp->w_dot.l);
	    int col = dot_to_vcol(wp) + 1;

	    if (line != wp->w_ruler_line
		|| col != wp->w_ruler_col) {
		wp->w_ruler_line = line;
		wp->w_ruler_col = col;
		wp->w_flag |= WFMODE;
	    }
	} else if (wp->w_flag & WFSTAT) {
	    wp->w_flag |= WFMODE;
	}
	clr_typed_flags(wp->w_flag, USHORT, WFSTAT);
    }
#endif

    do {
	/* update any windows that need refreshing */
	for_each_visible_window(wp) {
	    if (wp->w_flag) {
		if ((wp->w_flag & ~(WFMOVE)) && !updated++)
		    term.cursorvis(FALSE);
		/* if the window has changed, service it */
		if (wp->w_flag & (WFKILLS | WFINS)) {
		    scrflags |= (wp->w_flag & (WFINS | WFKILLS));
		    clr_typed_flags(wp->w_flag, USHORT, WFKILLS | WFINS);
		}
		if ((wp->w_flag & ~(WFMODE)) == WFEDIT) {
		    update_oneline(wp);		/* update EDITed line */
		} else if (wp->w_flag & ~(WFMOVE | WFMODE)) {
		    update_all(wp);	/* update all lines */
		}
#if OPT_SCROLLBARS
		if (wp->w_flag & (WFHARD | WFMOVE | WFSBAR))
		    gui_update_scrollbar(wp);
#endif /* OPT_SCROLLBARS */

#if OPT_VIDEO_ATTRS
		if (wp->w_flag & (WFHARD | WFEDIT))
		    update_window_attrs(wp);
#endif
		if (scrflags || (wp->w_flag & (WFMODE | WFCOLR)))
		    update_modeline(wp);	/* update modeline */
		wp->w_flag = 0;
		wp->w_force = 0;
	    }
	}

	/* Recalculate the current hardware cursor location.  If true, we've
	 * done a horizontal scroll.
	 */
    } while (update_cursor_position(&screenrow, &screencol));

    /* check for lines to de-extend */
    de_extend_lines();

    /* if screen is garbage, re-plot it */
    if (sgarbf || need_update)
	update_garbaged_screen();

    /* update the virtual screen to the physical screen */
    update_physical_screen(force);

    /* update the cursor and flush the buffers */
    if (reading_msg_line)
	movecursor(origrow, origcol);
    else
	movecursor(screenrow, screencol);

    if (updated)
	term.cursorvis(TRUE);
#if OPT_ICURSOR
    if (curwp != 0) {
	static int old_insertmode = -1;

	if (insertmode != old_insertmode) {
	    old_insertmode = insertmode;
	    term.icursor(insertmode);
	}
    }
#endif
    term.flush();
    endofDisplay();
    i_displayed = TRUE;

    while (allow_working_msg() && (chg_width && chg_height))
	newscreensize(chg_height, chg_width);
    returnCode(TRUE);
}

/*
 * Highlight the requested portion of the screen.  We're mucking with the video
 * attributes on the line here, so this is NOT good code - it would be better
 * if there was an individual colour attribute per character, rather than per
 * row, but I didn't write the original code.  Anyway, hilite is called only
 * once so far, so it's not that big a deal.
 *
 * row       - row to start highlighting
 * colfrom   - column to start highlighting
 * colto     - column to end highlighting
 * on        - start highlighting
 */
void
hilite(int row, int colfrom, int colto, int on)
{
#if !OPT_VIDEO_ATTRS
    VIDEO *vp1 = vscreen[row];
#endif
#ifdef WMDLINEWRAP
    WINDOW *wp = row2window(row);
    if (wp == NULL)
	return;
    if (w_val(wp, WMDLINEWRAP)) {
	if (colfrom < 0)
	    colfrom = 0;
	if (colfrom > term.cols) {
	    do {
		row++;
		colfrom -= term.cols;
		colto -= term.cols;
		hilite(row, colfrom, colto, on);
	    } while (colto > term.cols);
	    return;
	}
    }
#endif
    if (row < term.rows - 1 && (colfrom >= 0 || colto <= term.cols)) {
	if (colfrom < 0)
	    colfrom = 0;
	if (colto > term.cols)
	    colto = term.cols;
#if OPT_VIDEO_ATTRS
	if (on) {
	    int col;
	    for (col = colfrom; col < colto; col++)
		vscreen[row]->v_attrs[col] |= VAREV;
	} else {
	    int col;
	    for (col = colfrom; col < colto; col++) {
		clr_typed_flags(vscreen[row]->v_attrs[col], VIDEO_ATTR, VAREV);
	    }
	}
	vscreen[row]->v_flag |= VFCHG;
	update_line(row, 0, term.cols);
#else /* OPT_VIDEO_ATTRS */
	if (on) {
	    vp1->v_flag |= VFREQ;
	} else {
	    vp1->v_flag &= ~VFREQ;
	}
	update_line(row, colfrom, colto);
#endif /* OPT_VIDEO_ATTRS */
    }
}

/*
 * Send a command to the terminal to move the hardware cursor to row "row"
 * and column "col". The row and column arguments are origin 0. Optimize out
 * random calls. Update "ttrow" and "ttcol".
 */
void
movecursor(int row, int col)
{
    beginDisplay();
    if ((row != ttrow || col != ttcol)
	&& (row >= 0 && row < term.rows)
	&& (col >= 0 && col < term.cols)) {
	ttrow = row;
	ttcol = col;
	term.curmove(row, col);
    }
    endofDisplay();
}

void
bottomleft(void)
{
    movecursor(term.rows - 1, 0);
}

/*
 * Erase the message line. This is a special routine because the message line
 * is not considered to be part of the virtual screen. It always works
 * immediately; the terminal buffer is flushed via a call to the flusher.
 */
void
mlerase(void)
{
    if (!clhide) {
	beginDisplay();
	kbd_erase_to_end(0);
	kbd_flush();
	endofDisplay();
    }
}

int
mlsavec(int c)
{
    tb_append(&mlsave, c);
    return TRUE;
}

/*
 * Do the real message-line work.  Keep track of the physical cursor position.
 * A small class of printf like format items is handled.  Set the "message
 * line" flag TRUE.
 */
static void
mlmsg(const char *fmt, va_list app2)
{
    static int recur;

    va_list app;
    int end_at;
#if DISP_NTWIN
    int cursor_state = 0;
#endif
    int do_crlf = (strchr(fmt, '\n') != NULL
		   || strchr(fmt, '\r') != NULL);

    if (no_minimsgs) {
	kbd_alarm();
	return;
    }
    if (quiet)
	return;

#if DISP_NTWIN
    if (recur == 0) {
	/*
	 * Winvile internally manages its own cursor and this usually works
	 * fine.  However, if the user activates functionality that triggers
	 * windows message(s) (e.g., invoke the Win32 common open dialog) and
	 * if this functionality writes message line status text as a side
	 * effect, then winvile may inadvertently enable its cursor during the
	 * display update.  If this happens, the GDI drops a cursor glyph in
	 * the message line.  Fix this problem by forcing the winvile cursor
	 * off now.
	 */
	cursor_state = winvile_cursor_state(FALSE, FALSE);
    }
#endif
    if (recur++) {
	/*EMPTY */ ;
    } else if (sgarbf) {
	/* then we'll lose the message on the next update(),
	 * so save it now */
	tb_init(&mlsave, EOS);
#if OPT_POPUP_MSGS
	if (global_g_val(GMDPOPUP_MSGS) || !valid_window(curwp)) {
	    TRACE(("mlmsg popup_msgs #1 for '%s'\n", fmt));
	    popup_msgs();
	    msg_putc('\n');
	    dfoutfn = msg_putc;
	} else
#endif
	    dfoutfn = mlsavec;

	begin_va_copy(app, app2);
	dofmt(fmt, app);
	end_va_copy(app);
    } else {
	beginDisplay();

	kbd_expand = -1;
#if OPT_POPUP_MSGS
	if (global_g_val(GMDPOPUP_MSGS)) {
	    TRACE(("mlmsg popup_msgs #2 for '%s'\n", fmt));
	    popup_msgs();
	    if (tb_length(mlsave) == 0) {
		msg_putc('\n');
		dfoutfn = msg_putc;
	    } else {
		dfoutfn = kbd_putc;
	    }
	} else
#endif
	    dfoutfn = kbd_putc;

	if (*fmt != '\n') {

	    kbd_erase_to_end(0);

	    begin_va_copy(app, app2);
	    dofmt(fmt, app);
	    end_va_copy(app);

	    kbd_expand = 0;

	    /* if we can, erase to the end of screen */
	    end_at = wminip->w_dot.o;
	    kbd_erase_to_end(end_at);
	    tb_init(&mlsave, EOS);
	    kbd_flush();
	}
	if (do_crlf) {
	    term.openup();
	}
	endofDisplay();
    }
    recur--;
#if DISP_NTWIN
    if (recur == 0) {

	/* restore previous cursor state if it was ON. */

	if (cursor_state)
	    winvile_cursor_state(TRUE, TRUE);
    }
#endif
}

/*
 * Format a string onto the message line, but only if it's appropriate.
 * Keyboard macro replay, "dot" command replay, command buffer execution,
 * "terse" mode, and the user-accessible "discmd" state variable can all
 * suppress this.
 */
/* VARARGS1 */
void
mlwrite(const char *fmt, ...)
{
    if (fmt != NULL) {
	va_list ap;
	if (global_b_val(MDTERSE) || kbd_replaying(FALSE) || !vl_msgs) {
	    if (!clhide)
		bottomleft();
	    return;
	}
	va_start(ap, fmt);
	mlmsg(fmt, ap);
	va_end(ap);
    }
}

/*
 * Unconditionally format a string out onto the message line.
 */
/* VARARGS1 */
void
mlforce(const char *fmt, ...)
{
    if (fmt != NULL) {
	va_list ap;
	va_start(ap, fmt);
	mlmsg(fmt, ap);
	va_end(ap);
    }
}

/* VARARGS1 */
void
mlprompt(const char *fmt, ...)
{
    if (fmt != NULL) {
	va_list ap;
	int osgarbf = sgarbf;
	if (!vl_msgs) {
	    bottomleft();
	    return;
	}
	sgarbf = FALSE;
	va_start(ap, fmt);
	mlmsg(fmt, ap);
	va_end(ap);
	sgarbf = osgarbf;
    }
}

/* VARARGS */
void
dbgwrite(const char *fmt, ...)
{
    if (fmt != NULL) {
	char temp[80];

	va_list ap;		/* ptr to current data field */
	va_start(ap, fmt);
	lsprintf(temp, "[press ^G to continue] %s", fmt);
	mlmsg(temp, ap);
	va_end(ap);
	beginDisplay();
	while (term.getch() != '\007') {
	    ;
	}
	endofDisplay();
    }
}

/*
 * Do the equivalent of 'perror()' on the message line
 */
void
mlerror(const char *str)
{
    int save_err = errno;
    const char *t = NULL;
#ifdef HAVE_STRERROR
#if SYS_VMS
    if (save_err == EVMSERR)
	t = strerror(save_err, vaxc$errno);
    else
#endif /* SYS_VMS */
    if (save_err > 0)
	t = strerror(save_err);
#else
#ifdef HAVE_SYS_ERRLIST
    if (save_err > 0 && save_err < sys_nerr)
	t = sys_errlist[save_err];
#endif /* HAVE_SYS_ERRLIST */
#endif /* HAVE_STRERROR */
    if (t != NULL) {
	/* Borland's strerror() returns newlines on the end of the strings */
	static TBUFF *tt;
	if (tb_scopy(&tt, t) != NULL) {
	    t = mktrimmed(tb_values(tt));
	}
	mlwarn("[Error %s: %s]", str, t);
    } else {
	mlwarn("[Error %s: unknown system error %d]", str, save_err);
    }
}

/*
 * Emit a warning message (with alarm)
 */
/* VARARGS1 */
void
mlwarn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    mlmsg(fmt, ap);
    va_end(ap);
    kbd_alarm();
}

/*
 * Local sprintf -- similar to standard libc, but
 *  returns pointer to null character at end of buffer, so it can
 *  be called repeatedly, as in:
 *	cp = lsprintf(cp, fmt, args...);
 *
 */

static char *lsp;

static int
lspputc(int c)
{
    *lsp++ = (char) c;
    return TRUE;
}

/* VARARGS1 */
char *
lsprintf(char *buf, const char *fmt, ...)
{
    if ((lsp = buf) != NULL) {
	va_list ap;
	va_start(ap, fmt);

	dfoutfn = lspputc;

	dofmt(fmt, ap);
	va_end(ap);

	*lsp = EOS;
    }
    return lsp;
}

int
format_int(char *buf, UINT number, UINT radix)
{
    if ((lsp = buf) != NULL) {
	dfputi(lspputc, number, radix);
	*lsp = EOS;
    }
    return (int) (lsp - buf);
}

/*
 * Buffer printf -- like regular printf, but puts characters
 *	into the BUFFER.
 */
int
bputc(int c)
{
    int status;

    if (is_record_sep(curbp, c)) {
	status = lnewline();
    } else {
	status = lins_bytes(1, c);
    }

    return status;
}

int
bputsn(const char *src, int len)
{
    int rc = TRUE;

    if (len < 0) {
	rc = bputsn(src, (int) strlen(src));
    } else {
	while ((len > 0) && (rc == TRUE)) {
	    int n;
	    int chunk;
	    int rs = use_record_sep(curbp);

	    if (*src == rs) {
		rc = lnewline();
		chunk = 1;
	    } else {
		for (chunk = 0; chunk < len; ++chunk) {
		    if (src[chunk] == rs)
			break;
		}
		if ((rc = lins_bytes(chunk, ' ')) == TRUE) {
		    LINE *lp = DOT.l;
		    int offs = DOT.o - chunk;
		    for (n = 0; n < chunk; ++n) {
			lvalue(lp)[n + offs] = src[n];
		    }
		}
	    }
	    len -= chunk;
	    src += chunk;
	}
    }
    return rc;
}

#if OPT_EXTRA_COLOR
int
bputsn_attr(const char *src, int len, int attr)
{
    int rc;

    if (attr) {
	REGIONSHAPE save_regn = regionshape;
	VIDEO_ATTR save_attr = videoattribute;

	regionshape = rgn_EXACT;
	videoattribute = (VIDEO_ATTR) attr;

	if (len < 0)
	    len = (int) strlen(src);
	rc = bputsn(src, len);

	/*
	 * We cannot simply save MK before writing, since writing to an empty
	 * buffer for instance may split the original line, leaving MK pointing
	 * to the fragment _after_ the newline.  But limiting this call to
	 * strings without embedded newlines, we can reliably compute MK based
	 * on the string length.
	 */
	MK = DOT;
	MK.o -= len;
	(void) attributeregion();

	regionshape = save_regn;
	videoattribute = save_attr;
	MK = DOT;
    } else {
	rc = bputsn(src, len);
    }
    return rc;
}

int
bputsn_xcolor(const char *src, int len, XCOLOR_CODES code)
{
    int rc;
    int *valuep = lookup_extra_color(code);

    if (isEmpty(valuep)) {
	rc = bputsn(src, len);
    } else {
	rc = bputsn_attr(src, len, *valuep);
    }
    return rc;
}
#endif /* OPT_EXTRA_COLOR */

void
bpadc(int c, int count)
{
    while (count-- > 0)
	bputc(c);
}

/* printf into curbp, at DOT */
/* VARARGS */
void
bprintf(const char *fmt, ...)
{
    va_list ap;

    dfoutfn = bputc;

    va_start(ap, fmt);
    dofmt(fmt, ap);
    va_end(ap);
}

/*
 * Print into the given internal buffer, returning a pointer to the line
 * written.
 */
LINE *
b2vprintf(BUFFER *bp, const char *fmt, va_list app2)
{
    va_list app;
    LINE *result = NULL;
    WINDOW *save_wp;
    BUFFER *save_bp = curbp;
    W_TRAITS save_w_traits;
    OutFunc save_outfn;

    if ((save_wp = push_fake_win(bp)) != NULL) {
	int save_curgoal = curgoal;

	save_outfn = dfoutfn;
	dfoutfn = bputc;

	save_w_traits = curwp->w_traits;

	(void) gotoeob(FALSE, 1);
	(void) gotoeol(FALSE, 1);

	begin_va_copy(app, app2);
	dofmt(fmt, app);
	end_va_copy(app);

	result = lback(DOT.l);

	b_clr_changed(bp);
	curwp->w_traits = save_w_traits;

	pop_fake_win(save_wp, save_bp);
	dfoutfn = save_outfn;
	curgoal = save_curgoal;
    }
    return result;
}

/*
 * Write text (not necessarily a whole line) to the given buffer.
 */
LINE *
b2printf(BUFFER *bp, const char *fmt, ...)
{
    LINE *result;
    va_list ap;

    va_start(ap, fmt);
    result = b2vprintf(bp, fmt, ap);
    va_end(ap);
    return result;
}

#if OPT_EVAL || OPT_DEBUGMACROS
/* printf into [Trace] */
void
tprintf(const char *fmt, ...)
{
    static int nested;

    BUFFER *bp;
    va_list ap;
#if OPT_TRACE
    LINE *line;
#endif

    if (!nested
	&& (bp = make_ro_bp(TRACE_BufName, BFINVS)) != NULL) {
	nested = TRUE;

	va_start(ap, fmt);
#if OPT_TRACE
	line =
#endif
	    b2vprintf(bp, fmt, ap);
	va_end(ap);

#if OPT_TRACE
	if (line)
	    TRACE(("tprintf {%.*s}\n", llength(line), lvalue(line)));
#endif

	nested = FALSE;
    }
}
#endif

#if defined(SIGWINCH) && ! DISP_X11
/* ARGSUSED */
SIGT
sizesignal(int ACTUAL_SIG_ARGS GCC_UNUSED)
{
    int w, h;
    int old_errno = errno;

    getscreensize(&w, &h);

    if ((h > 1 && h != term.rows) || (w > 1 && w != term.cols))
	newscreensize(h, w);

    setup_handler(SIGWINCH, sizesignal);
    errno = old_errno;
    SIGRET;
}
#endif

void
newscreensize(int h, int w)
{
    TRACE(("newscreensize(h=%d, w=%d)\n", h, w));
    /* do the change later */
    if (im_displaying
#if OPT_WORKING
	|| !i_displayed
#endif
	) {
	chg_width = w;
	chg_height = h;
    } else {
	beginDisplay();
	chg_width = chg_height = 0;
	if ((h > term.maxrows) || (w > term.maxcols)) {
	    int old_r, old_c;
	    old_r = term.maxrows;
	    old_c = term.maxcols;
	    term.maxrows = h;
	    term.maxcols = w;
	    if (!vtinit()) {	/* allocation failure */
		term.maxrows = old_r;
		term.maxcols = old_c;
		endofDisplay();
		return;
	    }
	}
	endofDisplay();

	if (newlength(TRUE, h) && newwidth(TRUE, w))
	    (void) update(TRUE);
    }
}

#if OPT_WORKING
/*
 * Start the timer that controls the "working..." message.
 */
static void
start_working(void)
{
    TRACE2(("start_working\n"));
    setup_handler(SIGALRM, imworking);
    (void) alarm(1);
    im_timing = TRUE;
}

/*
 * When we stop the timer, we should cleanup the "working..." message.
 */
static void
stop_working(void)
{
    TRACE2(("stop_working\n"));
    if (mpresf) {		/* erase leftover working-message */
	int save_row = ttrow;
	int save_col = ttcol;
	kbd_overlay(NULL);
	kbd_flush();
	movecursor(save_row, save_col);
	term.flush();
    }
}

/*
 * Displays alternate
 *	"working..." and
 *	"...working"
 * at the end of the message line if it has been at least a second since
 * displaying anything or waiting for keyboard input.  The cur_working and
 * max_working values are used in 'slowreadf()' to show the progress of reading
 * large files.
 */

/*ARGSUSED*/
SIGT
imworking(int ACTUAL_SIG_ARGS GCC_UNUSED)
{
    static int flip;
    static int skip;

    TRACE2(("imworking(%d)\n", signo));
    /* (if GMDWORKING is _not_ set, or MDTERSE is set, we're allowed to erase,
     * but not to write.  If we do erase, we don't reschedule the alarm, since
     * setting the mode will call us again to start things up)
     */
    if (allow_working_msg()) {
	TRACE2(("...allow_working_msg\n"));
	if (im_waiting(-1)) {
	    TRACE2(("...im_waiting(-1)\n"));
	    im_timing = FALSE;
	    stop_working();
	} else if (ShowWorking()) {
	    TRACE2(("...ShowWorking()\n"));
	    if (skip) {
		TRACE2(("...skipped()\n"));
		skip = FALSE;
	    } else {
#if DISP_X11
		x_working();
#else
		static const char *const msg[] =
		{"working", "..."};
		char result[20];

		TRACE2(("...FINALLY!()\n"));
		result[0] = EOS;
		if (cur_working != 0
		    && cur_working != old_working) {
		    char temp[20];
		    int len = cur_working > 999999L ? 10 : 6;

		    old_working = cur_working;
		    vl_strncat(result,
			       right_num(temp, len, (long) cur_working),
			       sizeof(result));
		    if (len == 10) {
			/*EMPTY */ ;
		    } else if (max_working != 0) {
			vl_strncat(result, " ", sizeof(result));
			vl_strncat(result,
				   right_num(temp, 2,
					     (long) ((100 * cur_working)
						     / max_working)),
				   sizeof(result));
			vl_strncat(result, "%", sizeof(result));
		    } else {
			vl_strncat(result, " ...", sizeof(result));
		    }
		} else {
		    vl_strncat(result, msg[flip], sizeof(result));
		    vl_strncat(result, msg[!flip], sizeof(result));
		}
		kbd_overlay(result);
		kbd_flush();
#endif
	    }
	    start_working();
	    flip = !flip;
	} else {
	    TRACE2(("...NOT ShowWorking()\n"));
	    stop_working();
	    skip = TRUE;
	}
    } else {
	TRACE2(("... NOT allow_working_msg(%d/%d/%d)%s ShowWorking\n",
		vile_is_busy, im_displaying, !i_displayed,
		ShowWorking()? "" : " NOT"));
	if (ShowWorking()) {	/* keep the timer running */
	    start_working();
	    flip = !flip;
	}
    }
}
#endif /* OPT_WORKING */

/*
 * Returns true if we should/could show a "working..." message or other busy
 * indicator, assuming that 'working' mode is set.
 */
int
allow_working_msg(void)
{
    return !(vile_is_busy || im_displaying || !i_displayed);
}

/*
 * Maintain a flag that records whether we're waiting for keyboard input.  As a
 * side-effect, restart the 'working' timer if we see that it's been made
 * inactive while we were waiting.
 */
int
im_waiting(int flag)
{
    static int waiting;
    if (flag >= 0) {		/* TRUE or FALSE set, negative used to query */
	waiting = flag;
#if OPT_WORKING
	if (!waiting && !im_timing && ShowWorking())
	    start_working();
#endif
    }
    return waiting;
}

#if OPT_PSCREEN
/* Most of the code in this section is for making the message line work
 * right...it shouldn't be called to display the rest of the screen.
 */
static int psc_row;
static int psc_col;

#define SWAP_INT(x,y) \
	do { (x) = (x)+(y); (y) = (x)-(y); (x) = (x)-(y); } one_time
#define SWAP_VT_PSC \
	do { SWAP_INT(vtcol, psc_col); SWAP_INT(vtrow, psc_row); } one_time

OUTC_DCL
psc_putchar(int c)
{
    if (c == '\b') {
	if (psc_col > 0)
	    psc_col--;
    } else {
	char temp[2];

	SWAP_VT_PSC;

	temp[0] = (char) c;
	vtputc(wminip, temp, 1);
	vscreen[vtrow]->v_flag |= VFCHG;

	SWAP_VT_PSC;
    }
    OUTC_RET c;
}

void
psc_flush(void)
{
    update_line(term.rows - 1, 0, term.cols);
    term.pflush();
}

void
psc_move(int row, int col)
{
    psc_row = row;
    psc_col = col;
}

void
psc_eeol(void)
{
    if (ttrow >= 0 && ttrow < term.rows && ttcol >= 0) {
	VIDEO_ATTR *vp = &vscreen[ttrow]->v_attrs[ttcol];
	VIDEO_TEXT *cp = &vscreen[ttrow]->v_text[ttcol];
	VIDEO_TEXT *cplim = &vscreen[ttrow]->v_text[term.cols];
	vscreen[ttrow]->v_flag |= VFCHG;
	while (cp < cplim) {
	    *vp++ = 0;
	    *cp++ = ' ';
	}
    }
}

void
psc_eeop(void)
{
    int saverow = ttrow;
    int savecol = ttcol;
    while (ttrow < term.rows) {
	psc_eeol();
	ttrow++;
	ttcol = 0;
    }
    ttrow = saverow;
    ttcol = savecol;
}

/* ARGSUSED */
void
psc_rev(UINT huh GCC_UNUSED)
{
    /* do nothing */
}

#endif /* OPT_PSCREEN */

/* For memory-leak testing (only!), releases all display storage. */
#if NO_LEAKS
void
vt_leaks(void)
{
    vtfree();
#if OPT_UPBUFF
    FreeIfNeeded(recomp_tbl);
#endif
}
#endif
