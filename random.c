/*
 * This file contains the command processing functions for a number of random
 * commands. There is no functional grouping here, for sure.
 *
 * $Id: random.c,v 1.361 2025/01/26 14:29:47 tom Exp $
 */

#ifdef __BEOS__
#include <OS.h>
#endif

#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"

#if DISP_X11 && defined(HAVE_X11_XPOLL_H)
#include <X11/Xpoll.h>
#endif

#if SYS_UNIX
#if defined(HAVE_POLL) && defined(HAVE_POLL_H)
# include <poll.h>
#endif
#endif

#if SYS_VMS
#include	<cvtdef.h>	/* for float constants */
#endif

#include	"dirstuff.h"

#if CC_TURBO
#include <dir.h>		/* for 'chdir()' */
#endif

#if SYS_VMS
#include <starlet.h>
#include <lib$routines.h>
#endif

#define DIRS_FORCE    0		/* force DirStack buffer on screen if dirs_idx > 0.
				 * If DirStack on screen and dirs_idx == 0, kill
				 * buffer (if possible).
				 */
#define DIRS_OPT      1		/* update dirstack display if DirStack buffer on
				 * screen.  If not on screen, do nothing.  If
				 * DirStack on screen and dirs_idx == 0,
				 * kill buffer (if possible).
				 */
#define DIRS_SUPPRESS 2		/* if DirStack buffer on screen, kill it. */

static int did_chdir = FALSE;	/* set when $PWD is no longer valid */

#if OPT_SHELL

static char prevdir[NFILEN];

static int display_dirstack(int);
static int pushd_popd_active, dirs_add_active;

/*
 * Implement "dirstack" with an extensible array of pointers to char
 *
 * The top element of the stack is always understood to be <cwd>, but is
 * _not_ stored in the array.
 */
static int dirs_idx;		/* next array cell that can store a dir path */
static int dirs_len;		/* #cells allocated in dirstack */
static char **dirstack;

#endif

/*--------------------------------------------------------------------------*/

/*
 * Set default parameters for an automatically-generated, view-only buffer.
 * This is invoked after buffer creation, but usually before the buffer is
 * loaded with text.
 * the "mode" argument should be MDVIEW or MDREADONLY
 */
void
set_rdonly(BUFFER *bp, const char *name, int mode)
{
    ch_fname(bp, name);

    b_clr_changed(bp);		/* assumes text is loaded... */
    bp->b_active = TRUE;

    set_local_b_val(bp, mode, TRUE);
    set_local_b_val(bp, VAL_TAB, 8);
    set_local_b_val(bp, MDDOS, system_crlf);

    fix_cmode(bp, FALSE);
}

/*
 * This is a generic "lister", which takes care of popping a window/buffer pair
 * under the given name, and calling "func" with a couple of args to fill in
 * the buffer.
 */
int
liststuff(const char *name,
	  int appendit,
	  void (*func) (LIST_ARGS),	/* ptr to function to execute */
	  int iarg,
	  void *vargp)
{
    int rc = FALSE;
    BUFFER *bp;
    WINDOW *wp;
    int alreadypopped;
    BUFFER *ocurbp = curbp;
    L_NUM save_line = -1;
    C_NUM save_off = -1;
    int save_top = -1;

    TRACE((T_CALLED
	   "liststuff(name=%s, appendit=%d, func=%p, iarg=%d, vargp=%p)\n",
	   TRACE_NULL(name), appendit, func, iarg, (void *) vargp));

    /* create the buffer list buffer   */
    bp = bfind(name, BFSCRTCH);
    if (bp != NULL) {

#ifdef MDREUSE_POS
	if ((appendit <= 0)
	    && b_val(bp, MDREUSE_POS)
	    && (wp = bp2any_wp(bp)) != NULL) {
	    save_line = line_no(bp, wp->w_dot.l);
	    save_off = wp->w_dot.o;
	    save_top = line_no(bp, wp->w_line.l);
	}
#endif

	/* clear old text (?) */
	if ((appendit > 0) || (rc = bclear(bp)) == TRUE) {

	    b_set_scratch(bp);
	    alreadypopped = (bp->b_nwnd != 0);
	    if (popupbuff(bp) == FALSE) {
		(void) zotbuf(bp);
	    } else {

		if ((wp = bp2any_wp(bp)) != NULL) {
		    make_local_w_val(wp, WMDNUMBER);
		    set_w_val(wp, WMDNUMBER, FALSE);
		}
		if (appendit > 0) {
		    (void) gotoeob(FALSE, 1);
		    MK = DOT;
		}
		/*
		 * call the passed in function, giving it both the integer and
		 * character pointer arguments
		 */
		(*func) (iarg, vargp);
		if (alreadypopped && (appendit > 0)) {
		    (void) gomark(FALSE, 1);
		    (void) forwbline(FALSE, 1);
		    (void) reposition(FALSE, 1);
		} else if (wp != NULL && (appendit < 0 && save_line >= 0)) {
		    (void) vl_gotoline(save_top);
		    wp->w_line.l = wp->w_dot.l;
		    wp->w_line.o = 0;
		    if (save_line != save_top)
			(void) vl_gotoline(save_line);
		    gocol(save_off);
		} else {
		    /* if we're not appending, go to the top */
		    (void) gotobob(FALSE, 1);
		}
		set_rdonly(bp, non_filename(), MDVIEW);

		if (alreadypopped) {
		    /* don't switch to the popup if it wasn't there */
		    swbuffer(ocurbp);
		} else {
		    /* only resize if it's fresh */
		    shrinkwrap();
		}

		rc = TRUE;
	    }
	}
    }
    returnCode(rc);
}

/*
 * Display the current position of the cursor, lines and columns, in the file,
 * the character that is under the cursor (in hex), and the fraction of the
 * text that is before the cursor. The displayed column is not the current
 * column, but the column that would be used on an infinite width display.
 */
/* ARGSUSED */
int
showcpos(int f GCC_UNUSED, int n GCC_UNUSED)
{
#if OPT_POSFORMAT
    static TBUFF *result;
#else
    LINE *lp;			/* current line */
    B_COUNT numchars = 0;	/* # of chars in file */
    L_NUM numlines = 0;		/* # of lines in file */
    B_COUNT predchars = 0;	/* # chars preceding point */
    L_NUM predlines = 0;	/* # lines preceding point */
    int curchar = '\n';		/* character under cursor */
    long ratio;
    C_NUM col;
    C_NUM savepos;		/* temp save for current offset */
    C_NUM ecol;			/* column pos/end of current line */
#endif

#if CC_WATCOM || CC_DJGPP	/* for testing interrupts */
    if (f && n == 11) {
	mlwrite("DOS interrupt test.  hit control-C or control-BREAK");
	while (!interrupted())
	    continue;
	mlwrite("whew.  got interrupted");
	return ABORT;
    }
#endif
#if OPT_POSFORMAT
    if (is_empty_buf(curbp)) {
	mlforce("File is empty");
    } else {
	special_formatter(&result, position_format, curwp);
	mlforce("%s", tb_values(result));
    }
#else
    /* count chars and lines */
    for_each_line(lp, curbp) {
	/* if we are on the current line, record it */
	if (lp == DOT.l) {
	    predlines = numlines;
	    predchars = numchars + DOT.o;
	    curchar = CharAtDot();
	}
	/* on to the next line */
	++numlines;
	numchars += llength(lp) + 1;
    }

    if (!b_val(curbp, MDNEWLINE))
	numchars--;

    /* if at end of file, record it */
    if (is_header_line(DOT, curbp)) {
	predlines = numlines;
	predchars = numchars;
    }

    /* Get real column and end-of-line column. */
    col = mk_to_vcol(curwp, DOT, FALSE, 0, 0);
    savepos = DOT.o;
    DOT.o = llength(DOT.l);
    ecol = mk_to_vcol(curwp, DOT, FALSE, 0, 0);
    DOT.o = savepos;

    ratio = PERCENT(predchars, numchars);

    /* summarize and report the info */
    if (numlines == 0 && numchars == 0) {
	mlforce("File is empty");
    } else {
	mlforce("Line %d of %d, Col %d of %d, Char %ld of %ld (%ld%%) char is 0x%x or 0%o",
		predlines + 1, numlines,
		col + 1, ecol,
		predchars + 1, numchars,
		ratio, curchar, curchar);
    }
#endif
    return TRUE;
}

/* ARGSUSED */
int
showlength(int f GCC_UNUSED, int n GCC_UNUSED)
{
    /* actually, can be used to show any address-value */
    mlforce("%d", line_no(curbp, MK.l));
    return TRUE;
}

