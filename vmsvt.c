/*
 * VMS terminal driver
 *
 * Uses SMG to lookup capabilities of the terminal (see SYS$SYSTEM:SMGTERMS.TXT
 * and SYS$SYSTEM:TERMTABLE.TXT)
 *
 * Originally written by Curtis Smith in 1987, this module has been rewritten
 * by Tom Dickey and Clark Morgan to:
 *
 *   -- simplify it,
 *   -- extend it to support a wider variety of terminal types,
 *   -- add VT220 keyboard mappings,
 *   -- support syntax highlighting,
 *   -- support wide and narrow screen resolutions,
 *   -- support visual bells.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/vmsvt.c,v 1.51 1999/09/19 19:42:28 tom Exp $
 *
 */

#include	"estruct.h"		/* Emacs' structures		*/
#include	"edef.h"		/* Emacs' definitions		*/

#if	DISP_VMSVT

#include	<descrip.h>		/* Descriptor definitions	*/
#include	<iodef.h>		/* to get IO$_SENSEMODE		*/
#include	<ttdef.h>		/* to get TT$_UNKNOWN		*/
#include	<stsdef.h>		/* to get $VMS_STATUS_SUCCESS	*/

#include	<starlet.h>
#include	<smgtrmptr.h>		/* to get SMG$K_??? definitions */
#include	<smg$routines.h>
#include	<ssdef.h>

/** Forward references **/
static	void	vmsscrollregion (int top, int bot);
static	void	vmsscroll_reg (int from, int to, int n);
static	void	vmsscroll_delins (int from, int to, int n);
static	void	vmsopen (void);
static	void	vmsclose (void);
static	void	vmskopen (void);
static	void	vmskclose (void);
static	void	vmsmove (int row, int col);
static	void	vmseeol (void);
static	void	vmseeop (void);
static	void	vmsbeep (void);
#if OPT_VIDEO_ATTRS
static	void	vmsattr ( UINT attr );
#else
static	void	vmsrev	( UINT state );
#endif
static	int	vmscres (const char *);

/** SMG stuff (just like termcap) */
static	int	initialized;
static	int	termtype;
static	char *	tc_SO;	/* begin standout (reverse) */
static	char *	tc_SE;	/* end standout (reverse) */
static	char *	erase_to_end_line;
static	char *	erase_whole_display;

static	char *	delete_line;
static	char *	insert_line;
static	char *	scroll_forw;
static	char *	scroll_back;
static	char *	scroll_regn;

static	char *	set_narrow;
static	char *	set_wide;

static	long	wide_cols;
static	long	narrow_cols;

#if OPT_VIDEO_ATTRS
static	char *	tc_MD;		/* begin bold */
static	char *	tc_ME;		/* end bold */
static	char *	tc_US;		/* begin underscore */
static	char *	tc_UE;		/* end underscore */
#endif

#if OPT_FLASH
static	char *	dark_on;
static	char *	dark_off;
#endif

/* Dispatch table. All hard fields just point into the terminal I/O code. */
TERM	term	= {
	24,				/* Max number of rows allowable */
	/* Filled in */ 0,		/* Current number of rows used	*/
	132,				/* Max number of columns	*/
	/* Filled in */ 0,		/* Current number of columns	*/
	100,				/* # times thru update to pause */
	vmsopen,			/* Open terminal at the start	*/
	vmsclose,			/* Close terminal at end	*/
	nullterm_kopen,			/* Open keyboard		*/
	nullterm_kclose,		/* Close keyboard		*/
	ttgetc,				/* Get character from keyboard	*/
	ttputc,				/* Put character to display	*/
	tttypahead,			/* char ready for reading	*/
	ttflush,			/* Flush output buffers		*/
	vmsmove,			/* Move cursor, origin 0	*/
	vmseeol,			/* Erase to end of line		*/
	vmseeop,			/* Erase to end of page		*/
	vmsbeep,			/* Beep				*/
#if OPT_VIDEO_ATTRS
	vmsattr,			/* Set attribute video state	*/
#else
	vmsrev,				/* Set reverse video state	*/
#endif
	vmscres,			/* Change screen resolution	*/
	nullterm_setfore,		/* N/A: Set foreground color	*/
	nullterm_setback,		/* N/A: Set background color	*/
	nullterm_setpal,		/* N/A: Set palette colors	*/
	nullterm_setccol,
	nullterm_scroll,		/* set at init-time		*/
	nullterm_pflush,
	nullterm_icursor,
	nullterm_settitle,
	nullterm_watchfd,
	nullterm_unwatchfd,
	nullterm_cursorvis,
};

