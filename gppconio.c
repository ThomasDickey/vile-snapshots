
/* This file is properly part of the excellent DJGPP compiler/tools package
	put out by DJ Delorie for use under DOS.  Unfortunately, the module
	as distributed in many versions of the DJGPP is buggy -- this copy
	of gppconio.c fixes many of those bugs.  I understand that these
	changes have been sent back to the DJGPP maintainers -- hopefully
	vile will not need to include this file in the future.  -pgf 9/95
*/


#if __DJGPP__ < 2

/**********************************************************************
 *  
 *  NAME:           gppconio.c
 *  
 *  DESCRIPTION:    simulate Borland text video funcs for GNU C++
 *  
 *  copyright (c) 1991 J. Alan Eldridge
 * 
 *  M O D I F I C A T I O N   H I S T O R Y
 *
 *  when        who                 what
 *  -------------------------------------------------------------------
 *  10/27/91    J. Alan Eldridge    created
 *  01/06/92    D. Buerssner        make it work with extended characters
 * (buers@dg1.chemie.uni-konstanz.de) speed-up of cputs
 *                                  some missing brackets in VIDADDR
 *                                  don't need scrollwindow anymore
 *  07/15/93    D. Buerssner        take care of cursor tracking
 *                                    txinfo.curx and txinfo.cury
 *                                  fill in missing functionality
 *                                    - textmode
 *                                    - cscanf
 *                                    - cgets
 *                                    - getch and ungetch
 *                                    - _setcursortype
 *                                    - kbhit 
 *                                  (hpefully) proper initialization of
 *                                    txinfo.normattrib and txinfo.attribute  
 *                                  gotoxy(1,1) in clrscr (bug introduced
 *                                    by my previous patches)
 *                                  gotoxy(1,1) in window
 *                                  take care of BELL and BACKSPACE
 *                                    in putch and cputs
 *                                  take care of blinking bit in textcolor
 *                                    and textbackground
 *                                  declare (and ignore) directvideo 
 *  10/09/93    DJ Delorie          Switch to dosmem*() for DPMI
 *  05/01/94    DJ Delorie          Add _wscroll
 *********************************************************************/

#include    <stdlib.h>
#include    <stdio.h>
#include    <stdarg.h>
#include    <dos.h>
#include    <pc.h>
#include    <go32.h>
#include    "gppconio.h"

int _wscroll = 1;

int directvideo = 1;  /* We ignore this */

static void setcursor(unsigned int shape);
static int getvideomode(void);
static void bell(void);
static int get_screenattrib(void);
static int isEGA(void);
static int _scan_getche(FILE *fp);
static int _scan_ungetch(int c, FILE *fp);

#define DBGGTINFO   0

static unsigned ScreenAddress = 0xb8000UL; /* initialize just in case */
static struct text_info txinfo;
static int ungot_char;
static int char_avail = 0;

#define VIDADDR(r,c) (ScreenAddress + 2*(((r) * txinfo.screenwidth) + (c)))

int puttext(int c, int r, int c2, int r2, void *buf)
{
  short *cbuf = (short *)buf;
  /* we should check for valid parameters, and maybe return 0 */
  r--, r2--, c--, c2--;
  for (; r <= r2; r++)
  {
    dosmemput(cbuf, (c2-c+1)*2, VIDADDR(r, c));
    cbuf += c2-c+1;
  }
  return 1;
}

int gettext(int c, int r, int c2, int r2, void *buf)
{
  short *cbuf = (short *)buf;
  /* we should check for valid parameters, and maybe return 0 */
  r--, r2--, c--, c2--;
  for (; r <= r2; r++)
  {
    dosmemget(VIDADDR(r, c), (c2-c+1)*2, cbuf);
    cbuf += c2-c+1;
  }
  return 1;
}
        
void gotoxy(int col, int row)
{
  ScreenSetCursor(row + txinfo.wintop - 2, col + txinfo.winleft - 2);
  txinfo.curx = col;
  txinfo.cury = row;
}

int wherex(void)
{
  int row, col;
  
  ScreenGetCursor(&row, &col);
  
  return col - txinfo.winleft + 2;
}
    
int wherey(void)
{
  int row, col;
  
  ScreenGetCursor(&row, &col);
  
  return row - txinfo.wintop + 2;
}

