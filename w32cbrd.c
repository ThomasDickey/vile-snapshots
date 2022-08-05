/*
 * w32cbrd:  collection of common clipboard manipulation routines shared by
 *           the Win32 console- and GUI-based vile editor.
 *
 * Caveats
 * =======
 * -- On a stock Win95 host, the first copy to the clipboard from the
 *    console version of vile causes the busy thread cursor to be displayed
 *    (i.e., cursor changes to a pointer/hourglass icon).  This cursor stays
 *    active for 5-10 seconds (all apps are active) and then goes away.
 *    Subsequent copies do not show this cursor.  On an NT host, this
 *    phenomenon does not occur.
 *
 * $Id: w32cbrd.c,v 1.45 2022/08/04 21:16:11 tom Exp $
 */

#include "estruct.h"
#include "edef.h"

#include <stdlib.h>
#include <search.h>

#if defined(UNICODE) && defined(CF_UNICODETEXT)
#define USE_UNICODE 1
#else
#define USE_UNICODE 0
#endif

#if USE_UNICODE
#define myTextFormat CF_UNICODETEXT
#else
#define myTextFormat CF_TEXT
#endif

#define  CLIPBOARD_BUSY      "[Clipboard currently busy]"
#define  CLIPBOARD_COPY_MB   "[Clipboard copy from minibuffer not supported]"
#define  CLIPBOARD_COPYING   "[Copying...]"
#define  CLIPBOARD_COPY_FAIL "[Clipboard copy failed]"
#define  CLIPBOARD_COPY_MEM  "[Insufficient memory for copy operation]"

typedef struct rgn_cpyarg_struct {
    UINT nbyte, nline;
    W32_CHAR *dst;
} RGN_CPYARG;

static int print_low, print_high;

/* ------------------------------------------------------------------ */

static void
minibuffer_abort(void)
{
    W32_CHAR str[3];

    /*
     * Aborting out of the minibuffer is not easy.  When in doubt, use a
     * sledge hammer.
     */

    str[0] = ESC;
    str[1] = '\0';
    (void) w32_keybrd_write(str);
    update(TRUE);
}

static void
report_cbrdstats(UINT nbyte, UINT nline, int pasted)
{
    char buf[128];

    if (!global_b_val(MDTERSE)) {
	lsprintf(buf,
		 "[%s %d line%s, %d bytes %s clipboard]",
		 (pasted) ? "Pasted" : "Copied",
		 nline,
		 PLURAL(nline),
		 nbyte,
		 (pasted) ? "from" : "to");
	mlwrite(buf);
    } else
	mlforce("[%d lines]", nline);
}

/* The memory block handle _must_ be unlocked before calling this fn. */
static int
setclipboard(HGLOBAL hClipMem, UINT nbyte, UINT nline)
{
    int rc, i;

    TRACE((T_CALLED "setclipboard(%p, %d, %d)\n", hClipMem, nbyte, nline));

    for (rc = i = 0; i < 8 && (!rc); i++) {
	/* Try to open clipboard */

	if (!OpenClipboard(NULL))
	    Sleep(500);
	else
	    rc = 1;
    }
    if (!rc) {
	mlforce(CLIPBOARD_BUSY);
	GlobalFree(hClipMem);
	returnCode(FALSE);
    }
    EmptyClipboard();
    rc = (SetClipboardData(myTextFormat, hClipMem) != NULL);
    CloseClipboard();
    if (!rc) {
	mlforce(CLIPBOARD_COPY_FAIL);
	GlobalFree(hClipMem);
    } else {
	/* success */

	report_cbrdstats(nbyte - 1,	/* subtract terminating NUL */
			 nline,
			 FALSE);
    }
    returnCode(rc);
}

