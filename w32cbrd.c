/*
 * w32cbrd:  collection of common clipboard manipulation routines shared by
 *           the Win32 console- and GUI-based vile editor.
 *
 * Caveats
 * =======
 * -- This code has not been tested with NT 3.51 .
 *
 * -- On a stock Win95 host, the first copy to the clipboard from the
 *    console version of vile causes the busy thread cursor to be displayed
 *    (i.e., cursor changes to a pointer/hourglass icon).  This cursor stays
 *    active for 5-10 seconds (all apps are active) and then goes away.
 *    Subsequent copies do not show this cursor.  On an NT 4.0 host, this
 *    phenomenon does not occur.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/w32cbrd.c,v 1.6 1998/07/09 23:59:38 tom Exp $
 */

#include <windows.h>
#include <stdlib.h>
#include <search.h>

#include "estruct.h"
#include "edef.h"

#define  CLIPBOARD_BUSY      "Clipboard currently busy"
#define  CLIPBOARD_COPYING   "[Copying...]"
#define  _SPC_               ' '
#define  _TAB_               '\t'
#define  _TILDE_             '~'

static char ctrl_lookup[] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_";

static int  map_and_insert(unsigned, unsigned *);

/* ------------------------------------------------------------------ */

/*
 * Copy contents of unnamed register to Windows clipboard.  The control
 * flow is shamelessly copied from kwrite.  Bound to Alt+Insert.
 */
int
cbrdcpy_unnamed(int unused1, int unused2)
{
    register unsigned       c;
    HGLOBAL                 hClipMem;
    register int            i;
    KILL                    *kp;      /* pointer into unnamed register */
    DWORD                   nbyte;
    unsigned                nline;
    int                     rc;
    register unsigned char  *src, *dst;

    kregcirculate(FALSE);

    /* make sure there is something to put */
    if (kbs[ukb].kbufh == NULL)
    {
        ukb = 0;
        mlforce("Nothing to copy");
        return (FALSE);     /* not an error, just nothing */
    }

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
    kp = kbs[ukb].kbufh;
    while (kp != NULL)
    {
        i      = KbSize(ukb, kp);
        src    = (char *) kp->d_chunk;
        nbyte += i;
        while (i--)
        {
            if ((c = *src++) == '\n')
            {
                nline++;
                nbyte++;                 /* must prepend '\r'             */
            }
            else if (c < _SPC_ && c != _TAB_) /* assumes ASCII char set   */
                nbyte++;                 /* account for '^' meta char     */
            else if (c > _TILDE_)        /* assumes ASCII char set        */
                nbyte += 3;              /* account for '\xdd' meta chars */
        }
        kp = kp->d_next;
    }
    nbyte++;   /* Room for terminating null */

    /* 2nd pass -- alloc storage for data and copy to clipboard. */
    if ((hClipMem = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, nbyte)) == NULL)
    {
        ukb = 0;
        mlforce("Insufficient memory for copy operation");
        return (FALSE);
    }
    dst = GlobalLock(hClipMem);
    kp  = kbs[ukb].kbufh;
    while (kp != NULL)
    {
        i   = KbSize(ukb, kp);
        src = (char *) kp->d_chunk;
        while (i--)
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

                *dst++ = '\\';
                *dst++ = 'x';
                *dst++ = hexdigits[(c & 0xf0) >> 4];
                *dst++ = hexdigits[c & 0xf];
            }
        }
        kp = kp->d_next;
    }
    *dst = '\0';
    ukb  = 0;
    GlobalUnlock(hClipMem);
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
        mlforce("Clipboad copy failed");
        GlobalFree(hClipMem);
    }
    else
    {
        /* success */

        if (! global_b_val(MDTERSE))
        {
            char buf[128];

            _snprintf(buf,
                      sizeof(buf),
                      "[Copied %u line%s, %u bytes to clipboard]",
                      nline,
                      PLURAL(nline),
                      nbyte - 1);   /* subtract terminating nul */
            mlwrite(buf);
        }
        else
            mlforce("[%d lines]", nline);
    }
    return (rc);
}



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
    {
        mlforce("Memory allocation failed");

        /* Give user a chance to read message--more will surely follow. */
        Sleep(3000);
    }
    else
    {
        char buf[128];

        /*
         * Success.  Fiddle with dot and mark again (another chunk of doput()
         * code).  "Tha' boy shore makes keen use of cut and paste."
         */
		swapmark();                           /* I understand this. */
        if (is_header_line(DOT, curbp))
            DOT.l = lback(DOT.l);             /* This is a mystery. */

        if (! global_b_val(MDTERSE))
        {
            _snprintf(buf,
                      sizeof(buf),
                      "[Copied %u line%s, %u bytes from clipboard]",
                      nline,
                      PLURAL(nline),
                      nbyte);
            mlwrite(buf);
        }
        else
        {
            _snprintf(buf, sizeof(buf), "[%d line%s]", nline, PLURAL(nline));
            mlforce(buf);
        }
    }
    return (rc);
}



/*
 * Copy contents of specified region or register to Windows clipboard.
 * This command is an operaor and should mimic ^W.
 *
 * Unimplemented.  Bound to Ctrl+Insert.
 */
int
opercbrdcpy(int f, int n)
{
    mlforce("[oper clipboard copy (unimplemented)]");
    return (TRUE);
}


/* --------------------------------------------------------------- */

typedef struct { unsigned val; char *str; } MAP;

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