void textmode(int mode)
{
    union REGS regs;
    int mode_to_set = mode;
    if (mode == LASTMODE)
        mode = mode_to_set = txinfo.currmode;
    if (mode == C4350)
        /* 
         * just set mode 3 and load 8x8 font, idea taken 
         * (and code translated from Assembler to C)
         * form Csaba Biegels stdvga.asm
         */
        mode_to_set = 0x03;  
    regs.h.ah = 0x00; /* set mode */
    regs.h.al = mode_to_set;
    int86(0x10, &regs, &regs);
    if (mode == C80 || mode == BW80 || mode == C4350)
    {
        if (isEGA())
        {
            /* 
             * enable cursor size emulation, see Ralf Browns
             * interrupt list
             */
            regs.h.ah = 0x12;
            regs.h.bl = 0x34;
            regs.h.al = 0x00; /* 0: enable (1: disable) */
            int86(0x10, &regs, &regs);
	}
    }
    if (mode == C4350)
    {
        if (!isEGA())
            return;
        /* load 8x8 font */
        regs.x.ax = 0x1112;         
        regs.x.bx = 0;
        int86(0x10, &regs, &regs);
    }
/*    _setcursortype(_NORMALCURSOR); */
    /* reinitialize txinfo structure to take into account new mode */
    gppconio_init();
#if 0
    /*
     * For mode C4350 the screen is not cleared on my OAK-VGA.
     * Should we clear it here? TURBOC doesn't so we don't bother either.
     */
    clrscr();
#endif
}    
    
void textattr(int attr)
{
  txinfo.attribute = ScreenAttrib = (unsigned char)attr;
}

void textcolor(int color)
{
  /* strip blinking (highest) bit and textcolor */
  ScreenAttrib &= 0x70; /* strip blinking (highest) bit and textcolor */
  txinfo.attribute=(ScreenAttrib |= (color & 0x8f));
}

void textbackground(int color)
{
  /* strip background color, keep blinking bit */
  ScreenAttrib &= 0x8f; 
  /* high intensity background colors (>7) are not allowed 
     so we strip 0x08 bit (and higher bits) of color */
  txinfo.attribute=(ScreenAttrib |= ((color & 0x07) << 4));
}

void highvideo(void)
{
  txinfo.attribute=(ScreenAttrib |= 0x08);
}

void lowvideo(void)
{
  txinfo.attribute=(ScreenAttrib &= 0x07);
}

void normvideo(void)
{
  txinfo.attribute = ScreenAttrib = txinfo.normattr;
}

void _setcursortype(int type)
{
    unsigned cursor_shape;
    switch (type)
    {
        case _NOCURSOR:
            cursor_shape = 0x0700;
            break;
        case _SOLIDCURSOR:
            cursor_shape = 0x0007;
            break;
/*      case _NORMALCURSOR: */
        default:
            cursor_shape = 0x0607;
            break;
    }
    setcursor(cursor_shape);
}        

static void setcursor(unsigned int cursor_shape)
/* Sets the shape of the cursor */
{
    union REGS      reg;

    reg.h.ah = 1;
    reg.x.cx = cursor_shape;
    int86(0x10, &reg, &reg);
}				/* setcursor */

static void getwincursor(int *row, int *col)
{
  ScreenGetCursor(row, col);
}

void clreol(void)
{
  short   image[ 256 ];
  short   val = ' ' | (ScreenAttrib << 8);
  int     c, row, col, ncols;
  
  getwincursor(&row, &col);
  ncols = txinfo.winright - col;
  
  for (c = 0; c < ncols; c++)
    image[ c ] = val;
  
  puttext(col + 1, row + 1, txinfo.winright, row + 1, image);
}

static void fillrow(int row, int left, int right, int fill)
{
  int col;
  short filler[right-left+1];
  
  for (col = left; col <= right; col++)
    filler[col-left] = fill;
  dosmemput(filler, (right-left+1)*2, VIDADDR(row, left));
}

void clrscr(void)
{
  short filler[txinfo.winright - txinfo.winleft + 1];
  int row, col;
  for (col=0; col < txinfo.winright - txinfo.winleft + 1; col++)
    filler[col] = ' ' | (ScreenAttrib << 8);
  for (row=txinfo.wintop-1; row < txinfo.winbottom; row++)
    dosmemput(filler, (txinfo.winright - txinfo.winleft + 1)*2,
     VIDADDR(row, txinfo.winleft - 1));
  gotoxy(1, 1);
}

