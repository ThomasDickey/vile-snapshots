/*
 * The routines in this file provide support for the IBM-PC family
 * of machines, and their typical display cards:  mono, cga, ega, vga.
 * It goes directly to the graphics RAM to do screen output.
 *
 * Modified by Pete Ruczynski (pjr) for auto-sensing and selection of
 * display type.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/ibmpc.c,v 1.93 1999/09/19 19:41:25 tom Exp $
 *
 */

#define	termdef	1			/* don't define "term" external */

#include	"estruct.h"
#include	"edef.h"

#if DISP_BORLAND || !DISP_IBMPC
#error misconfigured:  DISP_IBMPC should be defined if using ibmpc.c
#error (and DISP_BORLAND should not be defined)
#endif

#if CC_DJGPP
#include <pc.h>
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define outp(p,v) outportb(p,v)
#define inp(p) inportb(p)
#define far
#include <io.h> /* for setmode() */
#include <fcntl.h> /* for O_BINARY */
#include <dpmi.h> /* for the register struct */
#include <go32.h>
#endif

#define NROW	50			/* Max Screen size.		*/
#define NCOL	80			/* Edit if you want to. 	*/
#define	NPAUSE	200			/* # times thru update to pause */
#define	SPACE	32			/* space character		*/

#if CC_WATCOM
#define	MONOADDR	(0xb000 << 4)	/* MONO screen memory */
#define	OTHERADDR	(0xb800 << 4)	/* CGA,EGA,VGA screen memory */
#endif

#if CC_DJGPP
#define FAR_POINTER(s,o) (0xe0000000 + s*16 + o)
#define FP_SEG(a)	((unsigned long)(a) >> 4L)
#define FP_OFF(a)	((unsigned long)(a) & 0x0fL)
#define	MONOADDR	0xb000		/* MONO screen memory */
#define	OTHERADDR	0xb800		/* CGA,EGA,VGA screen memory */
#endif

#ifndef OTHERADDR
#define	MONOADDR	0xb0000000L	/* MONO screen memory */
#define	OTHERADDR	0xb8000000L	/* CGA,EGA,VGA screen memory */
#endif

#ifndef FAR_POINTER
#define FAR_POINTER(s,o) (s)
#endif

#define	MONOSCR	0
#define	CGASCR	1
#define	EGASCR	2
#define	VGASCR	3

static	const	long	ScreenAddress[] = {
		MONOADDR,	/* MONOSCR: Monochrome adapter */
		OTHERADDR,	/* CGASCR: Color graphics adapter */
		OTHERADDR,	/* EGASCR: Enhanced graphics adapter */
		OTHERADDR	/* VGASCR: VGA adapter */
	};


#if CC_WATCOM
#ifdef __386__
#define	INTX86(a,b,c)		int386(a, b, c)
#define	INTX86X(a,b,c,d)	int386x(a, b, c, d)
#else
#define	INTX86(a,b,c)		int86(a, b, c)
#define	INTX86X(a,b,c,d)	int86x(a, b, c, d)
#endif
#define	_AX_		eax
#define	_BX_		ebx
#define	_CX_		ecx
#define	_DX_		edx
#define	_DI_		edi
#else
#define	INTX86(a,b,c)	int86(a, b, c)
#define	INTX86X(a,b,c,d)	int86x(a, b, c, d)
#define	_AX_		ax
#define	_BX_		bx
#define	_CX_		cx
#define	_DX_		dx
#define	_DI_		di
#endif

#define	ColorDisplay()	(dtype != MONOSCR && !monochrome)
#define	AttrColor(b,f)	(((ctrans[b] & 7) << 4) | (ctrans[f] & 15))
#define	Black(n)	((n) ? 0 : 7)
#define	White(n)	((n) ? 7 : 0)
#define	AttrMono(f)	(((Black(f) & 7) << 4) | (White(f) & 15))

#if OPT_MS_MOUSE
	static	int	ms_crsr2inx  ( int, int );
	static	void	ms_deinstall (void);
	static	void	ms_hidecrsr  (void);
	static	void	ms_install   (void);
	static	void	ms_setvrange ( int, int );
	static	void	ms_showcrsr  (void);
#else
# define ms_deinstall()
# define ms_install()
#endif

static	int	dtype = -1;	/* current display type		*/
#if CC_DJGPP
#define PACKED __attribute__ ((packed))
#else
#define PACKED
#endif

	/* mode-independent VGA-BIOS status information */
typedef	struct {
	UCHAR	video_modes[3] PACKED;	/* 00 Supported video-modes */
	UCHAR	reserved[4] PACKED;	/* 03 */
	UCHAR	text_scanlines PACKED;	/* 07 Number of pixel rows in text mode */
	UCHAR	num_charsets PACKED;	/* 08 Number of character sets that can be displayed */
	UCHAR	num_loadable PACKED;	/* 09 Number of character sets loadable into video RAM */
	UCHAR	capability PACKED;	/* 0A VGA-BIOS capability info */
	UCHAR	more_info PACKED;	/* 0B More VGA-BIOS capability info */
	UCHAR	reserved2[4] PACKED;	/* 0C */
	} static_VGA_info PACKED;

	/* mode-dependent VGA-BIOS status information */