/* Count lines and nonbuffer data added during "copy to clipboard" operation. */
static void
cbrd_count_meta_data(int len,
		     UINT * nbyte,
		     UINT * nline,
		     char *src)
{
    register UINT c;

    while (len--) {
	if ((c = (UCHAR) * src++) == '\n') {
	    (*nline)++;
	    (*nbyte)++;		/* API requires CR/LF terminator */
	} else if (c < _SPC_ && c != _TAB_)	/* assumes ASCII char set        */
	    (*nbyte)++;		/* account for '^' meta char     */
	else if (c > _TILDE_ && (!PASS_HIGH(c)))	/* assumes ASCII char set */
	    (*nbyte) += 3;	/* account for '\xdd' meta chars */
    }
}

/*
 * This function is called to process each logical line of data in a
 * user-selected region.  It counts the number of bytes of data in the line.
 */
static int
count_rgn_data(void *argp, int l, int r)
{
    RGN_CPYARG *cpyp;
    int len;
    LINE *lp;

    lp = DOT.l;

    /* Rationalize offsets */
    if (llength(lp) < l)
	return (TRUE);
    if (r > llength(lp))
	r = llength(lp);
    cpyp = argp;
    if (r == llength(lp) || regionshape == rgn_RECTANGLE) {
	/* process implied newline */

	cpyp->nline++;
	cpyp->nbyte += 2;	/* CBRD API maps NL -> CR/LF */
    }
    len = r - l;
    cpyp->nbyte += len;
    cbrd_count_meta_data(len, &cpyp->nbyte, &cpyp->nline, lvalue(lp) + l);
    return (TRUE);
}

static void
cbrd_copy_and_xlate(int len, W32_CHAR ** cbrd_ptr, UCHAR * src)
{
    register UINT c;
    W32_CHAR *dst = *cbrd_ptr;
    UCHAR *last = (len + src);

    while (src < last) {
	if ((c = *src) == '\n') {
	    *dst++ = '\r';
	    *dst++ = '\n';
	}
#if USE_UNICODE
	else {
	    UINT target;
	    int rc = vl_conv_to_utf32(&target, (const char *) src, len);

	    if (rc > 1) {
		*dst++ = (W32_CHAR) target;
		src += ((size_t) rc - 1);
	    } else {
		*dst++ = (W32_CHAR) * src;
	    }
	}
#else
	else if ((c >= _SPC_ && c <= _TILDE_) || (c == _TAB_))
	    *dst++ = (UCHAR) c;
	else if (c < _SPC_) {
	    *dst++ = '^';
	    *dst++ = ctrldigits[c];
	} else {
	    /* c > _TILDE_ */

	    if (!PASS_HIGH(c)) {
		*dst++ = '\\';
		*dst++ = 'x';
		*dst++ = hexdigits[(c & 0xf0) >> 4];
		*dst++ = hexdigits[c & 0xf];
	    } else
		*dst++ = (W32_CHAR) c;
	}
#endif
	++src;
    }
    *cbrd_ptr = dst;
}

/*
 * This function is called to process each logical line of data in a
 * user-selected region.  It copies region data to a buffer allocated on
 * the heap.
 */
static int
copy_rgn_data(void *argp, int l, int r)
{
    RGN_CPYARG *cpyp;
    int len;
    LINE *lp;

    lp = DOT.l;

    /* Rationalize offsets */
    if (llength(lp) < l)
	return (TRUE);
    if (r > llength(lp))
	r = llength(lp);
    cpyp = argp;
    len = r - l;
    cbrd_copy_and_xlate(len, &cpyp->dst, (UCHAR *) (lvalue(lp) + l));
    if (r == llength(lp) || regionshape == rgn_RECTANGLE) {
	/* process implied newline */

	*cpyp->dst++ = '\r';
	*cpyp->dst++ = '\n';
    }
    return (TRUE);
}

/*
 * Copy contents of [un]named register to Windows clipboard.  The control
 * flow is shamelessly copied from kwrite().
 */