int
line_report(L_NUM before)
{
    L_NUM after = vl_line_count(curbp);

    if (do_report(before - after)) {
	if (before > after)
	    mlwrite("[%d fewer lines]", before - after);
	else
	    mlwrite("[%d more lines]", after - before);
	return TRUE;
    }
    return FALSE;
}

/*
 * Restore a saved value of DOT when it is possible that the buffer may have
 * been modified since the value was saved.  In that case, leave DOT as is.
 */
int
restore_dot(MARK saved_dot)
{
    if (samepoint(DOT, saved_dot)) {
	return TRUE;
    } else if (!samepoint(saved_dot, nullmark)) {
	LINE *lp;
	for_each_line(lp, curbp) {
	    if (lp == saved_dot.l) {
		DOT = saved_dot;
		curwp->w_flag |= WFMOVE;
		return TRUE;
	    }
	}
    }
    return FALSE;
}

L_NUM
vl_line_count(BUFFER *the_buffer)
{
    L_NUM numlines = 0;		/* # of lines in file */
    if (the_buffer != NULL) {
#if SMALLER
	LINE *lp;		/* current line */

	for_each_line(lp, the_buffer)
	    ++ numlines;
#else
	(void) bsizes(the_buffer);
	numlines = the_buffer->b_linecount;
#endif
    }
    return numlines;
}

/* return the number of the given line */
L_NUM
line_no(BUFFER *the_buffer, LINE *the_line)
{
    L_NUM line_num = 0;

    if (the_line != NULL) {
#if SMALLER
	LINE *lp;

	line_num = 1;
	for_each_line(lp, the_buffer) {
	    if (lp == the_line)	/* found current line? */
		break;
	    ++line_num;
	}
#else
	(void) bsizes(the_buffer);
	if ((line_num = the_line->l_number) == 0)
	    line_num = the_buffer->b_linecount + 1;
#endif
    }
    return line_num;
}

#if OPT_EVAL
/*
 * Returns the index of the mark in the buffer
 */
B_COUNT
char_no(BUFFER *the_buffer, MARK the_mark)
{
    LINE *lp;
    B_COUNT curchar = 0;
    B_COUNT len_rs = (B_COUNT) len_record_sep(curbp);

    for_each_line(lp, the_buffer) {
	if (lp == the_mark.l) {
	    curchar += (B_COUNT) the_mark.o;
	    break;
	}
	curchar += line_length(lp);
    }

    return curchar;
}

B_COUNT
vl_getcchar(void)
{
    return char_no(curbp, DOT);
}

/* return the line number the cursor is on */
L_NUM
getcline(void)
{
    return line_no(curbp, DOT.l);
}
#endif /* OPT_EVAL */

#if !SMALLER
/*
 * Go to the given character index in the buffer.
 */
int
gotochr(int f, int n)
{
    LINE *lp;
    B_COUNT len;
    B_COUNT len_rs = (B_COUNT) len_record_sep(curbp);

    if (!f) {
	DOT.l = lback(buf_head(curbp));
	DOT.o = llength(DOT.l);
    } else {
	B_COUNT goal = (B_COUNT) n;
	for_each_line(lp, curbp) {
	    len = line_length(lp);
	    if (goal <= len) {
		DOT.l = lp;
		/* we cannot move into the middle of the line-separator */
		if ((DOT.o = (C_NUM) (goal - 1)) >= llength(lp))
		    DOT.o = llength(lp) - 1;
		goal = 0;
		break;
	    }
	    goal -= len;
	}
	if (goal > 0)
	    return FALSE;
    }
    curwp->w_flag |= WFMOVE;
    return TRUE;
}

/*
 * Go to the given percent index in the buffer, i.e., the line that matches
 * the given percent.
 */
int
gotopct(int f, int n)
{
    int status = FALSE;

    if (f && (n >= 0 && n <= 100)) {
	bsizes(curbp);
	status = gotoline(TRUE,
			  (int) ((curbp->b_linecount) * (n / 100.0) + 0.5));
    }
    return status;
}
#endif

/*
 * Get the (possibly wide) character at the given line and offset.
 *
 * If this is a wide character and the buffer is encoded for UTF-8,
 * replace the whole character at that offset, possibly changing the
 * length of the line.
 */
void
set_char2(LINE *lp, int offset, int ch)
{
#if OPT_MULTIBYTE
    if (b_is_utfXX(curbp)) {
	UCHAR buffer[MAX_UTF8];
	int old_len = bytes_at(lp, offset);
	int new_len = vl_conv_to_utf8(buffer, (UINT) ch, sizeof(buffer));
	int n;

	if (new_len > old_len) {
	    lins_bytes(new_len - old_len, ' ');
	} else if (new_len < old_len) {
	    ldel_bytes((B_COUNT) (old_len - new_len), FALSE);
	}
	for (n = 0; n < new_len; ++n)
	    lputc(lp, offset + n, buffer[n]);
    } else
#endif
	lputc(lp, offset, (char) ch);
}

/*
 * Get the (possibly wide) character at the given line and offset.
 */
int
get_char2(LINE *lp, int offset)
{
    int result = -1;

    if (offset >= 0 && offset < llength(lp)) {
	result = CharOf(lgetc(lp, offset));
#if OPT_MULTIBYTE
	if (b_is_utfXX(curbp)) {
	    UINT target;
	    int used = vl_conv_to_utf32(&target,
					lvalue(lp) + offset,
					(B_COUNT) (llength(lp) - offset));
	    if (used > 0) {
		result = (int) target;
	    }
	}
#endif
    }
    return result;
}

/*
 * Return the character at the given MARK, in the current buffer.
 */
int
char_at_mark(MARK mark)
{
    int curchar;

    if (mark.o == llength(mark.l)) {
	curchar = '\n';
    } else {
	curchar = get_char2(mark.l, mark.o);
    }
    return curchar;
}

/*
 * Return the screen column in any line given an offset.
 * Assume the line is in curwp/curbp.
 *
 * If 'actual' is false: compute the effective column (expand tabs),
 * otherwise compute the actual column (depends on 'list' mode)
 */
int
getcol(MARK mark, int actual)
{
    C_NUM i;
    C_NUM col = 0;

    if (mark.l != NULL
	&& llength(mark.l) > 0) {
	if (actual) {
	    col = offs2col(curwp, mark.l, mark.o) - nu_width(curwp);
	} else {
	    C_NUM len = mark.o;
	    if (len > llength(mark.l))
		len = llength(mark.l);
	    for (i = w_left_margin(curwp); i < len; i += BytesAt(mark.l, i)) {
		col = next_column(mark.l, i, col);	/* assumes curbp */
	    }
	    /*
	     * If we are positioning the cursor on the right-side of a multi-
	     * column character, update the goal to reflect that.
	     */
	    if (!global_g_val(GMDALTTABPOS) && i < llength(mark.l)) {
		col = (next_column(mark.l, i, col) - 1);
	    }
	}
    }
    return col;
}

/*
 * Return current screen column.  Stop at first non-blank given TRUE argument.
 */
int
getccol(int bflg)
{
    return getcol(DOT, bflg);
}

int
need_a_count(int f, int n, int need)
{
    if (!f)
	n = need;
    return n;
}

int
need_at_least(int f, int n, int need)
{
    return ((!f || n < need) ? need : n);
}

/*
 * Set current column, based on counting from 1
 */
int
gotocol(int f, int n)
{
    n = need_at_least(f, n, 1);
    return gocol(n - 1);
}

/* given a column, return the offset */
/*  if there aren't that many columns, return how too few we were */
/*	also, if non-null, return the column we _did_ reach in *rcolp */
int
getoff(C_NUM goal, C_NUM * rcolp)
{
    C_NUM offs;
    C_NUM ccol = 0;
    C_NUM llen = llength(DOT.l);

    /* calculate column as we move across line */
    for (offs = w_left_margin(curwp); offs < llen;) {

	if (ccol >= goal)
	    break;

	/* move right */
	ccol = next_column(DOT.l, offs, ccol);
	if (ccol > goal)
	    break;
	offs += BytesAt(DOT.l, offs);
    }

    if (rcolp)
	*rcolp = ccol;

    if (ccol < goal) {		/* we did not make it */
	offs = ccol - goal;	/* show how far short (in columns) we were */
    }
    return offs;
}