typedef	struct {
	static_VGA_info *static_info PACKED; /* 00 Address of table containing static info */
	UCHAR	code_number PACKED;	/* 04 Code number of current video mode */
	USHORT	num_columns PACKED;	/* 05 Number of displayed screen or pixel cols */
	USHORT	page_length PACKED;	/* 07 Length of display page in video RAM */
	USHORT	curr_page PACKED;	/* 09 Starting address of current display-page in video RAM */
	UCHAR	crsr_pos[8][2] PACKED;	/* 0B Cursor positions in display-pages in col/row order */
	UCHAR	crsr_end PACKED;	/* 1B Ending row of cursor (pixel) row */
	UCHAR	crsr_start PACKED;	/* 1C Starting row of cursor (pixel) row */
	UCHAR	curr_page_num PACKED;	/* 1D Number of current display page */
	USHORT	port_crt PACKED;	/* 1E Port address of the CRT controller address register */
	UCHAR	curr_crtc PACKED;	/* 20 Current contents of CRTC control registers */
	UCHAR	curr_color_sel PACKED;	/* 21 Current color selection register contents */
	UCHAR	num_rows PACKED;	/* 22 Number of screen rows displayed */
	USHORT	char_height PACKED;	/* 23 Height of characters in pixel rows */
	UCHAR	code_active PACKED;	/* 25 Code number of active video adapter */
	UCHAR	code_inactive PACKED;	/* 26 Code number of inactive video adapter */
	USHORT	num_colors PACKED;	/* 27 Number of displayable colors (0=monochrome) */
	UCHAR	num_pages PACKED;	/* 29 Number of screen pages */
	UCHAR	num_pixel_rows PACKED;	/* 2A Number of displayed pixel rows (RES_200, etc.) */
	UCHAR	num_cset0 PACKED;	/* 2B Number of char-table used with chars whose 3rd attr bit is 0 */
	UCHAR	num_cset1 PACKED;	/* 2C Number of char-table used with chars whose 3rd attr bit is 1 */
	UCHAR	misc_info PACKED;	/* 2D Miscellaneous information */
	UCHAR	reserved[3] PACKED;	/* 2E */
	UCHAR	size_of_ram PACKED;	/* 31 Size of available video RAM (0=64k, 1=128k, 2=192k, 3=256k) */
	UCHAR	reserved2[14] PACKED;	/* 32 */
	} dynamic_VGA_info PACKED;	/* length == 64 bytes */

	/* scan-line resolution codes */
#define	RES_200	0
#define	RES_350	1
#define	RES_400	2
#define RES_480 3

	/* character-size codes */
#define	C8x8	0x12
#define	C8x14	0x11
#define	C8x16	0x14

	/* character-size in pixels, for mouse-positioning */
static	int	chr_wide = 8;
static	int	chr_high = 8;

typedef	struct	{
	char	*name;
	UCHAR	type;
	UCHAR	mode;
	UCHAR	vchr;	/* code for setting character-height */
	UCHAR	rows;
	UCHAR	cols;
	UCHAR	vres;	/* required scan-lines, RES_200, ... */
	} DRIVERS;

#define ORIGTYPE  0	/* store original info in this slot.
			   (these values should all (?) get replaced
			   at open time) */

/* order this table so that more higher resolution entries for the same
 *  name come first.  remember -- synonyms take only 10 bytes, plus the
 *  name itself.
 */
static	DRIVERS drivers[] = {
		{"default",ORIGTYPE,    3,      C8x8,   25,  80, RES_200},
		{"2",      VGASCR,	3,	C8x16,	25,  80, RES_400},
		{"25",     VGASCR,	3,	C8x16,	25,  80, RES_400},
		{"2",      CGASCR,	3,	C8x8,	25,  80, RES_200},
		{"25",     CGASCR,	3,	C8x8,	25,  80, RES_200},
		{"CGA",    CGASCR,	3,	C8x8,	25,  80, RES_200},
		{"MONO",   MONOSCR,	3,	C8x8,	25,  80, RES_200},
		{"80x25",  VGASCR,	3,	C8x16,	25,  80, RES_400},
		{"80x28",  VGASCR,	3,	C8x14,  28,  80, RES_400},
		{"EGA",    EGASCR,	3,	C8x8,	43,  80, RES_350},
		{"4",      EGASCR,	3,	C8x8,	43,  80, RES_350},
		{"43",     EGASCR,	3,	C8x8,	43,  80, RES_350},
		{"80x43",  VGASCR,	3,	C8x8,   43,  80, RES_350},
		{"5",      VGASCR,	3,	C8x8,	50,  80, RES_400},
		{"50",     VGASCR,	3,	C8x8,	50,  80, RES_400},
		{"VGA",    VGASCR,	3,	C8x8,	50,  80, RES_400},
		{"80x50",  VGASCR,	3,	C8x8,	50,  80, RES_400},
		{"80x14",  VGASCR,	3,	C8x14,  14,  80, RES_200},
		{"40x12",  VGASCR,	1,	C8x16,	12,  40, RES_200},
		{"40x21",  VGASCR,	1,	C8x16,	21,  40, RES_350},
		{"40x25",  VGASCR,	1,	C8x16,	25,  40, RES_400},
		{"40x28",  VGASCR,	1,	C8x14,  28,  40, RES_400},
		{"40x50",  VGASCR,	1,	C8x8,   50,  40, RES_400},

	};

/*  the following should be sized dynamically, but it may not be worth it,
	if we come up in the biggest size anyway, since we never shrink
	but only grow */
static USHORT *screen_lines[NROW];	/* pointer to screen lines	*/
static USHORT *screen2_lines[NROW];	/* pointer to page-1 lines	*/


static	int	ibm_opened,
		original_page,	/* display-page (we use 0)	*/
		allowed_vres,	/* possible scan-lines, 1 bit per value */
		original_curs,	/* start/stop scan lines	*/
		monochrome	= FALSE;

extern  void	ibmmove   (int,int);
extern  void	ibmeeol   (void);
extern  void	ibmeeop   (void);
extern  void	ibmbeep   (void);
extern  void    ibmopen   (void);
extern	void	ibmrev    (UINT);
extern	int	ibmcres   (const char *);
extern	void	ibmclose  (void);
extern	void	ibmputc   (int);
extern	void	ibmkopen  (void);
extern	void	ibmkclose (void);

#if	OPT_COLOR
extern	void	ibmfcol   (int);
extern	void	ibmbcol   (int);

