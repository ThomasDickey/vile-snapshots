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
 * $Header: /users/source/archives/vile.vcs/RCS/w32cbrd.c,v 1.13 1998/11/24 11:02:05 cmorgan Exp $
 */

#include <windows.h>
#include <stdlib.h>
#include <search.h>

#include "estruct.h"
#include "edef.h"

#define  CLIPBOARD_BUSY      "Clipboard currently busy"
#define  CLIPBOARD_COPYING   "[Copying...]"
#define  CLIPBOARD_COPY_FAIL "Clipboad copy failed"
#define  CLIPBOARD_COPY_MEM  "Insufficient memory for copy operation"
#define  PASS_HIGH(c)        ((c) <= print_high && (c) >= print_low)
#define  _SPC_               ' '
#define  _TAB_               '\t'
#define  _TILDE_             '~'

typedef struct rgn_cpyarg_struct
{
    unsigned      nbyte, nline;
    unsigned char *dst;
} RGN_CPYARG;

static char ctrl_lookup[] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_";
static int  print_low, print_high;

/* ------------------------------------------------------------------ */

static void
report_cbrdstats(unsigned nbyte, unsigned nline, char *direction)
{
    char buf[128];

    if (! global_b_val(MDTERSE))
    {
        lsprintf(buf,
                  "[Copied %d line%s, %d bytes %s clipboard]",
                  nline,
                  PLURAL(nline),
                  nbyte,
                  direction);
        mlwrite(buf);
    }
    else
        mlforce("[%d lines]", nline);
}



/* The memory block handle _must_ be unlocked before calling this fn. */
static int
setclipboard(HGLOBAL hClipMem, unsigned nbyte, unsigned nline)
{
    int rc, i;

    for (rc = i = 0; i < 8 && (! rc); i++)
    {
        /* Try to open clipboard */

        if (! OpenClipboard(NULL))
            Sleep(500);
        else
            rc = 1;
    }
    if (! rc)
    {
        mlforce(CLIPBOARD_BUSY);
        GlobalFree(hClipMem);
        return (FALSE);
    }
    EmptyClipboard();
    rc = (SetClipboardData(CF_TEXT, hClipMem) != NULL);
    CloseClipboard();
    if (! rc)
    {
        mlforce(CLIPBOARD_COPY_FAIL);
        GlobalFree(hClipMem);
    }
    else
    {
        /* success */

        report_cbrdstats(nbyte - 1,  /* subtract terminating NUL */
                         nline,
                         "to");
    }
    return (rc);
}



