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
 * $Id: vmsvt.c,v 1.69 2010/02/14 18:43:48 tom Exp $
 *
 */

#include	"estruct.h"	/* Emacs' structures            */
#include	"edef.h"	/* Emacs' definitions           */
#include	"nefunc.h"	/* f_backchar_to_bol            */

#if DISP_VMSVT

#include	<descrip.h>	/* Descriptor definitions       */
#include	<iodef.h>	/* to get IO$_SENSEMODE         */
#include	<ttdef.h>	/* to get TT$_UNKNOWN           */
#include	<stsdef.h>	/* to get $VMS_STATUS_SUCCESS   */

#include	<starlet.h>
#include	<smgtrmptr.h>	/* to get SMG$K_??? definitions */
#include	<smg$routines.h>
#include	<ssdef.h>

#include	<descrip.h>
#include	<iodef.h>
#include	<ttdef.h>
#include	<tt2def.h>

/* function prototypes needed for the dispatch table */
static OUTC_DCL vmsvt_putc(int c);
static int vmsvt_typahead(void);
static void vmsvt_flush(void);
static void vmsvt_scrollregion(int top, int bot);
static void vmsvt_scroll_reg(int from, int to, int n);
static void vmsvt_scroll_delins(int from, int to, int n);

/* SMG stuff (just like termcap) */
static int already_open;
static int termtype;
static char *tc_SO;		/* begin standout (reverse) */
static char *tc_SE;		/* end standout (reverse) */
static char *erase_to_end_line;
static char *erase_whole_display;

static char *delete_line;
static char *insert_line;
static char *scroll_forw;
static char *scroll_back;
static char *scroll_regn;

static char *set_narrow;
static char *set_wide;

static long wide_cols;
static long narrow_cols;

#if OPT_VIDEO_ATTRS
static char *tc_MD;		/* begin bold */
static char *tc_ME;		/* end bold */
static char *tc_US;		/* begin underscore */
static char *tc_UE;		/* end underscore */
#endif