int	cfcolor = -1;		/* current forground color */
int	cbcolor = -1;		/* current background color */
static	const char *initpalettestr = "0 4 2 6 1 5 3 7 8 12 10 14 9 13 11 15";
/* black, red, green, yellow, blue, magenta, cyan, white   */
#endif
extern	void	ibmscroll (int,int,int);

static	int	screen_init(int);
static	int	identify_adapter(void);
static	int	blank_attr(void);

static	const struct {
	char  *seq;
	int   code;
} keyseqs[] = {	/* use 'Z' in place of leading 0, so these can be C-strings */
	/* Arrow keys */
	{"Z\110",     KEY_Up},
	{"Z\120",     KEY_Down},
	{"Z\115",     KEY_Right},
	{"Z\113",     KEY_Left},
	/* page scroll */
	{"Z\121",     KEY_Next},
	{"Z\111",     KEY_Prior},
	{"Z\107",     KEY_Home},
	{"Z\117",     KEY_End},
	/* editing */
	{"ZR",	      KEY_Insert},
	{"Z\123",     KEY_Delete},
	/* function keys */
	{"Z;",	    KEY_F1},
	{"Z<",      KEY_F2},
	{"Z=",      KEY_F3},
	{"Z>",      KEY_F4},
	{"Z?",      KEY_F5},
	{"Z@",      KEY_F6},
	{"ZA",      KEY_F7},
	{"ZB",      KEY_F8},
	{"ZC",      KEY_F9},
	{"ZD",	    KEY_F10},
};

static int current_ibmtype;

/*
 * Standard terminal interface dispatch table. Most of the fields point into
 * "termio" code.
 */
TERM    term    = {
	NROW,
	NROW,
	NCOL,
	NCOL,
	NPAUSE,
	ibmopen,
	ibmclose,
	ibmkopen,
	ibmkclose,
	ttgetc,
	ibmputc,
	tttypahead,
	ttflush,
	ibmmove,
	ibmeeol,
	ibmeeop,
	ibmbeep,
	ibmrev,
	ibmcres,
#if	OPT_COLOR
	ibmfcol,
	ibmbcol,
	set_ctrans,
#else
	nullterm_setfore,
	nullterm_setback,
	nullterm_setpal,
#endif
	nullterm_setccol,
	ibmscroll,
	nullterm_pflush,
	nullterm_icursor,
	nullterm_settitle,
	nullterm_watchfd,
	nullterm_unwatchfd,
	nullterm_cursorvis,
};

#if CC_DJGPP
_go32_dpmi_seginfo vgainfo;
#endif

static int
get_vga_bios_info(dynamic_VGA_info *buffer)
{
# if CC_DJGPP
	_go32_dpmi_registers regs;
	vgainfo.size = (sizeof (dynamic_VGA_info)+15) / 16;
	if (_go32_dpmi_allocate_dos_memory(&vgainfo) != 0) {
		fprintf(stderr,"Couldn't allocate vgainfo memory\n");
		exit(BADEXIT);
	}

	regs.x.ax = 0x1b00;
	regs.x.bx = 0;
	regs.x.ss = regs.x.sp = 0;
	regs.x.es = vgainfo.rm_segment;
	regs.x.di = 0;
	_go32_dpmi_simulate_int(0x10, &regs);

	dosmemget( vgainfo.rm_segment*16, sizeof (dynamic_VGA_info), buffer);

	_go32_dpmi_free_dos_memory(&vgainfo);

	return (regs.h.al == 0x1b);
#else
	struct SREGS segs;
	union REGS regs;

#if CC_WATCOM
	segread(&segs);
#else
	segs.es   = FP_SEG(buffer);
#endif
	regs.x._DI_ = FP_OFF(buffer);
	regs.x._AX_ = 0x1b00;
	regs.x._BX_ = 0;
	INTX86X(0x10, &regs, &regs, &segs); /* Get VGA-BIOS status */

	return (regs.h.al == 0x1b);
#endif
}

static void
set_display (int mode)
{
	union REGS regs;
	regs.h.ah = 0;
	regs.h.al = mode;
	INTX86(0x10, &regs, &regs);
}

static void
set_page (int page)
{
	union REGS regs;
	regs.h.ah = 5;
	regs.h.al = page;
	INTX86(0x10, &regs, &regs);
}

#ifdef MUCK_WITH_KBD_RATE
/* this code turned off because there's currently no facility
 * to reset to the original rate when we quit.  i _hate_ programs
 * that don't put the world back they way they found it.
 */
/*  set the keyboard rate to max */
static void
maxkbdrate (void)
{
	union REGS regs;
	regs.h.ah = 0x3;
	regs.h.al = 0x5;
	regs.h.bh = 0x0;
	regs.h.bl = 0x0;
	INTX86(0x16, &regs, &regs);
}
#endif

static void
set_char_size(int code)
{
	union REGS regs;
	switch (code) {
	case C8x8:	chr_wide = 8;	chr_high = 8;	break;
	case C8x14:	chr_wide = 8;	chr_high = 14;	break;
	case C8x16:	chr_wide = 8;	chr_high = 16;	break;
	default:	return;		/* cannot set this one! */
	}
	regs.h.ah = 0x11;		/* set char. generator function code	*/
	regs.h.al = code;		/*  to specified ROM			*/
	regs.h.bl = 0;		/* Character table 0			*/
	INTX86(0x10, &regs, &regs);	/* VIDEO - TEXT-MODE CHARACTER GENERATOR FUNCTIONS */
}

static void
set_cursor(int start_stop)
{
	union REGS regs;
	regs.h.ah = 1;		/* set cursor size function code */
	regs.x._CX_ = (drivers[ORIGTYPE].mode <= 3) ?
		start_stop & 0x707 : start_stop;
	INTX86(0x10, &regs, &regs);	/* VIDEO - SET TEXT-MODE CURSOR SHAPE */
}