/* really set column, based on counting from 0, for internal use */
int
gocol(int n)
{
    int offs;			/* column number the cursor is on */

    if (DOT.l != NULL) {
	offs = getoff(n, (C_NUM *) 0);

	if (offs >= 0) {
	    DOT.o = offs;
	    return TRUE;
	}

	DOT.o = llength(DOT.l) - 1;
    }
    return FALSE;

}

#if ! SMALLER
/*
 * Twiddle the two characters on under and to the left of dot.  If dot is
 * at the end of the line twiddle the two characters before it.  This fixes
 * up a very common typo with a single stroke.  This always works within a
 * line, so "WFEDIT" is good enough.
 */
/* ARGSUSED */
int
twiddle(int f GCC_UNUSED, int n GCC_UNUSED)
{
    MARK dot;
    int cl;
    int cr;

    dot = DOT;
    if (llength(dot.l) <= 1)
	return FALSE;
    if (is_at_end_of_line(dot))
	dot.o -= BytesBefore(dot.l, dot.o);
    if (dot.o == 0)
	dot.o = BytesAt(dot.l, 0);
    cr = get_char2(dot.l, dot.o);
    dot.o -= BytesBefore(dot.l, dot.o);
    cl = get_char2(dot.l, dot.o);
    CopyForUndo(dot.l);
    set_char2(dot.l, dot.o, cr);
    dot.o += BytesAt(dot.l, dot.o);
    set_char2(dot.l, dot.o, cl);
    DOT.o = dot.o;
    chg_buff(curbp, WFEDIT);
    return (TRUE);
}
#endif

/*
 * Force zero or more blank lines at dot.  no argument leaves no blanks.
 * Lines will be deleted or added as needed to match the argument if it
 * is given.
 */
int
forceblank(int f, int n)
{
    LINE *lp1;
    LINE *lp2;
    B_COUNT nld;
    B_COUNT n_arg;
    C_NUM nchar;
    int s = TRUE;
    B_COUNT len_rs = (B_COUNT) len_record_sep(curbp);

    n_arg = (B_COUNT) need_at_least(f, n, 0);

    lp1 = DOT.l;
    /* scan backward */
    while (firstchar(lp1) < 0 &&
	   (lp2 = lback(lp1)) != buf_head(curbp))
	lp1 = lp2;
    lp2 = lp1;

    nld = 0;
    nchar = 0;

    /* scan forward */
    while ((lp2 = lforw(lp2)) != buf_head(curbp) &&
	   firstchar(lp2) < 0) {
	++nld;
	if (nld > n_arg && lisreal(lp2))
	    nchar += (C_NUM) line_length(lp2);
    }

    DOT.l = lforw(lp1);
    DOT.o = b_left_margin(curbp);

    if (n_arg == nld) {		/* things are just right */
	/*EMPTY */ ;
    } else if (n_arg < nld) {	/* delete (nld - n_arg) lines */
	DOT.l = lp2;
	DOT.o = b_left_margin(curbp);
	if (nchar) {
	    backchar(TRUE, nchar);
	    s = ldel_chars((B_COUNT) nchar, FALSE);
	}
    } else {			/* insert (n_arg - nld) lines */
	n_arg = n_arg - nld;
	while (s && n_arg--)
	    s = lnewline();
    }

    /* scan forward */
    while ((firstchar(DOT.l) < 0) &&
	   (DOT.l != buf_head(curbp))) {
	dot_next_bol();
    }

    return s;
}

/* '~' is the traditional vi flip: flip a char and advance one */
int
flipchar(int f, int n)
{
    int s;

    havemotion = &f_forwchar_to_eol;
    s = operflip(f, n);
    if (s != TRUE)
	return s;
    return forwchar_to_eol(f, n);
}

/* 'x' is synonymous with 'd<space>' */
int
forwdelchar(int f, int n)
{
    havemotion = &f_forwchar_to_eol;
    return operdel(f, n);
}

/* 'X' is synonymous with 'd<backspace>' */
int
backdelchar(int f, int n)
{
    havemotion = &f_backchar_to_bol;
    return operdel(f, n);
}

/* 'D' is synonymous with 'd$' */
/* ARGSUSED */
int
deltoeol(int f GCC_UNUSED, int n GCC_UNUSED)
{
    if (is_empty_line(DOT))
	return TRUE;

    havemotion = &f_gotoeol;
    return operdel(FALSE, 1);
}

/* 'C' is synonymous with 'c$' */
int
chgtoeol(int f, int n)
{
    if (is_empty_line(DOT)) {
	return ins();
    } else {
	havemotion = &f_gotoeol;
	return operchg(f, n);
    }
}

/* 'Y' is synonymous with 'yy' */
int
yankline(int f, int n)
{
    havemotion = &f_godotplus;
    return (operyank(f, n));
}

/* 'S' is synonymous with 'cc' */
int
chgline(int f, int n)
{
    havemotion = &f_godotplus;
    return (operchg(f, n));
}

/* 's' is synonymous with 'c<space>' */
int
chgchar(int f, int n)
{
    havemotion = &f_forwchar_to_eol;
    return (operchg(f, n));
}

/*	This function simply clears the message line,
		mainly for macro usage			*/

/* ARGSUSED */
int
clrmes(int f GCC_UNUSED, int n GCC_UNUSED)
{
    mlerase();
    return (TRUE);
}

#if ! SMALLER

/*	This function writes a string on the message line
		mainly for macro usage			*/

/* ARGSUSED */
int
writemsg(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int status;
    TBUFF *buf = NULL;

    if ((status = kbd_reply("Message to write: ", &buf, eol_history, '\n',
			    KBD_NORMAL, no_completion)) == TRUE)
	mlforce("%s", tb_values(buf));
    tb_free(&buf);
    return (status);
}

/* ARGSUSED */
int
userbeep(int f GCC_UNUSED, int n GCC_UNUSED)
{
    term.beep();
    return TRUE;
}
#endif /* !SMALLER */

#ifdef __BEOS__
/*
 * BeOS's select() is declared in socket.h, so the configure script
 * does not see it.  That's just as well, since that function works
 * only for sockets.  This (using snooze and ioctl) was distilled from
 * Be's patch for ncurses which uses a separate thread to simulate
 * select().
 *
 * FIXME: the return values from the ioctl aren't very clear if we get
 * interrupted.
 */
int
beos_has_input(int fd)
{
    int n = 0, howmany;

    /* this does not appear in any header-file */
    howmany = ioctl(fd, 'ichr', &n);
    return (howmany >= 0 && n > 0);
}

int
beos_can_output(int fd)
{
    int n = 0, howmany;

    /* this is a guess, based on the 'ichr' example, and seems to work */
    howmany = ioctl(fd, 'ochr', &n);
    return (howmany >= 0 && n > 0);
}

void
beos_napms(int milli)
{
    snooze(milli * 1000);
}
#endif

/*
 * Delay for the given number of milliseconds.  If "watchinput" is true,
 * then user input will abort the delay.  Return true if we did find user input.
 */
