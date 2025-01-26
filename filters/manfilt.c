/*
 * manfilt.c		-- replace backspace sequences with attribute
 *			   information for vile
 *
 * Author: Kevin A. Buettner
 * Creation: 4/17/94
 *
 * This program filters backspace sequences often found in manual pages
 * for vile/xvile.  Backspace sequences representing italicized or bold
 * text are fixed up by removing the backspaces, underlines, and duplicate
 * characters (leaving just the text as it should appear on the screen).
 * Attributed text is so indicated by inserting a Cntrl-A sequence in front
 * of the text to be attributed.  These Cntrl-A sequences take the following
 * form:
 *   	^A<Count><Attr>:
 *
 * <Count> is a sequence of digits representing the number of characters
 * following the ':' to be attributed.
 *
 * <Attr> is a sequence of characters which indicates how to attribute the
 * characters following the ':'.  The following characters are presently
 * recognized by vile:
 *
 *	'I'	-- italic
 *	'B'	-- bold
 *	'U'	-- underline
 *	'R'	-- reverse video
 *
 * Examples:
 *	Before					After
 *	------					-----
 *	_^Hi_^Ht_^Ha_^Hl_^Hi_^Hc		^A6I:italic
 *
 *	b^Hbo^Hol^Hld^Hd			^A4B:bold
 *
 *	_^HB^HB_^Ho^Ho_^Ht^Ht_^Hh^Hh		^A4IB:Both
 *
 * On many systems, bold sequences are actually quite a bit longer.  On
 * some systems, the repeated character is repeated as many as four times.
 * Thus the letter "B" would be represented as B^HB^HB^HB.
 *
 * For comparison, see the description of the BSD 'col' program (for
 * information about the types of escape sequences that might be emitted by
 * nroff).
 *
 * vile will choose some appropriate fallback (such as underlining) if
 * italics are not available.
 *
 * $Id: manfilt.c,v 1.79 2025/01/26 17:00:27 tom Exp $
 *
 */

#define CAN_TRACE 0
#define CAN_VMS_PATH 0
#include <filters.h>

#if OPT_LOCALE
#include	<locale.h>
#include	<ctype.h>

#undef sys_isdigit
#undef sys_isprint

#ifdef HAVE_WCTYPE
#include	<wctype.h>
#define sys_isdigit(n)  (iswdigit((wint_t)(n)) || (((wint_t)(n) < 256 && isdigit((wint_t)(n)))))
#define sys_isprint(n)  (iswprint((wint_t)(n)) || (((wint_t)(n) < 256 && isprint((wint_t)(n)))))
#else
#define sys_isdigit(n)  isdigit(n)
#define sys_isprint(n)  isprint(n)
#endif

#else
#define sys_isdigit(n)  isdigit(n)
#define sys_isprint(n)  isprint(n)

#endif

#if !defined(HAVE_STDLIB_H)
# if !defined(HAVE_CONFIG_H) || defined(MISSING_EXTERN_CALLOC)
extern char *calloc(size_t nmemb, size_t size);
# endif
# if !defined(HAVE_CONFIG_H) || defined(MISSING_EXTERN_REALLOC)
extern char *realloc(char *ptr, size_t size);
# endif
#endif

#define backspace() \
		if (cur_line != NULL \
		 && cur_line->l_this > 0) \
		    cur_line->l_this -= 1;

#define MAX_LINES 200

	/* SGR codes that we use as mask values */
#define ATR_NORMAL 0
#define ATR_BOLD   1
#define ATR_ITAL   2
#define ATR_UNDER  4
#define ATR_REVERS 8

#define SHL_COLOR  4
#define ATR_COLOR  (0xf << SHL_COLOR)

	/* character names */
#define ESCAPE    '\033'
#define CNTL_A    '\001'
#define SHIFT_OUT '\016'
#define SHIFT_IN  '\017'

#define SPACE     ' '
#define UNDERLINE '_'

#define CS_NORMAL    0
#define CS_ALTERNATE 1
/*
 * Each column of a line is represented by a linked list of the characters that
 * are printed to that position.  When storing items in this list, we keep a
 * canonical order to simplify analysis when dumping the line.
 */