static int
get_cursor(void)
{
	union REGS regs;
	regs.h.ah = 3;
	regs.h.bh = 0;
	INTX86(0x10, &regs, &regs);	/* VIDEO - GET CURSOR POSITION */
	return regs.x._CX_;
}

static void
set_vertical_resolution(int code)
{
	union REGS regs;
	regs.h.ah = 0x12;
	regs.h.al = code;
	regs.h.bl = 0x30;
	INTX86(0x10, &regs, &regs);	/* VIDEO - SELECT VERTICAL RESOLUTION */
	if (code == RES_200)
		delay(50);	/* patch: timing problem? */
}

/*--------------------------------------------------------------------------*/

#if	OPT_COLOR
void
ibmfcol(		/* set the current output color */
int color)		/* color to set */
{
	if (color < 0)
		color = C_WHITE;
	cfcolor = ctrans[color];
}

void
ibmbcol(		/* set the current background color */
int color)		/* color to set */
{
	if (color < 0)
		color = C_BLACK;
	cbcolor = ctrans[color];
}
#endif

void
ibmmove(int row, int col)
{
	union REGS regs;
	regs.h.ah = 2;		/* set cursor position function code */
	regs.h.dl = col;
	regs.h.dh = row;
	regs.h.bh = 0;		/* set screen page number */
	INTX86(0x10, &regs, &regs);
}

/* erase to the end of the line */
void
ibmeeol(void)
{
	int ccol,crow;	/* current column,row for cursor */
	union REGS regs;

	/* find the current cursor position */
	regs.h.ah = 3;		/* read cursor position function code */
	regs.h.bh = 0;		/* current video page */
	INTX86(0x10, &regs, &regs);

	ccol = regs.h.dl;		/* record current column */
	crow = regs.h.dh;		/* and row */

	scwrite(crow, ccol, term.cols-ccol, NULL, NULL, gfcolor, gbcolor);

}

/* put a character at the current position in the current colors */
void
ibmputc(int ch)
{
	union REGS regs;
	regs.h.ah = 14;		/* write char to screen with current attrs */
	regs.h.al = ch;
#if	OPT_COLOR
	regs.h.bl = ColorDisplay() ? cfcolor : White(TRUE);
#else
	regs.h.bl = White(TRUE);
#endif
	regs.h.bh = 0;		/* current video page */
	INTX86(0x10, &regs, &regs);
}

void
ibmeeop(void)
{
	union REGS regs;
	regs.h.ah = 6;		/* scroll page up function code */
	regs.h.al = 0;		/* # lines to scroll (clear it) */
	regs.x._CX_ = 0;		/* upper left corner of scroll */
	regs.h.dh = term.rows - 1; /* lower right corner of scroll */
	regs.h.dl = term.cols - 1;
	regs.h.bh = blank_attr();
	INTX86(0x10, &regs, &regs);
}

void
ibmrev(		/* change reverse video state */
UINT state)	/* TRUE = reverse, FALSE = normal */
{
	/* This never gets used under the IBM-PC driver */
}

/* change screen properties */
int
ibmcres(
const char *res)	/* name for new "resolution" */
{
	int i;
	int status = FALSE;

	if (!res || !strcmp(res, "?"))	/* find the default configuration */
		res = "default";

	/* look up the driver by name, initialize screen */
	for (i = 0; i < TABLESIZE(drivers); i++) {
		if (strcmp(res, drivers[i].name) == 0) {
			if ((status = screen_init(i)) == TRUE) {
				strcpy(current_res_name, res);
				break;
			}
		}
	}
	return status;
}

#if OPT_MS_MOUSE || OPT_FLASH
extern VIDEO **vscreen;	/* patch: edef.h */

static	int	vp2attr ( VIDEO *, int );
static	void	move2nd ( int, int );
static	void	copy2nd ( int );

static int
vp2attr(VIDEO *vp, int inverted)
{
	int	attr;
#if	OPT_COLOR
	if (ColorDisplay())
		attr = AttrColor(
			inverted ? ReqFcolor(vp) : ReqBcolor(vp),
			inverted ? ReqBcolor(vp) : ReqFcolor(vp));
	else
#endif
	 attr = AttrMono(!inverted);
	return (attr << 8);
}

static void
move2nd(int row, int col)
{
	union REGS regs;
	regs.h.ah = 0x02;
	regs.h.bh = 1;
	regs.h.dh = row;
	regs.h.dl = col;
	INTX86(0x10, &regs, &regs);
}

static void
copy2nd(int inverted)
{
	WINDOW	*wp;
	VIDEO	*vp;
	USHORT *lp;
	char *tt;
	register int	row, col, attr;

	for_each_visible_window(wp) {
		for (row = wp->w_toprow; row <= mode_row(wp); row++) {
			vp = vscreen[row];
			lp = screen2_lines[row];
			tt = vp->v_text;
			attr = vp2attr(vp, inverted ^ (row == mode_row(wp)));
			for (col = 0; col < term.cols; col++) {
				lp[col] = (attr | tt[col]);
			}
		}
	}

	/* fill in the message line */
	lp = screen2_lines[term.rows-1];
	attr = blank_attr() << 8;
	for (col = 0; col < term.cols; col++)
		lp[col] = attr | SPACE;
	move2nd(ttrow, ttcol);
}
#endif

/*
 * Reading back from display memory does not work properly when using a mouse,
 * because the portion of the display line beginning with the mouse cursor is
 * altered (by the mouse driver, apparently).  Flash the display by building an
 * inverted copy of the screen in the second display-page and toggling
 * momentarily to that copy.
 *
 * Note: there's no window from which to copy the message line.
 */
#if OPT_FLASH
void
flash_display (void)
{
	copy2nd(TRUE);
	set_page(1);
	catnap(100,FALSE); /* if we don't wait, we cannot see the flash */
	set_page(0);
}
#endif	/* OPT_FLASH */