struct vmskeyseqs
{
	char	*seq;
	int	code;
};

struct vmskeyseqs vt100seqs[] =
{
	{ "\033[A",   KEY_Up },
	{ "\033[B",   KEY_Down },
	{ "\033[C",   KEY_Right },
	{ "\033[D",   KEY_Left },
	{ "\033OP",   KEY_KP_F1 },
	{ "\033OQ",   KEY_KP_F2 },
	{ "\033OR",   KEY_KP_F3 },
	{ "\033OS",   KEY_KP_F4 },
	/* vt220 keys (F1-F5 do not transmit control sequences) include vt100 */
	{ "\033[17~", KEY_F6 },
	{ "\033[18~", KEY_F7 },
	{ "\033[19~", KEY_F8 },
	{ "\033[20~", KEY_F9 },
	{ "\033[21~", KEY_F10 },
	{ "\033[23~", KEY_F11 },
	{ "\033[24~", KEY_F12 },
	{ "\033[25~", KEY_F13 },
	{ "\033[26~", KEY_F14 },
	{ "\033[28~", KEY_F15 },
	{ "\033[29~", KEY_F16 },
	{ "\033[31~", KEY_F17 },
	{ "\033[32~", KEY_F18 },
	{ "\033[33~", KEY_F19 },
	{ "\033[34~", KEY_F20 },
	{ "\033[3~",  KEY_Delete },	/* aka "Remove" */
	{ "\033[6~",  KEY_Next },
	{ "\033[5~",  KEY_Prior },	/* aka "Prev" */
	{ "\033[1~",  KEY_Find },
	{ "\033[2~",  KEY_Insert },
	{ "\033[4~",  KEY_Select },
	/* 8-bit codes are the same, substituting the first two characters */
	{ "\233A",    KEY_Up },
	{ "\233B",    KEY_Down },
	{ "\233C",    KEY_Right },
	{ "\233D",    KEY_Left },
	{ "\217P",    KEY_KP_F1 },
	{ "\217Q",    KEY_KP_F2 },
	{ "\217R",    KEY_KP_F3 },
	{ "\217S",    KEY_KP_F4 },
	/* vt220 keys (F1-F5 do not transmit control sequences) include vt100 */
	{ "\23317~",  KEY_F6 },
	{ "\23318~",  KEY_F7 },
	{ "\23319~",  KEY_F8 },
	{ "\23320~",  KEY_F9 },
	{ "\23321~",  KEY_F10 },
	{ "\23323~",  KEY_F11 },
	{ "\23324~",  KEY_F12 },
	{ "\23325~",  KEY_F13 },
	{ "\23326~",  KEY_F14 },
	{ "\23328~",  KEY_F15 },
	{ "\23329~",  KEY_F16 },
	{ "\23331~",  KEY_F17 },
	{ "\23332~",  KEY_F18 },
	{ "\23333~",  KEY_F19 },
	{ "\23334~",  KEY_F20 },
	{ "\2333~",   KEY_Delete },	/* aka "Remove" */
	{ "\2336~",   KEY_Next },
	{ "\2335~",   KEY_Prior },	/* aka "Prev" */
	{ "\2331~",   KEY_Find },
	{ "\2332~",   KEY_Insert },
	{ "\2334~",   KEY_Select },
};
struct vmskeyseqs vt52seqs[] =
{
	{ "\33A", KEY_Up },
	{ "\33B", KEY_Down },
	{ "\33C", KEY_Right },
	{ "\33D", KEY_Left },
	{ "\33P", KEY_KP_F1 },
	{ "\33Q", KEY_KP_F2 },
	{ "\33R", KEY_KP_F3 },
	{ "\33S", KEY_KP_F4 },
};

