/*
 *  Advanced VMS terminal driver
 *
 *  Knows about any terminal defined in SMGTERMS.TXT and TERMTABLE.TXT
 *  located in SYS$SYSTEM.
 *
 *  Author:  Curtis Smith
 *  Last Updated: 07/14/87
 *
 * $Header: /users/source/archives/vile.vcs/RCS/vmsvt.c,v 1.42 1999/02/11 11:32:52 cmorgan Exp $
 *
 */

#include	"estruct.h"		/* Emacs' structures		*/
#include	"edef.h"		/* Emacs' definitions		*/

#if	DISP_VMSVT

#include	<descrip.h>		/* Descriptor definitions	*/
#include	<iodef.h>		/* to get IO$_SENSEMODE		*/
#include	<ttdef.h>		/* to get TT$_UNKNOWN		*/

#include	<starlet.h>
#include	<smgtrmptr.h>		/* to get SMG$K_??? definitions	*/
#include	<smg$routines.h>

/** Forward references **/
static	void	vmsscrollregion (int top, int bot);
static	void	vmsscroll_reg (int from, int to, int n);
static	void	vmsscroll_delins (int from, int to, int n);
static	void	vmsopen	(void);
static	void	vmskopen (void);
static	void	vmskclose (void);
static	void	vmsmove (int row, int col);
static	void	vmseeol	(void);
static	void	vmseeop	(void);
static	void	vmsbeep	(void);
#if OPT_VIDEO_ATTRS
static	void	vmsattr ( UINT attr );
#else
static	void	vmsrev  ( UINT state );
#endif
static	int	vmscres	(const char *);

extern	int	eolexist, revexist;
extern	char	sres[];

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
	64,				/* Min margin for extended lines*/
	8,				/* Size of scroll region	*/
	100,				/* # times thru update to pause */
	vmsopen,			/* Open terminal at the start	*/
	ttclose,			/* Close terminal at end	*/
	vmskopen,			/* Open keyboard		*/
	vmskclose,			/* Close keyboard		*/
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
	null_t_setfor,			/* N/A: Set foreground color	*/
	null_t_setback,			/* N/A: Set background color	*/
	null_t_setpal,			/* N/A: Set palette colors	*/
	null_t_scroll,			/* set at init-time		*/
	null_t_pflush,
	null_t_icursor,
	null_t_title,
	null_t_watchfd,
	null_t_unwatchfd,
	null_t_cursor,
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
 *  putpad_tgoto - 2-argument request
 *
 *  Nothing returned
 ***/
static void
putpad_tgoto(request_code, parm1, parm2)
{
	char buffer[32];
	int ret_length;
	static int max_buffer_length = sizeof(buffer);
	static int arg_list[3] = { 2 };
	register char * cp;

	register int i;

	if (!initialized) {
		printf("\n");
		return;
	}

	/* Set the arguments into the arg_list array
	 */
	arg_list[1] = parm1;
	arg_list[2] = parm2;

	if ((smg$get_term_data(		/* Get terminal data		*/
		&termtype,		/* Terminal table address	*/
		&request_code,		/* Request code			*/
		&max_buffer_length,	/* Maximum buffer length	*/
		&ret_length,		/* Return length		*/
		buffer,			/* Capability data buffer	*/
		arg_list)		/* Argument list array		*/

	/* We'll know soon enough if this doesn't work		*/
			& 1) == 0) {
				ttputs("OOPS");
				return;
			}

	/* Send out resulting sequence				*/
	i = ret_length;
	cp = buffer;
	while (i-- > 0)
		ttputc(*cp++);
}


/***
 *  vmsmove  -  Move the cursor (0 origin)
 *
 *  Nothing returned
 ***/