void
ibmbeep(void)
{
#if	OPT_FLASH
	if (global_g_val(GMDFLASH) && ibm_opened) {
		flash_display();
		return;
	}
#endif
	bdos(6, BEL, 0);	/* annoying!! */
}

void
ibmopen(void)
{
	register DRIVERS *driver = &drivers[ORIGTYPE];
	int i;
	union REGS regs;

	if (sizeof(dynamic_VGA_info) != 64) {
		printf("DOS vile build error -- dynamic_VGA_info struct"
			" must be packed (ibmpc.c)\n");
		exit(BADEXIT);
	}

	regs.h.ah = 0xf;
	INTX86(0x10,&regs, &regs);	/* VIDEO - GET DISPLAY MODE */

	driver->vchr  = C8x8;
	driver->vres  = RES_200;
	driver->rows  = 25;
	driver->cols  = regs.h.ah;
	driver->mode  = regs.h.al;
	original_page = regs.h.bh;
	driver->type  = identify_adapter();
	allowed_vres  = (1<<RES_200);	/* assume only 200 scan-lines */

	if (driver->type == VGASCR) {	/* we can determine original rows */
		dynamic_VGA_info buffer;

		if (get_vga_bios_info(&buffer)) {
#undef TESTIT
#ifdef TESTIT
			printf ("call succeeded\n");
			printf ("static_info is 0x%x\n", buffer.static_info);
			printf ("code_number is 0x%x\n", buffer.code_number);
			printf ("num_columns is 0x%x\n", buffer.num_columns);
			printf ("page_length is 0x%x\n", buffer.page_length);
			printf ("curr_page is 0x%x\n", buffer.curr_page);
			printf ("num_rows is 0x%x\n", buffer.num_rows);
#endif
			switch (buffer.char_height) {
			case 8:		driver->vchr = C8x8;	break;
			case 14:	driver->vchr = C8x14;	break;
			case 16:	driver->vchr = C8x16;	break;
			}
			driver->rows = buffer.num_rows;
#if CC_DJGPP
			{ u_long staticinfop;
			static_VGA_info static_info;
			staticinfop = ((u_long)buffer.static_info & 0xffffL);
			staticinfop += (((u_long)buffer.static_info >> 12) &
						0xffff0L);
			dosmemget( staticinfop, sizeof(static_info),
					&static_info);
			allowed_vres = static_info.text_scanlines;
			}
#elif CC_WATCOM
			{
			static_VGA_info __far *staticinfop;
			staticinfop = MK_FP (
			((unsigned long)(buffer.static_info) >> 16) & 0xffffL,
			((unsigned long)(buffer.static_info)      ) & 0xffffL
				);
			allowed_vres = staticinfop->text_scanlines;
			}
#else
			allowed_vres = buffer.static_info->text_scanlines;
#endif
			driver->vres = buffer.num_pixel_rows;
		}
	} else if (driver->type == EGASCR) {
		allowed_vres |= (1<<RES_350);
	}

	original_curs = get_cursor();

#ifdef PVGA
	set_display(10);	/* set graphic 640x350 mode */
	regs.x._AX_ = 0x007F;
	regs.h.bh = 0x01;		/* set non-VGA mode */
	INTX86(0x10,&regs, &regs);
	set_display(7);		/* set Hercule mode */
	current_res_name = "25";
#endif

#if	OPT_COLOR
	set_palette(initpalettestr);
#endif
	if (!ibmcres(current_res_name))
		(void)screen_init(ORIGTYPE);
	revexist = TRUE;
	ttopen();

#ifdef MUCK_WITH_KBD_RATE
	maxkbdrate();   /* set the keyboard rate to max */
#endif
	for (i = TABLESIZE(keyseqs); i--; ) {
		int len = strlen(keyseqs[i].seq);
		if (keyseqs[i].seq[0] == 'Z') {
			keyseqs[i].seq[0] = '\0';
		}
		addtosysmap(keyseqs[i].seq, len, keyseqs[i].code);
	}

	ibm_opened = TRUE;	/* now safe to use 'flash', etc. */
}

void
ibmclose(void)
{
	int	ctype = current_ibmtype;

	if (current_ibmtype != ORIGTYPE)
		(void)screen_init(ORIGTYPE);
	if (original_page != 0)
		set_page(original_page);
	set_cursor(original_curs);

	current_ibmtype = ctype; /* ...so subsequent term.open restores us */

	dtype = MONOSCR;		/* ...force monochrome */
	kbd_erase_to_end(0);	/* ...force colors _off_ */
	kbd_flush();
}

/* "keyboard" open and close */
void
ibmkopen(void)
{
	ms_install();
#if CC_DJGPP
	setmode(0,O_BINARY);
#endif
}

void
ibmkclose(void)
{
#if CC_DJGPP
	setmode(0,O_TEXT);
#endif
	ms_deinstall();
}