int
catnap(int milli, int watchinput)
{
#if DISP_X11
    if (watchinput)
	x_milli_sleep(milli);
    else
#endif
    {
#if SYS_UNIX
# if defined(HAVE_SELECT) && defined(HAVE_TYPE_FD_SET)

	struct timeval tval;
	fd_set read_bits;
#  ifdef HAVE_SIGPROCMASK
	sigset_t newset, oldset;
	sigemptyset(&newset);
	sigaddset(&newset, SIGALRM);
	sigprocmask(SIG_BLOCK, &newset, &oldset);
#  endif

	FD_ZERO(&read_bits);
	if (watchinput) {
	    FD_SET(0, &read_bits);
	}
	tval.tv_sec = milli / 1000;
	tval.tv_usec = (milli % 1000) * 1000;	/* microseconds */
	(void) select(1, &read_bits, (fd_set *) 0, (fd_set *) 0, &tval);

#  ifdef HAVE_SIGPROCMASK
	sigprocmask(SIG_SETMASK, &oldset, (sigset_t *) 0);
#  endif
	if (watchinput)
	    return FD_ISSET(0, &read_bits);

# else
#  if defined(HAVE_POLL) && defined(HAVE_POLL_H)

	struct pollfd pfd;
	int fdcnt;
	int found;
	if (watchinput) {
	    pfd.fd = 0;
	    pfd.events = POLLIN;
	    fdcnt = 1;
	} else {
	    fdcnt = 0;
	}
	found = poll(&pfd, fdcnt, milli);	/* milliseconds */

	if (watchinput && (found > 0))
	    return found;
#  else
#   ifdef __BEOS__
	if (watchinput) {
	    int d;
	    if (milli <= 0)
		milli = 1;

	    for (d = 0; d < milli; d += 5) {
		if (beos_has_input(0))
		    return TRUE;
		if (milli > 0)
		    beos_napms(5);
	    }
	} else if (milli > 0) {
	    beos_napms(milli);
	}
#   else

	int seconds = (milli + 999) / 1000;
	if (watchinput) {
	    while (seconds > 0) {
		sleep(1);
		if (term.typahead())
		    return TRUE;
		seconds -= 1;
	    }
	} else {
	    sleep(seconds);
	}

#   endif
#  endif
# endif
#define have_slept 1
#endif /* SYS_UNIX */

#if SYS_VMS
	float seconds = milli / 1000.;
	unsigned flags = 0;
#ifdef USE_IEEE_FLOAT
	unsigned type = CVT$K_IEEE_S;
#else
	unsigned type = CVT$K_VAX_F;
#endif
	if (watchinput) {
	    float tenth = .1;
	    while (seconds > 0.1) {
		lib$wait(&tenth, &flags, &type);
		if (term.typahead())
		    return TRUE;
		seconds -= tenth;
	    }
	}
	if (seconds > 0.0)
	    lib$wait(&seconds, &flags, &type);
#define have_slept 1
#endif

#if SYS_WINNT || (CC_NEWDOSCC && SYS_MSDOS) || (CC_WATCOM && (SYS_OS2 || SYS_MSDOS)) || DISP_VIO
#if SYS_WINNT
#define delay(a) Sleep(a)
#endif
#if DISP_VIO
#define delay(a) DosSleep(a)
#endif
	if (watchinput) {
	    while (milli > 100) {
		delay(100);
		if (term.typahead())
		    return TRUE;
		milli -= 100;
	    }
	}
	delay(milli);
#define have_slept 1
#endif

#ifndef have_slept
	long i;
	for (i = 0; i < term.pausecount; i++)
	    continue;
#endif
    }
    return FALSE;
}

static char current_dirname[NFILEN];

/* Return a string naming the current directory.  If we're unable to read the
 * directory name, return "."; otherwise we'll expect a fully-resolved path.
 */
char *
current_directory(int force)
{
    char *s;
    static char *cwd;

    if (!force && cwd)
	return cwd;
#ifdef HAVE_GETCWD
#ifdef HAVE_REALPATH
    /*
     * For slow (read:  simulated) filesystems, a getcwd() can be very slow.
     * We always make a call to this function during startup, but wish to avoid
     * the overhead of getcwd() in that case.  However, it is not unusual to
     * invoke vile from other applications, which do not set $PWD.  In that
     * case, the result from this function would be incorrect, and lots of
     * things break.
     *
     * Given the choice between broken and slow behavior, we can resolve it
     * with another environment variable, set in the user's shell.
     */
    s = ((did_chdir != 0 || getenv("VILE_PWD") == NULL)
	 ? NULL
	 : getenv("PWD"));
    cwd = realpath(s ? s : ".", current_dirname);
    if (cwd == NULL)
#endif
	cwd = getcwd(current_dirname, (size_t) NFILEN);
#else
# ifdef HAVE_GETWD
    cwd = getwd(current_dirname);
# else
    {
	FILE *f;
	int n;
	f = npopen("pwd", "r");
	if (f != NULL) {
	    n = fread(current_dirname, 1, NFILEN, f);
	    current_dirname[n] = EOS;
	    npclose(f);
	    cwd = current_dirname;
	} else
	    cwd = 0;
    }
# endif
#endif
    if (cwd == NULL) {
	cwd = current_dirname;
	current_dirname[0] = '.';
	current_dirname[1] = EOS;
    } else {
	cwd[NFILEN - 1] = EOS;
	s = strchr(cwd, '\n');
	if (s)
	    *s = EOS;
    }
#if OPT_MSDOS_PATH
    bsl_to_sl_inplace(cwd);
    lengthen_path(cwd);
    cwd[NFILEN - 1] = EOS;
    if (!global_g_val(GMDFILENAME_IC))
	mklower(cwd);
#endif

#if SYS_MSDOS || SYS_OS2
    update_dos_drv_dir(cwd);
#endif

#if OPT_VMS_PATH && !SYS_VMS
    strcat(cwd, "/");
    (void) unix2vms_path(cwd, cwd);
#endif

    TRACE(("current_directory(%s)\n", cwd));
    return cwd;
}

#if SYS_MSDOS || SYS_OS2

/* convert drive index to _letter_ */
static int
drive2char(UINT d)
{
    if (d >= 26) {
	mlforce("[Illegal drive index %d]", d);
	d = 0;
    }
    return (d + 'A');
}

/* convert drive _letter_ to index */
static UINT
char2drive(int d)
{
    if (isAlpha(d)) {
	if (isLower(d))
	    d = toUpper(d);
    } else {
	mlforce("[Not a drive '%c']", d);
	d = curdrive();
    }
    return (d - 'A');
}

/* returns drive _letter_ */
int
curdrive(void)
{
    int result;
#if SYS_OS2
    UINT d;
# if CC_CSETPP
    d = _getdrive();
# else
    _dos_getdrive(&d);		/* A=1 B=2 ... */
# endif
    result = drive2char((d - 1) & 0xff);
#else
    result = drive2char(bdos(0x19, 0, 0) & 0xff);
#endif
    return result;
}

/* take drive _letter_ as arg. */
int
setdrive(int d)
{
    if (isAlpha(d)) {
#if SYS_OS2
# if CC_CSETPP
	_chdrive(char2drive(d) + 1);
# else
	UINT maxdrives;
	_dos_setdrive(char2drive(d) + 1, &maxdrives);	/* 1 based */
# endif
#else
	bdos(0x0e, char2drive(d), 0);	/* 0 based */
#endif
	return TRUE;
    }
    mlforce("[Bad drive specifier]");
    return FALSE;
}

static int curd;		/* current drive-letter */
static char *cwds[26];		/* list of current dirs on each drive */

const char *
curr_dir_on_drive(int drive)
{
    int n = char2drive(drive);

    if (n != 0) {
	if (curd == 0)
	    curd = curdrive();

	if (cwds[n])
	    return cwds[n];
	else {
	    cwds[n] = castalloc(char, NFILEN);

	    if (cwds[n]) {
		if (setdrive(drive) == TRUE) {
		    (void) strcpy(cwds[n], current_directory(TRUE));
		    (void) setdrive(curd);
		    (void) current_directory(TRUE);
		    return cwds[n];
		}
	    }
	}
    }
    return current_directory(FALSE);
}

void
update_dos_drv_dir(char *cwd)
{
    char *s;

    if ((s = is_msdos_drive(cwd)) != 0) {
	int n = char2drive(*cwd);

	if (!cwds[n])
	    cwds[n] = castalloc(char, NFILEN);

	if (cwds[n])
	    (void) strcpy(cwds[n], s);
    }
}
#endif

#if OPT_SHELL
/* ARGSUSED */
int
vl_chdir(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int status;
    static TBUFF *last;
    char cdirname[NFILEN];

    status = mlreply_dir("Change to directory: ", &last, cdirname);
#if SYS_UNIX || SYS_VMS || SYS_WINNT
    if (status == FALSE) {	/* empty reply, go HOME */
	(void) vl_strncpy(cdirname, "~", sizeof(cdirname));
	status = TRUE;
    }
#endif
    if (status == TRUE)
	status = set_directory(cdirname);
    return status;
}

/* ARGSUSED */
int
pwd(int f GCC_UNUSED, int n GCC_UNUSED)
{
    mlforce("%s", current_directory(f));
    return TRUE;
}