static int
cbrd_reg_copy(void)
{
    HGLOBAL hClipMem;
    register int i;
    KILL *kp;			/* pointer into [un]named register */
    UINT nbyte;
    UINT nline;
    UCHAR *copy;
    int rc;

    TRACE((T_CALLED "cbrd_reg_copy()\n"));
    /* make sure there is something to put */
    if (kbs[ukb].kbufh == NULL) {
	mlforce("[Nothing to copy]");
	returnCode(FALSE);	/* not an error, just nothing */
    }

    print_high = global_g_val(GVAL_PRINT_HIGH);
    print_low = global_g_val(GVAL_PRINT_LOW);

    /* tell us we're writing */
    mlwrite(CLIPBOARD_COPYING);
    nline = 0;
    nbyte = 0;

    /*
     * Make 2 passes over the data.  1st pass counts the data and
     * adjusts for the fact that:
     *
     * 1) each '\n' must be warped to "\r\n" to satisfy clipboard APIs.
     * 2) unprintable data (modulo tabs) must be warped to a printable
     *    equivalent.
     */
    for (kp = kbs[ukb].kbufh; kp; kp = kp->d_next) {
	i = KbSize(ukb, kp);
	nbyte += i;
	cbrd_count_meta_data(i, &nbyte, &nline, (char *) kp->d_chunk);
    }
    nbyte++;			/* Add room for terminating null */

    /* 2nd pass -- alloc storage for data and copy to clipboard. */
    TRACE(("copying %u bytes to clipboard\n", nbyte));
    hClipMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, nbyte * sizeof(W32_CHAR));
    if (hClipMem == NULL) {
	mlforce(CLIPBOARD_COPY_MEM);
	rc = FALSE;
    } else {
	copy = malloc(nbyte);
	if (copy != 0) {
	    UCHAR *tmp = copy;
	    W32_CHAR *dst;
	    if ((dst = GlobalLock(hClipMem)) != NULL) {
		W32_CHAR *old = dst;
		for (kp = kbs[ukb].kbufh; kp; kp = kp->d_next) {
		    int size = KbSize(ukb, kp);
		    memcpy(tmp, kp->d_chunk, size);
		    tmp += size;
		}
		cbrd_copy_and_xlate((int) (tmp - copy), &dst, copy);
		*dst = '\0';
		GlobalUnlock(hClipMem);
		rc = setclipboard(hClipMem, (UINT) (dst - old), nline);
	    } else {
		mlforce("[Cannot copy]");
		rc = FALSE;
	    }
	    free(copy);
	} else {
	    mlforce("[Cannot copy]");
	    rc = FALSE;
	}
    }
    returnCode(rc);
}

/*
 * Copy contents of unnamed register to Windows clipboard.
 *
 * Bound to Alt+Insert.
 */
int
cbrdcpy_unnamed(int unused1 GCC_UNUSED, int unused2 GCC_UNUSED)
{
    int rc;

    if (reading_msg_line) {
	minibuffer_abort();	/* FIXME -- goes away some day? */
	mlerase();
	mlforce(CLIPBOARD_COPY_MB);
	return (ABORT);
    }
    kregcirculate(FALSE);
    rc = cbrd_reg_copy();
    ukb = 0;
    return (rc);
}

/*
 * Copy the currently-selected region (i.e., the range of lines from DOT to
 * MK, inclusive) to the windows clipboard.  Lots of code has been borrowed
 * and/or adapted from operyank() and writereg().
 */