static int
screen_init(			/* initialize the screen head pointers */
int newtype)		/* type of adapter to init for */
{
	dynamic_VGA_info buffer;
	union REGS regs;
	union {
		long laddr;		/* long form of address */
		USHORT *paddr;		/* pointer form of address */
	} addr;
	long pagesize;
	register DRIVERS *driver;
	register int i;
	int	     type, rows, cols;

	driver = &drivers[newtype];

	/* check to see if we're allow this many scan-lines */
	if ((allowed_vres & (1 << (driver->vres))) == 0)
		return FALSE;

	type = driver->type;
	rows = driver->rows;
	cols = driver->cols;

	/* and set up the various parameters as needed */
	set_vertical_resolution(driver->vres);

	set_display(driver->mode);
	set_vertical_resolution(driver->vres);
	set_char_size(driver->vchr);

	/* reset the original cursor -- it gets changed above somewhere */
	set_cursor(original_curs);

	/*
	 * Install an alternative hardcopy routine which prints as many lines
	 * as are displayed on the screen. The normal BIOS routine always
	 * prints 25 lines.
	 */
	if (rows > 25) {
		regs.h.ah = 0x12;		/* alternate select function code    */
		regs.h.bl = 0x20; 	/* alt. print screen routine	     */
		INTX86(0x10, &regs, &regs);	/* VIDEO - SELECT ALTERNATE PRTSCRN  */
	}

	if (driver->type == EGASCR) {
		outp(0x3d4, 10);	/* video bios bug patch */
		outp(0x3d5, 6);
	}

	/* reset the $sres environment variable */
	(void)strcpy(screen_desc, driver->name);

	if ((type == MONOSCR) != (dtype == MONOSCR))
		sgarbf = TRUE;
	dtype = type;
	current_ibmtype = newtype;

	/* initialize the screen pointer array */
	if (monochrome)
		addr.laddr = FAR_POINTER(ScreenAddress[MONOSCR],0x0000);
	else if (type == MONOSCR)
		addr.laddr = FAR_POINTER(ScreenAddress[CGASCR],0x0000);
	else
		addr.laddr = FAR_POINTER(ScreenAddress[type],0x0000);

	for (i = 0; i < rows; i++)
		screen_lines[i] = addr.paddr + (cols * i);

#if OPT_FLASH || OPT_MS_MOUSE
	/* Build row-indices for display page #1, to use it in screen flashing
	 * or for mouse highlighting.
	 */
	if ((type == VGASCR) && get_vga_bios_info(&buffer)) {
		/*
		 * Setting page-1 seems to make Window 3.1 "aware" that we're
		 * going to write to that page.  Otherwise, the "flash" mode
		 * doesn't write to the correct address.
		 */
		set_page(1);
		get_vga_bios_info(&buffer);
		pagesize = (long)buffer.page_length; /* also, page-1 offset */
		set_page(0);
	} else {
		/*
		 * This was tested for all of the VGA combinations running
		 * with MSDOS, but isn't general -- dickey@software.org
		 */
		pagesize = (rows * cols);
		switch (cols) {
		case 40:	pagesize += ((4*cols) - 32);	break;
		case 80:	pagesize += ((2*cols) - 32);	break;
		}
		pagesize <<= 1;
	}
	addr.laddr += pagesize;
	for (i = 0; i < rows; i++)
		screen2_lines[i] = addr.paddr + (cols * i);
#endif
	/* changing the screen-size forces an update, so we do this last */
	newscreensize(rows, cols);
#if OPT_MS_MOUSE
	if (ms_exists()) {
		ms_deinstall();
		ms_install();
	}
#endif
	return TRUE;
}


int identify_adapter(void)
{
	union REGS regs;
	monochrome = FALSE;

	/* check for VGA or MCGA */
	regs.x._AX_ = 0x1a00;
	regs.h.bl = 0x00;
	INTX86(0x10,&regs, &regs); /* GET DISPLAY COMB. CODE (PS,VGA,MCGA) */

	if (regs.h.al == 0x1a) {	/* function succeeded */
		switch (regs.h.bl) {
		case 0x01:	monochrome = TRUE;
				return MONOSCR;

		case 0x02:	return CGASCR;

		case 0x05:	monochrome = TRUE;
		case 0x04:	return EGASCR;

		case 0x07:	monochrome = TRUE;
		case 0x08:	return VGASCR;

		case 0x0b:	monochrome = TRUE;
		case 0x0a:
		case 0x0c:	return CGASCR;	/* MCGA */
		}
	}

	/*
	 * Check for MONO board
	 */
	INTX86(0x11, &regs, &regs);	/* BIOS - GET EQUIPMENT LIST */

	/* Bits 4-5 in ax are:
	 *	00 EGA, VGA or PGA
	 *	01 40x25 color
	 *	10 80x25 color
	 *	11 80x25 monochrome
	 */
	if ((regs.x._AX_ & 0x30) == 0x30) {
		monochrome = TRUE;
		return MONOSCR;
	}

	/*
	 * Check for EGA board
	 */
	regs.h.ah = 0x12;
	regs.h.bl = 0x10;
	INTX86(0x10, &regs, &regs);	/* VIDEO - GET EGA INFO */

	if (regs.h.bl != 0x10)
		return EGASCR;

	return CGASCR;
}

/* write a line out*/
void
scwrite(int row, int col,	/* where */
	int nchar,		/* length of outstr and attrstr */
	const char *outstr,  	/* string to write */
	VIDEO_ATTR *attrstr, 	/* attributes to write out */
	int forg,		/* foreground color to write */
	int bacg)		/* background color to write */
{
	register USHORT *lnptr;
	register int i;

	if (row > term.rows-1)
		return;

	lnptr = screen_lines[row]+col;

	if (attrstr) {
		register USHORT attrnorm;
		register USHORT attrrev;
#if	OPT_COLOR
		if (ColorDisplay()) {
			attrnorm = AttrColor(bacg,forg) << 8;
			attrrev = AttrColor(forg,bacg) << 8;
		} else
#endif
		{
		attrnorm = AttrMono(bacg < forg) << 8;
		attrrev = AttrMono(forg < bacg) << 8;
		}
		for (i = 0; i < nchar; i++) {
			*lnptr++ = ((outstr != 0)
				? (outstr[i+col] & 0xff)
				: SPACE) | ((attrstr[i+col] & (VAREV|VASEL))
					? attrrev
					: attrnorm);
		}
	} else {
		register USHORT attr; /* attribute byte mask to place in RAM */
		/* build the attribute byte and setup the screen pointer */
#if	OPT_COLOR
		attr = ColorDisplay() ? AttrColor(bacg,forg) :
		    			AttrMono(bacg < forg);
#else
		attr = AttrMono(bacg < forg);
#endif
		attr <<= 8;
		for (i = 0; i < nchar; i++) {
			*lnptr++ = ((outstr != 0)
				? (outstr[i+col] & 0xff)
				: SPACE) | attr;
		}
	}

}

