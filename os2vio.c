/*
 * OS/2 VIO (character-mode) console routines.
 * Modified from a really old version of "borland.c" (before the VIO
 * stuff went in there.)
 *
 * $Header: /users/source/archives/vile.vcs/RCS/os2vio.c,v 1.9 1997/03/15 13:31:02 tom Exp $
 */

#include "estruct.h"
#include "edef.h"

#if !SYS_OS2
#error Configuration error:  OS/2 is required for VIO support.
#endif

#define INCL_VIO
#define INCL_DOSPROCESS
#define INCL_NOPMAPI
#include <os2.h>

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

#define	AttrColor(b, f)	(((ctrans[b] & 15) << 4) | (ctrans[f] & 15))

static	void	vio_move   (int,int);
static	void	vio_eeol   (void);
static	void	vio_eeop   (void);
static	void	vio_beep   (void);
static	void    vio_open   (void);
#if OPT_VIDEO_ATTRS
static	void	vio_attr   (int);
#else
static	void	vio_rev    (int);
#endif	/* OPT_VIDEO_ATTRS */
static	int	vio_cres   (char *);
static	void	vio_close  (void);
static	void	vio_putc   (int);
static	void	vio_kopen  (void);
static	void	vio_kclose (void);
static	void	vio_flush  (void);

#if OPT_COLOR
static	void	vio_fcol   (int);
static	void	vio_bcol   (int);
static	void	vio_spal   (char *);
#else
#define	vio_fcol   null_t_setfor
#define	vio_bcol   null_t_setback
#define	vio_spal   null_t_setpal
#endif

#if	SCROLLCODE
static	void	vio_scroll (int,int,int);
#else
#define	vio_scroll null_t_scroll
#endif

static	int	scinit     (int);

int	cfcolor = -1;		/* current foreground color */
int	cbcolor = -1;		/* current background color */

/* ANSI to IBM color translation table: */
int	ctrans[NCOLORS] = 
{
	0,		/* black */
	4,		/* red */
	2,		/* green */
	14,		/* yellow */
	1,		/* blue */
	5,		/* magenta */
	3,		/* cyan */
	7		/* white */
};

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
	ttgetc,
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
	vio_spal,
	vio_scroll,
	null_t_pflush,
	null_t_icursor,
	null_t_title,
};

/* Extended key prefix macros. */
#define	KE0(code)		{ 0xe0, code }
#define	K00(code)		{ 0x00, code }

static struct
{
	char seq[2];
	int code;
}
VIO_KeyMap[] =
{
	{ KE0('H'), KEY_Up },
	{ KE0('P'), KEY_Down },
	{ KE0('K'), KEY_Left },
	{ KE0('M'), KEY_Right },
	{ KE0('R'), KEY_Insert },
	{ KE0('S'), KEY_Delete },
	{ KE0('G'), KEY_Home },
	{ KE0('O'), KEY_End },
	{ KE0('I'), KEY_Prior },
	{ KE0('Q'), KEY_Next },

	/*
	 * Unshifted function keys.  The VIO console driver generates
	 * different scan codes when these keys are pressed with Shift,
	 * Ctrl, and Alt; those codes are presently unsupported.
	 */
	{ K00(';'), KEY_F1 },
	{ K00('<'), KEY_F2 },
	{ K00('='), KEY_F3 },
	{ K00('>'), KEY_F4 },
	{ K00('?'), KEY_F5 },
	{ K00('@'), KEY_F6 },
	{ K00('A'), KEY_F7 },
	{ K00('B'), KEY_F8 },
	{ K00('C'), KEY_F9 },
	{ K00('D'), KEY_F10 },
	{ K00(133), KEY_F11 },
	{ K00(134), KEY_F12 },

	/* Keypad codes (with Num Lock off): */
	{ K00('G'), KEY_Home },
	{ K00('H'), KEY_Up },
	{ K00('I'), KEY_Prior },
	{ K00('K'), KEY_Left },
	{ K00('L'), KEY_Select },
	{ K00('M'), KEY_Right },
	{ K00('O'), KEY_End },
	{ K00('P'), KEY_Down },
	{ K00('Q'), KEY_Next },
	{ K00('R'), KEY_Insert },
	{ K00('S'), KEY_Delete }
};

#define	TEXT_BUFFER_SIZE		256

static char  TextBuf[TEXT_BUFFER_SIZE];
static int   TextFree;
static char *TextOut;
static int   TextRow, TextColumn;
static BYTE  TextAttr;
static int   MaxRows, MaxColumns;
static int   CursorRow, CursorColumn;
static BYTE  BlankCell[2] = { SPACE, 0 };

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

/*
 * Reset the palette registers.
 */
void
vio_spal(char *thePalette)
{
	/* this is pretty simplistic.  big deal. */
	(void)sscanf(thePalette, "%i %i %i %i %i %i %i %i",
	    	&ctrans[0], &ctrans[1], &ctrans[2], &ctrans[3],
	    	&ctrans[4], &ctrans[5], &ctrans[6], &ctrans[7]);
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
vio_attr(int attr)
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
	if (attr & VAREV)
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
vio_rev(int reverse)
{
	flush_if_necessary();

	if (reverse)
		TextAttr = AttrColor(cfcolor, cbcolor);
	else
		TextAttr = AttrColor(cbcolor, cfcolor);
}
#endif	/* OPT_VIDEO_ATTRS */

static int
vio_cres(char *res)	/* change screen resolution */
			/* resolution to change to */
{
	return scinit(-1);
}

static void
vio_beep()
{
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
	return;
}

static void
vio_kclose()	/* close the keyboard */
{
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

#endif /* DISP_VIO */