static void
vmsmove (int row, int col)
{
	putpad_tgoto(SMG$K_SET_CURSOR_ABS, row+1, col+1);
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
			 && (s = *(tbl[n].end))  != 0) {
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

/***
 *  vmseeol  -  Erase to end of line
 *
 *  Nothing returned
 ***/
static void
vmseeol(void)
{
	ttputs(erase_to_end_line);
}


/***
 *  vmseeop  -  Erase to end of page (clear screen)
 *
 *  Nothing returned
 ***/
static void
vmseeop(void)
{
	ttputs(erase_whole_display);
}


/***
 *  vmsgetstr  -  Get an SMG string capability by name
 *
 *  Returns:	Escape sequence
 *		NULL	No escape sequence available
 ***/
static char *
vmsgetstr(int request_code)
{
	register char * result;
	static char seq_storage[1024];
	static char * buffer = seq_storage;
	static int arg_list[] = { 1, 1 };
	int max_buffer_length, ret_length;

	/*  Precompute buffer length */

	max_buffer_length = (seq_storage + sizeof(seq_storage)) - buffer;

	/* Get terminal commands sequence from master table */

	if ((smg$get_term_data(	/* Get terminal data		*/
		&termtype,	/* Terminal table address	*/
		&request_code,	/* Request code			*/
		&max_buffer_length,/* Maximum buffer length	*/
		&ret_length,	/* Return length		*/
		buffer,		/* Capability data buffer	*/
		arg_list)	/* Argument list array		*/

	/* If this doesn't work, try again with no arguments */

		& 1) == 0 &&

		(smg$get_term_data(	/* Get terminal data		*/
			&termtype,	/* Terminal table address	*/
			&request_code,	/* Request code			*/
			&max_buffer_length,/* Maximum buffer length	*/
			&ret_length,	/* Return length		*/
			buffer)		/* Capability data buffer	*/

	/* Return NULL pointer if capability is not available */

			& 1) == 0)
				return NULL;

	/* Check for empty result */
	if (ret_length == 0)
		return NULL;

	/* Save current position so we can return it to caller */

	result = buffer;

	/* NIL terminate the sequence for return */

	buffer[ret_length] = 0;

	/* Advance buffer */

	buffer += ret_length + 1;

	/* Return capability to user */
	return result;
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

/***
 *  vmsgtty - Get terminal type from system control block
 *
 *  Nothing returned
 ***/
static void
vmsgtty(void)
{
	short fd;
	int status;
	struct iosb iostatus;
	$DESCRIPTOR(devnam, "SYS$COMMAND");

	/* Assign input to a channel */
	status = sys$assign(&devnam, &fd, 0, 0);
	if ((status & 1) == 0)
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
	if ((sys$dassgn(fd) & 1) == 0)
		tidy_exit(status);

	/* Jump out if bad status */
	if ((status & 1) == 0)
		tidy_exit(status);
	if ((iostatus.i_cond & 1) == 0)
		tidy_exit(iostatus.i_cond);
}


/***
 *  vmsopen  -  Get terminal type and open terminal
 *
 *  Nothing returned
 ***/
static void
vmsopen(void)
{
	static struct {
		int	smg_code;
		char **	string;
	} tcaps[] = {
		{ SMG$K_BEGIN_REVERSE,		&tc_SO	},
		{ SMG$K_END_REVERSE,		&tc_SE	},
#if OPT_VIDEO_ATTRS
		{ SMG$K_BEGIN_BOLD,		&tc_MD	},
		{ SMG$K_END_BOLD,		&tc_ME	},
		{ SMG$K_BEGIN_UNDERSCORE,	&tc_US	},
		{ SMG$K_END_UNDERSCORE,		&tc_UE	},
#endif
#if OPT_FLASH
		{ SMG$K_DARK_SCREEN,		&dark_on	},
		{ SMG$K_LIGHT_SCREEN,		&dark_off	},
#endif
		{ SMG$K_ERASE_TO_END_LINE,	&erase_to_end_line },
		{ SMG$K_ERASE_WHOLE_DISPLAY,	&erase_whole_display },
		{ SMG$K_INSERT_LINE,		&insert_line	},	/* al */
		{ SMG$K_DELETE_LINE,		&delete_line	},	/* dl */
		{ SMG$K_SCROLL_FORWARD,		&scroll_forw	},	/* SF */
		{ SMG$K_SCROLL_REVERSE,		&scroll_back	},	/* SR */
		{ SMG$K_SET_SCROLL_REGION,	&scroll_regn	},	/* CS */
	};

	int	i, keyseq_tablesize;
	struct vmskeyseqs *keyseqs;


	/* Get terminal type */
	vmsgtty();
	if (tc.t_type == TT$_UNKNOWN) {
		printf("Terminal type is unknown!\n");
		printf("Try set your terminal type with SET TERMINAL/INQUIRE\n");
		printf("Or get help on SET TERMINAL/DEVICE_TYPE\n");
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
	if ((smg$init_term_table_by_type(&tc.t_type, &termtype) & 1) == 0) {
		return;
	}

	/* Set sizes */
	term.t_nrow = ((UINT) tc.t_mandl >> 24);
	term.t_ncol = tc.t_width;

	if (term.t_mrow < term.t_nrow)
		term.t_mrow = term.t_nrow;

	if (term.t_mcol < term.t_ncol)
		term.t_mcol = term.t_ncol;

	/* Get some capabilities */
	for (i = 0; i < sizeof(tcaps)/sizeof(tcaps[0]); i++) {
		*(tcaps[i].string) = vmsgetstr(tcaps[i].smg_code);
	}

	revexist	= tc_SO	!= NULL
		&&	  tc_SE	!= NULL;

	eolexist = erase_whole_display != NULL;

	/*
	 * I tried 'vmsgetstr()' for a VT100 terminal and it had no codes
	 * for insert_line or for delete_line.  (Have to work up a test for
	 * that).
	 */

	if (scroll_regn && scroll_back) {
		if (scroll_forw == NULL) /* assume '\n' scrolls forward */
			scroll_forw = "\n";
		term.t_scroll = vmsscroll_reg;
	} else if (delete_line && insert_line) {
		term.t_scroll = vmsscroll_delins;
	} else {
		term.t_scroll = null_t_scroll;
	}

	/* Set resolution */
	(void)strcpy(sres, (term.t_ncol != 132) ? "NORMAL" : "WIDE");

	/* Open terminal I/O drivers */
	ttopen();

	/* Set predefined keys */
	for (i = keyseq_tablesize; i--; ) {
		addtosysmap(keyseqs[i].seq, strlen(keyseqs[i].seq), keyseqs[i].code);
	}
	initialized = TRUE;
}


/***
 *  vmskopen  -  Open keyboard (not used)
 *
 *  Nothing returned
 ***/
static void
vmskopen(void)
{
}


/***
 *  vmskclose  -  Close keyboard (not used)
 *
 *  Nothing returned
 ***/
static void
vmskclose(void)
{
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
	vmsscrollregion(0, term.t_nrow-1);
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
	putpad_tgoto(SMG$K_SET_SCROLL_REGION, top+1, bot+1);
}

/***
 *  vmscres  -  Change screen resolution
 *
 *  support these values:   WIDE -> 132 columns, NORMAL -> 80 columns
 *
 *  T -> if resolution successfully changed, F otherwise.
 ***/
static int
vmscres(const char *res)
{
#define COLS_132 "\033[?3h"
#define COLS_80  "\033[?3l"

    char buf[NLINE];
    int  rc = FALSE;

    if (tc.t_type == TT$_VT52)
    {
        mlforce("[sres not supported for VT52-style terminals]");
        return (rc);
    }

    strcpy(buf, res);
    mkupper(buf);
    if (strcmp(buf, "WIDE") == 0)
    {
        ttputs(COLS_132);
        term.t_ncol = 132;
        rc          = TRUE;
    }
    else if (strcmp(buf, "NORMAL") == 0)
    {
        ttputs(COLS_80);
        term.t_ncol = 80;
        rc          = TRUE;
    }
    else
        mlforce("[invalid sres value (use NORMAL or WIDE)]");
    if (rc)
        newwidth(1, term.t_ncol);
    return (rc);

#undef COLS_132
#undef COLS_80
}



/***
 *  vmsbeep  -  Ring the bell
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
		ttputs(dark_on);
	}
	if (! hit && tc.t_type != TT$_VT52)
	{
		static char *seq[][2] =
		{
			{ NULL, NULL },                /* vtflash = off */
			{ VTFLASH_NORMSEQ, VTFLASH_REVSEQ }, /* reverse */
			{ VTFLASH_REVSEQ, VTFLASH_NORMSEQ }, /* normal  */
		};
		char *str1, *str2;
		int  val;

		val  = global_g_val(GVAL_VTFLASH);
		str1 = seq[val][0];
		if (str1)
		{
			str2 = seq[val][1];
			ttputs(str1);
			TTflush();
			catnap(150, FALSE);
			ttputs(str2);
			hit = 1;
		}
	}
	if (! hit)
#endif
	ttputc(BEL);
}

#else

/***
 *  hellovms  -  Avoid error because of empty module
 *
 *  Nothing returned
 ***/
void
hellovms(void)
{
}

#endif