/* reads back a line into a VIDEO struct, used in line-update computation */
VIDEO *
scread(VIDEO *vp, int row)
{
	register int	i;
	USHORT *slp;		/* screen line image		*/

	if (row > term.rows-1)
		return 0;

	if (vp == 0) {
		static	VIDEO	*mine;
		if (video_alloc(&mine))
			vp = mine;
		else
			tidy_exit(BADEXIT);
	}

	slp = &screen_lines[row];
	for (i = 0; i < term.cols; i++)
		vp->v_text[i] = slp[i];

	return vp;
}

/* returns attribute for blank/empty space */
static int
blank_attr(void)
{
	register int attr;
#if	OPT_COLOR
	attr = ColorDisplay() ? AttrColor(gbcolor,gfcolor) : AttrMono(TRUE);
#else
	attr = AttrMono(TRUE);
#endif
	return attr;
}

/*
 * Move 'n' lines starting at 'from' to 'to'
 *
 * OPT_PRETTIER_SCROLL is prettier but slower -- it scrolls a line at a time
 *	instead of all at once.
 */
void
ibmscroll(int from, int to, int n)
{
	union REGS regs;
#if OPT_PRETTIER_SCROLL
	if (absol(from-to) > 1) {
		ibmscroll(from, (from < to) ? to-1 : to+1, n);
		if (from < to)
			from = to-1;
		else
			from = to+1;
	}
#endif
	if (from > to) {
		regs.h.ah = 0x06;		/* scroll window up */
		regs.h.al = from - to;	/* number of lines to scroll */
		regs.h.ch = to;		/* upper window row */
		regs.h.dh = from + n - 1;	/* lower window row */
	} else {
		regs.h.ah = 0x07;		/* scroll window down */
		regs.h.al = to - from;	/* number of lines to scroll */
		regs.h.ch = from;		/* upper window row */
		regs.h.dh = to + n - 1;	/* lower window row */
	}
	regs.h.bh = blank_attr();	/* attribute to use for line-fill */
	regs.h.cl = 0;		/* left window column */
	regs.h.dl = term.cols - 1; /* lower window column */
	INTX86(0x10, &regs, &regs);
}


/*--------------------------------------------------------------------------*/

#if OPT_MS_MOUSE

/* Define translations between mouse (pixels) and chars (row/col).
 * patch: why does 8 by 8 work, not chr_high by chr_wide?
 */
#define MS_CHAR_WIDE	((term.cols == 80) ? 8 : 16)
#define MS_CHAR_HIGH    8

#define	pixels2col(x)   ((x)/MS_CHAR_WIDE)
#define pixels2row(y)   ((y)/MS_CHAR_HIGH)

#define col2pixels(x)   ((x)*MS_CHAR_WIDE)
#define row2pixels(y)   ((y)*MS_CHAR_HIGH)

/* Define a macro for calling mouse services */
#define MouseCall INTX86(0x33, &regs, &regs)

#define MS_MOVEMENT	iBIT(0)	/* mouse cursor movement */
#define	MS_BTN1_PRESS   iBIT(1)	/* left button */
#define MS_BTN1_RELEASE iBIT(2)
#define MS_BTN2_PRESS   iBIT(3)	/* right button */
#define MS_BTN2_RELEASE iBIT(4)
#define MS_BTN3_PRESS   iBIT(5)	/* center button */
#define MS_BTN3_RELEASE iBIT(6)

#define BLINK 0x8000

	/* These have to be "far", otherwise TurboC doesn't force the
	 * segment register to be specified from 'ms_event_handler()'
	 */
int	far	button_pending;	/* 0=none, 1=pressed, 2=released */
int	far	button_number;	/* 1=left, 2=right, 3=center */
int	far	button_press_x;
int	far	button_press_y;
int	far	button_relsd_x;
int	far	button_relsd_y;
int		rodent_exists;
int		rodent_cursor_display;

#if CC_TURBO
#define	MsButtonPending() button_pending
#endif

/*
 * If we're using a DPMI configuration, we'll not be able to use the mouse
 * event handler, since it relies on a call rather than an interrupt.  We can
 * do polling instead.
 */
#if !CC_TURBO
static int
MsButtonPending(void)
{
	union REGS regs;
	regs.x._AX_ = 3;	/* query the mouse */
	MouseCall;

	if (regs.x._BX_ & 7) {
		if (regs.x._BX_ & 1)
			button_number = 1;
		else if (regs.x._BX_ & 2)
			button_number = 2;
		else if (regs.x._BX_ & 4)
			button_number = 3;

		button_press_x = button_relsd_x = regs.x._CX_;
		button_press_y = button_relsd_y = regs.x._DX_;
		button_pending = 1;

	} else {
		if (button_pending)
			button_pending = 2;
		button_relsd_x = regs.x._CX_;
		button_relsd_y = regs.x._DX_;
	}
	return button_pending;
}
#endif

int
ms_exists (void)
{
	return rodent_exists;
}

