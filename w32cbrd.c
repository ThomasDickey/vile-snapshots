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
 * $Header: /users/source/archives/vile.vcs/RCS/w32cbrd.c,v 1.2 1998/04/08 09:01:45 cmorgan Exp $
 */

#include <windows.h>
#include <assert.h>

#include "estruct.h"
#include "edef.h"

#define  _SPC_    ' '
#define  _TAB_    '\t'
#define  _TILDE_  '~'

static char ctrl_lookup[] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_";
static char hex_lookup[]  = "0123456789ABCDEF";

/* ------------------------------------------------------------------ */

/*
 * Copy contents of unnamed register to Windows clipboard.  The control
 * flow is shamelessly copied from kwrite.
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
    mlwrite("[Copying...]");
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
    nbyte++;   /* Room for terminating null

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
                *dst++ = hex_lookup[(c & 0xf0) >> 4];
                *dst++ = hex_lookup[c & 0xf];
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
        mlforce("Clipboad currently busy");
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
 * Paste contents of windows clipboard (if TEXT) to either the unnamed 
 * register or the ';' register (a la xvile).  Decision to be made by 
 * future implementor and/or vile coding list.
 *
 * Another possible semantic:  cbrdpaste copies windows clipboard to
 * current buffer.
 *
 * Unimplemented.  Bound to Shift+Insert.
 */
int 
cbrdpaste(int f, int n)
{
    mlforce("[clipboard paste (unimplemented)]");
    return (TRUE);
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