static int
cd_and_pwd(char *path)
{
#if SYS_UNIX
    const char *p;
#endif

#if CC_CSETPP
#undef  chdir
#define chdir(p) _chdir(p)
#endif

    if (chdir(SL_TO_BSL(path)) == 0) {
	did_chdir = TRUE;

	if (dirs_add_active) {
	    /* the directory is legit -- that's all we care about */

	    return (TRUE);
	}
#if SYS_UNIX
	p = current_directory(TRUE);
	if (!is_slashc(*p)) {
	    if (is_slashc(*prevdir))
		p = lengthen_path(pathcat(current_dirname, prevdir, path));
	    sgarbf = TRUE;	/* some shells print to stderr */
	    update(TRUE);
	}

	mlforce("%s", p);
#else
	(void) pwd(TRUE, 1);
#endif
#if OPT_TITLE
	set_editor_title();
#endif
	run_a_hook(&cdhook);
	updatelistbuffers();

	/* if dirstack on screen, update it */
	(void) display_dirstack(DIRS_OPT);

#if SYS_WINNT && DISP_NTWIN
	/* if $recent-folders > 0, save directory path in registry */
	{
	    char cwd[FILENAME_MAX + 1];

	    /*
	     * must ask OS for cwd because "path" may be something like, say:
	     *
	     *   ../some_dir
	     *
	     * which is not usable later when "." changes.
	     */
	    if (getcwd(cwd, sizeof(cwd))) {
		/*
		 * Store path in Unix format, since that's the external
		 * presentation format used by the editor.
		 */
		bsl_to_sl_inplace(cwd);
		store_recent_file_or_folder(cwd, FALSE);
	    } else {
		return FALSE;
	    }
	}
#endif
	return TRUE;
    }
    return FALSE;
}

#if OPT_EVAL
char *
previous_directory(void)
{
    if (*prevdir)
	return prevdir;
    else
	return current_directory(FALSE);
}
#endif

/* move to the named directory.  (Dave Lemke) */
int
set_directory(const char *dir)
{
#define CHANGE_FAILED "[Couldn't change to \"%s\"]"

    char exdir[NFILEN], *exdp, cdpathdir[NFILEN], tmp[NFILEN];
    const char *cdpath;
#if SYS_MSDOS || SYS_OS2
    int curd = curdrive();
#endif
    int outlen;

    upmode();

    TRACE(("set_directory(%s)\n", dir));
    exdp = vl_strncpy(exdir, dir, sizeof(exdir));

    if (doglob(exdp)) {
#if SYS_MSDOS || SYS_OS2
	char *s;
	if ((s = is_msdos_drive(exdp)) != 0) {
	    if (setdrive(*exdp) == TRUE) {
		exdp = s;	/* skip device-part */
		if (!*exdp) {
		    return pwd(TRUE, 1);
		}
	    } else {
		return FALSE;
	    }
	}
#endif
	/*
	 * "cd -" switches to the previous directory.
	 */
	if (!strcmp(exdp, "-")) {
	    if (*prevdir)
		(void) strcpy(exdp, prevdir);
	    else {
		mlforce("[No previous directory]");
		return FALSE;
	    }
	}

	if (!dirs_add_active) {
	    /* Save current directory for subsequent "cd -". */

	    (void) vl_strncpy(prevdir, current_directory(FALSE), sizeof(prevdir));
	}
#if OPT_VMS_PATH
	if (!*exdp)
	    strcpy(exdp, "~");
	if (!strcmp(exdp, "/"))	/* cannot really "cd /" on vms */
	    lengthen_path(exdp);
	if (!is_vms_pathname(exdp, TRUE)) {
	    int end = strlen(exdp);
	    if (exdp[end - 1] != SLASHC) {
		exdp[end++] = SLASHC;
		exdp[end] = EOS;
	    }
	    unix2vms_path(exdp, exdp);
	}
#endif

	if (cd_and_pwd(exdp))
	    return TRUE;

#if OPT_PATHLOOKUP
# if SYS_VMS
	/*
	 * chdir() failed.  Attempt to match dir using $CDPATH on a VMS
	 * host.  Note that we can't test using is_pathname() because the
	 * indirectly referenced is_abs_pathname() code sez that any
	 * filespec with an embedded '[' or ':' is absolute--not what we
	 * want here.
	 */
	if (is_directory(exdp)) {
	    int first = TRUE;
	    /* For each comma-delimited component in CDPATH */
	    cdpath = get_cdpath();
	    while ((cdpath = parse_pathlist(cdpath, cdpathdir, &first)) != 0) {
		int len;
		char *tmp, newdir[NFILEN];

		newdir[0] = '\0';
		len = strlen(cdpathdir);
		if (len > 0) {
		    char **globvec = glob_string(cdpathdir);

		    if (glob_length(globvec) == 1) {
			vl_strncpy(cdpathdir, globvec[0], sizeof(cdpathdir));
			glob_free(globvec);
			tmp = &cdpathdir[len - 1];
			if (*tmp == R_BLOCK &&
			    *exdp == L_BLOCK &&
			    exdp[1] == '.') {
			    /*
			     * Concatenating dir that ends
			     * in bracket with subdir.
			     *
			     * Ex:  dvc:[dir] + [.subdir]
			     */
			    *tmp = '\0';
			    sprintf(newdir, "%s%s", cdpathdir, &exdp[1]);
			} else if (*tmp == ':' &&
				   *exdp == L_BLOCK) {
			    /*
			     * Concatenating rooted logical
			     * with dir.
			     *
			     * Ex:  dvc: + [dir]
			     */
			    sprintf(newdir, "%s%s", cdpathdir, exdp);
			}
			if (newdir[0] && cd_and_pwd(newdir))
			    return TRUE;
		    } else {
			glob_free(globvec);
		    }
		}
	    }
	}
# else
	/*
	 * chdir failed.  If the directory name doesn't begin with any of
	 * "/", "./", or "../", get the CDPATH environment variable and check
	 * if the specified directory name is a subdirectory of a
	 * directory in CDPATH.
	 */
	if (!is_pathname(exdp)) {
	    int first = TRUE;
	    /* For each appropriately delimited component in CDPATH */
	    cdpath = get_cdpath();
	    while ((cdpath = parse_pathlist(cdpath, cdpathdir, &first)) != NULL) {
		char **globvec = glob_string(cdpathdir);

		if (glob_length(globvec) == 1) {
		    vl_strncpy(cdpathdir, globvec[0], sizeof(cdpathdir));
		    glob_free(globvec);
		    if (cd_and_pwd(pathcat(cdpathdir, cdpathdir, exdp))) {
			return TRUE;
		    }
		} else {
		    glob_free(globvec);
		}
	    }
	}
# endif
#endif
    }
#if SYS_MSDOS || SYS_OS2
    setdrive(curd);
    current_directory(TRUE);
#endif

    outlen = (int) ((unsigned) (term.cols - 1) - (sizeof(CHANGE_FAILED) - 3));
    mlforce(CHANGE_FAILED, path_trunc(exdir, outlen, tmp, (int) sizeof(tmp)));
    return FALSE;

#undef CHANGE_FAILED
}

void
set_directory_from_file(BUFFER *bp)
{
    TRACE((T_CALLED "set_directory_from_file(%s)\n", bp->b_fname));
    if (!isInternalName(bp->b_fname)) {
	char name[NFILEN];
	char *leaf = pathleaf(vl_strncpy(name, bp->b_fname, (size_t) NFILEN));
	if (leaf != NULL
	    && leaf != name) {
	    *leaf = EOS;
	    bsl_to_sl_inplace(name);
	    set_directory(name);
	}
    }
    returnVoid();
}

#endif /* OPT_SHELL */

void
ch_fname(BUFFER *bp, const char *fname)
{
    int len;
    char *np;
    char *holdp = NULL;

    /*
     * ch_fname() can receive a very long filename string from capturecmd().
     */
    beginDisplay();
    if ((np = castalloc(char, strlen(fname) + NFILEN)) == NULL) {
	bp->b_fname = out_of_mem;
	bp->b_fnlen = (int) strlen(bp->b_fname);
	no_memory("ch_fname");
    } else {
	strcpy(np, fname);

	/* produce a full pathname, unless already absolute or "internal" */
	if (!isInternalName(np))
	    (void) lengthen_path(np);

	len = (int) strlen(np) + 1;

	if (bp->b_fname == NULL || strcmp(bp->b_fname, np)) {

	    if (bp->b_fname && bp->b_fnlen < len) {
		/* don't free it yet -- it _may_ have been passed in as
		 * the current file-name
		 */
		holdp = bp->b_fname;
		bp->b_fname = NULL;
	    }

	    if (!bp->b_fname) {
		if ((bp->b_fname = strmalloc(np)) == NULL) {
		    bp->b_fname = out_of_mem;
		    bp->b_fnlen = (int) strlen(bp->b_fname);
		    no_memory("ch_fname");
		    (void) free(np);
		    endofDisplay();
		    return;
		}
		bp->b_fnlen = len;
	    }

	    /* it'll fit, leave len untouched */
	    (void) strcpy(bp->b_fname, np);

	    if (holdp != out_of_mem)
		FreeIfNeeded(holdp);
	    updatelistbuffers();
	}
#ifdef	MDCHK_MODTIME
	(void) get_modtime(bp, &(bp->b_modtime));
	bp->b_modtime_at_warn = 0;
#endif
	fileuid_set_if_valid(bp, fname);
	(void) free(np);
    }
    endofDisplay();
}