#if OPT_FLASH
static char *dark_on;
static char *dark_off;
#endif
/* *INDENT-OFF* */
struct vmskeyseqs {
    char *seq;
    int code;
};
static struct vmskeyseqs vt100seqs[] =
{
    { "\033[A",   KEY_Up },
    { "\033[B",   KEY_Down },
    { "\033[C",   KEY_Right },
    { "\033[D",   KEY_Left },
    { "\033OP",   KEY_KP_F1 },
    { "\033OQ",   KEY_KP_F2 },
    { "\033OR",   KEY_KP_F3 },
    { "\033OS",   KEY_KP_F4 },
    /* these are transmitted by xterm and similar programs */
    { "\033[11~", KEY_F1 },
    { "\033[12~", KEY_F2 },
    { "\033[13~", KEY_F3 },
    { "\033[14~", KEY_F4 },
    { "\033[15~", KEY_F5 },
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
    /* these are transmitted by xterm */
    { "\233[11~", KEY_F1 },
    { "\233[12~", KEY_F2 },
    { "\233[13~", KEY_F3 },
    { "\233[14~", KEY_F4 },
    { "\233[15~", KEY_F5 },
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
static struct vmskeyseqs vt52seqs[] =
{
    { "\33A",     KEY_Up },
    { "\33B",     KEY_Down },
    { "\33C",     KEY_Right },
    { "\33D",     KEY_Left },
    { "\33P",     KEY_KP_F1 },
    { "\33Q",     KEY_KP_F2 },
    { "\33R",     KEY_KP_F3 },
    { "\33S",     KEY_KP_F4 },
};
/* *INDENT-ON* */

typedef struct {
    USHORT status;		/* I/O completion status */
    USHORT count;		/* byte transfer count   */
    int dev_dep_data;		/* device-dependent data */
} QIO_SB;			/* This is a QIO I/O Status Block */

#define NIBUF	1024		/* Input buffer size            */
#define NOBUF	1024		/* MM says big buffers win!     */
#define EFN	0		/* Event flag                   */

static char obuf[NOBUF];	/* Output buffer                */
static int nobuf;		/* # of bytes in above          */

static char ibuf[NIBUF];	/* Input buffer                 */
static int nibuf;		/* # of bytes in above          */

static int ibufi;		/* Read index                   */

/*
 * Public data also used by spawn.c
 */
int oldmode[3];			/* Old TTY mode bits            */
int newmode[3];			/* New TTY mode bits            */
short iochan;			/* TTY I/O channel              */

/***
 *  Send a string to vmsvt_putc
 *
 *  Nothing returned
 ***/
static void
vmsvt_puts(char *string)	/* String to write              */
{
    if (string)
	while (*string != EOS)
	    vmsvt_putc(*string++);
}

/***
 *  2-argument request, e.g., for cursor movement.
 *
 *  Nothing returned
 ***/
static void
vmsvt_tgoto(request_code, parm1, parm2)
{
    char buffer[32];
    int ret_length;
    static int max_buffer_length = sizeof(buffer);
    static int arg_list[3] =
    {2};

    register int i;

    if (!already_open) {
	printf("\n");
	return;
    }

    /* Set the arguments into the arg_list array
     */
    arg_list[1] = parm1;
    arg_list[2] = parm2;

    if (!$VMS_STATUS_SUCCESS(smg$get_term_data(&termtype,
					       &request_code,
					       &max_buffer_length,
					       &ret_length,
					       buffer,
					       arg_list))) {
	vmsvt_puts("OOPS");
	return;
    } else if (ret_length > 0) {
	char *cp = buffer;
	while (ret_length-- > 0)
	    vmsvt_putc(*cp++);
    }
}

static void
vmsvt_move(int row, int col)
{
    vmsvt_tgoto(SMG$K_SET_CURSOR_ABS, row + 1, col + 1);
}

#if OPT_VIDEO_ATTRS
static void
vmsvt_attr(UINT attr)
{
#define VA_SGR (VASEL|VAREV|VAUL|VAITAL|VABOLD)
    /* *INDENT-OFF* */
    static const struct {
	char	**start;
	char	**end;
	UINT	mask;
    } tbl[] = {
	{ &tc_SO, &tc_SE, VASEL|VAREV },
	{ &tc_US, &tc_UE, VAUL },
	{ &tc_US, &tc_UE, VAITAL },
	{ &tc_MD, &tc_ME, VABOLD },
    };
    /* *INDENT-ON* */

    static UINT last;

    attr = VATTRIB(attr);
    if (attr & VASPCOL) {
	attr = VCOLORATTR((VCOLORNUM(attr) & (NCOLORS - 1)));
    } else {
	attr &= ~(VAML | VAMLFOC);
    }

    if (attr != last) {
	register size_t n;
	register char *s;
	UINT diff = attr ^ last;
	int ends = FALSE;

	/* turn OFF old attributes */
	for (n = 0; n < TABLESIZE(tbl); n++) {
	    if ((tbl[n].mask & diff) != 0
		&& (tbl[n].mask & attr) == 0
		&& (s = *(tbl[n].end)) != 0) {
		vmsvt_puts(s);
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
	    if ((tbl[n].mask & diff) != 0
		&& (tbl[n].mask & attr) != 0
		&& (s = *(tbl[n].start)) != 0) {
		vmsvt_puts(s);
		diff &= ~(tbl[n].mask);
	    }
	}

	if (tc_SO != 0 && tc_SE != 0) {
	    if (ends && (attr & (VAREV | VASEL))) {
		vmsvt_puts(tc_SO);
	    } else if (diff & VA_SGR) {		/* we didn't find it */
		vmsvt_puts(tc_SE);
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

#else /* highlighting is a minimum attribute */

/*
 * Change reverse video status.
 * FALSE = normal video, TRUE = reverse video
 */
static void
tcaprev(UINT state)
{
    static int revstate = -1;

    if (state == revstate)
	return;
    revstate = state;
    if (state)
	vmsvt_puts(tc_SO);
    else
	vmsvt_puts(tc_SE);
}

#endif /* OPT_VIDEO_ATTRS */

static void
vmsvt_eeol(void)
{
    vmsvt_puts(erase_to_end_line);
}

static void
vmsvt_eeop(void)
{
    vmsvt_puts(erase_whole_display);
}

static void
cache_capabilities(void)
{
    /* *INDENT-OFF* */
    static struct caps_struct {
	long  code;
	char **cap_string;
    } tbl[] = {
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
    /* *INDENT-ON* */

    char *buf;
    struct caps_struct *csp;
    int i, buf_len, ret_len;
    long rqst_code;
    static char storage[1024];

    for (i = 0, buf = storage, csp = tbl; i < TABLESIZE(tbl); i++, csp++) {
	buf_len = (storage + sizeof(storage)) - buf;
	if (!$VMS_STATUS_SUCCESS(smg$get_term_data(&termtype,
						   &csp->code,
						   &buf_len,
						   &ret_len,
						   buf)) || ret_len == 0) {
	    *(csp->cap_string) = NULL;
	} else {
	    buf[ret_len] = 0;
	    *(csp->cap_string) = buf;
	    buf += ret_len + 1;
	}
    }
    rqst_code = SMG$K_COLUMNS;
    if (!$VMS_STATUS_SUCCESS(smg$get_numeric_data(&termtype,
						  &rqst_code,
						  &narrow_cols))) {
	narrow_cols = 80;
    }
    rqst_code = SMG$K_WIDE_SCREEN_COLUMNS;
    if (!$VMS_STATUS_SUCCESS(smg$get_numeric_data(&termtype,
						  &rqst_code,
						  &wide_cols))) {
	wide_cols = 132;
    }
    term.maxcols = wide_cols;
}

/** I/O information block definitions **/
struct iosb {			/* I/O status block                     */
    short i_cond;		/* Condition value                      */
    short i_xfer;		/* Transfer count                       */
    long i_info;		/* Device information                   */
};
struct termchar {		/* Terminal characteristics             */
    char t_class;		/* Terminal class                       */
    char t_type;		/* Terminal type                        */
    short t_width;		/* Terminal width in characters         */
    long t_mandl;		/* Terminal's mode and length           */
    long t_extend;		/* Extended terminal characteristics    */
};
static struct termchar tc;	/* Terminal characteristics             */

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
    status = sys$qiow(0,	/* Wait on event flag zero      */
		      fd,	/* Channel to input terminal    */
		      IO$_SENSEMODE,	/* Get current characteristic   */
		      &iostatus,	/* Status after operation       */
		      0, 0,	/* No AST service               */
		      &tc,	/* Terminal characteristics buf */
		      sizeof(tc),	/* Size of the buffer           */
		      0, 0, 0, 0);	/* P3-P6 unused                 */

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
vmsvt_open(void)
{
    QIO_SB iosb;
    int status;
    $DESCRIPTOR(odsc, "SYS$COMMAND");

    int i, keyseq_tablesize;
    struct vmskeyseqs *keyseqs;

    if (!already_open) {
	already_open = TRUE;

	get_terminal_type();
	if (tc.t_type == TT$_UNKNOWN) {
	    printf("Terminal type is unknown!\n");
	    printf("Set your terminal type using $ SET TERMINAL/INQUIRE\n");
	    tidy_exit(3);
	}
	if (tc.t_type != TT$_VT52) {
	    keyseqs = vt100seqs;
	    keyseq_tablesize = TABLESIZE(vt100seqs);
	} else {
	    keyseqs = vt52seqs;
	    keyseq_tablesize = TABLESIZE(vt52seqs);
	}

	/* Access the system terminal definition table for the          */
	/* information of the terminal type returned by IO$_SENSEMODE   */
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
	 * for insert_line or for delete_line.  (Have to work up a test for
	 * that) - TD
	 */

	if (scroll_regn && scroll_back) {
	    if (scroll_forw == NULL)	/* assume '\n' scrolls forward */
		scroll_forw = "\n";
	    term.scroll = vmsvt_scroll_reg;
	} else if (delete_line && insert_line)
	    term.scroll = vmsvt_scroll_delins;
	else
	    term.scroll = nullterm_scroll;

	/* Set resolution */
	(void) strcpy(screen_desc,
		      (term.cols == narrow_cols)
		      ? "NORMAL"
		      : "WIDE");

	/* Open terminal I/O drivers */
	status = sys$assign(&odsc, &iochan, 0, 0);
	if (status != SS$_NORMAL)
	    tidy_exit(status);
	status = sys$qiow(EFN, iochan, IO$_SENSEMODE, &iosb, 0, 0,
			  oldmode, sizeof(oldmode), 0, 0, 0, 0);
	if (status != SS$_NORMAL
	    || iosb.status != SS$_NORMAL)
	    tidy_exit(status);
	newmode[0] = oldmode[0];
	newmode[1] = oldmode[1];
	newmode[1] |= TT$M_NOBRDCST;	/* turn on no-broadcast */
	newmode[1] &= ~TT$M_TTSYNC;
	newmode[1] &= ~TT$M_ESCAPE;	/* turn off escape-processing */
	newmode[1] &= ~TT$M_HOSTSYNC;
	newmode[1] &= ~TT$M_NOTYPEAHD;	/* turn off no-typeahead */
	newmode[2] = oldmode[2];
	newmode[2] |= TT2$M_PASTHRU;	/* turn on pass-through */
	newmode[2] |= TT2$M_ALTYPEAHD;	/* turn on big typeahead buffer */
	status = sys$qiow(EFN, iochan, IO$_SETMODE, &iosb, 0, 0,
			  newmode, sizeof(newmode), 0, 0, 0, 0);
	if (status != SS$_NORMAL
	    || iosb.status != SS$_NORMAL)
	    tidy_exit(status);
	term.rows = (newmode[1] >> 24);
	term.cols = newmode[0] >> 16;

	/* make sure backspace is bound to backspace */
	asciitbl[backspc] = &f_backchar_to_bol;

	/* Set predefined keys */
	for (i = keyseq_tablesize; i--;)
	    addtosysmap(keyseqs[i].seq, strlen(keyseqs[i].seq), keyseqs[i].code);
    }
}

/* copied/adapted from 'tcap.c' 19-apr-1993 dickey@software.org */

/* move howmany lines starting at from to to */
static void
vmsvt_scroll_reg(int from, int to, int n)
{
    int i;
    if (to == from)
	return;
    if (to < from) {
	vmsvt_scrollregion(to, from + n - 1);
	vmsvt_move(from + n - 1, 0);
	for (i = from - to; i > 0; i--)
	    vmsvt_puts(scroll_forw);
    } else {			/* from < to */
	vmsvt_scrollregion(from, to + n - 1);
	vmsvt_move(from, 0);
	for (i = to - from; i > 0; i--)
	    vmsvt_puts(scroll_back);
    }
    vmsvt_scrollregion(0, term.rows - 1);
}

/*
OPT_PRETTIER_SCROLL is prettier but slower -- it scrolls
		a line at a time instead of all at once.
*/

/* move howmany lines starting at from to to */
static void
vmsvt_scroll_delins(int from, int to, int n)
{
    int i;
    if (to == from)
	return;
    /* patch: should make this more like 'tcap.c', or merge logic somehow */
#if OPT_PRETTIER_SCROLL
    if (absol(from - to) > 1) {
	vmsvt_scroll_delins(from, (from < to) ? to - 1 : to + 1, n);
	if (from < to)
	    from = to - 1;
	else
	    from = to + 1;
    }
#endif
    if (to < from) {
	vmsvt_move(to, 0);
	for (i = from - to; i > 0; i--)
	    vmsvt_puts(delete_line);
	vmsvt_move(to + n, 0);
	for (i = from - to; i > 0; i--)
	    vmsvt_puts(insert_line);
    } else {
	vmsvt_move(from + n, 0);
	for (i = to - from; i > 0; i--)
	    vmsvt_puts(delete_line);
	vmsvt_move(from, 0);
	for (i = to - from; i > 0; i--)
	    vmsvt_puts(insert_line);
    }
}

/* cs is set up just like cm, so we use tgoto... */
static void
vmsvt_scrollregion(int top, int bot)
{
    vmsvt_tgoto(SMG$K_SET_SCROLL_REGION, top + 1, bot + 1);
}

/***
 *  Change screen resolution
 *
 *  support these values:   WIDE -> 132 columns, NORMAL -> 80 columns
 *
 *  T -> if resolution successfully changed, F otherwise.
 ***/
static int
vmsvt_cres(const char *res)
{
    char buf[NLINE];
    int rc = FALSE;

    if (tc.t_type == TT$_VT52) {
	mlforce("[sres not supported for VT52-style terminals]");
	return (rc);
    }

    strcpy(buf, res);
    mkupper(buf);
    if (strcmp(buf, "WIDE") == 0 && set_wide != 0) {
	vmsvt_puts(set_wide);
	term.cols = wide_cols;
	rc = TRUE;
    } else if (strcmp(buf, "NORMAL") == 0 && set_narrow != 0) {
	vmsvt_puts(set_narrow);
	term.cols = narrow_cols;
	rc = TRUE;
    } else
	mlforce("[invalid sres value (use NORMAL or WIDE)]");
    if (rc)
	newwidth(1, term.cols);
    return (rc);
}

static void
vmsvt_close(void)
{
    if (already_open) {
	/*
	 * Note: this code used to check for errors when closing the output,
	 * but it didn't work properly (left the screen set in 1-line mode)
	 * when I was running as system manager, so I took out the error
	 * checking -- T.Dickey 94/7/15.
	 */
	int status;
	QIO_SB iosb;

	if (tc.t_type != TT$_VT52) {
	    /*
	     * Restore terminal width to previous state (if necessary) and then
	     * cleanup as usual.
	     */
	    if (tc.t_width != term.cols)
		vmsvt_cres((tc.t_width == narrow_cols) ? "NORMAL" : "WIDE");
	}
	vmsvt_flush();
	status = sys$qiow(EFN, iochan, IO$_SETMODE, &iosb, 0, 0,
			  oldmode, sizeof(oldmode), 0, 0, 0, 0);
	if (status != SS$_IVCHAN)
	    (void) sys$dassgn(iochan);

	already_open = FALSE;
    }
}

/***
 *  Ring the bell
 *
 *  Nothing returned
 ***/
static void
vmsvt_beep(void)
{
#if OPT_FLASH
    int hit = 0;

    if (global_g_val(GMDFLASH) && dark_off != NULL && dark_on != NULL) {
	hit = 1;
	vmsvt_puts(dark_off);
	term.flush();
	catnap(200, FALSE);
	vmsvt_puts(dark_on);
    }
    if (!hit && tc.t_type != TT$_VT52) {
	/* *INDENT-OFF* */
	static char *seq[][2] = {
	    { NULL, NULL },		       /* vtflash = off */
	    { VTFLASH_NORMSEQ, VTFLASH_REVSEQ }, /* reverse */
	    { VTFLASH_REVSEQ, VTFLASH_NORMSEQ }, /* normal	*/
	};
	/* *INDENT-ON* */

	char *str1, *str2;
	int val;

	val = global_g_val(GVAL_VTFLASH);
	str1 = seq[val][0];
	if (str1) {
	    str2 = seq[val][1];
	    vmsvt_puts(str1);
	    term.flush();
	    catnap(200, FALSE);
	    vmsvt_puts(str2);
	    hit = 1;
	}
    }
    if (!hit)
#endif
	vmsvt_putc(BEL);
}

static void
read_vms_tty(int length)
{
    int status;
    QIO_SB iosb;
    int term[2] =
    {0, 0};
    unsigned mask = (IO$_READVBLK
		     | IO$M_NOECHO
		     | IO$M_NOFILTR
		     | IO$M_TRMNOECHO);

    status = sys$qiow(EFN, iochan,
		      ((length == 1)
		       ? mask
		       : mask | IO$M_TIMED),
		      &iosb, 0, 0, ibuf, length, 0, term, 0, 0);

    if (status != SS$_NORMAL)
	tidy_exit(status);
    if (iosb.status == SS$_ENDOFFILE)
	tidy_exit(status);

    nibuf = iosb.count;
    ibufi = 0;
}

/*
 * Read a character from the terminal, performing no editing and doing no echo
 * at all.  More complex in VMS than almost anyplace else, which figures.
 */
static int
vmsvt_getc(void)
{
    while (ibufi >= nibuf) {
	if (!vmsvt_typahead())
	    read_vms_tty(1);
    }
    return (ibuf[ibufi++] & 0xFF);	/* Allow multinational  */
}

/*
 * Write a character to the display. On VMS, terminal output is buffered, and
 * we just put the characters in the big array, after checking for overflow.
 */
static OUTC_DCL
vmsvt_putc(int c)
{
    if (nobuf >= NOBUF)
	vmsvt_flush();
    obuf[nobuf++] = c;
    OUTC_RET c;
}

static int
vmsvt_typahead(void)
{
    if (ibufi >= nibuf) {
	read_vms_tty(NIBUF);
	return (nibuf > 0);
    }
    return TRUE;
}

/*
 * Flush terminal buffer. Does real work where the terminal output is buffered
 * up. A no-operation on systems where byte at a time terminal I/O is done.
 */
static void
vmsvt_flush(void)
{
    QIO_SB iosb;

    if (nobuf != 0) {
	(void) sys$qiow(EFN, iochan, IO$_WRITELBLK | IO$M_NOFORMAT,
			&iosb, 0, 0, obuf, nobuf, 0, 0, 0, 0);
	nobuf = 0;
    }
}

static void
vmsvt_clean(int f)
{
    if (f)
	term.openup();

    term.flush();
    term.close();
    term.kclose();
}

static void
vmsvt_unclean(void)
{
}

/* Dispatch table. */
TERM term =
{
    24,				/* Max number of rows allowable */
				/* Filled in */ 0,
				/* Current number of rows used  */
    132,			/* Max number of columns        */
				/* Filled in */ 0,
				/* Current number of columns    */
    dumb_set_encoding,
    dumb_get_encoding,
    vmsvt_open,			/* Open terminal at the start   */
    vmsvt_close,		/* Close terminal at end        */
    nullterm_kopen,		/* Open keyboard                */
    nullterm_kclose,		/* Close keyboard               */
    vmsvt_clean,		/* cleanup keyboard             */
    vmsvt_unclean,		/* uncleanup keyboard           */
    nullterm_openup,
    vmsvt_getc,			/* Get character from keyboard  */
    vmsvt_putc,			/* Put character to display     */
    vmsvt_typahead,		/* char ready for reading       */
    vmsvt_flush,		/* Flush output buffers         */
    vmsvt_move,			/* Move cursor, origin 0        */
    vmsvt_eeol,			/* Erase to end of line         */
    vmsvt_eeop,			/* Erase to end of page         */
    vmsvt_beep,			/* Beep                         */
#if OPT_VIDEO_ATTRS
    vmsvt_attr,			/* Set attribute video state    */
#else
    vmsvt_rev,			/* Set reverse video state      */
#endif
    vmsvt_cres,			/* Change screen resolution     */
    nullterm_setfore,		/* N/A: Set foreground color    */
    nullterm_setback,		/* N/A: Set background color    */
    nullterm_setpal,		/* N/A: Set palette colors      */
    nullterm_setccol,
    nullterm_scroll,		/* set at init-time             */
    nullterm_pflush,
    nullterm_icursor,
    nullterm_settitle,
    nullterm_watchfd,
    nullterm_unwatchfd,
    nullterm_cursorvis,
    nullterm_mopen,
    nullterm_mclose,
    nullterm_mevent,
};

#endif