static int
cbrdcpy_region(void)
{
    RGN_CPYARG cpyarg;
    DORGNLINES dorgn;
    HGLOBAL hClipMem;
    MARK odot;
    int rc;

    TRACE((T_CALLED "cbrdcpy_region()\n"));
    mlwrite(CLIPBOARD_COPYING);
    print_high = global_g_val(GVAL_PRINT_HIGH);
    print_low = global_g_val(GVAL_PRINT_LOW);
    odot = DOT;			/* do_lines_in_region() moves DOT. */
    cpyarg.nbyte = cpyarg.nline = 0;
    dorgn = get_do_lines_rgn();

    /*
     * Make 2 passes over the data.  1st pass counts the data and
     * adjusts for the fact that:
     *
     * 1) each '\n' must be warped to "\r\n" to satisfy clipboard APIs.
     * 2) unprintable data (modulo tabs) must be warped to a printable
     *    equivalent.
     */
    rc = dorgn(count_rgn_data, &cpyarg, TRUE);
    DOT = odot;
    if (!rc)
	returnCode(FALSE);
    cpyarg.nbyte++;		/* Terminating nul */

    /* 2nd pass -- alloc storage for data and copy to clipboard. */
    hClipMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cpyarg.nbyte);
    if (hClipMem == NULL) {
	mlforce(CLIPBOARD_COPY_MEM);
	returnCode(FALSE);
    }
    if ((cpyarg.dst = GlobalLock(hClipMem)) != NULL) {
	/*
	 * Pass #2 -> The actual copy.  Don't need to restore DOT, that
	 * is handled by opercbrdcpy().
	 */
	rc = dorgn(copy_rgn_data, &cpyarg, TRUE);
	GlobalUnlock(hClipMem);
    } else {
	rc = FALSE;
    }
    if (!rc) {
	GlobalFree(hClipMem);
	returnCode(FALSE);
    }
    *cpyarg.dst = '\0';
    returnCode(setclipboard(hClipMem, cpyarg.nbyte, cpyarg.nline));
}

/*
 * Copy contents of specified region or register to Windows clipboard.
 * This command is an operator and mimics the functionality of ^W, but
 * mimics operyank()'s implementation.
 *
 * Bound to Ctrl+Insert.
 */
int
opercbrdcpy(int f, int n)
{
    if (reading_msg_line) {
	minibuffer_abort();	/* FIXME -- goes away some day? */
	mlerase();
	mlforce(CLIPBOARD_COPY_MB);
	return (ABORT);
    }
    if (ukb != 0)
	return (cbrd_reg_copy());
    else {
	MARK odot;
	int rc;

	odot = DOT;
	opcmd = OPDEL;
	rc = vile_op(f, n, cbrdcpy_region, "Clipboard copy");
	DOT = odot;		/* cursor does not move */
	return (rc);
    }
}

/* ------------------- Paste Functionality ----------------------- */

#define MAX_MAPPED_STR 16	/* fairly conservative :-) */

static int map_and_insert(UINT, UINT *);

typedef struct {
    UINT val;
    char *str;
} MAP;

/* Keep this table sorted by "val" . */
/* *INDENT-OFF* */
static MAP cbrdmap[] =
{
    { 0x85, "..."  },
    { 0x8B, "<"    },
    { 0x91, "'"    },
    { 0x92, "'"    },
    { 0x93, "\""   },
    { 0x94, "\""   },
    { 0x96, "-"    },
    { 0x97, "--"   },
    { 0x99, "(TM)" },
    { 0x9B, ">"    },
    { 0xA6, "|"    },
    { 0xA9, "(C)"  },
    { 0xAB, "<<"   },
    { 0xAD, "-"    },
    { 0xAE, "(R)"  },
    { 0xB1, "+/-"  },
    { 0xBB, ">>"   },
    { 0xBC, "1/4"  },
    { 0xBD, "1/2"  },
    { 0xBE, "3/4"  },
};
/* *INDENT-ON* */

/* --------------------------------------------------------------- */

static int
map_compare(const void *elem1, const void *elem2)
{
    return (((const MAP *) elem1)->val - ((const MAP *) elem2)->val);
}

static int
map_cbrd_char(UINT c, W32_CHAR mapped_rslt[MAX_MAPPED_STR])
{
    MAP key, *rslt_p;
    int nmapped = 0;
    char *str;

    key.val = c;
    rslt_p = bsearch(&key,
		     cbrdmap,
		     sizeof(cbrdmap) / sizeof(cbrdmap[0]),
		     sizeof(cbrdmap[0]),
		     map_compare);
    if (!rslt_p)
	mapped_rslt[nmapped++] = (UCHAR) c;
    else {
	for (str = rslt_p->str; *str; str++)
	    mapped_rslt[nmapped++] = *str;
    }
    mapped_rslt[nmapped] = '\0';
    return (nmapped);
}