int putch(int c)
{
  int     row, col;
  
  ScreenGetCursor(&row, &col);
  
  /*  first, handle the character */
  if (c == '\n')
    {
      row++;
    }
  else if (c == '\r')
    {
      col = txinfo.winleft - 1;
    }
  else if (c == '\b')
  {
      if (col > txinfo.winleft - 1)
          col--;  
      else if (row > txinfo.wintop -1)
      {
          /* 
           * Turbo-C ignores this case; we are smarter.
           */
          row--;
          col = txinfo.winright-1;
      }  
  }      
  else if (c == 0x07)
    bell();
  else {
    /* short   val = c | (ScreenAttrib << 8); */
    /* puttext(col + 1, row + 1, col + 1, row + 1, &val); */
    ScreenPutChar(c, ScreenAttrib, col, row);
    col++;
  }
  
  /* now, readjust the window     */
  
  if (col >= txinfo.winright) {
    col = txinfo.winleft - 1;
    row++;
  }
  
  if (row >= txinfo.winbottom) {
    /* scrollwin(0, txinfo.winbottom - txinfo.wintop, 1); */
    if (_wscroll)
    {
      ScreenSetCursor(txinfo.wintop-1,0);
      delline();
    }
    row--;
  }
  
  ScreenSetCursor(row, col);
  txinfo.cury = row - txinfo.wintop + 2;
  txinfo.curx = col - txinfo.winleft + 2;
  return c;
}

int getche(void)
{
  if (char_avail)
    /*
     * We don't know, wether the ungot char was already echoed
     * we assume yes (for example in cscanf, probably the only
     * place where ungetch is ever called.
     * There is no way to check for this really, because
     * ungetch could have been called with a character that
     * hasn't been got by a conio function.
     * We don't echo again.
     */ 
    return(getch());
  return (putch(getch()));
}

int getch(void)
{
    union REGS regs;
    int c;
    if (char_avail)
    {
        c = ungot_char;
        char_avail = 0;
    }
    else
    {
        regs.x.ax = 0x0700;
        int86(0x21, &regs, &regs);
        c = regs.h.al;
    }
    return(c);
}

int ungetch(int c)
{
    if (char_avail)
        return(EOF);
    ungot_char = c;
    char_avail = 1;
    return(c);
}

/* 
 * kbhit from libc in libsrc/c/dos/kbhit.s doesn't check
 * for ungotten chars, so we have to provide a new one
 * Don't call it kbhit, rather use a new name (_conio_kbhit)
 * and do a #define kbhit _conio_kbhit in gppconio.h.
 * The old kbhit still can be used if gppconio.h
 * is not included of after #undef kbhit
 * If you don't use ungetch (directly or indirectly by cscanf)
 * both kbhit and _conio_kbhit are the same.
 * So this shouldn't cause any trouble with previously written
 * source, because ungetch wasn't available.
 * The only problem might be, if anybody just included gppconio.h
 * and has not linked with libpc, (I can't think of a good reason
 * for this). This will result a link error (undefined symbol _conio_kbhit).
 */

#undef kbhit  /* want to be able to call kbhit from libc */

/* The kbhit in libc doesn't work for the second byte of extended chars. */
int kbhit(void)
{
    union REGS regs;
    regs.h.ah = 0x0b;
    int86(0x21, &regs, &regs);
    return regs.h.al ? -1 : 0;
}

int _conio_kbhit(void)
{
    if (char_avail)
        return(1);
    else
        return(kbhit());
}    

/*
 * The next two functions are needed by cscanf
 */
static int _scan_getche(FILE *fp)
{
    return(getche());
}

static int _scan_ungetch(int c, FILE *fp)
{
    return(ungetch(c));
}


void insline(void)
{
  int row, col, left, right, nbytes, bot, fill;
  ScreenGetCursor(&row, &col);
  left = txinfo.winleft - 1;
  right = txinfo.winright - 1;
  nbytes = (right-left+1)*2;
  bot = txinfo.winbottom-1;
  fill = ' ' | (ScreenAttrib << 8);
  while(bot > row)
    {
      movedata(_go32_conventional_mem_selector(), VIDADDR(bot-1, left),
               _go32_conventional_mem_selector(), VIDADDR(bot, left),
               nbytes);
      bot--;
    }
  if (row <= bot)
    {
      fillrow(row,left,right,fill);
    }
}