/* Count lines and nonbuffer data added during "copy to clipboard" operation. */
static void
cbrd_count_meta_data(int           len,
                     unsigned      *nbyte,
                     unsigned      *nline,
                     unsigned char *src)
{
    register unsigned c;

    while (len--)
    {
        if ((c = *src++) == '\n')
        {
            (*nline)++;
            (*nbyte)++;                    /* API requires CR/LF terminator */
        }
        else if (c < _SPC_ && c != _TAB_)  /* assumes ASCII char set        */
            (*nbyte)++;                    /* account for '^' meta char     */
        else if (c > _TILDE_ && (! PASS_HIGH(c))) /* assumes ASCII char set */
            (*nbyte) += 3;                 /* account for '\xdd' meta chars */
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
    int        len;
    LINE       *lp;

    lp = DOT.l;

    /* Rationalize offsets */
    if (llength(lp) < l)
        return (TRUE);
    if (r > llength(lp))
        r = llength(lp);
    cpyp = argp;
    if (r == llength(lp) || regionshape == RECTANGLE)
    {
        /* process implied newline */

        cpyp->nline++;
        cpyp->nbyte += 2;   /* CBRD API maps NL -> CR/LF */
    }
    len          = r - l;
    cpyp->nbyte += len;
    cbrd_count_meta_data(len, &cpyp->nbyte, &cpyp->nline, lp->l_text + l);
    return (TRUE);
}



static void
cbrd_copy_and_xlate(int len, unsigned char **cbrd_ptr, unsigned char *src)
{
    register unsigned c;
    unsigned char     *dst = *cbrd_ptr;

    while (len--)
    {
        if ((c = *src++) == '\n')
        {
            *dst++ = '\r';
            *dst++ = '\n';
        }
        else if ((c >= _SPC_ && c <= _TILDE_) || (c == _TAB_))
            *dst++ = c;
        else if (c < _SPC_)
        {
            *dst++ = '^';
            *dst++ = ctrl_lookup[c];
        }
        else
        {
            /* c > _TILDE_ */

            if (! PASS_HIGH(c))
            {
                *dst++ = '\\';
                *dst++ = 'x';
                *dst++ = hexdigits[(c & 0xf0) >> 4];
                *dst++ = hexdigits[c & 0xf];
            }
            else
                *dst++ = c;
        }
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
    int        len;
    LINE       *lp;

    lp = DOT.l;

    /* Rationalize offsets */
    if (llength(lp) < l)
        return (TRUE);
    if (r > llength(lp))
        r = llength(lp);
    cpyp = argp;
    len  = r - l;
    cbrd_copy_and_xlate(len, &cpyp->dst, lp->l_text + l);
    if (r == llength(lp) || regionshape == RECTANGLE)
    {
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
    HGLOBAL                 hClipMem;
    register int            i;
    KILL                    *kp;      /* pointer into [un]named register */
    DWORD                   nbyte;
    unsigned                nline;
    unsigned char           *dst;

    /* make sure there is something to put */
    if (kbs[ukb].kbufh == NULL)
    {
        mlforce("Nothing to copy");
        return (FALSE);     /* not an error, just nothing */
    }

    print_high = global_g_val(GVAL_PRINT_HIGH);
    print_low  = global_g_val(GVAL_PRINT_LOW);

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
    for (kp = kbs[ukb].kbufh; kp; kp = kp->d_next)
    {
        i      = KbSize(ukb, kp);
        nbyte += i;
        cbrd_count_meta_data(i, &nbyte, &nline, kp->d_chunk);
    }
    nbyte++;   /* Add room for terminating null */

    /* 2nd pass -- alloc storage for data and copy to clipboard. */
    if ((hClipMem = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, nbyte)) == NULL)
    {
        mlforce(CLIPBOARD_COPY_MEM);
        return (FALSE);
    }
    dst = GlobalLock(hClipMem);
    for (kp = kbs[ukb].kbufh; kp; kp = kp->d_next)
        cbrd_copy_and_xlate((int) KbSize(ukb, kp), &dst, kp->d_chunk);
    *dst = '\0';
    GlobalUnlock(hClipMem);
    return (setclipboard(hClipMem, nbyte, nline));
}



/*
 * Copy contents of unnamed register to Windows clipboard.
 *
 * Bound to Alt+Insert.
 */
int
cbrdcpy_unnamed(int unused1, int unused2)
{
    int rc;

    kregcirculate(FALSE);
    rc  = cbrd_reg_copy();
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
    RGN_CPYARG              cpyarg;
    DORGNLINES              dorgn;
    HGLOBAL                 hClipMem;
    MARK                    odot;
    int                     rc;

    mlwrite(CLIPBOARD_COPYING);
    print_high   = global_g_val(GVAL_PRINT_HIGH);
    print_low    = global_g_val(GVAL_PRINT_LOW);
    odot         = DOT;          /* do_lines_in_region() moves DOT. */
    cpyarg.nbyte = cpyarg.nline = 0;
    dorgn        = get_do_lines_rgn();

    /*
     * Make 2 passes over the data.  1st pass counts the data and
     * adjusts for the fact that:
     *
     * 1) each '\n' must be warped to "\r\n" to satisfy clipboard APIs.
     * 2) unprintable data (modulo tabs) must be warped to a printable
     *    equivalent.
     */
    rc  = dorgn(count_rgn_data, &cpyarg, TRUE);
    DOT = odot;
    if (!rc)
        return (FALSE);
    cpyarg.nbyte++;        /* Terminating nul */

    /* 2nd pass -- alloc storage for data and copy to clipboard. */
    if ((hClipMem = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE,
                                cpyarg.nbyte)) == NULL)
    {
        mlforce(CLIPBOARD_COPY_MEM);
        return (FALSE);
    }
    cpyarg.dst = GlobalLock(hClipMem);

    /*
     * Pass #2 -> The actual copy.  Don't need to restore DOT, that
     * is handled by opercbrdcpy().
     */
    rc = dorgn(copy_rgn_data, &cpyarg, TRUE);
    GlobalUnlock(hClipMem);
    if (! rc)
    {
        GlobalFree(hClipMem);
        return (FALSE);
    }
    *cpyarg.dst = '\0';
    return (setclipboard(hClipMem, cpyarg.nbyte, cpyarg.nline));
}



/*
 * Copy contents of specified region or register to Windows clipboard.
 * This command is an operaor and mimics the functionality of ^W, but
 * mimics operyank()'s implemenation.
 *
 * Bound to Ctrl+Insert.
 */
int
opercbrdcpy(int f, int n)
{
    if (ukb != 0)
        return (cbrd_reg_copy());
    else
    {
        MARK odot;
        int  rc;

        odot  = DOT;
        opcmd = OPDEL;
        rc    = vile_op(f, n, cbrdcpy_region, "Clipboard copy");
        DOT   = odot;   /* cursor does not move */
        return (rc);
    }
}

/* ------------------- Paste Functionality ----------------------- */

static int  map_and_insert(unsigned, unsigned *);

typedef struct { unsigned val; char *str; } MAP;

/* --------------------------------------------------------------- */

/*
 * Paste contents of windows clipboard (if TEXT) to current buffer.
 * Bound to Shift+Insert.
 */
int
cbrdpaste(int f, int n)
{
    register unsigned      c;
    register unsigned char *data;
    HANDLE                 hClipMem;
    int                    i, rc, suppressnl;
    unsigned               nbyte, nline;

    for (rc = i = 0; i < 8 && (! rc); i++)
    {
        /* Try to open clipboard */

        if (! OpenClipboard(NULL))
            Sleep(500);
        else
            rc = 1;
    }
    if (! rc)
    {
        mlforce(CLIPBOARD_BUSY);
        return (FALSE);
    }
    if ((hClipMem = GetClipboardData(CF_TEXT)) == NULL)
    {
        CloseClipboard();
        mlforce("[Clipboard empty or not TEXT data]");
        return (FALSE);
    }
    if ((data = GlobalLock(hClipMem)) == NULL)
    {
        CloseClipboard();
        mlforce("[Can't lock clipboard memory]");
        return (FALSE);
    }
    mlwrite(CLIPBOARD_COPYING);
    nbyte = nline = 0;
    rc    = TRUE;

    /*
     * Before stuffing data in the current buffer, save info regarding dot
     * and mark.  The dot/mark manipulation code is more or less cribbed
     * from doput() and PutChar().  Note also that clipboard data is always
     * copied into the current region as if it were an "exact" shape, which
     * should be the most intuitive result for Windows users who work with
     * the clipboard (I hope).
     */
    suppressnl = is_header_line(DOT, curbp);
    if (! is_at_end_of_line(DOT))
        forwchar(TRUE,1);
    (void) setmark();
    while(*data && rc)
    {
        if ((c = *data) == '\n')
        {
            nbyte++;
            nline++;
            rc = lnewline();
        }
        else if (c == '\r' && *(data + 1) == '\n')
        {

            /* Clipboard end of line delimiter is crlf.  Ignore cr. */

            ;
        }
        else if (c > _TILDE_)
            rc = map_and_insert(c, &nbyte);
        else
        {
            nbyte++;
            rc = linsert(1, (int) c);
        }
        data++;
    }
    if (rc)
    {
        if (nbyte > 0 && (data[-1] == '\n') && suppressnl)
        {
            /*
             * Last byte inserted was a newline and DOT was originally
             * pointing at the beginning of the buffer(??).  In this
             * situation, linsert() has added an additional newline to the
             * buffer.  Remove it.
             */

            (void) ldelete(1, FALSE);
        }
    }
    GlobalUnlock(hClipMem);
    CloseClipboard();
    if (! rc)
        (void) no_memory("cbrdpaste()");
    else
    {
        /*
         * Success.  Fiddle with dot and mark again (another chunk of doput()
         * code).  "Tha' boy shore makes keen use of cut and paste."
         */

		swapmark();                           /* I understand this. */
        if (is_header_line(DOT, curbp))
            DOT.l = lback(DOT.l);             /* This is a mystery. */
        report_cbrdstats(nbyte, nline, "from");
    }
    return (rc);
}



static int
map_compare(const void *elem1, const void *elem2)
{
    return (((MAP *) elem1)->val - ((MAP *) elem2)->val);
}



/*
 * Map selected characters from the ANSI character set to their ASCII
 * equivalents and insert same in the current buffer.
 */
static int
map_and_insert(unsigned c,       /* ANSI char to insert   */
               unsigned *nbyte   /* total #chars inserted */
               )
{
    int  rc;
    MAP  key, *rslt_p;
    char *str;

    /* Keep this table sorted by "val" . */
    static MAP map[] =
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

    key.val = c;
    rslt_p  = bsearch(&key,
                      map,
                      sizeof(map) / sizeof(map[0]),
                      sizeof(map[0]),
                      map_compare);
    if (! rslt_p)
    {
        (*nbyte)++;
        rc = linsert(1, c);
    }
    else
    {
        for (rc = TRUE, str = rslt_p->str; *str && rc; str++)
        {
            (*nbyte)++;
            rc = linsert(1, *str);
        }
    }
    return (rc);
}