#if OPT_HOOKS
static const char *
name_of_hook(HOOK * hook)
{
    const char *result = "unknown";
    if (hook == &cdhook)
	result = "$cd-hook";
    else if (hook == &readhook)
	result = "$read-hook";
    else if (hook == &writehook)
	result = "$write-hook";
    else if (hook == &bufhook)
	result = "$buffer-hook";
    else if (hook == &exithook)
	result = "$exit-hook";
#if OPT_COLOR
    else if (hook == &autocolorhook)
	result = "autocolor-hook";
#endif
#if OPT_MAJORMODE
    else if (hook == &majormodehook)
	result = "$majormode-hook";
#endif
    return result;
}

int
run_a_hook(HOOK * hook)
{
    int status = FALSE;

    if (hook->proc[0] && !doingopcmd) {
	if (!DisableHook(hook)) {
	    MARK save_pre_op_dot;	/* ugly hack */
	    int save_dotcmdactive;	/* another ugly hack */

	    save_dotcmdactive = dotcmdactive;
	    save_pre_op_dot = pre_op_dot;

	    TPRINTF(("running %s HOOK with %s, current %s\n",
		     name_of_hook(hook), hook->proc, curbp->b_bname));
	    status = docmd(hook->proc, TRUE, FALSE, 1);

	    pre_op_dot = save_pre_op_dot;
	    dotcmdactive = save_dotcmdactive;
	}
	EnableHook(hook);
    }
    return (status);
}

int
run_readhook(void)
{
    int result = FALSE;
    if (!b_is_temporary(curbp)) {
	int save = count_fline;	/* read-hook may run a filter - ignore */
	count_fline = 0;
	result = run_a_hook(&readhook);
	count_fline = save;
    }
    return result;
}
#endif

#if (OPT_AUTOCOLOR || OPT_ELAPSED) && defined(VL_ELAPSED)
/*
 * Returns elapsed time in milliseconds.
 */
double
vl_elapsed(VL_ELAPSED * first, int begin)
{
    double result;

#if defined(HAVE_GETTIMEOFDAY) && (SYS_UNIX && !SYS_MINGW)
#define	SECS(tv)	((double) tv.tv_sec + ((double) tv.tv_usec / 1.0e6))
    VL_ELAPSED tv1;
    gettimeofday(&tv1, NULL);
    if (begin)
	*first = tv1;
    result = 1000.0 * (SECS(tv1) - SECS((*first)));
#elif SYS_WINNT || SYS_MINGW
    VL_ELAPSED tv1 = GetTickCount();
    if (begin)
	*first = tv1;
    result = (tv1 - *first);
#else
    VL_ELAPSED tv1 = time((time_t *) 0);
    if (begin)
	*first = tv1;
    result = 1000.0 * (tv1 - *first);
#endif

    return result;
}
#endif

#if OPT_AUTOCOLOR
static int
can_autocolor(BUFFER *bp)
{
    int rc = FALSE;
    if (b_val(bp, VAL_AUTOCOLOR) > 0
#if OPT_MAJORMODE
	&& (bp->majr != NULL || !b_is_temporary(bp))
	&& b_val(bp, MDHILITE)
#endif
	) {
	rc = TRUE;
    }
    return rc;
}
#endif

void
autocolor(void)
{
#if OPT_AUTOCOLOR
    if (!(reading_msg_line || vile_is_busy || doingsweep)) {
	BUFFER *bp;
	WINDOW *wp;
	VL_ELAPSED begin_time;
	double elapsed_time;
	int goal_time;

	int do_update = 0;

	(void) vl_elapsed(&begin_time, TRUE);

	TRACE((T_CALLED "autocolor\n"));
	for_each_visible_window(wp) {
	    bp = wp->w_bufp;
	    if (can_autocolor(bp)
		&& b_is_recentlychanged(bp)) {
		/*
		 * Check if we're skipping autocolor (see below) because the
		 * buffer is large enough to interfere with the autocolor
		 * interval.
		 */
		if (bp->next_autocolor_time > 0) {
		    bp->next_autocolor_time--;
		}
		if (bp->next_autocolor_time == 0) {
		    BUFFER *oldbp = curbp;
		    WINDOW *oldwp = push_fake_win(bp);

		    in_autocolor = TRUE;
		    if (run_a_hook(&autocolorhook)) {
			b_clr_recentlychanged(bp);
			++do_update;
			bp->next_autocolor_time--;
		    }
		    in_autocolor = FALSE;
		    pop_fake_win(oldwp, oldbp);
		}
	    }
	}

	/*
	 * If do_update is set, we colored at least one buffer.
	 *
	 * Now check to see if the process took longer than we would like.  If
	 * the process lasts longer than the fraction of the autocolor interval
	 * given by the percent-autocolor mode, we will set values to reduce
	 * the number of times that we'll recolor the buffers that took a long
	 * time.
	 */
	if (do_update) {
	    elapsed_time = vl_elapsed(&begin_time, FALSE);

	    goal_time = global_b_val(VAL_AUTOCOLOR);
	    if (global_b_val(VAL_PERCENT_AUTOCOLOR) > 0) {
		goal_time *= global_b_val(VAL_PERCENT_AUTOCOLOR);
		if (goal_time <= 0)
		    goal_time = 1;
	    }
	    if (elapsed_time > goal_time) {
		for_each_visible_window(wp) {
		    bp = wp->w_bufp;

		    /*
		     * Check if we have just autocolored the buffer.
		     */
		    if (can_autocolor(bp)
			&& bp->next_autocolor_time < 0) {
			/*
			 * Compute the desired autocolor interval for the
			 * buffer.  This might be different from the global
			 * value, since each buffer can have its own
			 * percent-autocolor value.
			 *
			 * Ignore the buffer's local autocolor value, since we
			 * use that only for displaying results.
			 */
			goal_time = ((global_b_val(VAL_AUTOCOLOR)
				      + do_update - 1)
				     / do_update);
			if (b_val(bp, VAL_PERCENT_AUTOCOLOR) > 0) {
			    goal_time *= b_val(bp, VAL_PERCENT_AUTOCOLOR);
			    goal_time /= 100;
			    if (goal_time <= 0)
				goal_time = 1;
			}
			if (bp->last_autocolor_time > goal_time) {
			    int next_time = (int) bp->last_autocolor_time;
			    int next_skip = ((next_time + goal_time - 1)
					     / goal_time);

			    /*
			     * Make a local autocolor mode to show the user
			     * what the actual interval is.
			     */
			    set_local_b_val(bp, VAL_AUTOCOLOR, next_time);
			    /*
			     * Set counter to skip autocolor intervals.
			     */
			    bp->next_autocolor_time = next_skip;
			}
		    }
		}
	    }

	    /*
	     * Reset the state of buffers that were updated during this
	     * interval.
	     */
	    for_each_visible_window(wp) {
		bp = wp->w_bufp;
		if (bp->next_autocolor_time < 0)
		    bp->next_autocolor_time = 0;
	    }

	    update(FALSE);
	}
	TRACE((T_RETURN "\n"));
    }
#endif
}

#if OPT_SHELL

/*
 * provide a mechanism to prevent set_directory() from updating
 * dirstack wdw/buffer.
 */
static int
pushd_popd_set_dir(const char *dir)
{
    int rc;

    pushd_popd_active = TRUE;
    rc = set_directory(dir);
    pushd_popd_active = FALSE;
    return (rc);
}

/*
 * Add a directory to the top of the dir stack, extending its container
 * when necessary.
 */
static int
dirstack_extend(const char *dir, const char *fnname)
{
    beginDisplay();
    if (dirs_idx >= dirs_len) {
	if (dirs_len == 0) {
	    dirs_len = 16;
	    dirstack = castalloc(char *,
				   ((unsigned) dirs_len * sizeof(dirstack[0])));
	} else {
	    dirs_len *= 2;
	    safe_castrealloc(char *,
			     dirstack,
			       ((unsigned) dirs_len * sizeof(dirstack[0])));
	}
	if (!dirstack)
	    return (no_memory(fnname));
    }
    dirstack[dirs_idx] = castalloc(char, strlen(dir) + 1);
    if (!dirstack[dirs_idx])
	return (no_memory(fnname));
    strcpy(dirstack[dirs_idx++], dir);
    endofDisplay();
    return (TRUE);
}

