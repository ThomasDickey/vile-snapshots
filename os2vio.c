/*
 * OS/2 VIO (character-mode) console routines.
 * Modified from a really old version of "borland.c" (before the VIO
 * stuff went in there.)
 *
 * $Header: /users/source/archives/vile.vcs/RCS/os2vio.c,v 1.21 1998/11/11 21:56:30 tom Exp $
 */

#include "estruct.h"
#include "edef.h"

#if !SYS_OS2
#error Configuration error:  OS/2 is required for VIO support.
#endif

#define INCL_WIN
#define INCL_BASE
#define INCL_MOU
#define INCL_VIO
#define INCL_DOSPROCESS
#define INCL_NOPMAPI
#include <os2.h>

#include <conio.h>

#if DISP_VIO

#define SCROLLCODE        1

/* The code to control the cursor shape isn't complete.  No big loss. */
#define CURSOR_SHAPE 0

#define NROW	60			/* Max Screen size.		*/
#define NCOL    80			/* Edit if you want to.         */
#define	MARGIN	8			/* size of minimum margin and	*/
#define	SCRSIZ	64			/* scroll size for extended lines */
#define	NPAUSE	200			/* # times thru update to pause */
#define	SPACE	32			/* space character		*/

#define	AttrColor(b, f)	((((UINT)ctrans[b] & 15) << 4) | ((UINT)ctrans[f] & 15))

static	void	vio_move   (int,int);
static	void	vio_eeol   (void);
static	void	vio_eeop   (void);
static	void	vio_beep   (void);
static	void    vio_open   (void);
#if OPT_VIDEO_ATTRS
static	void	vio_attr   (UINT);
#else
static	void	vio_rev    (UINT);
#endif	/* OPT_VIDEO_ATTRS */
static	int	vio_cres   (const char *);
static	void	vio_close  (void);
static	int	vio_getc   (void);
static	void	vio_putc   (int);
static	void	vio_kopen  (void);
static	void	vio_kclose (void);
static	void	vio_flush  (void);

#if OPT_COLOR
static	void	vio_fcol   (int);
static	void	vio_bcol   (int);
#define vio_spal   set_ctrans
#else
#define	vio_fcol   null_t_setfor
#define	vio_bcol   null_t_setback
#define vio_spal   null_t_setpal
#endif

#if	SCROLLCODE
static	void	vio_scroll (int,int,int);
#else
#define	vio_scroll null_t_scroll
#endif

#if	OPT_TITLE
static	void	vio_title (char *);
#endif

static	int	scinit     (int);

int	cfcolor = -1;		/* current foreground color */
int	cbcolor = -1;		/* current background color */

static	const char *initpalettestr = "0 4 2 6 1 5 3 7 8 12 10 14 9 13 11 15";

/*
 * Standard terminal interface dispatch table. Most of the fields point into
 * "termio" code.
 */
TERM term = {
	NROW - 1,
	NROW - 1,
	NCOL,
	NCOL,
	MARGIN,
	SCRSIZ,
	NPAUSE,
	vio_open,
	vio_close,
	vio_kopen,
	vio_kclose,
	vio_getc,
	vio_putc,
	tttypahead,
	vio_flush,
	vio_move,
	vio_eeol,
	vio_eeop,
	vio_beep,
#if OPT_VIDEO_ATTRS
	vio_attr,
#else
	vio_rev,
#endif	/* OPT_VIDEO_ATTRS */
	vio_cres,
	vio_fcol,
	vio_bcol,
	set_ctrans,
	vio_scroll,
	null_t_pflush,
	null_t_icursor,
#if OPT_TITLE
	vio_title,
#else
	null_t_title,
#endif
	null_t_watchfd,
	null_t_unwatchfd,
	null_t_cursor,
};

#include "os2keys.h"

#define	TEXT_BUFFER_SIZE		256

static char  TextBuf[TEXT_BUFFER_SIZE];
static int   TextFree;
static char *TextOut;
static int   TextRow, TextColumn;
static BYTE  TextAttr;
static int   MaxRows, MaxColumns;
static int   CursorRow, CursorColumn;
static BYTE  BlankCell[2] = { SPACE, 0 };
static HMOU  hmou;