void delline(void)
{
  int row, col, left, right, nbytes, bot, fill;
  ScreenGetCursor(&row, &col);
  left = txinfo.winleft - 1;
  right = txinfo.winright - 1;
  nbytes = (right-left+1)*2;
  bot = txinfo.winbottom-1;
  fill = ' ' | (ScreenAttrib << 8);
  while(row < bot)
    {
      movedata(_go32_conventional_mem_selector(), VIDADDR(row+1, left),
               _go32_conventional_mem_selector(), VIDADDR(row, left),
               nbytes);
      row++;
    }
  fillrow(bot,left,right,fill);
}


void window(int left, int top, int right, int bottom)
{
  if (top < 1 || left < 1 || right > txinfo.screenwidth ||
      bottom > txinfo.screenheight)
    return;
  
  txinfo.wintop = top;
  txinfo.winleft = left;
  txinfo.winright = right;
  txinfo.winbottom = bottom;
  gotoxy(1,1);
}


int cputs(const char *s)
{
  int     row, col,c;
  const unsigned char *ss = (const unsigned char *)s;
  short *viaddr;
  short sa = ScreenAttrib << 8;
  ScreenGetCursor(&row, &col);
  viaddr = (short *)VIDADDR(row,col);
  /* 
   * Instead of just calling putch; we do everything by hand here,
   * This is much faster. We don't move the cursor after each character,
   * only after the whole string is written, because ScreenSetCursor
   * needs to long because of switching to real mode needed with djgpp. 
   * You won't recognize the difference.
   */
  while ((c = *ss++))
    {
      /*  first, handle the character */
      if (c == '\n')
	{
	  row++;
	  viaddr += txinfo.screenwidth;
	}
      else if (c == '\r')
	{
	  col = txinfo.winleft - 1;
	  viaddr = (short *)VIDADDR(row,col);
	}
      else if (c == '\b')
        {
          if (col > txinfo.winleft-1) 
          {
              col--;
              viaddr--;
          }
          else if (row > txinfo.wintop -1)
          {
              /* 
               * Turbo-C ignores this case. We want to be able to
               * edit strings with backspace in gets after
               * a linefeed, so we are smarter
               */
              row--;
              col = txinfo.winright-1;
              viaddr = (short *)VIDADDR(row,col);
          }
        }
      else if (c == 0x07)
          bell();
      else {
        short q = c | sa;
        dosmemput(&q, 2, (int)viaddr);
	viaddr++;
	col++;
      }
      
      /* now, readjust the window     */
      
      if (col >= txinfo.winright) {
	col = txinfo.winleft - 1;
	row++;
	viaddr = (short *)VIDADDR(row,col);
      }
      
      if (row >= txinfo.winbottom) {
	ScreenSetCursor(txinfo.wintop-1,0); /* goto first line in window */
	delline();                          /* and delete it */
	row--;
	viaddr -= txinfo.screenwidth;
      }
    }
  
  ScreenSetCursor(row, col);
  txinfo.cury = row - txinfo.wintop + 2;
  txinfo.curx = col - txinfo.winleft + 2;
  return(*(--ss));
}


int cprintf(const char *fmt, ...)
{
  int     cnt;
  char    buf[ 2048 ]; /* this is buggy, because buffer might be too small. */
  va_list ap;
  
  va_start(ap, fmt);
  cnt = vsprintf(buf, fmt, ap);
  va_end(ap);
  
  cputs(buf);
  return cnt;
}

char *cgets(char *string)
{
    unsigned len = 0;
    unsigned int maxlen_wanted;
    char *sp;
    int c;
    /*
     * Be smart and check for NULL pointer.
     * Don't know wether TURBOC does this.
     */
    if (!string)
        return(NULL);
    maxlen_wanted = (unsigned int)((unsigned char)string[0]);
    sp = &(string[2]);
    /* 
     * Should the string be shorter maxlen_wanted including or excluding
     * the trailing '\0' ? We don't take any risk.
     */
    while(len < maxlen_wanted-1)
    {
        c=getch();
        /*
         * shold we check for backspace here?
         * TURBOC does (just checked) but doesn't in cscanf (thats harder
         * or even impossible). We do the same.
         */
        if (c == '\b')
        {
            if (len > 0)
            {
               cputs("\b \b"); /* go back, clear char on screen with space
                                  and go back again */
               len--;
               sp[len] = '\0'; /* clear the character in the string */
            }
        }
        else if (c == '\r')
        {
            sp[len] = '\0';
            break;
        }
        else if (c == 0)
        {
            /* special character ends input */
            sp[len] = '\0';
            ungetch(c); /* keep the char for later processing */
            break;
        }
        else
        {
           sp[len] = putch(c);
           len++;
        }
     }
     sp[maxlen_wanted-1] = '\0';
     string[1] = (char)((unsigned char)len);
     return(sp);   
}    