static int
do_popd(int uindx,		/* user-specified dirstack index */
	int sign)
{
    int i, j;
    int rc = TRUE;

    beginDisplay();
    if ((uindx == 0 && sign > 0) || (uindx == dirs_idx && sign < 0)) {
	char *path;

	/*
	 * handle special case:  pop top stack entry (the virtual cwd) and
	 * then cd to the next entry on the directory stack.
	 */
	path = dirstack[dirs_idx - 1];
	if ((rc = pushd_popd_set_dir(path)) == TRUE) {
	    (void) free(path);
	    dirstack[--dirs_idx] = NULL;	/* reuse == death */
	}
    } else {
	if (sign > 0) {
	    /* rationalize +idx notation with actual dirstack index */

	    uindx--;		/* user-based index includes virtual cwd at top-of-stack */
	    uindx = (dirs_idx - 1) - uindx;
	}
	(void) free(dirstack[uindx]);
	dirstack[uindx] = NULL;	/* mark empty */

	/* shrink the stack by one element */
	for (i = j = 0; i < dirs_idx; i++) {
	    if (dirstack[i])
		dirstack[j++] = dirstack[i];
	}
	dirstack[--dirs_idx] = NULL;	/* stack has shrunk, reuse == death */
    }
    endofDisplay();
    return (rc);
}

static int
do_pushd(int uindx,		/* user-specified dirstack index */
	 int sign)
{
    int rc;
    char oldcwd[NFILEN], *path;

    vl_strncpy(oldcwd, current_directory(TRUE), sizeof(oldcwd));

    beginDisplay();
    if ((uindx == 0 && sign > 0) || (uindx == dirs_idx && sign < 0)) {
	/*
	 * handle special case:  swap the top stack entry (the virtual cwd)
	 * and the next entry on the directory stack and then cd to top
	 * new top of stack.
	 */

	path = dirstack[dirs_idx - 1];
	if ((rc = pushd_popd_set_dir(path)) == TRUE) {
	    (void) free(path);
	    dirstack[dirs_idx - 1] = castalloc(char, strlen(oldcwd) + 1);
	    if (!dirstack[dirs_idx - 1])
		rc = no_memory("do_pushd");
	    else
		strcpy(dirstack[dirs_idx - 1], oldcwd);
	}
    } else {
	if (sign > 0) {
	    /* rationalize +idx notation with actual dirstack index */

	    uindx--;		/* user-based index includes virtual cwd at top-of-stack */
	    uindx = (dirs_idx - 1) - uindx;
	}
	if ((rc = pushd_popd_set_dir(dirstack[uindx])) == TRUE) {
	    /* all is well, update dirstack */

	    (void) free(dirstack[uindx]);
	    dirstack[uindx] = castalloc(char, strlen(oldcwd) + 1);
	    if (!dirstack[uindx])
		rc = no_memory("do_pushd");
	    else
		strcpy(dirstack[uindx], oldcwd);
	}
    }
    endofDisplay();
    return (rc);
}

/* supported syntax:  pushd {dir | [{+|-}n]} */
int
pushd(int f GCC_UNUSED, int n GCC_UNUSED)
{
    long dirlocn;
    int failed, rc, sign;
    int ch;
    char oldcwd[NFILEN], newcwd[NFILEN], *tmp, *cp;
    static TBUFF *last;

    newcwd[0] = '\0';
    if ((rc = mlreply_dir("Directory: ", &last, newcwd)) == ABORT)
	return (rc);
    mktrimmed(newcwd);
    cp = newcwd;
    if (rc == FALSE || *cp == '\0') {
	/* empty response, swap cwd with top stack entry */

	if (last && ((tmp = tb_values(last)) != NULL) && *tmp) {
	    /* add an empty response to list of user inputs */

	    tb_scopy(&last, "");
	}
	if (dirs_idx == 0)
	    mlforce("[Empty directory stack, nothing to swap]");
	else
	    rc = do_pushd(0, 1);
    } else if (rc == TRUE) {

	/* handle simple case first:  pushd without +|-n arg */
	if (*cp != '+' && *cp != '-') {
	    vl_strncpy(oldcwd, current_directory(TRUE), sizeof(oldcwd));
	    if ((rc = pushd_popd_set_dir(newcwd)) != TRUE)
		return (rc);

	    /* all is well, update dirstack */
	    if (!dirstack_extend(oldcwd, "pushd"))
		return (FALSE);
	} else {
	    if (dirs_idx == 0) {
		mlforce("[Empty directory stack, nothing to swap]");
		rc = FALSE;
	    } else {
		sign = (*cp == '-') ? -1 : 1;
		cp++;
		rc = FALSE;
		ch = *cp++;
		if (isDigit(ch)) {
		    cp = skip_number(cp);
		    if (*cp == '\0')
			rc = TRUE;
		}
		if (!rc) {
		    mlforce("[Invalid pushd syntax,  usage:  pushd [{+|-}n] ]");
		    return (rc);
		}
		dirlocn = vl_atol(newcwd + 1, 10, &failed);
		if (failed) {
		    mlforce("[Invalid pushd index]");
		    return (FALSE);
		}
		if (dirlocn > dirs_idx) {
		    /*
		     * not "dirs_idx - 1" because cwd is a virtual member
		     * of the directory stack.
		     */

		    mlforce("[Pushd index out-of-range]");
		    return (FALSE);
		}
		rc = do_pushd((int) dirlocn, sign);
	    }
	}
    }
    /* else rc == SORTOFTRUE */

    if (rc == TRUE)
	vl_dirs(0, 0);		/* show directory stack */
    return (rc);
}

/* supported syntax:  popd [{+|-}n] */
int
popd(int f GCC_UNUSED, int n GCC_UNUSED)
{
    char *cp, newcwd[NFILEN];
    long dirlocn;
    static TBUFF *last;
    int sign = 0, rc, failed;
    int ch;

    rc = mlreply2("Directory stack entry: ", &last);
    if (rc == ABORT)
	return (rc);
    if (dirs_idx == 0) {
	mlforce("[Empty directory stack, nothing to pop]");
	return (FALSE);
    }
    if (last) {
	(void) vl_strncpy(newcwd, tb_values(last), sizeof(newcwd));
	newcwd[sizeof(newcwd) - 1] = '\0';
    } else
	newcwd[0] = '\0';
    mktrimmed(newcwd);
    if (newcwd[0] == '\0') {
	/*
	 * this is:  popd
	 *
	 * strip top directory from stack (which is just cwd) and
	 * cd to next element on dir stack
	 */

	if ((rc = do_popd(0, 1)) == TRUE)
	    (void) vl_dirs(0, 0);
	return (rc);
    }

    /* ck user input */
    cp = newcwd;
    rc = FALSE;
    if (*cp == '-' || *cp == '+') {
	sign = (*cp == '-') ? -1 : 1;
	cp++;
	ch = *cp++;
	if (isDigit(ch)) {
	    cp = skip_number(cp);
	    if (*cp == '\0')
		rc = TRUE;
	}
    }
    if (!rc) {
	mlforce("[Invalid popd syntax,  usage:  popd [{+|-}n] ]");
	return (rc);
    }
    dirlocn = vl_atol(newcwd + 1, 10, &failed);
    if (failed) {
	mlforce("[Invalid popd index]");
	return (FALSE);
    }
    if (dirlocn > dirs_idx) {
	/*
	 * not "dirs_idx - 1" because cwd is a virtual member of the
	 * directory stack.
	 */

	mlforce("[Popd index out-of-range]");
	return (FALSE);
    }
    if ((rc = do_popd((int) dirlocn, sign)) == TRUE)
	(void) vl_dirs(0, 0);
    return (rc);
}

static int
kill_dirstack_buffer(void)
{
    BUFFER *bp;
    int killed, nwdw, ndirstack_wdw;
    WINDOW *wp;

    killed = FALSE;
    if ((bp = find_b_name(DIRSTACK_BufName)) == NULL)
	return (killed);	/* buffer doesn't exist */
    if (bp->b_nwnd == 0)
	return (zotwp(bp));	/* no windows on buffer -- kill it */

    /* if there is only one window, we're not killing anything */
    if (wheadp->w_wndp == NULL)
	return (killed);

    nwdw = ndirstack_wdw = 0;
    for_each_visible_window(wp) {
	nwdw++;
	if (wp->w_bufp == bp)
	    ndirstack_wdw++;
    }
    if (nwdw > ndirstack_wdw)
	killed = zotwp(bp);	/* Okay to kill it */
    return (killed);
}