/***
 *  ttputs  -  Send a string to ttputc
 *
 *  Nothing returned
 ***/
static void
ttputs(char * string)			/* String to write		*/
{
	if (string)
		while (*string != EOS)
			ttputc(*string++);
}

/***
 *  vmsvt_tgoto - 2-argument request, e.g., for cursor movement.
 *
 *  Nothing returned
 ***/
static void
vmsvt_tgoto(request_code, parm1, parm2)
{
	char buffer[32];
	int ret_length;
	static int max_buffer_length = sizeof(buffer);
	static int arg_list[3] = { 2 };

	register int i;

	if (!initialized) {
		printf("\n");
		return;
	}

	/* Set the arguments into the arg_list array
	 */
	arg_list[1] = parm1;
	arg_list[2] = parm2;

	if (!$VMS_STATUS_SUCCESS(smg$get_term_data(
		&termtype,
		&request_code,
		&max_buffer_length,
		&ret_length,
		buffer,
		arg_list)))
	{
		ttputs("OOPS");
		return;
	}
	else if (ret_length > 0)
	{
		char *cp = buffer;
		while (ret_length-- > 0)
			ttputc(*cp++);
	}
}


static void
vmsmove (int row, int col)
{
	vmsvt_tgoto(SMG$K_SET_CURSOR_ABS, row+1, col+1);
}

#if OPT_VIDEO_ATTRS
static void
vmsattr(UINT attr)
{
#define VA_SGR (VASEL|VAREV|VAUL|VAITAL|VABOLD)
	static	const	struct	{
		char	**start;
		char	**end;
		UINT	mask;
	} tbl[] = {
		{ &tc_SO, &tc_SE, VASEL|VAREV },
		{ &tc_US, &tc_UE, VAUL },
		{ &tc_US, &tc_UE, VAITAL },
		{ &tc_MD, &tc_ME, VABOLD },
	};
	static	UINT last;

	attr = VATTRIB(attr);
	if (attr & VASPCOL) {
		attr = VCOLORATTR((attr & (NCOLORS - 1)));
	} else {
		attr &= ~(VAML|VAMLFOC);
	}

	if (attr != last) {
		register SIZE_T n;
		register char *s;
		UINT	diff = attr ^ last;
		int	ends = FALSE;

		/* turn OFF old attributes */
		for (n = 0; n < TABLESIZE(tbl); n++) {
			if ((tbl[n].mask & diff) != 0
			 && (tbl[n].mask & attr) == 0
			 && (s = *(tbl[n].end))	 != 0) {
				ttputs(s);
#if OPT_COLOR
				if (!ends)	/* do this once */
					reinitialize_colors();
#endif
				ends = TRUE;
				diff &= ~(tbl[n].mask);
			}
		}

		/* turn ON new attributes */
		for (n = 0; n < TABLESIZE(tbl); n++) {
			if ((tbl[n].mask & diff)  != 0
			 && (tbl[n].mask & attr)  != 0
			 && (s = *(tbl[n].start)) != 0) {
				ttputs(s);
				diff &= ~(tbl[n].mask);
			}
		}

		if (tc_SO != 0 && tc_SE != 0) {
			if (ends && (attr & (VAREV|VASEL))) {
				ttputs(tc_SO);
			} else if (diff & VA_SGR) {  /* we didn't find it */
				ttputs(tc_SE);
			}
		}
#if OPT_COLOR
		if (attr & VACOLOR)
			tcapfcol(VCOLORNUM(attr));
		else if (given_fcolor != gfcolor)
			tcapfcol(gfcolor);
#endif
		last = attr;
	}
}

#else	/* highlighting is a minimum attribute */

static void
tcaprev(		/* change reverse video status */
UINT state)		/* FALSE = normal video, TRUE = reverse video */
{
	static int revstate = -1;
	if (state == revstate)
		return;
	revstate = state;
	if (state)
		ttputs(tc_SO);
	else
		ttputs(tc_SE);
}