#define blank_cell()	( BlankCell[1] = TextAttr, BlankCell )

#define flush_if_necessary() \
	{ if (TextOut > TextBuf) vio_flush(); }

#if CURSOR_SHAPE
static void
set_cursor(int cmode)
{
	VIOCURSORINFO cinfo;

	cinfo.cx = 0;			/* Width of character cell */
	cinfo.cEnd = -100;		/* Cursor ends at bottom of cell */
	cinfo.attr = 0;			/* Visible cursor */

	switch (cmode) {
	case 0:
		/* Hidden cursor. */
		cinfo.attr = -1;
		break;

	case 1:
		/* Block cursor. */
		cinfo.yStart = 0;
		break;

	case 2:
		/* 'Normal' cursor. */
		cinfo.yStart = -75;
		break;
	}

	(void) VioSetCurType(&cinfo, 0);
}
#endif /* CURSOR_SHAPE */

#if OPT_COLOR
/*
 * Set the current foreground colour.  'color' specifies an ANSI colour
 * index.
 */
static void
vio_fcol(int color)
{
	/* Flush any old text that needs to be written using the old colour. */
	flush_if_necessary();

	cfcolor  = color;
	TextAttr = AttrColor(cbcolor, cfcolor);
}

/*
 * Set the current background colour.  'color' specifies an ANSI colour
 * index.
 */
static void
vio_bcol(int color)
{
	/* Flush any old text that needs to be written using the old colour. */
	flush_if_necessary();

	cbcolor  = color;
	TextAttr = AttrColor(cbcolor, cfcolor);
}
#endif

static void
vio_move(int row, int col)
{
	flush_if_necessary();

	TextRow    = row;
	TextColumn = col;
}

/* erase to the end of the line */
static void
vio_eeol(void)
{
	int length;

	flush_if_necessary();

	if ((length = MaxColumns - TextColumn) > 0)
	{
		(void) VioWrtNCell(blank_cell(), length, TextRow, TextColumn, 0);
	}
}

static int
decode_mouse_state(USHORT state)
{
	int button = 0;
	if (state & MOUSE_MOTION_WITH_BN1_DOWN
	 || state & MOUSE_BN1_DOWN) {
		button = 1;
	}
	if (state & MOUSE_MOTION_WITH_BN2_DOWN
	 || state & MOUSE_BN2_DOWN) {
		button = 2;
	}
	if (state & MOUSE_MOTION_WITH_BN3_DOWN
	 || state & MOUSE_BN3_DOWN) {
		button = 3;
	}
	return button;
}

/* If we have a mouse event, process it.  Otherwise, read a character. */
static int
vio_getc(void)
{
	MOUEVENTINFO mouev;
	USHORT	nowait = MOU_NOWAIT;
	USHORT	rc;
	int	button = 0;

	do {
		rc = MouReadEventQue(&mouev, &nowait, hmou);
		if (rc == 0 && mouev.fs != 0) {
			if (button) {
				int last = button;
				button = decode_mouse_state(mouev.fs);
				if (button) {
					if (last == button) {
						if (!setcursor(mouev.row, mouev.col))
							continue;
						if (!sel_extend(TRUE, TRUE))
							continue;
					} else {
						sel_release();
					}
					(void)update(TRUE);
				} else {
					sel_yank(0);
				}
			} else {
				button = decode_mouse_state(mouev.fs);
				if (button == 1) {
					if (!setcursor(mouev.row, mouev.col))
						continue;
					(void)sel_begin();
					(void)update(TRUE);
				}
			}
		}
	} while (!kbhit());

	return getch();
}

/* put a character at the current position in the current colors */
static void
vio_putc(int ch)
{
	BYTE b;

	if (TextFree <= 0)
		vio_flush();

	/*
	 * Control character kludges.  Sigh.
	 */
	switch (ch)
	{
		case '\b':
			flush_if_necessary();
			TextColumn--;
			break;

		case '\n':
		case '\r':
			flush_if_necessary();
			b = ch;
			VioWrtTTY(&b, 1, 0);
			break;

		default:
			*TextOut++ = ch;
			TextFree--;
			break;
	}
}