typedef struct CharCell {
    struct CharCell *link;
    int c_attrs;		/* base cell only: ATR_NORMAL, etc. */
    char c_ident;		/* CS_NORMAL/CS_ALTERNATE */
    char c_level;		/* 0=base, 1=up halfline, -1=down halfline */
    unsigned c_value;		/* the actual value */
} CHARCELL;

typedef struct LineData {
    struct LineData *l_next;
    struct LineData *l_prev;
    size_t l_last;		/* the number of cells allocated in l_cells[] */
    size_t l_used;		/* the number of cells used in l_cells[] */
    size_t l_this;		/* the current cell within the line */
    CHARCELL *l_cell;
} LINEDATA;

static void flush_line(void);

static const char *program = "vile-manfilt";
static LINEDATA *all_lines;
static LINEDATA *cur_line;
static long total_lines;
static int opt_8bit = 0;
static int (*my_getc) (FILE *);
static int (*my_putc) (int);

static void
failed(const char *s)
{
    int save_err = errno;
    fflush(stdout);
    fclose(stdout);
    fprintf(stderr, "%s: ", program);
    errno = save_err;
    perror(s);
    exit(BADEXIT);
}

static int
ansi_getc(FILE *fp)
{
    return vl_getc(fp);
}

static int
ansi_putc(int ch)
{
    return vl_putc(ch, stdout);
}

#if OPT_LOCALE
/*
 * groff 1.18 versions generates UTF-8 - we want Latin1.  Decode it.
 */
static int
utf8_getc(FILE *fp)
{
#define UCS_REPL '?'
    int utf_count = 0;
    int utf_char = 0;

    do {
	int c = vl_getc(fp);
	/* Combine UTF-8 into Unicode */
	if (c < 0x80) {
	    /* We received an ASCII character */
	    /* if (utf_count > 0), prev. sequence incomplete */
	    utf_char = c;
	    utf_count = 0;
	} else if (c < 0xc0) {
	    /* We received a continuation byte */
	    if (utf_count < 1) {
		utf_char = UCS_REPL;	/* ... unexpectedly */
	    } else {
		if (!utf_char && !((c & 0x7f) >> (7 - utf_count))) {
		    utf_char = UCS_REPL;
		}
		/* characters outside UCS-2 become UCS_REPL */
		if (utf_char > 0x03ff) {
		    /* value would be >0xffff */
		    utf_char = UCS_REPL;
		} else {
		    utf_char <<= 6;
		    utf_char |= (c & 0x3f);
		}
		if ((utf_char >= 0xd800 &&
		     utf_char <= 0xdfff) ||
		    (utf_char == 0xfffe) ||
		    (utf_char == 0xffff)) {
		    utf_char = UCS_REPL;
		}
		utf_count--;
	    }
	} else {
	    /* We received a sequence start byte */
	    /* if (utf_count > 0), prev. sequence incomplete */
	    if (c < 0xe0) {
		utf_count = 1;
		utf_char = (c & 0x1f);
		if (!(c & 0x1e))
		    utf_char = UCS_REPL;	/* overlong sequence */
	    } else if (c < 0xf0) {
		utf_count = 2;
		utf_char = (c & 0x0f);
	    } else if (c < 0xf8) {
		utf_count = 3;
		utf_char = (c & 0x07);
	    } else if (c < 0xfc) {
		utf_count = 4;
		utf_char = (c & 0x03);
	    } else if (c < 0xfe) {
		utf_count = 5;
		utf_char = (c & 0x01);
	    } else {
		utf_char = UCS_REPL;
		utf_count = 0;
	    }
	}
    } while (utf_count > 0);

    if (opt_8bit) {
	if (utf_char > 255) {
	    switch (utf_char) {
	    case 0x2018:
		utf_char = '`';
		break;
	    case 0x2019:
		utf_char = '\'';
		break;
	    case 0x2010:
	    case 0x2212:
		utf_char = '-';
		break;
	    default:
		utf_char = '?';
		break;
	    }
	}
    }
    return utf_char;
}