int cscanf(const char *fmt, ...)
{
    return(_doscan_low(NULL, _scan_getche, _scan_ungetch, 
                       fmt, (void **)((&fmt)+1)));
}

int movetext(int left, int top, int right, int bottom, int dleft, int dtop)
{
  char    *buf = malloc((right - left + 1) * (bottom - top + 1) * 2);
  
  if (!buf)
    return 0;
  
  gettext(left, top, right, bottom, buf);
  puttext(dleft, dtop, dleft + right - left, dtop + bottom - top, buf);
  free(buf);
  return 1;
}

static void _gettextinfo(struct text_info *t)
{
  int row, col;
  
  t->winleft = t->wintop = 1;
  t->winright = t->screenwidth = ScreenCols();
  t->winbottom = t->screenheight = ScreenRows();
  ScreenAttrib = t->attribute = t->normattr = get_screenattrib();
  t->currmode = getvideomode();
  ScreenGetCursor(&row, &col);
  t->curx = col+1;
  t->cury = row+1;
#if DBGGTINFO
  printf("left=%2d,right=%2d,top=%2d,bottom=%2d\n",t->winleft,
	 t->winright,t->wintop,t->winbottom);
  printf("scrht=%2d,scrwid=%2d,norm=%2x,mode=%2d,x=%2d,y=%2d\n",
	 t->screenheight, t->screenwidth, t->normattr, t->currmode,
	 t->curx, t->cury);
#endif
}

void gettextinfo(struct text_info *t)
{
  *t = txinfo; 
#if DBGGTINFO
  printf("left=%2d,right=%2d,top=%2d,bottom=%2d\n",t->winleft,
	 t->winright,t->wintop,t->winbottom);
  printf("scrht=%2d,scrwid=%2d,norm=%2x,mode=%2d,x=%2d,y=%2d\n",
	 t->screenheight, t->screenwidth, t->normattr, t->currmode,
	 t->curx, t->cury);
#endif
}

static int
getvideomode(void)
{
    int mode = ScreenMode();
    /* 
     * in mode C80 we might have loaded a different font
     */
    if (mode == C80)
        if (ScreenRows() > 25)
           mode = C4350;
    return(mode);
}
    

static void bell(void)
{
    union REGS regs;
#if 0
    /* use BIOS */
    regs.h.ah = 0x0e; /* write */
    regs.h.al = 0x07; /* bell */
    int86(0x10, &regs, &regs);
#else
    /* use DOS */
    regs.h.ah = 0x06; /* write */
    regs.h.dl = 0x07; /* bell */
    int86(0x21, &regs, &regs);
#endif
}

static int 
get_screenattrib(void)
{
    union REGS regs;
    regs.h.ah = 0x08; /* read character and attribute */
    regs.h.bh = 0;    /* video page 0 */
    int86(0x10, &regs, &regs);
    return(regs.h.ah & 0x7f); /* strip highest (BLINK) bit */
}

/* check if we have at least EGA (idea form Ralf Browns interrupt list) */
static int
isEGA(void)
{
    union REGS regs;
    regs.h.ah = 0x12;
    regs.h.bl = 0x10;
    regs.h.bh = 0xff;
    int86(0x10, &regs, &regs);
    return(regs.h.bh != 0xff);
}


extern int _gppconio_init;

void gppconio_init(void)
{
    static int oldattrib =  -1;
    if (oldattrib == -1)
        oldattrib = get_screenattrib();   
    _gettextinfo(&txinfo);
    if (txinfo.currmode == 7)    /* MONO */
        ScreenAddress = 0xb0000UL;
    else
	ScreenAddress = 0xb8000UL;
    ScreenAttrib = txinfo.normattr = txinfo.attribute = oldattrib;
    _gppconio_init = 1;
}

#endif /* __DJGPP__ < 2 */