/* paste a single line from the clipboard to the minibuffer. */
static int
paste_to_minibuffer(W32_CHAR * cbrddata)
{
    int rc = TRUE;
    W32_CHAR *cp = cbrddata;
#if !USE_UNICODE
    W32_CHAR one_char[2];
    W32_CHAR map_str[MAX_MAPPED_STR + 1];
#endif

    while (*cp) {
	if (*cp == '\r' && *(cp + 1) == '\n') {
	    *cp = '\0';

	    /*
	     * Don't allow more than one line of data to be pasted into the
	     * minibuffer (to protect the user from seriously bad side
	     * effects when s/he pastes in the wrong buffer).  We don't
	     * report an error here in an effort to retain compatibility
	     * with a couple of "significant" win32 apps (e.g., IE and
	     * Outlook) that simply truncate a paste at one line when it
	     * only makes sense to take a single line of input.
	     */
	    break;
	}
	cp++;
    }
#if USE_UNICODE
    rc = w32_keybrd_write(cbrddata);
#else
    one_char[1] = '\0';
    while (*cbrddata && rc) {
	if (*cbrddata > _TILDE_) {
	    (void) map_cbrd_char(*cbrddata, map_str);
	    rc = w32_keybrd_write(map_str);
	} else {
	    one_char[0] = *cbrddata;
	    rc = w32_keybrd_write(one_char);
	}
	cbrddata++;
    }
#endif
    return (rc);
}

/*
 * Paste contents of windows clipboard (if TEXT) to current buffer.
 * Bound to Shift+Insert.
 */