static int
utf8_putc(int source)
{
    /* FIXME - this is cut/paste from vl_conv_to_utf8() */
#define CH(n) CharOf((source) >> ((n) * 8))
    int rc = 0;
    unsigned char target[10];

    if (source <= 0x0000007f)
	rc = 1;
    else if (source <= 0x000007ff)
	rc = 2;
    else if (source <= 0x0000ffff)
	rc = 3;
    else if (source <= 0x001fffff)
	rc = 4;
    else if (source <= 0x03ffffff)
	rc = 5;
    else			/* (source <= 0x7fffffff) */
	rc = 6;

    switch (rc) {
    case 1:
	target[0] = CH(0);
	break;

    case 2:
	target[1] = CharOf(0x80 | (CH(0) & 0x3f));
	target[0] = CharOf(0xc0 | (CH(0) >> 6) | ((CH(1) & 0x07) << 2));
	break;

    case 3:
	target[2] = CharOf(0x80 | (CH(0) & 0x3f));
	target[1] = CharOf(0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	target[0] = CharOf(0xe0 | ((int) (CH(1) & 0xf0) >> 4));
	break;

    case 4:
	target[3] = CharOf(0x80 | (CH(0) & 0x3f));
	target[2] = CharOf(0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	target[1] = CharOf(0x80 |
			   ((int) (CH(1) & 0xf0) >> 4) |
			   ((int) (CH(2) & 0x03) << 4));
	target[0] = CharOf(0xf0 | ((int) (CH(2) & 0x1f) >> 2));
	break;

    case 5:
	target[4] = CharOf(0x80 | (CH(0) & 0x3f));
	target[3] = CharOf(0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	target[2] = CharOf(0x80 |
			   ((int) (CH(1) & 0xf0) >> 4) |
			   ((int) (CH(2) & 0x03) << 4));
	target[1] = CharOf(0x80 | (CH(2) >> 2));
	target[0] = CharOf(0xf8 | (CH(3) & 0x03));
	break;

    case 6:
	target[5] = CharOf(0x80 | (CH(0) & 0x3f));
	target[4] = CharOf(0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	target[3] = CharOf(0x80 | (CH(1) >> 4) | ((CH(2) & 0x03) << 4));
	target[2] = CharOf(0x80 | (CH(2) >> 2));
	target[1] = CharOf(0x80 | (CH(3) & 0x3f));
	target[0] = CharOf(0xfc | ((int) (CH(3) & 0x40) >> 6));
	break;
    }
    target[rc] = 0;
    return vl_fputs((char *) target, stdout);
#undef CH
}
#endif /* OPT_LOCALE */

/*
 * Allocate a CHARCELL struct
 */
static CHARCELL *
allocate_cell(void)
{
    CHARCELL *p = typecallocn(CHARCELL, 1);
    if (p == NULL)
	failed("allocate_cell");
    return p;
}

/*
 * Allocate a LINEDATA struct
 */
static LINEDATA *
allocate_line(void)
{
    LINEDATA *p = typecallocn(LINEDATA, 1);

    if (p == NULL)
	failed("allocate_line");

    if (all_lines == NULL)
	all_lines = p;

    if (total_lines++ > MAX_LINES)
	flush_line();

    return p;
}

/*
 * (Re)allocate the l_cell[] array for the current line
 */
static void
extend_line(size_t want)
{
    size_t have = cur_line->l_last;
    if (want < cur_line->l_this)
	want = cur_line->l_this;
    if (want >= have) {
	CHARCELL *c = cur_line->l_cell;
	want += 80;
	if (c == NULL) {
	    c = typecallocn(CHARCELL, want);
	} else {
	    c = typereallocn(CHARCELL, c, want);
	}
	if (c == NULL) {
	    failed("extend_line");
	} else {
	    while (have < want) {
		c[have].link = NULL;
		c[have].c_attrs = ATR_NORMAL;
		c[have].c_value = SPACE;
		c[have].c_level = 0;
		c[have].c_ident = CS_NORMAL;
		have++;
	    }
	    cur_line->l_last = want;
	    cur_line->l_cell = c;
	}
    }
}

/*
 * Store a character at the current position, updating the current position.
 * We expect (but do not require) that an underscore precedes a nonblank
 * character that will overstrike it.  (Some programs produce the underscore
 * second, rather than first).
 */
static void
put_cell(int c, int level, int ident, int attrs)
{
    size_t col;
    size_t len;
    CHARCELL *p, *q;

    if (cur_line == NULL)
	cur_line = allocate_line();

    len = cur_line->l_used;
    col = cur_line->l_this++;
    extend_line(col);

    p = &(cur_line->l_cell[col]);
    p->c_attrs = attrs;

    if (len > col) {		/* some type of overstrike */
	if (c == UNDERLINE) {
	    while ((q = p->link) != NULL)
		p = q;
	    q = allocate_cell();
	    p->link = q;
	    p = q;
	} else {
	    if ((c != SPACE)
		|| (p->c_value == UNDERLINE)) {
		q = allocate_cell();
		*q = *p;
		p->link = q;
	    }
	}
    }

    p->c_value = (unsigned) c;
    p->c_level = (char) level;
    p->c_ident = (char) ident;

    if (cur_line->l_used < cur_line->l_this)
	cur_line->l_used = cur_line->l_this;
}

#define MAXPARAMS 10

static void
erase_cell(int col)
{
    CHARCELL *p = &(cur_line->l_cell[col]);
    if (p != NULL) {
	/* *INDENT-EQLS* */
	p->link     = NULL;
	p->c_value  = SPACE;
	p->c_level  = 0;
	p->c_ident  = CS_NORMAL;
    }
}

#define DefaultOne(nparams, params) \
	((nparams == 0) ? 1 : (nparams == 1) ? params[0] : -1)

#define DefaultZero(nparams, params) \
	((nparams == 0) ? 0 : (nparams == 1) ? params[0] : -1)

static void
ansi_CUB(int code)
{
    if (code > 0 && ((int) cur_line->l_this - code) >= 0) {
	cur_line->l_this = cur_line->l_this - (size_t) code;
    }
}

static void
ansi_CUF(int code)
{
    if (code > 0) {
	size_t col;

	if (cur_line == NULL)
	    cur_line = allocate_line();

	col = cur_line->l_this + (size_t) code;
	while (col > cur_line->l_last) {
	    extend_line(col);
	}
	if (cur_line->l_used < col)
	    cur_line->l_used = col;
	cur_line->l_this = col;
    }
}

static void
ansi_DCH(int code)
{
    size_t dst;
    size_t src;

    if (code > 0) {
	if (cur_line == NULL)
	    cur_line = allocate_line();
	for (dst = cur_line->l_this; dst < cur_line->l_used; ++dst) {
	    src = dst + (size_t) code;
	    if (src < cur_line->l_used) {
		cur_line->l_cell[dst] = cur_line->l_cell[src];
	    } else {
		erase_cell((int) dst);
	    }
	}
	cur_line->l_used -= (size_t) code;
    }
}

static void
ansi_EL(int code)
{
    size_t col;

    if (cur_line == NULL)
	cur_line = allocate_line();

    switch (code) {
    case 0:			/* Erase to Right (default) */
	for (col = cur_line->l_this; col <= cur_line->l_used; ++col)
	    erase_cell((int) col);
	break;
    case 1:			/* Erase to Left */
    case 2:			/* Erase All */
	for (col = 0; col <= cur_line->l_this; ++col)
	    erase_cell((int) col);
	break;
    }
}

static void
ansi_ICH(int code)
{
    size_t dst;
    size_t src;

    if (code > 0) {
	size_t last;

	if (cur_line == NULL)
	    cur_line = allocate_line();

	last = cur_line->l_last - 1;
	for (dst = last; dst >= cur_line->l_this; --dst) {
	    src = dst - (size_t) code;
	    if (src >= cur_line->l_this) {
		cur_line->l_cell[dst] = cur_line->l_cell[src];
	    } else {
		erase_cell((int) dst);
	    }
	}
	if (cur_line->l_used < last)
	    cur_line->l_used++;
    }
}

static void
ansi_HPA(int code)
{
    if (code > 0) {
	size_t col;

	if (cur_line == NULL)
	    cur_line = allocate_line();

	col = (size_t) code - 1;
	while (col > cur_line->l_last) {
	    extend_line(col);
	}
	if (cur_line->l_used < col)
	    cur_line->l_used = col;
	cur_line->l_this = col;
    }
}

/*
 * Interpret equivalent overstrike/underline for an ANSI escape sequence.
 */
static int
ansi_escape(FILE *ifp, int last_code)
{
    int code = last_code;
    int final = 0;
    int value = 0;
    int digits = 0;
    int private = 0;
    int params[MAXPARAMS + 1];
    int nparams = 0;
    int c;

    while ((c = my_getc(ifp)) != EOF) {
	if (c >= 0x30 && c <= 0x39) {
	    value = (value * 10) + (c - '0');
	    digits++;
	} else if (c == ';' || (c >= 0x40 && c <= 0x7e)) {
	    if (nparams < MAXPARAMS) {
		if (digits) {
		    params[nparams++] = value;
		}
	    }
	    if (c != ';') {
		final = c;
		break;
	    }
	    value = 0;
	    digits = 0;
	} else if (c >= 0x3a && c <= 0x3f) {
	    private = 1;
	} else if (c < 0x30 || c > 0x7e) {
	    break;		/* ...at least, nothing we can handle */
	}
    }

    if (!private) {
	/*
	 * Multiline controls are implemented (but are noted below) because
	 * doing this correctly depends on the filter knowing the width of
	 * the terminal when the log/typescript was produced.  That width was
	 * taken into account by the program which wrote the log, and is not
	 * necessarily the same as that used in the current session.
	 */
	switch (final) {
	case '@':
	    ansi_ICH(DefaultOne(nparams, params));
	    break;
	case 'A':
	    /* CUU - not implemented (multiline) */
	    break;
	case 'B':
	    /* CUD - not implemented (multiline) */
	    break;
	case 'C':
	    ansi_CUF(DefaultOne(nparams, params));
	    break;
	case 'D':
	    ansi_CUB(DefaultOne(nparams, params));
	    break;
	case '`':
	case 'G':
	    ansi_HPA(DefaultOne(nparams, params));
	    break;
	case 'H':
	    /* CUP - not implemented (multiline) */
	    break;
	case 'J':
	    /* ED - not implemented (multiline) */
	    break;
	case 'K':
	    ansi_EL(DefaultZero(nparams, params));
	    break;
	case 'L':
	    /* IL - not implemented (multiline) */
	    break;
	case 'M':
	    /* DL - not implemented (multiline) */
	    break;
	case 'P':
	    ansi_DCH(DefaultOne(nparams, params));
	    break;
	case 'd':
	    /* VPA - not implemented (multiline) */
	    break;
	case 'h':
	case 'l':
	    /* 4 is IRM - not implemented (ICH is preferred) */
	    break;
	case 'm':
	    /* SGR */
	    if (nparams != 0) {
		for (c = 0; c < nparams; ++c) {
		    value = params[c];
		    switch (value) {
		    case 0:
			code = ATR_NORMAL;
			break;
		    case 1:
			code |= ATR_BOLD;
			break;
		    case 3:
			code |= ATR_ITAL;
			break;
		    case 4:
			code |= ATR_UNDER;
			break;
		    case 7:
			code |= ATR_REVERS;
			break;
		    case 22:
			code &= ~ATR_BOLD;
			break;
		    case 23:
			code &= ~ATR_BOLD;
			break;
		    case 24:
			code &= ~ATR_UNDER;
			break;
		    case 27:
			code &= ~ATR_REVERS;
			break;
		    default:
			/* we handle only foreground (text) colors */
			if (value >= 30 && value <= 37) {
			    value -= 30;
			    value <<= SHL_COLOR;
			    code &= ~ATR_COLOR;
			    code |= value;
			} else if (value >= 90 && value <= 97) {
			    value -= 90;
			    value += 8;
			    value <<= SHL_COLOR;
			    code &= ~ATR_COLOR;
			    code |= value;
			}
			break;
		    }
		}
	    } else {
		code = ATR_NORMAL;
	    }
	    break;
	}
    }
    return code;
}

/*
 * Simply ignore OSC strings, e.g., as used to set window title.
 * This does not actually wait for a string terminator, since we may be stuck
 * with some Linux console junk.
 */
static void
osc_ignored(FILE *ifp)
{
    int c;

    while ((c = my_getc(ifp)) != EOF) {
	if (c == ESCAPE) {
	    c = my_getc(ifp);
	    break;
	} else if (c < 0x20 || ((c >= 0x80) && (c < 0xa0))) {
	    break;
	}
    }

}

/*
 * Set the current pointer to the previous line, if possible.
 */
static void
prev_line(void)
{
    LINEDATA *old_line;

    /*
     * We can't go back to lines that were already flushed and discarded.
     */
    assert(cur_line != NULL);
    if (cur_line->l_prev == NULL)
	return;

    old_line = cur_line;
    cur_line = cur_line->l_prev;
    cur_line->l_next = old_line;
    cur_line->l_this = old_line->l_this;
}

/*
 * Set the current pointer to the next line, allocating it if necessary
 */
static void
next_line(void)
{
    LINEDATA *old_line;

    if (cur_line == NULL)
	cur_line = allocate_line();

    if (cur_line->l_next == NULL)
	cur_line->l_next = allocate_line();

    old_line = cur_line;
    cur_line = cur_line->l_next;
    cur_line->l_prev = old_line;
    cur_line->l_this = old_line->l_this;
}

/*
 * If we've got a blank line to write onto, fake half-lines that way.
 * Otherwise, eat them.  We assume that half-line controls occur in pairs.
 */
static int
half_up(int level)
{
    prev_line();
    if (cur_line->l_this < cur_line->l_used) {
	next_line();
	return level + 1;
    }
    return 0;
}

static int
half_down(int level)
{
    if (level == 0) {
	next_line();
	return 0;
    }
    return level - 1;
}

static int
cell_code(LINEDATA * line, size_t col)
{
    int code = ATR_NORMAL;
    CHARCELL *p = &(line->l_cell[col]);

    if (p != NULL) {
	CHARCELL *q;
	code = p->c_attrs;

	while ((q = p->link) != NULL) {
	    if (q->c_value == UNDERLINE
		&& q->c_value != p->c_value) {
		code |= ATR_UNDER;
	    } else
		code |= ATR_BOLD;
	    p = q;
	}
    }
    return code;
}

static void
free_line(LINEDATA * l)
{
    if (l != NULL) {
	if (l->l_cell != NULL)
	    free(l->l_cell);
	if (l->l_prev != NULL) {
	    if (l->l_prev->l_next == l)
		l->l_prev->l_next = NULL;
	    l->l_prev = NULL;
	}
	if (l->l_next != NULL) {
	    if (l->l_next->l_prev == l)
		l->l_next->l_prev = NULL;
	    l->l_next = NULL;
	}
	free(l);
	total_lines--;
    }
}

/*
 * Write the oldest line from memory to standard output and deallocate its
 * storage.
 */
static void
flush_line(void)
{
    size_t col;
    size_t ccol;
    int ref_code;
    int tst_code;
    int counter;
    LINEDATA *l = all_lines;
    CHARCELL *p;

    if (l != NULL && l->l_next != NULL) {
	all_lines = l->l_next;
	if (cur_line == l)
	    cur_line = all_lines;

	ref_code = ATR_NORMAL;
	counter = 0;
	for (col = 0; col < l->l_used; col++) {
	    if (--counter <= 0) {
		tst_code = cell_code(l, col);
		if (tst_code != ref_code) {
		    ref_code = tst_code;
		    for (ccol = col + 1; ccol < l->l_used; ccol++) {
			tst_code = cell_code(l, ccol);
			if (tst_code != ref_code)
			    break;
		    }
		    if (ref_code != ATR_NORMAL) {
			printf("%c%u", CNTL_A, (unsigned) (ccol - col));
			if (ref_code & ATR_BOLD)
			    my_putc('B');
			if (ref_code & ATR_ITAL)
			    my_putc('I');
			if (ref_code & ATR_UNDER)
			    my_putc('U');
			if (ref_code & ATR_REVERS)
			    my_putc('R');
			if (ref_code & ATR_COLOR)
			    printf("C%X", (ref_code >> SHL_COLOR) & 0xf);
			my_putc(':');
		    }
		}
	    }
	    my_putc((int) l->l_cell[col].c_value);

	    while ((p = l->l_cell[col].link) != NULL) {
		l->l_cell[col].link = p->link;
		free(p);
	    }
	}
	my_putc('\n');

	free_line(l);
    }
}

/*
 * Filter an entire file, writing the result to the standard output.
 */
static void
ManFilter(FILE *ifp)
{
    int c;
    int level = 0;
    int ident = CS_NORMAL;
    int esc_mode = ATR_NORMAL;

    while ((c = my_getc(ifp)) != EOF) {
	switch (c) {
	case '\b':
	    backspace();
	    break;

	case '\r':
	    if (cur_line != NULL)
		cur_line->l_this = 0;
	    break;

	case '\n':
	    next_line();
	    cur_line->l_this = 0;
	    break;

	case '\t':
	    do {
		put_cell(SPACE, level, ident, esc_mode);
	    } while (cur_line->l_this & 7);
	    break;

	case '\v':
	    prev_line();
	    break;

	case SHIFT_IN:
	    ident = CS_NORMAL;
	    break;

	case SHIFT_OUT:
	    ident = CS_ALTERNATE;
	    break;

	case ESCAPE:
	    switch (my_getc(ifp)) {
	    case '[':
		esc_mode = ansi_escape(ifp, esc_mode);
		break;
	    case ']':
		osc_ignored(ifp);
		break;
	    case '\007':
	    case '7':
		prev_line();
		break;
	    case '\010':
	    case '8':
		level = half_up(level);
		break;
	    case '\011':
	    case '9':
		level = half_down(level);
		break;
	    default:		/* ignore everything else */
		break;
	    }
	    break;

	case 0xb7:		/* bullet character */
	    if (!sys_isprint(c)) {
		c = 'o';
	    }
	    goto printable;

	case 0xad:		/* mis-used softhyphen */
	    c = '-';
	    /* FALLTHRU */

	  printable:
	default:		/* ignore other nonprinting characters */
	    if (sys_isprint(c)) {
		put_cell(c, level, ident, esc_mode);
		if (c != SPACE) {
		    if (esc_mode & ATR_BOLD) {
			backspace();
			put_cell(c, level, ident, esc_mode);
		    }
		    if (esc_mode & ATR_UNDER) {
			backspace();
			put_cell('_', level, ident, esc_mode);
		    }
		}
	    }
	    break;
	}
    }

    while (all_lines != NULL) {
	long save = total_lines;
	flush_line();
	if (save == total_lines)
	    break;
    }

    total_lines = 0;
}

int
main(int argc, char **argv)
{
    int n;
    FILE *fp;

    program = argv[0];
    my_getc = ansi_getc;
    my_putc = ansi_putc;
#if OPT_LOCALE
    {
	char *env;

	if (((env = getenv("LC_ALL")) != NULL && *env != 0) ||
	    ((env = getenv("LC_CTYPE")) != NULL && *env != 0) ||
	    ((env = getenv("LANG")) != NULL && *env != 0)) {

	    if (strstr(env, ".UTF-8") != NULL
		|| strstr(env, ".utf-8") != NULL
		|| strstr(env, ".UTF8") != NULL
		|| strstr(env, ".utf8") != NULL) {
		my_getc = utf8_getc;
		my_putc = utf8_putc;
	    }
	}
	setlocale(LC_CTYPE, "");
    }
#endif

    if (argc > 1) {
	for (n = 1; n < argc; n++) {
	    char *param = argv[n];
	    if (*param == '-') {
		if (!strcmp(param, "-8"))
		    opt_8bit = 1;
	    } else {
		fp = fopen(argv[n], "r");
		if (fp == NULL)
		    failed(argv[n]);
		ManFilter(fp);
		(void) fclose(fp);
	    }
	}
    } else {
	ManFilter(stdin);
    }
    fflush(stdout);
    fclose(stdout);
#if NO_LEAKS
    free_line(cur_line);
#endif
    exit(GOODEXIT);		/* successful exit */
    /*NOTREACHED */
}