void
ms_processing (void)
{
	WINDOW	*wp;
	int	copied = FALSE,
		invert, normal,
		first, first_x = ttcol, first_y = ttrow,
		last,  last_x  = ttcol,  last_y = ttrow,
		that,  that_x,  that_y,
		attr, delta;
	USHORT *s2page = screen2_lines[0];

	ms_showcrsr();
	while (MsButtonPending()) {
		if (button_pending == 2) {	/* released? */
			button_pending = 0;
			break;
		} else {	/* selection */

#if CC_TURBO
			disable();
			that_x = button_relsd_x;
			that_y = button_relsd_y;
			enable();
#else
			that_x = button_relsd_x;
			that_y = button_relsd_y;
#endif

			if (!copied) {
				int x = pixels2col(button_press_x);
				int y = pixels2row(button_press_y);
				wp = row2window(y);
				/* Set the dot-location if button 1 was pressed
				 * in a window.
				 */
				if (wp != 0
				 && ttrow != term.rows - 1
				 && setcursor(y, x)) {
					(void)update(TRUE);
					ms_setvrange(wp->w_toprow,
						     mode_row(wp) - 1);
				} else {	/* cannot reposition */
					kbd_alarm();
					while (MsButtonPending() != 2)
						;
					continue;
				}
				first_x = last_x = col2pixels(ttcol);
				first_y = last_y = row2pixels(ttrow);
				first = ms_crsr2inx(first_x, first_y);

				copy2nd(FALSE);
				set_page(1);
				copied = TRUE;

				invert = vp2attr(vscreen[ttrow],TRUE);
				normal = vp2attr(vscreen[ttrow],FALSE);
			}

			that  = ms_crsr2inx(that_x, that_y);
			last  = ms_crsr2inx(last_x, last_y);
			delta = (last < that) ? 1 : -1;

			if (that != last) {
				register int j;
				if (((last < that) && (last <= first))
				 || ((last > that) && (last >= first))) {
					attr = normal;
				} else {
					attr = invert;
				}
#define HiLite(n,attr) s2page[n] = attr | (s2page[n] & 0xff)

				for (j = last; j != that; j += delta) {
					if (j == first) {
						if (attr == normal) {
							attr = invert;
						} else {
							attr = normal;
						}
					}
					HiLite(j,attr);
				}
				HiLite(that,invert|BLINK);
			}
			last_x = that_x;
			last_y = that_y;
		}
	}

	/*
	 * If we've been highlighting the selection, finish it off.
	 */
	if (copied) {
		set_page(0);
		ms_setvrange(0, term.rows-1);
		mlerase();
		setwmark(pixels2row(last_y), pixels2col(last_x));
		if (DOT.l != MK.l
		 || DOT.o != MK.o) {
			regionshape = EXACT;
			(void)yankregion();
			(void)update(TRUE);
		}
		movecursor(pixels2row(first_y), pixels2col(first_x));
	}
}

/* translate cursor position (pixels) to array-index of the text-position */
static int
ms_crsr2inx(int x, int y)
{
	return pixels2col(x) + (pixels2row(y) * term.cols);
}

static void
ms_deinstall(void)
{
	union REGS regs;
	ms_hidecrsr();
	regs.x._AX_ = 0;	/* reset the mouse */
	MouseCall;
	rodent_exists = FALSE;
}

	/* This event-handler cannot do I/O; tracing it can be tricky...
	 */
#if CC_TURBO
void far
ms_event_handler (void)
{
	UINT	ms_event  = _AX;
/*	UINT	ms_button = _BX;*/
	UINT	ms_horz   = _CX;
	UINT	ms_vert   = _DX;

	if (ms_event & MS_BTN1_PRESS) {
		button_pending = 1;
		button_number  = 1;
		button_press_x = button_relsd_x = ms_horz;
		button_press_y = button_relsd_y = ms_vert;
	} else {	/* movement or release */
		if (ms_event & MS_BTN1_RELEASE)
			button_pending = 2;
		button_relsd_x = ms_horz;
		button_relsd_y = ms_vert;
	}
	return;
}
#endif

static void
ms_hidecrsr(void)
{
	union REGS regs;
	/* Hides the mouse cursor if it is displayed */
	if (rodent_cursor_display) {
		rodent_cursor_display = FALSE;
		regs.x._AX_ = 0x02;
		MouseCall;
	}
} /* End of ms_hidecrsr() */

static void
ms_install(void)
{
	union REGS regs;
	if (rodent_exists)
		return;

	/* If a mouse is installed, initializes the mouse and
	 * sets rodent_exists to 1. If no mouse is installed,
	 * sets rodent_exists to 0.
	 */
	regs.x._AX_ = 0;
	MouseCall;
	rodent_exists = regs.x._AX_;
	rodent_cursor_display = FALSE; /* safest assumption */
	if (ms_exists()) {
		struct SREGS segs;

		memset(&regs, 0, sizeof(regs));
		memset(&segs, 0, sizeof(segs));

		regs.x._AX_ = 0x4;	/* set mouse position */
		regs.x._CX_ = 0;
		regs.x._DX_ = 0;
		MouseCall;

#if CC_TURBO
		regs.x._AX_ = 0xc;	/* define event handler */
		regs.x._CX_ = MS_MOVEMENT | MS_BTN1_PRESS | MS_BTN1_RELEASE;
		regs.x._DX_ = FP_OFF(ms_event_handler);
		segs.es = FP_SEG(ms_event_handler);
		INTX86X(0x33, &regs, &regs, &segs);
#endif
	}
}

static void
ms_setvrange(int upperrow, int lowerrow)
{
	union REGS regs;
	/* Restricts vertical cursor movement to the screen region
	 * between upperrow and lowerrow.  If the cursor is outside the range,
	 * it is moved inside.
	 */
	regs.x._AX_ = 0x08;
	regs.x._CX_ = row2pixels(upperrow);
	regs.x._DX_ = row2pixels(lowerrow);
	MouseCall;
} /* End of ms_setvrange() */


static void
ms_showcrsr(void)
{
	union REGS regs;
	/* Displays the mouse cursor */
	int counter;

	/* Call Int 33H Function 2AH to get the value of the display counter */
	regs.x._AX_ = 0x2A;
	MouseCall;
	counter = regs.x._AX_;

	/* Call Int 33H Function 01H as many times as needed to display */
	/* the mouse cursor */
	while (counter-- > 0) {
		regs.x._AX_ = 0x01;
		MouseCall;
	}
	rodent_cursor_display = TRUE;
} /* End of ms_showcrsr() */
#endif /* OPT_MS_MOUSE */