static void
vio_flush(void)
{
	int length = TextOut - TextBuf;

	if (length > 0)
	{
		(void) VioWrtCharStrAtt(TextBuf, length, TextRow, TextColumn,
			&TextAttr, 0);
		TextColumn += length;
	}
	TextOut  = TextBuf;
	TextFree = TEXT_BUFFER_SIZE;

	/* Make the cursor 'catch up', if necessary. */
	if (CursorColumn != TextColumn || CursorRow != TextRow)
	{
		(void) VioSetCurPos(TextRow, TextColumn, 0);
		CursorRow = TextRow;
		CursorColumn = TextColumn;
	}
}

static void
vio_eeop(void)
{
	int length;

	flush_if_necessary();

	length = (MaxRows * MaxColumns)
		   - (TextRow * MaxColumns + TextColumn);
	if (length > 0)
		(void) VioWrtNCell(blank_cell(), length, TextRow, TextColumn, 0);
}

#if OPT_VIDEO_ATTRS
static void
vio_attr(UINT attr)
{
	attr = VATTRIB(attr);
	attr &= ~(VAML|VAMLFOC);

	flush_if_necessary();

#if OPT_COLOR
	if (attr & VACOLOR)
		vio_fcol(VCOLORNUM(attr));
	else
		vio_fcol(gfcolor);
#endif
	if (attr & (VAREV|VASEL))
		TextAttr = AttrColor(cfcolor, cbcolor);
	else
		TextAttr = AttrColor(cbcolor, cfcolor);

	if (attr & VABOLD)
		TextAttr |= 0x8;
}

#else	/* highlighting is a minimum attribute */

/*
 * Choose the text attributes for reverse or normal video.  Reverse video
 * is selected if 'reverse' is TRUE, and normal video otherwise.
 */
static void
vio_rev(UINT reverse)
{
	flush_if_necessary();

	if (reverse)
		TextAttr = AttrColor(cfcolor, cbcolor);
	else
		TextAttr = AttrColor(cbcolor, cfcolor);
}
#endif	/* OPT_VIDEO_ATTRS */

static int
vio_cres(const char *res) /* change screen resolution */
			/* resolution to change to */
{
	return scinit(-1);
}

#if	OPT_FLASH
static void
flash_display(void)
{
	VIOMODEINFO data;
	UINT cellsize;
	BYTE *buf1;
	BYTE *buf2;
	USHORT got, length;

	data.cb = sizeof(data);
	VioGetMode(&data, 0);
	cellsize = 1 + data.attrib;

	buf1 = malloc(data.full_length);
	buf2 = malloc(data.full_length);

	length = data.full_length;
	VioReadCellStr(buf1, &length, 0, 0, 0);

	length = data.full_length;
	VioReadCellStr(buf2, &length, 0, 0, 0);

	for (got = 0; got < data.full_length; got += cellsize) {
		buf2[got + 1] ^= 8;
	}
	VioWrtCellStr(buf2, data.full_length, 0, 0, 0);
	DosSleep(200);
	VioWrtCellStr(buf1, data.full_length, 0, 0, 0);

	free(buf1);
	free(buf2);
}
#endif

static void
vio_beep()
{
#if	OPT_FLASH
	if (global_g_val(GMDFLASH)) {
		flash_display();
		return;
	}
#endif
	/* A nice, brief beep. */
	DosBeep(440, 50);
}

static void
vio_open(void)
{
	int i;

	/* Initialize output buffer. */
	TextRow = 0;
	TextColumn = 0;
	TextOut = TextBuf;
	TextFree = TEXT_BUFFER_SIZE;

	for (i = 0; i < sizeof(VIO_KeyMap) / sizeof(VIO_KeyMap[0]); i++)
	{
		addtosysmap(VIO_KeyMap[i].seq, 2, VIO_KeyMap[i].code);
	}

#if OPT_COLOR
	{
		VIOINTENSITY intense;
		intense.cb   = sizeof(intense);
		intense.type = 2;
		intense.fs   = 1;	/* ask for bright colors, not blink */
		VioSetState(&intense, 0);
	}
	set_palette(initpalettestr);
	vio_fcol(gfcolor);
	vio_bcol(gbcolor);
#endif

#if CURSOR_SHAPE
	set_cursor(1);
#endif

	if (!vio_cres(current_res_name))
		(void) scinit(-1);
}