int
cbrdpaste(int f GCC_UNUSED, int n GCC_UNUSED)
{
    UINT c;
    W32_CHAR *data;
    HANDLE hClipMem;
    int i, rc, suppressnl;
    UINT nbyte, nline;

    TRACE((T_CALLED "cbrdpaste\n"));
    for (rc = i = 0; i < 8 && (!rc); i++) {
	/* Try to open clipboard */

	if (!OpenClipboard(NULL))
	    Sleep(500);
	else
	    rc = 1;
    }
    if (!rc) {
	if (reading_msg_line) {
	    minibuffer_abort();	/* FIXME -- goes away some day? */
	    rc = ABORT;
	} else
	    rc = FALSE;
	mlforce(CLIPBOARD_BUSY);
	returnCode(rc);
    }
    if ((hClipMem = GetClipboardData(myTextFormat)) == NULL) {
	CloseClipboard();
	if (reading_msg_line) {
	    minibuffer_abort();	/* FIXME -- goes away some day? */
	    rc = ABORT;
	} else
	    rc = FALSE;
	mlforce("[Clipboard empty or not TEXT data]");
	returnCode(rc);
    }
    if ((data = GlobalLock(hClipMem)) == NULL) {
	CloseClipboard();
	if (reading_msg_line) {
	    minibuffer_abort();	/* FIXME -- goes away some day? */
	    rc = ABORT;
	} else
	    rc = FALSE;
	mlforce("[Can't lock clipboard memory]");
	returnCode(rc);
    }
    if (reading_msg_line) {
	rc = paste_to_minibuffer(data);
	GlobalUnlock(hClipMem);
	CloseClipboard();
	returnCode(rc);
    }
    mlwrite(CLIPBOARD_COPYING);
    nbyte = nline = 0;
    rc = TRUE;

    /*
     * Before stuffing data in the current buffer, save info regarding dot
     * and mark.  The dot/mark manipulation code is more or less cribbed
     * from doput() and PutChar().  Note also that clipboard data is always
     * copied into the current region as if it were an "exact" shape, which
     * should be the most intuitive result for Windows users who work with
     * the clipboard (I hope).
     */
    suppressnl = is_header_line(DOT, curbp);
    (void) setmark();
    while (*data && rc) {
	c = *data;

	if (c == '\n') {
	    nbyte++;
	    nline++;
	    rc = lnewline();
	} else if (c == '\r' && *(data + 1) == '\n') {
	    /* Clipboard end of line delimiter is crlf.  Ignore cr. */
	    ;
	} else if (!b_is_utfXX(curbp) && (c > _TILDE_)) {
	    rc = map_and_insert(c, &nbyte);
	} else {
	    int chunk;
	    int c2;
	    int base = DOT.o;
#ifdef UNICODE
	    int in_chunk;
	    int chunk_bytes = 0;
	    char *dst;
#endif

	    for (chunk = 0; data[chunk] != 0; ++chunk) {
		if ((c2 = data[chunk]) == '\n'
		    || (c2 == '\r' && data[chunk + 1] == '\n')
		    || (!b_is_utfXX(curbp) && (c > _TILDE_))) {
		    break;
		}
	    }

#ifdef UNICODE
	    /* sizeof(W32_CHAR) > 1 */

	    /* compute 'chunk_bytes' here based on actual bytes */
	    for (in_chunk = 0; in_chunk < chunk; ++in_chunk) {
		chunk_bytes += vl_conv_to_utf8((UCHAR *) 0, data[in_chunk], MAX_UTF8);
	    }

	    rc = lins_bytes(chunk_bytes, (int) c);
	    if (!rc)
		break;

	    nbyte += chunk_bytes;
	    for (in_chunk = 0, dst = lvalue(DOT.l) + base;
		 in_chunk < chunk;
		 ++in_chunk) {
		UCHAR target[MAX_UTF8];
		int len = vl_conv_to_utf8(target, data[in_chunk], MAX_UTF8);
		memcpy(dst, target, len);
		dst += len;
	    }
	    data += ((size_t) chunk - 1);
#else
	    /* sizeof(W32_CHAR) == 1 */

	    rc = lins_bytes(chunk, (int) c);
	    if (!rc)
		break;

	    nbyte += chunk;
	    memcpy(lvalue(DOT.l) + base, data, chunk);
	    data += (chunk - 1);
#endif
	}
	data++;
    }
    if (rc) {
	if (nbyte > 0 && (data[-1] == '\n') && suppressnl) {
	    /*
	     * Last byte inserted was a newline and DOT was originally
	     * pointing at the beginning of the buffer(??).  In this
	     * situation, lins_bytes() has added an additional newline to the
	     * buffer.  Remove it.
	     */

	    (void) ldel_bytes(1, FALSE);
	}
    }
    GlobalUnlock(hClipMem);
    CloseClipboard();
    if (!rc)
	(void) no_memory("cbrdpaste()");
    else {
	/*
	 * Success.  Fiddle with dot and mark again (another chunk of doput()
	 * code).  "Tha' boy shore makes keen use of cut and paste."
	 * Don't swap if inserting - this allows you to continue typing
	 * after a paste operation without additional futzing with DOT.
	 */

	if (!insertmode)
	    swapmark();		/* I understand this. */
	if (is_header_line(DOT, curbp))
	    DOT.l = lback(DOT.l);	/* This is a mystery. */
	report_cbrdstats(nbyte, nline, TRUE);
    }
    returnCode(rc);
}

/*
 * Map selected characters from the ANSI character set to their ASCII
 * equivalents and insert same in the current buffer.
 */
static int
map_and_insert(UINT c,		/* ANSI char to insert   */
	       UINT * nbyte	/* total #chars inserted */
)
{
    W32_CHAR mapped_str[MAX_MAPPED_STR];
    int i, nmapped, rc;

    nmapped = map_cbrd_char(c, mapped_str);
    *nbyte += nmapped;
    for (rc = TRUE, i = 0; i < nmapped && rc; i++)
	rc = lins_bytes(1, mapped_str[i]);
    return (rc);
}

/* --------------------- Cut Functionality ----------------------- */

/*
 * Cut current [win]vile selection to windows clipboard.
 * Bound to Shift+Delete.
 */
int
cbrdcut(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return (w32_del_selection(TRUE));
}