#endif	/* OPT_VIDEO_ATTRS */

static void
vmseeol(void)
{
	ttputs(erase_to_end_line);
}


static void
vmseeop(void)
{
	ttputs(erase_whole_display);
}



static void
cache_capabilities(void)
{
    static struct caps_struct
    {
	long  code;
	char **cap_string;
    } tbl[] =
    {
	{ SMG$K_BEGIN_REVERSE,	     &tc_SO },
	{ SMG$K_END_REVERSE,	     &tc_SE },
#if OPT_VIDEO_ATTRS
	{ SMG$K_BEGIN_BOLD,	     &tc_MD },
	{ SMG$K_END_BOLD,	     &tc_ME },
	{ SMG$K_BEGIN_UNDERSCORE,    &tc_US },
	{ SMG$K_END_UNDERSCORE,	     &tc_UE },
#endif
#if OPT_FLASH
	{ SMG$K_DARK_SCREEN,	     &dark_on },
	{ SMG$K_LIGHT_SCREEN,	     &dark_off },
#endif
	{ SMG$K_ERASE_TO_END_LINE,   &erase_to_end_line },
	{ SMG$K_ERASE_WHOLE_DISPLAY, &erase_whole_display },
	{ SMG$K_INSERT_LINE,	     &insert_line },	/* al */
	{ SMG$K_DELETE_LINE,	     &delete_line },	/* dl */
	{ SMG$K_SCROLL_FORWARD,	     &scroll_forw },	/* SF */
	{ SMG$K_SCROLL_REVERSE,	     &scroll_back },	/* SR */
	{ SMG$K_SET_SCROLL_REGION,   &scroll_regn },	/* CS */
	{ SMG$K_WIDTH_NARROW,	     &set_narrow },	/* 80 columns */
	{ SMG$K_WIDTH_WIDE,	     &set_wide },	/* 132 columns */
    };
    char	       *buf;
    struct caps_struct *csp;
    int		       i, buf_len, ret_len;
    long	       rqst_code;
    static char	       storage[1024];

    for (i = 0, buf = storage, csp = tbl; i < TABLESIZE(tbl); i++, csp++)
    {
	buf_len = (storage + sizeof(storage)) - buf;
	if (!$VMS_STATUS_SUCCESS(smg$get_term_data(&termtype,
			       &csp->code,
			       &buf_len,
			       &ret_len,
			       buf)) || ret_len == 0)
	{
	    *(csp->cap_string) = NULL;
	}
	else
	{
	    buf[ret_len]       = 0;
	    *(csp->cap_string) = buf;
	    buf		      += ret_len + 1;
	}
    }
    rqst_code = SMG$K_COLUMNS;
    if (!$VMS_STATUS_SUCCESS(smg$get_numeric_data(&termtype,
						  &rqst_code,
						  &narrow_cols)))
    {
	narrow_cols = 80;
    }
    rqst_code = SMG$K_WIDE_SCREEN_COLUMNS;
    if (!$VMS_STATUS_SUCCESS(smg$get_numeric_data(&termtype,
						  &rqst_code,
						  &wide_cols)))
    {
	wide_cols = 132;
    }
    term.maxcols = wide_cols;
}

/** I/O information block definitions **/
struct iosb {			/* I/O status block			*/
	short	i_cond;		/* Condition value			*/
	short	i_xfer;		/* Transfer count			*/
	long	i_info;		/* Device information			*/
};
struct termchar {		/* Terminal characteristics		*/
	char	t_class;	/* Terminal class			*/
	char	t_type;		/* Terminal type			*/
	short	t_width;	/* Terminal width in characters		*/
	long	t_mandl;	/* Terminal's mode and length		*/
	long	t_extend;	/* Extended terminal characteristics	*/
};
static struct termchar tc;	/* Terminal characteristics		*/