static void
vio_close(void)
{
#if CURSOR_SHAPE
	set_cursor(2);
#endif
	vio_move(MaxRows-1, 0);
	TTeeop();
}

static void
vio_kopen()	/* open the keyboard */
{
	MouOpen(NULL, &hmou);
	MouDrawPtr(hmou);
	return;
}

static void
vio_kclose()	/* close the keyboard */
{
	MouClose(hmou);
	return;
}

static int
scinit(int rows)	/* initialize the screen head pointers */
{
	VIOMODEINFO vinfo;

	vinfo.cb = sizeof(vinfo);
	(void) VioGetMode(&vinfo, 0);

	MaxRows = vinfo.row;
	MaxColumns = vinfo.col;
	newscreensize(MaxRows, MaxColumns);

	return TRUE;
}

#if SCROLLCODE
/*
 * Move 'n' lines starting at 'from' to 'to'
 *
 * OPT_PRETTIER_SCROLL is prettier but slower -- it scrolls a line at a time
 * instead of all at once.
 */

static void
vio_scroll(int from, int to, int n)
{
	/* Ignore single-line scrolling regions. */
	if (to == from)
		return;

#if OPT_PRETTIER_SCROLL
	/* Easier for the eye to follow, but slower. */
	if (absol(from-to) > 1) {
		vio_scroll(from, (from < to) ? to - 1 : to + 1, n);
		if (from < to)
			from = to-1;
		else
			from = to+1;
	}
#endif

	if (to < from)
	{
		(void) VioScrollUp(to, 0, from + n - 1, MaxColumns - 1,
			from - to, blank_cell(), 0);
/*
		vio_move(to + n, 0);
*/
	}
	else
	{
		(void) VioScrollDn(from, 0, to + n - 1, MaxColumns - 1,
			to - from, blank_cell(), 0);
/*
		vio_move(to + n, 0);
*/
	}
}
#endif	/* SCROLLCODE */

#if OPT_TITLE
/*
 * This sets the title in the window-list, but doesn't work for the
 * actual window title.  It's adapted from a standalone code example
 * that uses
 *	WinSetWindowText(hwndActive, swcntrl.szSwtitle);
 * before
 *	WinChangeSwitchEntry(hSwitch, &swcntrl);
 *
 * The standalone example is linked for PM versus this program's VIO mode.
 */
static void
vio_title(char *title)		/* set the current window title */
{
      HAB  hab = WinInitialize( 0 );
      HMQ  hmq = WinCreateMsgQueue( hab, 0 );
      HWND hwndActive = WinQueryActiveWindow(HWND_DESKTOP);
      HWND hwndFrame = WinQueryWindow(hwndActive, QW_PARENT);
      char temp[24];
      BOOL ok;
      LONG nn;

      WinQuerySessionTitle(hab, 0, temp, sizeof(temp));

      if (strlen(title))
      {
         HSWITCH hSwitch;
         SWCNTRL swcntrl;

         if (hSwitch = WinQuerySwitchHandle(hwndActive, (PID)0L))
         {
            if (!WinQuerySwitchEntry(hSwitch, &swcntrl))
            {
               strncpy(swcntrl.szSwtitle, title, MAXNAMEL);
               swcntrl.szSwtitle[MAXNAMEL] = '\0';
#if 0
               WinSetWindowText(hwndActive, swcntrl.szSwtitle);
#endif
               WinChangeSwitchEntry(hSwitch, &swcntrl);
            }
         }
      }

      WinDestroyMsgQueue( hmq );
      WinTerminate( hab );
}
#endif /* OPT_TITLE */

#endif /* DISP_VIO */