/* 2 levels of indirection :-) */
static void
dirstack_lister(int unused1 GCC_UNUSED, void *unused2 GCC_UNUSED)
{
    int i, up, dn;
    char bufup[128], bufdn[128], rslt[NFILEN * 2];

    up = 0;
    dn = dirs_idx;
    sprintf(bufup, "+%d", up);
    sprintf(bufdn, "-%d", dn);
    sprintf(rslt, "%4s%4s  %s", bufup, bufdn, current_directory(TRUE));
    bprintf("%s", rslt);
    for (i = dirs_idx - 1; i >= 0; i--) {
	up++;
	dn--;
	sprintf(bufup, "+%d", up);
	sprintf(bufdn, "-%d", dn);
	sprintf(rslt, "\n%4s%4s  %s", bufup, bufdn, dirstack[i]);
	bprintf("%s", rslt);
    }
}

/*
 * action is one of: DIRS_OPT, DIRS_FORCE, DIRS_SUPPRESS
 */
static int
display_dirstack(int action)
{
    if (pushd_popd_active)
	return (TRUE);
    if (action == DIRS_SUPPRESS) {
	/* if DirStack buffer onscreen, kill it if possible */

	return (kill_dirstack_buffer());
    }
    if (action == DIRS_OPT && find_b_name(DIRSTACK_BufName) == NULL)
	return (TRUE);		/* dirstack buffer not onscreen, nothing to do */
    if (dirs_idx == 0) {
	/* directory stack is empty. */

	int one_liner = TRUE;

	if (find_b_name(DIRSTACK_BufName) != NULL) {
	    /* try to kill it and its window(s), if any. */

	    one_liner = kill_dirstack_buffer();
	}
	if (one_liner)
	    return (pwd(TRUE, 1));	/* show cwd in message line. */

	/*
	 * else could not kill dirstack buffer.  in that case, fall thru and
	 * update its onscreen contents.
	 */
    }

    /* else create/update a DirStack buffer and populate it. */
    return (liststuff(DIRSTACK_BufName, FALSE, dirstack_lister, 0, NULL));
}

int
vl_dirs(int f, int n GCC_UNUSED)
{
    return (display_dirstack((f) ? DIRS_SUPPRESS : DIRS_FORCE));
}

int
vl_dirs_clear(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int i;

    if (dirstack) {
	beginDisplay();
	for (i = 0; i < dirs_idx; i++)
	    (void) free(dirstack[i]);
	(void) free(dirstack);
	dirstack = NULL;
	endofDisplay();
    }
    dirs_idx = dirs_len = 0;
    return (display_dirstack(DIRS_OPT));
}

/* add a directory to top of dirstack without changing cwd */
int
vl_dirs_add(int f GCC_UNUSED, int n GCC_UNUSED)
{
    char newdir[NFILEN], savedcwd[NFILEN];
    static TBUFF *last;
    int rc;

    if ((rc = mlreply2("Directory: ", &last)) == ABORT)
	return (rc);
    if (last) {
	(void) vl_strncpy(newdir, tb_values(last), sizeof(newdir));
	newdir[sizeof(newdir) - 1] = '\0';
	mktrimmed(newdir);
    } else
	newdir[0] = '\0';
    if (rc == TRUE && newdir[0]) {
	/*
	 * add new dir to dir stack -- assuming it is a directory, which
	 * might be accessible via CDPATH
	 */

	vl_strncpy(savedcwd, current_directory(TRUE), sizeof(savedcwd));
	dirs_add_active = TRUE;
	if (set_directory(newdir)) {
	    rc = dirstack_extend(current_directory(TRUE), "vl_dirs_add");
	    (void) set_directory(savedcwd);	/* restore user's cwd */

	    /* update [DirStack] if on screen */
	    (void) display_dirstack(DIRS_OPT);
	} else {
	    rc = FALSE;
	    mlforce("[\"%s\" is not a directory]", newdir);
	}
	dirs_add_active = FALSE;
	(void) current_directory(TRUE);		/* update cached cwd */
    }
    return (rc);
}

#endif /* OPT_SHELL */

/*
 * if ANSI C compiler available, convert a string to a long, trapping all
 * possible conversion errors.
 *
 * Note: strtol() and strtoul() appear on most of the systems we've seen within
 * recent memory; so the ifdef checks if this code will compile and leaves the
 * detail of errno values in a don't-care mode.
 */
long
vl_atol(const char *str, int base, int *failed)
{
    long result = 0;

    *failed = FALSE;

    if (str != NULL) {
#if defined(EDOM) && defined(ERANGE)
	char *prem;

	str = skip_cblanks(str);
	if (!*str)
	    *failed = TRUE;
	else {
	    set_errno(0);
	    result = strtol(str, &prem, base);
	    if (errno == EDOM || errno == ERANGE) {
		*failed = TRUE;
	    } else {
		if (*skip_blanks(prem) != EOS) {
		    *failed = TRUE;	/* trailing garbage */
		}
	    }
	}
#else
	/*
	 * too bad, so sad.  if this routine must be implemented using K&R,
	 * it should be doable with sscanf().
	 */
	result = atoi(str);
#endif
    }
    return result;
}

/*
 * if ANSI C compiler available, convert a string to an unsigned long,
 * trapping all possible conversion errors.
 */
#ifdef HAVE_STRTOUL
ULONG
vl_atoul(const char *str, int base, int *failed)
{
    *failed = FALSE;

#if defined(EDOM) && defined(ERANGE)
    {
	char *prem;
	ULONG rslt = 0;

	str = skip_cblanks(str);
	if (!*str || *str == '+' || *str == '-') {
	    *failed = TRUE;
	} else {
	    set_errno(0);
	    rslt = strtoul(str, &prem, base);
	    if (errno == EDOM || errno == ERANGE) {
		*failed = TRUE;
	    } else {
		if (*skip_blanks(prem) != EOS)
		    *failed = TRUE;	/* trailing garbage */
	    }
	}
	return (rslt);
    }
#else
    /*
     * too bad, so sad.  if this routine must be implemented using K&R,
     * it should be doable with sscanf().
     */
    return atoi(str);
#endif
}
#endif

#ifndef vl_stricmp
/*
 * Compare two strings ignoring case
 */
int
vl_stricmp(const char *a, const char *b)
{
    int ch1, ch2, cmp;

    for (;;) {
	ch1 = CharOf(*a++);
	ch2 = CharOf(*b++);
	if (isLower(ch1))
	    ch1 = toUpper(ch1);
	if (isLower(ch2))
	    ch2 = toUpper(ch2);
	cmp = ch1 - ch2;
	if (cmp != 0 || ch1 == EOS || ch2 == EOS)
	    break;
    }
    return cmp;
}
#endif

#ifndef vl_strnicmp
/*
 * Compare two strings ignoring case
 */
int
vl_strnicmp(const char *a, const char *b, size_t n)
{
    int ch1, ch2, cmp = 0;

    while (n-- != 0) {
	ch1 = CharOf(*a++);
	ch2 = CharOf(*b++);
	if (isLower(ch1))
	    ch1 = toUpper(ch1);
	if (isLower(ch2))
	    ch2 = toUpper(ch2);
	cmp = ch1 - ch2;
	if (cmp != 0 || ch1 == EOS || ch2 == EOS)
	    break;
    }
    return cmp;
}
#endif

/*
 * Format a visible representation of the given character, returns the buffer
 * address to simplify using this in formatting a whole buffer.
 */
char *
vl_vischr(char *buffer, int ch)
{
#if OPT_MULTIBYTE
    if (ch > 255) {
	sprintf(buffer, "\\u%04x", ch & 0xffff);
    } else
#endif
    {
	ch = CharOf(ch);
	if (isPrint(ch)) {
	    buffer[0] = (char) ch;
	    buffer[1] = '\0';
	} else {
	    if (ch >= 128)
		sprintf(buffer, "\\%03o", ch);
	    else if (ch == 127)
		strcpy(buffer, "^?");
	    else
		sprintf(buffer, "^%c", ch | '@');
	}
    }
    return buffer;
}

/*
 * Construct a TBUFF that holds a null-terminated, visible representation of
 * the given buffer.
 */
TBUFF *
tb_visbuf(const char *buffer, size_t len)
{
    char temp[20];
    size_t n;
    TBUFF *result = NULL;

    if (buffer != NULL && len != 0
	&& tb_init(&result, EOS) != NULL) {
	for (n = 0; n < len; ++n) {
	    tb_sappend0(&result, vl_vischr(temp, buffer[n]));
	}
    }
    return result;
}