static void
get_terminal_type(void)
{
	short fd;
	int status, deassign_status;
	struct iosb iostatus;
	$DESCRIPTOR(devnam, "SYS$COMMAND");

	/* Assign input to a channel */
	if (!$VMS_STATUS_SUCCESS(status = sys$assign(&devnam, &fd, 0, 0)))
		tidy_exit(status);

	/* Get terminal characteristics */
	status = sys$qiow(		/* Queue and wait		*/
		0,			/* Wait on event flag zero	*/
		fd,			/* Channel to input terminal	*/
		IO$_SENSEMODE,		/* Get current characteristic	*/
		&iostatus,		/* Status after operation	*/
		0, 0,			/* No AST service		*/
		&tc,			/* Terminal characteristics buf */
		sizeof(tc),		/* Size of the buffer		*/
		0, 0, 0, 0);		/* P3-P6 unused			*/

	/* De-assign the input device */
	if (!$VMS_STATUS_SUCCESS(deassign_status = sys$dassgn(fd)))
		tidy_exit(deassign_status);

	/* Jump out if bad qiow status */
	if (!$VMS_STATUS_SUCCESS(status))
		tidy_exit(status);
	if ((iostatus.i_cond & 1) == 0)
		tidy_exit(iostatus.i_cond);
}

static void
vmsopen(void)
{
	int	i, keyseq_tablesize;
	struct vmskeyseqs *keyseqs;

	get_terminal_type();
	if (tc.t_type == TT$_UNKNOWN)
	{
		printf("Terminal type is unknown!\n");
		printf("Set your terminal type using $ SET TERMINAL/INQUIRE\n");
		tidy_exit(3);
	}
	if (tc.t_type != TT$_VT52)
	{
		keyseqs = vt100seqs;
		keyseq_tablesize = TABLESIZE(vt100seqs);
	}
	else
	{
		keyseqs = vt52seqs;;
		keyseq_tablesize = TABLESIZE(vt52seqs);
	}

	/* Access the system terminal definition table for the		*/
	/* information of the terminal type returned by IO$_SENSEMODE	*/
	if (!$VMS_STATUS_SUCCESS(smg$init_term_table_by_type(&tc.t_type, &termtype)))
		return;

	/* Set sizes */
	term.rows = ((UINT) tc.t_mandl >> 24);
	term.cols = tc.t_width;

	if (term.maxrows < term.rows)
		term.maxrows = term.rows;

	if (term.maxcols < term.cols)
		term.maxcols = term.cols;

	cache_capabilities();

	revexist = (tc_SO != NULL && tc_SE != NULL);
	eolexist = erase_whole_display != NULL;

	/*
	 * I tried 'vmsgetstr()' for a VT100 terminal and it had no codes
	 * for insert_line or for delete_line.	(Have to work up a test for
	 * that) - TD
	 */

	if (scroll_regn && scroll_back)
	{
		if (scroll_forw == NULL) /* assume '\n' scrolls forward */
			scroll_forw = "\n";
		term.scroll = vmsscroll_reg;
	}
	else if (delete_line && insert_line)
		term.scroll = vmsscroll_delins;
	else
		term.scroll = nullterm_scroll;

	/* Set resolution */
	(void) strcpy(screen_desc, (term.cols == narrow_cols) ? "NORMAL" : "WIDE");

	/* Open terminal I/O drivers */
	ttopen();

	/* Set predefined keys */
	for (i = keyseq_tablesize; i--; )
		addtosysmap(keyseqs[i].seq, strlen(keyseqs[i].seq), keyseqs[i].code);

	initialized = TRUE;
}


/* copied/adapted from 'tcap.c' 19-apr-1993 dickey@software.org */

/* move howmany lines starting at from to to */
static void
vmsscroll_reg(int from, int to, int n)
{
	int i;
	if (to == from) return;
	if (to < from) {
		vmsscrollregion(to, from + n - 1);
		vmsmove(from + n - 1,0);
		for (i = from - to; i > 0; i--)
			ttputs(scroll_forw);
	} else { /* from < to */
		vmsscrollregion(from, to + n - 1);
		vmsmove(from,0);
		for (i = to - from; i > 0; i--)
			ttputs(scroll_back);
	}
	vmsscrollregion(0, term.rows-1);
}

/*
OPT_PRETTIER_SCROLL is prettier but slower -- it scrolls
		a line at a time instead of all at once.
*/

/* move howmany lines starting at from to to */
static void
vmsscroll_delins(int from, int to, int n)
{
	int i;
	if (to == from) return;
	/* patch: should make this more like 'tcap.c', or merge logic somehow */
#if OPT_PRETTIER_SCROLL
	if (absol(from-to) > 1) {
		vmsscroll_delins(from, (from<to) ? to-1:to+1, n);
		if (from < to)
			from = to-1;
		else
			from = to+1;
	}
#endif
	if (to < from) {
		vmsmove(to,0);
		for (i = from - to; i > 0; i--)
			ttputs(delete_line);
		vmsmove(to+n,0);
		for (i = from - to; i > 0; i--)
			ttputs(insert_line);
	} else {
		vmsmove(from+n,0);
		for (i = to - from; i > 0; i--)
			ttputs(delete_line);
		vmsmove(from,0);
		for (i = to - from; i > 0; i--)
			ttputs(insert_line);
	}
}

/* cs is set up just like cm, so we use tgoto... */
static void
vmsscrollregion(int top, int bot)
{
	vmsvt_tgoto(SMG$K_SET_SCROLL_REGION, top+1, bot+1);
}

/***
 *  vmscres  -	Change screen resolution
 *
 *  support these values:   WIDE -> 132 columns, NORMAL -> 80 columns
 *
 *  T -> if resolution successfully changed, F otherwise.
 ***/
static int
vmscres(const char *res)
{
    char buf[NLINE];
    int	 rc = FALSE;

    if (tc.t_type == TT$_VT52)
    {
	mlforce("[sres not supported for VT52-style terminals]");
	return (rc);
    }

    strcpy(buf, res);
    mkupper(buf);
    if (strcmp(buf, "WIDE") == 0 && set_wide != 0)
    {
	ttputs(set_wide);
	term.cols = wide_cols;
	rc	  = TRUE;
    }
    else if (strcmp(buf, "NORMAL") == 0 && set_narrow != 0)
    {
	ttputs(set_narrow);
	term.cols = narrow_cols;
	rc	  = TRUE;
    }
    else
	mlforce("[invalid sres value (use NORMAL or WIDE)]");
    if (rc)
	newwidth(1, term.cols);
    return (rc);
}



static void
vmsclose(void)
{
    if (tc.t_type != TT$_VT52)
    {
	/*
	 * Restore terminal width to previous state (if necessary) and then
	 * cleanup as usual.
	 */

	if (tc.t_width != term.cols)
	    vmscres((tc.t_width == narrow_cols) ? "NORMAL" : "WIDE");
    }
    ttclose();
}



/***
 *  vmsbeep  -	Ring the bell
 *
 *  Nothing returned
 ***/
static void
vmsbeep(void)
{
#if OPT_FLASH
	int hit = 0;

	if (global_g_val(GMDFLASH) && dark_off != NULL && dark_on != NULL)
	{
		hit = 1;
		ttputs(dark_off);
		term.flush();
		catnap(200, FALSE);
		ttputs(dark_on);
	}
	if (! hit && tc.t_type != TT$_VT52)
	{
		static char *seq[][2] =
		{
			{ NULL, NULL },		       /* vtflash = off */
			{ VTFLASH_NORMSEQ, VTFLASH_REVSEQ }, /* reverse */
			{ VTFLASH_REVSEQ, VTFLASH_NORMSEQ }, /* normal	*/
		};
		char *str1, *str2;
		int  val;

		val  = global_g_val(GVAL_VTFLASH);
		str1 = seq[val][0];
		if (str1)
		{
			str2 = seq[val][1];
			ttputs(str1);
			term.flush();
			catnap(200, FALSE);
			ttputs(str2);
			hit = 1;
		}
	}
	if (! hit)
#endif
	ttputc(BEL);
}

#else

void
vmsvt_dummy(void)
{
}

#endif
