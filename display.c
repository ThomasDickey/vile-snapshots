/*
 * The functions in this file handle redisplay. There are two halves, the
 * ones that update the virtual display screen, and the ones that make the
 * physical display screen the same as the virtual display screen. These
 * functions use hints that are left in the windows by the commands.
 *
 *
 * $Header: /users/source/archives/vile.vcs/RCS/display.c,v 1.271 1999/03/24 11:47:39 pgf Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"
#include	"pscreen.h"

#define vMAXINT ((int)((unsigned)(~0)>>1))	/* 0x7fffffff */
#define vMAXNEG (-vMAXINT)			/* 0x80000001 */

#define	NU_WIDTH 8

#define	MRK_EMPTY        '~'
#define	MRK_EXTEND_LEFT  '<'
#define	MRK_EXTEND_RIGHT '>'

VIDEO	**vscreen;			/* Virtual screen. */
VIDEO	**pscreen;			/* Physical screen. */
#if	FRAMEBUF
#define PSCREEN vscreen
#else
#define PSCREEN pscreen
#endif
static	int *lmap;

#if DISP_IBMPC
#define PScreen(n) scread((VIDEO *)0,n)
#else
#define	PScreen(n) pscreen[n]
#endif

#if OPT_SCROLLCODE && (DISP_IBMPC || !FRAMEBUF)
#define CAN_SCROLL 1
#else
#define CAN_SCROLL 0
#endif

static	int	i_displayed;		/* false until we're in screen-mode */
static	int	im_displaying;		/* flag set during screen updates */
static	int	mpresf;			/* zero if message-line empty */
#if OPT_WORKING
static	int	im_timing;
#endif

/*
 * MARK2COL may be greater than mark2col if the mark does not point to the end
 * of the line, and if it points to a nonprinting character.  We use that value
 * when setting visible attributes, to keep tabs and other nonprinting
 * characters looking 'right'.
 */
#define MARK2COL(wp, mk)  offs2col(wp, mk.l, mk.o + 1) - 1
#define mark2col(wp, mk)  offs2col(wp, mk.l, mk.o)

#ifdef WMDLINEWRAP
#define TopRow(wp) (wp)->w_toprow + (wp)->w_line.o
static	int	allow_wrap;
#else
#define TopRow(wp) (wp)->w_toprow
#endif

/* for window size changes */
static	int chg_width, chg_height;

/******************************************************************************/

typedef	void	(*OutFunc) (int c);

static	OutFunc	dfoutfn;

static	int	endofline(char *s, int n);
static	int	texttest (int vrow, int prow);
static	int	updext(int col, int excess, int use_excess);
static	int	updpos(int *screenrowp, int *screencolp);
static	int	vtalloc (void);
static	void	l_to_vline(WINDOW *wp, LINEPTR lp, int sline);
static	void	mlmsg(const char *fmt, va_list *app);
static	void	modeline(WINDOW *wp);
static	void	reframe(WINDOW *wp);
static	void	scrscroll(int from, int to, int count);
static	void	updall (WINDOW *wp);
static	void	updateline (int row, int colfrom, int colto);
static	void	upddex (void);
static	void	updgar (void);
static	void	updone (WINDOW *wp);
static	void	updupd (int force);
static	void	vtlistc (int c);

#if OPT_VIDEO_ATTRS
static	void	updattrs(WINDOW *wp);
#endif

#if	OPT_UPBUFF
static	void	recompute_buffer(BUFFER *bp);
#endif

#if CAN_SCROLL
static	int	scrolls (int inserts);
#endif

/*--------------------------------------------------------------------------*/

/*
 * Format a number, right-justified, returning a pointer to the formatted
 * buffer.
 */
static char *
right_num (char *buffer, int len, long value)
{
	char	temp[NSTRING];
	register char	*p = lsprintf(temp, "%D", value);
	register char	*q = buffer + len;

	*q = EOS;
	while (q != buffer)
		*(--q) = (p != temp) ? *(--p) : ' ';
	return buffer;
}

/*
 * Do format a string.
 */
static int
dfputsn(OutFunc outfunc, const char *s, int n)
{
	register int c = 0;
	register int l = 0;
	TRACE(("...str=%s\n", visible_buff(s, n)))
	while ((n-- != 0) && ((c = *s++) != EOS)) {
		(*outfunc)(c);
		l++;
	}
	return l;
}

/* as above, but uses null-terminated string's length */
static int
dfputs(OutFunc outfunc, const char *s)
{
	return dfputsn(outfunc, s, -1);
}

/*
 * Do format an integer, in the specified radix.
 */
static int
dfputi(OutFunc outfunc, UINT i, UINT r)
{
	int q;

	TRACE(("...int=%d\n", i))

	q = (i >= r) ? dfputi(outfunc, i/r, r) : 0;

	(*outfunc)(hexdigits[i%r]);
	return q + 1; /* number of digits printed */
}

/*
 * do the same except as a long integer.
 */
static int
dfputli(OutFunc outfunc, ULONG l, UINT r)
{
	int q;

	TRACE(("...long=%ld\n", l))

	q = (l >= r) ? dfputli(outfunc, (l/r), r) : 0;

	return q + dfputi(outfunc, (l%r), r);
}

/*
 *	Do format a scaled integer with two decimal places
 */
static int
dfputf(OutFunc outfunc, UINT s)
{
	int n;
	UINT i;		/* integer portion of number */
	UINT f;		/* fractional portion of number */

	/* break it up */
	i = s / 100;
	f = s % 100;

	/* send out the integer portion */
	n = dfputi(outfunc, i, 10);
	(*outfunc)('.');
	(*outfunc)((f / 10) + '0');
	(*outfunc)((f % 10) + '0');
	return n + 3;
}

/*
 * Generic string formatter.  Takes printf-like args, and calls
 * the global function (*dfoutfn)(c) for each c
 */
static void
dofmt(const char *fmt, va_list *app)
{
	register int c;		/* current char in format string */
	register int wid;
	register int n;
	register int nchars = 0;
	int islong;
	int ivalue;
	long lvalue;
	UINT radix;
	OutFunc outfunc = dfoutfn;  /* local copy, for recursion */

	TRACE(("dofmt fmt='%s'\n", fmt))
	while ((c = *fmt++) != 0 ) {
		if (c != '%') {
			(*outfunc)(c);
			nchars++;
			continue;
		}
		c = *fmt++;
		wid = 0;
		islong = FALSE;
		if (c == '*') {
			wid = va_arg(*app,int);
			c = *fmt++;
		} else while (isDigit(c)) {
			wid = (wid * 10) + c - '0';
			c = *fmt++;
		}
		if (c == 'l') {
			islong = TRUE;
			c = *fmt++;
		}
		switch (c) {
			case EOS:
				n = 0;
				break;
			case 'c':
				(*outfunc)(va_arg(*app,int));
				n = 1;
				break;

			case 'd':
				if (!islong) {
					ivalue = va_arg(*app,int);
					if (ivalue < 0) {
						if (ivalue < vMAXNEG) {
							n = dfputs(outfunc,"OVFL");
							break;
						}
						ivalue = -ivalue;
						(*outfunc)('-');
					}
					n = dfputi(outfunc, (UINT)ivalue, 10);
					break;
				}
				/* FALLTHROUGH */
			case 'D':
				lvalue = va_arg(*app,long);
				if (lvalue < 0) {
					lvalue = -lvalue;
					(*outfunc)('-');
				}
				n = dfputli(outfunc, (ULONG)lvalue, 10);
				break;

			case 'o':
				n = dfputi(outfunc, va_arg(*app,UINT), 8);
				break;

			case 'x':
				if (!islong) {
					n = dfputi(outfunc, va_arg(*app,UINT), 16);
					break;
				}
				/* FALLTHROUGH */
			case 'X':
				n = dfputli(outfunc, va_arg(*app,ULONG), 16);
				break;

			case 'r':
			case 'R':
				radix = va_arg(*app, UINT);
				if (radix < 2 || radix > 36) radix = 10;
				if (islong || c == 'R')
					n = dfputli(outfunc,
						va_arg(*app,ULONG), radix);
				else
					n = dfputi(outfunc,
						va_arg(*app,UINT), radix);
				break;

			case 's':
				n = dfputs(outfunc, va_arg(*app,char *));
				break;

			case 'S': /* use wid as max width */
				n = dfputsn(outfunc, va_arg(*app,char *),wid);
				break;

			case 'f':
				n = dfputf(outfunc, va_arg(*app,UINT));
				break;

			case 'P': /* output padding -- pads total output to
					"wid" chars, using c as the pad char */
				wid -= nchars;
				/* FALLTHROUGH */

			case 'p': /* field padding -- puts out "wid"
					copies of c */
				n = 0;
				c = va_arg(*app,int);
				while (n < wid) {
					(*outfunc)(c);
					n++;
				}
				break;

			default:
				(*outfunc)(c);
				n = 1;
		}
		wid -= n;
		nchars += n;
		while (wid-- > 0) {
			(*outfunc)(' ');
			nchars++;
		}
	}

}

/******************************************************************************/

/*
 * Line-number mode
 */
int
nu_width(WINDOW *wp)
{
	return w_val(wp,WMDNUMBER) ? NU_WIDTH : 0;
}

int
col_limit(WINDOW *wp)
{
#ifdef WMDLINEWRAP
	if (w_val(wp,WMDLINEWRAP))
		return curcol + 1;	/* effectively unlimited */
#endif
	return term.t_ncol - 1 - nu_width(wp);
}

/*
 * Initialize the data structures used by the display code. The edge vectors
 * used to access the screens are set up.
 */
int
vtinit(void)
{
    register int i;
    register VIDEO *vp;

#if	OPT_MLFORMAT
    if (!modeline_format)
	modeline_format = strmalloc(
	    "%-%i%- %b %m:: :%f:is : :%=%F: : :%l:(:,:%c::) :%p::% :%S%-%-%|"
	);
#endif

    /* allocate new display memory */
    if (vtalloc() == FALSE) /* if we fail, only serious if not a realloc */
	return (vscreen != NULL);

    for (i = 0; i < term.t_mrow; ++i) {
	vp = vscreen[i];
	vp->v_flag = 0;
#if OPT_COLOR
	ReqFcolor(vp) = gfcolor;
	ReqBcolor(vp) = gbcolor;
#endif
    }
#if OPT_WORKING
    if (!i_displayed && !im_displaying)
	imworking(0);
#endif
    return TRUE;
}

#if OPT_VIDEO_ATTRS
static void
set_vattrs(int row, int col, VIDEO_ATTR attr, size_t len)
{
	while (len--)
		vscreen[row]->v_attrs[col++] = attr;
}
#else
#define set_vattrs(row, col, attr, len) /*nothing*/
#endif

static void
freeVIDEO(register VIDEO *vp)
{
	if (vp != 0) {
#if OPT_VIDEO_ATTRS
		FreeIfNeeded (vp->v_attrs);
#endif
		free((char *)vp);
	}
}

int
video_alloc(VIDEO **vpp)
{
	register VIDEO *vp;
	/* struct VIDEO already has 4 of the bytes */
	vp = typeallocplus(VIDEO, term.t_mcol - 4);
	if (vp == 0)
		return FALSE;
	(void)memset((char *)vp, 0, sizeof(VIDEO) + term.t_mcol - 4);

#if OPT_VIDEO_ATTRS
	vp->v_attrs = typecallocn(VIDEO_ATTR, (ALLOC_T)term.t_mcol);
	if (vp->v_attrs == 0) {
		free((char *)vp);
		return FALSE;
	}
#endif
	freeVIDEO(*vpp);
	*vpp = vp;
	return TRUE;
}

static int
vtalloc(void)
{
	register int i, first;
	static int vcols, vrows;

	if (term.t_mrow > vrows) {
		GROW(vscreen, VIDEO *, vrows, term.t_mrow);
#if	! FRAMEBUF
		GROW(pscreen, VIDEO *, vrows, term.t_mrow);
#endif
		GROW(lmap, int, vrows, term.t_mrow);
	} else {
		for (i = term.t_mrow; i < vrows; i++) {
			freeVIDEO(vscreen[i]);
#if	! FRAMEBUF
			freeVIDEO(pscreen[i]);
#endif
		}
	}

	first = (term.t_mcol > vcols) ? 0 : vrows;

	for (i = first; i < term.t_mrow; ++i) {
		if (!video_alloc(&vscreen[i]))
			return FALSE;
#if	! FRAMEBUF
		if (!video_alloc(&pscreen[i]))
			return FALSE;
#endif	/* !FRAMEBUF */
	}
	vcols = term.t_mcol;
	vrows = term.t_mrow;

	return TRUE;
}

/* free all video memory, in anticipation of a (growing) resize */
#if NO_LEAKS
static void
vtfree(void)
{
	register int i;

	if (vscreen) {
		for (i = 0; i < term.t_mrow; ++i) {
			freeVIDEO(vscreen[i]);
		}
		free ((char *)vscreen);
		vscreen = 0;
	}

#if	! FRAMEBUF
	if (pscreen) {
		for (i = 0; i < term.t_mrow; ++i) {
			freeVIDEO(pscreen[i]);
		}
		free ((char *)pscreen);
		pscreen = 0;
	}
#endif
	FreeIfNeeded (lmap);
}
#endif


/*
 * Set the virtual cursor to the specified row and column on the virtual
 * screen. There is no checking for nonsense values.
 */
static void
vtmove(int row, int col)
{
    vtrow = row;
    vtcol = col;
}

/* Write a character to the virtual screen. The virtual row and
   column are updated. Only print characters if they would be
   "visible".  If the line is too long put a ">" in the last column.
   This routine only puts printing characters into the virtual
   terminal buffers. Only column overflow is checked.
*/

static void
vtputc(int c)
{
	/* since we don't allow wrapping on the message line, we only need
	 * to evaluate this once.  */
	int lastcol = vtrow == term.t_nrow-1 ?  term.t_ncol-1 : term.t_ncol;
	register VIDEO *vp;

#ifdef WMDLINEWRAP
	if (vtrow < 0) {
		static VIDEO *fake_line;
		static int   length;
		if (length != term.t_mcol || fake_line == 0) {
			if (!video_alloc(&fake_line))
				return;
			length = term.t_mcol;
		}
		vp = fake_line;
	}
	else
#endif
	 vp = vscreen[vtrow];

	if (isPrint(c) && vtcol >= 0 && vtcol < lastcol) {
		VideoText(vp)[vtcol++] = (c & (N_chars-1));
#ifdef WMDLINEWRAP
		if ((allow_wrap != 0)
		 && (vtcol == lastcol)
		 && (vtrow <  allow_wrap)) {
			vtcol = 0;
			if (++vtrow >= 0)
				vscreen[vtrow]->v_flag |= VFCHG;
			horscroll += lastcol;
		}
#endif
		return;
	}

	if (vtcol >= lastcol) {
		VideoText(vp)[lastcol - 1] = MRK_EXTEND_RIGHT;
	} else if (c == '\t') {
		do {
			vtputc(' ');
		} while (((vtcol + horscroll)%curtabval) != 0
			  && vtcol < lastcol);
	} else if (c == '\n') {
		return;
	} else if (isPrint(c)) {
		++vtcol;
	} else {
		vtlistc(c);
	}
}

/* how should high-bit unprintable chars be shown? */
static int vt_octal;

/* shows non-printing character */
static void
vtlistc(int c)
{
	if (isPrint(c)) {
	    vtputc(c);
	    return;
	}

	if (c & HIGHBIT) {
	    vtputc('\\');
	    if (vt_octal) {
		vtputc(((c>>6)&3)+'0');
		vtputc(((c>>3)&7)+'0');
		vtputc(((c   )&7)+'0');
	    } else {
		vtputc('x');
		vtputc(hexdigits[(c>>4) & 0xf]);
		vtputc(hexdigits[(c   ) & 0xf]);
	    }
	} else {
	    vtputc('^');
	    vtputc(toalpha(c));
	}
}

static int
vtgetc(int col)
{
	return vscreen[vtrow]->v_text[col];
}

static void
vtputsn(const char *s, int n)
{
	int c = 0;
	while (n-- > 0 && (c = *s++) != EOS)
		vtputc(c);
}

/*
 * Write a line to the screen at the current video coordinates, allowing for
 * line-wrap or right-shifting.
 */
static void
vtset(LINEPTR lp, WINDOW *wp)
{
	register char *from;
	register int n = llength(lp);
	BUFFER	*bp  = wp->w_bufp;
	int	skip = -vtcol,
		list = w_val(wp,WMDLIST);

	vt_octal = w_val(wp,WMDNONPRINTOCTAL);

#ifdef WMDLINEWRAP
	/*
	 * If the window's offset is negative, we've got a case of linewrap
	 * where the line's beginning is forced before the beginning of the
	 * window.
	 */
	if (wp->w_line.o < 0) {
		vtrow -= wp->w_line.o;
		skip = col2offs(wp, lp, -(wp->w_line.o * term.t_ncol));
		n -= skip;
	}
	else
#endif
	if (w_val(wp,WMDNUMBER)) {
		register int j, k, jk;
		L_NUM	line = line_no(bp, lp);
		int	fill = ' ';
		char	temp[NU_WIDTH+2];

		vtcol = 0;	/* make sure we always see line numbers */
		vtputsn(right_num(temp, NU_WIDTH-2, (long)line), NU_WIDTH-2);
		vtputsn("  ", 2);
		horscroll = skip - vtcol;

		/* account for leading fill; this repeats logic in vtputc so
		 * I don't have to introduce a global variable... */
		from = lp->l_text;
		for (j = k = jk = 0; (j < n) && (k < skip); j++) {
			register int	c = from[j];
			if ((list || (c != '\t')) && !isPrint(c)) {
				if (c & HIGHBIT) {
				    k += 4;
				    fill = '\\';  /* FIXXXX */
				} else {
				    k += 2;
				    fill = toalpha(c);
				}
			} else {
				if (c == '\t')
					k += (curtabval - (k % curtabval));
				else if (isPrint(c))
					k++;
				fill = ' ';
			}
			jk = j+1;
		}
		while (k-- > skip)
			vtputc(fill);
		if ((skip = jk) < 0)
			skip = 0;
		n -= skip;
	} else
		skip = 0;

#if OPT_B_LIMITS
	horscroll -= w_left_margin(wp);
#endif
	from = lp->l_text + skip;
#ifdef WMDLINEWRAP
	allow_wrap = w_val(wp,WMDLINEWRAP) ? mode_row(wp)-1 : 0;
#endif

	/*
	 * The loops below are split up for a reason - there's a hidden side effect
	 * which makes the final increment of vtcol inconsistent, so it won't
	 * terminate until the end of the line.  For very long lines, that's a
	 * performance hit.
	 */
#define VTSET_PUT(ch,count) if(list) vtlistc(ch); else vtputc(ch); count
#ifdef WMDLINEWRAP
	if (w_val(wp,WMDLINEWRAP))
	{
		while ((vtcol < term.t_ncol)
		  &&   ((vtrow == term.t_nrow-1) || (vtrow < mode_row(wp)))
		  &&   (n > 0)) {
			VTSET_PUT(*from++,n--);
		}
		if ((vtcol <= term.t_ncol)
		  &&   ((vtrow == term.t_nrow-1) || (vtrow < mode_row(wp)))
		  &&   (n > 0)) {
			VTSET_PUT(*from++,n--);
		}
	}
	else
#endif
	{
		while ((vtcol < term.t_ncol)
		  &&   (n > 0)) {
			VTSET_PUT(*from++,n--);
		}
		if ((vtcol <= term.t_ncol)
		  &&   (n > 0)) {
			VTSET_PUT(*from++,n--);
		}
	}

	/* Display a "^J" if 'list' mode is active, unless we've suppressed
	 * it for some reason.
	 */
	if (list && (n >= 0)) {
		if (b_is_scratch(bp) && listrimmed(lp))
			/*EMPTY*/;
		else if (!b_val(bp,MDNEWLINE) && (lforw(lp) == buf_head(bp)))
			/*EMPTY*/;
		else
			vtlistc('\n');
	}
#ifdef WMDLINEWRAP
	allow_wrap = 0;
#endif
}

/*
 * Erase from the end of the software cursor to the end of the line on which
 * the software cursor is located.
 */
static void
vteeol(void)
{
	if (vtcol < term.t_ncol) {
	    int n = (vtcol >= 0) ? vtcol : 0;
#ifdef WMDLINEWRAP
	    if (vtrow >= 0)
#endif
		if (n >= 0) {
			(void)memset(&vscreen[vtrow]->v_text[n],
				' ', (SIZE_T)(term.t_ncol-n));
		}
		vtcol = term.t_ncol;
	}
}

/* upscreen:	user routine to force a screen update
		always finishes complete update		*/
#if !SMALLER
/* ARGSUSED */
int
upscreen(int f GCC_UNUSED, int n GCC_UNUSED)
{
	return update(TRUE);
}
#endif

static	UINT	scrflags;

/* line to virtual column */
int
mk_to_vcol (MARK mark, int expanded, int base)
{
	int	col = 0;
	int	i = base;
	int	c;
	int	lim;
	LINEPTR lp;

	lp = mark.l;
	lim = mark.o
		+ ((!global_g_val(GMDALTTABPOS) && !insertmode) ? 1 : 0);
	if (lim > llength(lp))
		lim = llength(lp);

	while (i < lim) {
		c = lgetc(lp, i++);
		if (c == '\t' && !expanded) {
			col += curtabval - (col%curtabval);
		} else {
			if (!isPrint(c)) {
				col += (c & HIGHBIT) ? 3 : 1;
			}
			++col;
		}

	}
	col += base;
	if (!global_g_val(GMDALTTABPOS) && !insertmode &&
			col != 0 && mark.o < llength(lp))
		col--;
	return col;
}

void
kbd_openup(void)
{
#if !FRAMEBUF && !OPT_PSCREEN
	int i;
	size_t alen = sizeof(VIDEO_ATTR) * term.t_ncol;
#endif
	kbd_flush();
	bottomleft();
	TTputc('\n');
	TTputc('\r');
	TTflush();
#if !FRAMEBUF && !OPT_PSCREEN
	if (pscreen != 0) {
		for (i = 0; i < term.t_nrow-1; ++i) {
			(void)memcpy(
				pscreen[i]->v_text,
				pscreen[i+1]->v_text,
				(SIZE_T)(term.t_ncol));
#if OPT_VIDEO_ATTRS
			(void)memcpy(
				pscreen[i]->v_attrs,
				pscreen[i+1]->v_attrs,
				alen);
#endif
		}
		(void)memset(pscreen[i]->v_text, ' ', (SIZE_T)(term.t_ncol));
#if OPT_VIDEO_ATTRS
		(void)memset(pscreen[i]->v_attrs, VADIRTY, alen);
#endif
	}
#endif
}

/* cannot be allocated since it's used by OPT_HEAPSIZE */
static char my_overlay[20];

/* save/erase text for the overlay on the message line */
void
kbd_overlay(const char *s)
{
	my_overlay[0] = EOS;
	if ((mpresf = (s != 0 && *s != EOS)) != 0) {
		strncpy(my_overlay, s, sizeof(my_overlay)-1);
	}
}

void
kbd_flush(void)
{
	int ok;

	beginDisplay();
	if (vscreen != 0) {
		int row = term.t_nrow - 1;

		vtmove(row, -w_val(wminip,WVAL_SIDEWAYS));

		ok = (wminip != 0
		   && wminip->w_dot.l != 0
		   && wminip->w_dot.l->l_text != 0);
		if (ok) {
			TRACE(("SHOW:%2d:%s\n",
				llength(wminip->w_dot.l),
				lp_visible(wminip->w_dot.l)))
			lsettrimmed(wminip->w_dot.l);
			vtset(wminip->w_dot.l, wminip);
		}

		vteeol();
		set_vattrs(row, 0,
			(VIDEO_ATTR) (miniedit
					? global_g_val(GVAL_MINI_HILITE)
					: 0),
			term.t_ncol);
		if (my_overlay[0] != EOS) {
			int n = term.t_ncol - strlen(my_overlay) - 1;
			if (n > 0) {
				(void)memcpy(&vscreen[row]->v_text[n],
					my_overlay,
					strlen(my_overlay));
			}
		}
		vscreen[row]->v_flag |= VFCHG;
		updateline(row, 0, term.t_ncol);
		if (ok)
			movecursor(row,
				offs2col(wminip, wminip->w_dot.l, wminip->w_dot.o));
	}
	TTflush();
	endofDisplay();
}

static int
TypeAhead(int force)
{
	if (force != TRUE) {
		if ((force == FALSE && !global_g_val(GMDSMOOTH_SCROLL))
		 || (force == SORTOFTRUE))
			return (keystroke_avail());
	}
	return FALSE;
}

/*
 * Make sure that the display is right. This is a three part process. First,
 * scan through all of the windows looking for dirty ones. Check the framing,
 * and refresh the screen. Second, make sure that "currow" and "curcol" are
 * correct for the current window. Third, make the virtual and physical
 * screens the same.
 */
int
update(
int force)	/* force update past type ahead? */
{
	register WINDOW *wp;
	int origrow, origcol;
	int screenrow, screencol;
	int updated = FALSE;

	/* Get row and column prior to doing the update in case we are
	 * reading the message line.
	 */
	origrow = ttrow;
	origcol = ttcol;

	if (clhide)
		return TRUE;
	if (!curbp || !vscreen || !curwp) /* not initialized */
		return FALSE;
	/* don't try to update if we got called via a read-hook on a window
	 * that isn't complete.
	 */
	if (curwp->w_bufp == 0
	 || curwp->w_bufp->b_nwnd == 0
	 || curwp->w_ntrows < 1)
		return FALSE;
	if (TypeAhead(force))
		return SORTOFTRUE;
#if	OPT_VISIBLE_MACROS == 0
	if (!force && kbd_replaying(TRUE) && (get_recorded_char(FALSE) != -1))
		return SORTOFTRUE;
#endif

	beginDisplay();

	/* first, propagate mode line changes to all instances of
		a buffer displayed in more than one window */
	for_each_visible_window(wp) {
		if (wp->w_flag & WFMODE) {
			if (wp->w_bufp->b_nwnd > 1) {
				/* make sure all previous windows have this */
				register WINDOW *owp;
				for_each_visible_window(owp)
					if (owp->w_bufp == wp->w_bufp)
						owp->w_flag |= WFMODE;
			}
		}
	}

	/* look for scratch-buffers that should be recomputed.  */
#if	OPT_UPBUFF
	for_each_visible_window(wp)
		if (b_is_obsolete(wp->w_bufp))
			recompute_buffer(wp->w_bufp);
#endif

	/* look for windows that need the ruler updated */
#ifdef WMDRULER
	for_each_visible_window(wp) {
		if (w_val(wp,WMDRULER)) {
			int	line  = line_no(wp->w_bufp, wp->w_dot.l);
			int	col   = mk_to_vcol(wp->w_dot, w_val(wp,WMDLIST), 0) + 1;

			if (line != wp->w_ruler_line
			 || col  != wp->w_ruler_col) {
				wp->w_ruler_line = line;
				wp->w_ruler_col  = col;
				wp->w_flag |= WFMODE;
			}
		} else if (wp->w_flag & WFSTAT) {
			wp->w_flag |= WFMODE;
		}
		wp->w_flag &= ~WFSTAT;
	}
#endif

	do {
		/* update any windows that need refreshing */
		for_each_visible_window(wp) {
			if (wp->w_flag) {
				if ((wp->w_flag & ~(WFMOVE)) && !updated++)
					TTcursor(FALSE);
				curtabval = tabstop_val(wp->w_bufp);
				/* if the window has changed, service it */
				reframe(wp);	/* check the framing */
				if (wp->w_flag & (WFKILLS|WFINS)) {
					scrflags |= (wp->w_flag & (WFINS|WFKILLS));
					wp->w_flag &= ~(WFKILLS|WFINS);
				}
				if ((wp->w_flag & ~(WFMODE)) == WFEDIT)
					updone(wp);	/* update EDITed line */
				else if (wp->w_flag & ~(WFMOVE))
					updall(wp);	/* update all lines */
#if OPT_SCROLLBARS
				if (wp->w_flag & (WFHARD | WFSBAR))
					gui_update_scrollbar(wp);
#endif /* OPT_SCROLLBARS */

#if OPT_VIDEO_ATTRS
				if (wp->w_flag & (WFHARD | WFEDIT))
					updattrs(wp);
#endif
				if (scrflags || (wp->w_flag & (WFMODE|WFCOLR)))
					modeline(wp);	/* update modeline */
				wp->w_flag = 0;
				wp->w_force = 0;
			}
		}
		curtabval = tabstop_val(curbp);

	/* Recalculate the current hardware cursor location.  If true, we've
	 * done a horizontal scroll.
	 */
	} while (updpos(&screenrow, &screencol));

	/* check for lines to de-extend */
	upddex();

	/* if screen is garbage, re-plot it */
	if (sgarbf)
		updgar();

	/* update the virtual screen to the physical screen */
	updupd(force);

	/* update the cursor and flush the buffers */
	if (reading_msg_line)
	    movecursor(origrow, origcol);
	else
	    movecursor(screenrow, screencol);

	if (updated)
		TTcursor(TRUE);
	TTflush();
	endofDisplay();
	i_displayed = TRUE;

	while (chg_width || chg_height)
		newscreensize(chg_height,chg_width);
	return(TRUE);
}

/*	reframe:	check to see if the cursor is on in the window
			and re-frame it if needed or wanted		*/
static void
reframe(WINDOW *wp)
{
	register LINEPTR dlp;
	register LINEPTR lp;
	register int i = 0;
	register int rows;
	int	founddot = FALSE;	/* set to true iff we find dot */
	int	tildecount;

	/* if not a requested reframe, check for a needed one */
	if ((wp->w_flag & WFFORCE) == 0) {
		/* initial update in main.c may not set these first... */
		if (wp->w_dot.l == (LINE *)0) {
			wp->w_dot.l = lforw(win_head(wp));
			wp->w_dot.o = 0;
		}
		if (wp->w_line.l == (LINE *)0) {
			wp->w_line.l = wp->w_dot.l;
			wp->w_line.o = 0;
		}
#if CAN_SCROLL
		/* loop from one line above the window to one line after */
		lp = lback(wp->w_line.l);
		i  = -line_height(wp,lp);
#else
		/* loop through the window */
		lp = wp->w_line.l;
		i  = 0;
#endif
		for_ever {
			/* if the line is in the window, no reframe */
			if (lp == wp->w_dot.l) {
				founddot = TRUE;
#if CAN_SCROLL
				/* if not _quite_ in, we'll reframe gently */
				if ( i < 0 || i >= wp->w_ntrows) {
					/* if the terminal can't help, then
						we're simply outside */
					if (term.t_scroll == null_t_scroll)
						i = wp->w_force;
					break;
				}
#endif
#ifdef WMDLINEWRAP
				if (w_val(wp,WMDLINEWRAP)
				 && i > 0
				 && i + line_height(wp,lp) > wp->w_ntrows) {
					i = wp->w_ntrows;
					break;
				}
#endif
				lp = wp->w_line.l;
				goto kill_tildes;
			}

			/* if we are at the end of the file, reframe */
			if (i >= 0 && lp == win_head(wp))
				break;

			/* on to the next line */
			if (i >= wp->w_ntrows) {
				i = 0;	/* dot-not-found */
				break;
			}
			i += line_height(wp,lp);
			lp = lforw(lp);
		}
	}

#if CAN_SCROLL
	if (i < 0) {	/* we're just above the window */
		i = 1;	/* put dot at first line */
		scrflags |= WFINS;
	} else if (founddot && (i >= wp->w_ntrows)) {
		/* we're just below the window */
		i = -1;	/* put dot at last line */
		scrflags |= WFKILLS;
	} else /* put dot where requested */
#endif
		i = wp->w_force;  /* (is 0, unless reposition() was called) */

	wp->w_flag |= WFMODE;
	wp->w_line.o = 0;

	/* w_force specifies which line of the window dot should end up on */
	/*	positive --> lines from the top				*/
	/*	negative --> lines from the bottom			*/
	/*	zero --> middle of window				*/

	lp = wp->w_dot.l;

#ifdef WMDLINEWRAP
	/*
	 * Center dot in middle of screen with line-wrapping
	 */
	if (i == 0 && w_val(wp,WMDLINEWRAP)) {
		rows = (wp->w_ntrows - line_height(wp,lp) + 2) / 2;
		while (rows > 0) {
			dlp = lback(lp);
			if (dlp == win_head(wp))
				break;
			if ((rows -= line_height(wp, dlp)) < 0)
				break;
			lp = dlp;
		}
	} else
#endif
	{
		rows = (i != 0)
			? wp->w_ntrows
			: wp->w_ntrows / 2;
		while (rows > 0) {
			if ((i > 0)
			 && (--i <= 0))
				break;
			dlp = lback(lp);
			if (dlp == win_head(wp))
				break;
			if ((rows -= line_height(wp, lp)) < 0)
				break;
			lp = dlp;
		}
		if (rows < line_height(wp, lp)
		 && (lp != wp->w_dot.l)) {
			while (i++ < 0) {
				dlp = lforw(lp);
				if (dlp == win_head(wp))
					break;
				else
					lp = dlp;
			}
		}
	}
kill_tildes:
	/* Eliminate as many tildes as possible from bottom */
	dlp = lp;
	rows = wp->w_ntrows;
	while (rows > 0 && (dlp != win_head(wp))) {
		rows -= line_height(wp, dlp);
		dlp = lforw(dlp);
	}
	dlp = lback(lp);

	tildecount = (wp->w_ntrows * ntildes)/100;
	if (tildecount == wp->w_ntrows)
		tildecount--;

	while (dlp != win_head(wp)
	       && (rows -= line_height(wp, dlp)) >= tildecount) {
		lp = dlp;
		dlp = lback(lp);
	}

	/* and reset the current line-at-top-of-window */
	if (lp != win_head(wp) /* mouse click could be past end */
	 && lp != wp->w_line.l) { /* no need to set it if already there */
		wp->w_line.l = lp;
		wp->w_flag |= WFHARD;
		wp->w_flag &= ~WFFORCE;
	}
#ifdef WMDLINEWRAP
	/*
	 * Ensure that dot will be visible, by adjusting the w_line.o value if
	 * necessary.  That's used to start the beginning of the first line in
	 * a window "before" the start of the window.
	 */
	if (w_val(wp,WMDLINEWRAP)
	 && sameline(wp->w_line, wp->w_dot)) {
		int want = mark2col(wp, wp->w_dot) / term.t_ncol;
		if (want + wp->w_line.o >= wp->w_ntrows) {
			wp->w_line.o = wp->w_ntrows - want - 1;
			wp->w_flag |= WFHARD;
			wp->w_flag &= ~WFFORCE;
		}
		else if (want + wp->w_line.o < 0) {
			wp->w_line.o = -want;
			wp->w_flag |= WFHARD;
			wp->w_flag &= ~WFFORCE;
		}
	}
#endif
}

/*	updone:	update the current line	to the virtual screen		*/

static void
updone(
WINDOW *wp)	/* window to update current line in */
{
	register LINEPTR lp;	/* line to update */
	register int sline;	/* physical screen line to update */

	/* search down the line we want */
	lp = wp->w_line.l;
	sline = TopRow(wp);
	while (lp != wp->w_dot.l) {
		sline += line_height(wp,lp);
		lp = lforw(lp);
	}

	l_to_vline(wp,lp,sline);
	vteeol();
}

/*	updall:	update all the lines in a window on the virtual screen */

static void
updall(
WINDOW *wp)	/* window to update lines in */
{
	register LINEPTR lp;	/* line to update */
	register int sline;	/* physical screen line to update */

	/* search down the lines, updating them */
	lp = wp->w_line.l;
	sline = TopRow(wp);
	while (sline < mode_row(wp)) {
		l_to_vline(wp,lp,sline);
		vteeol();
		sline += line_height(wp,lp);
		if (lp != win_head(wp))
			lp = lforw(lp);
	}
}

/* line to virtual screen line */
static void
l_to_vline(
WINDOW *wp,	/* window to update lines in */
LINEPTR lp,
int sline)
{
	C_NUM	left;

	/*
	 * Mark the screen lines changed, resetting the requests for reverse
	 * video.  Set the global 'horscroll' to the amount of horizontal
	 * scrolling.
	 */
#ifdef WMDLINEWRAP
	if (w_val(wp,WMDLINEWRAP)) {
		int top_line = sline - wp->w_line.o;
		register int	m = (top_line >= 0) ? top_line : 0;
		register int	n = top_line + line_height(wp, lp);
		while (n > m)
			if (--n < mode_row(wp)) {
				vscreen[n]->v_flag |= VFCHG;
				vscreen[n]->v_flag &= ~VFREQ;
			}
		horscroll = 0;
	} else
#endif
	{
		vscreen[sline]->v_flag |= VFCHG;
		vscreen[sline]->v_flag &= ~VFREQ;
		if (w_val(wp,WVAL_SIDEWAYS))
			horscroll = w_val(wp,WVAL_SIDEWAYS);
	}
	left = horscroll;

	if (lp != win_head(wp)) {
		vtmove(sline, -left);
		vtset(lp, wp);
		if (left && sline >= 0) {
			register int	zero = nu_width(wp);
			vscreen[sline]->v_text[zero] = MRK_EXTEND_LEFT;
			if (vtcol <= zero) vtcol = zero+1;
		}
	} else {
		vtmove(sline, 0);
		vtputc(MRK_EMPTY);
	}
	horscroll = 0;
#if	OPT_COLOR
	if (sline >= 0) {
		ReqFcolor(vscreen[sline]) = gfcolor;
		ReqBcolor(vscreen[sline]) = gbcolor;
	}
#endif
}

/*	updpos:	update the position of the hardware cursor and handle extended
		lines. This is the only update for simple moves.
		returns the screen column for the cursor, and
		a boolean indicating if full sideways scroll was necessary */
static int
updpos(
int *screenrowp,
int *screencolp)
{
	register LINEPTR lp;
#ifdef WMDLINEWRAP
	register int i;
#endif
	register int col, excess;
	register int collimit;
	int moved = FALSE;
	int nuadj = is_empty_buf(curwp->w_bufp) ? 0 : nu_width(curwp);
	int liadj = (w_val(curwp,WMDLIST)) ? 1 : 0;

	/* find the current row */
	lp = curwp->w_line.l;
	currow = TopRow(curwp);
	while (lp != DOT.l) {
		currow += line_height(curwp,lp);
		lp = lforw(lp);
		if (lp == curwp->w_line.l
		 || currow > mode_row(curwp)) {
			mlforce("BUG:  lost dot updpos().  setting at top");
			lp = curwp->w_line.l = DOT.l = lforw(buf_head(curbp));
			currow = TopRow(curwp);
		}
	}

	/* find the current column */
	col = mk_to_vcol(DOT, w_val(curwp,WMDLIST), w_left_margin(curwp));

#ifdef WMDLINEWRAP
	if (w_val(curwp,WMDLINEWRAP)) {
		curcol = col;
		collimit = term.t_ncol - nuadj;
		*screenrowp = currow;
		if (col >= collimit) {
			col -= collimit;
			*screenrowp += 1;
			if (col >= term.t_ncol)
				*screenrowp += (col / term.t_ncol);
			*screencolp = col % term.t_ncol;
		} else {
			*screencolp = col + nuadj;
		}
		/* kludge to keep the cursor within the window */
		i = mode_row(curwp) - 1;
		if (*screenrowp > i) {
			*screenrowp = i;
			*screencolp = term.t_ncol - 1;
		}
		return FALSE;
	} else
#endif
	 *screenrowp = currow;

	/* ...adjust to offset from shift-margin */
	curcol = col - w_val(curwp,WVAL_SIDEWAYS);
	*screencolp = curcol;

	/* if extended, flag so and update the virtual line image */
	collimit = col_limit(curwp);
	excess = curcol - collimit + liadj;
	if ((excess > 0) || (excess == 0 &&
			(DOT.o < llength(DOT.l) - 1 ))) {
		if (w_val(curwp,WMDHORSCROLL)) {
			(void)mvrightwind(TRUE, excess + collimit/2 );
			moved = TRUE;
		} else {
			*screencolp = updext(col, excess, TRUE);
		}
	} else if (w_val(curwp,WVAL_SIDEWAYS) && (curcol < 1)) {
		if (w_val(curwp,WMDHORSCROLL)) {
			(void)mvleftwind(TRUE, -curcol + collimit/2 + 1);
			moved = TRUE;
		} else {
			*screencolp = updext(col, 0, FALSE);
		}
	} else {
		if (vscreen[currow]->v_flag & VFEXT) {
			l_to_vline(curwp,lp,currow);
			vteeol();
			/* this line no longer is extended */
			vscreen[currow]->v_flag &= ~VFEXT;
		}
	}
	if (!moved)
		*screencolp += nuadj;
	return moved;
}

/*	upddex:	de-extend any line that deserves it		*/

static void
upddex(void)
{
	register WINDOW *wp;
	register LINEPTR lp;
	register int i;

	for_each_visible_window(wp) {
		lp = wp->w_line.l;
		i = TopRow(wp);

		curtabval = tabstop_val(wp->w_bufp);

		while (i < mode_row(wp)) {
			if (i >= 0
			 && vscreen[i]->v_flag & VFEXT) {
				if ((wp != curwp)
				 || (lp != wp->w_dot.l)
				 || ((i != currow)
				  && (curcol < col_limit(wp)))) {
					l_to_vline(wp,lp,i);
					vteeol();
					/* this line no longer is extended */
					vscreen[i]->v_flag &= ~VFEXT;
				}
			}
			i += line_height(wp,lp);
			lp = lforw(lp);
		}
	}
	curtabval = tabstop_val(curbp);
}

/*	updgar:	if the screen is garbage, clear the physical screen and
		the virtual screen and force a full update		*/

static void
updgar(void)
{
#if !FRAMEBUF && !OPT_PSCREEN
	register int j;
#endif
	register int i;

	for (i = 0; i < term.t_nrow; ++i) {
		vscreen[i]->v_flag |= VFCHG;
#if	OPT_REVSTA
		vscreen[i]->v_flag &= ~VFREV;
#endif
#if	OPT_COLOR
		CurFcolor(vscreen[i]) = -1;
		CurBcolor(vscreen[i]) = -1;
#endif
#if	! FRAMEBUF && ! OPT_PSCREEN
		for (j = 0; j < term.t_ncol; ++j) {
			CELL_TEXT(i,j) = ' ';
#if OPT_VIDEO_ATTRS
			CELL_ATTR(i,j) = 0;
#endif /* OPT_VIDEO_ATTRS */
		}
#endif
	}
#if !OPT_PSCREEN
#if	OPT_COLOR
	TTforg(gfcolor);		 /* Need to set before erasing. */
	TTbacg(gbcolor);
#endif
	movecursor(0, 0);		 /* Erase the screen. */
	TTeeop();
#else
	kbd_erase_to_end(0);
#endif
	sgarbf = FALSE;			 /* Erase-page clears */
	kbd_flush();
}

/*	updupd:	update the physical screen from the virtual screen	*/

static void
updupd(
int force GCC_UNUSED)	/* forced update flag */
{
	register int i;

#if CAN_SCROLL
	if (scrflags & WFKILLS)
		(void)scrolls(FALSE);
	if (scrflags & WFINS)
		(void)scrolls(TRUE);
	scrflags = 0;
#endif

	for (i = 0; i < term.t_nrow; ++i) {
		/* for each line that needs to be updated*/
		if ((vscreen[i]->v_flag & (VFCHG|VFCOL)) != 0) {
#if !DISP_X11
			if (TypeAhead(force))
				return;
#endif
			updateline(i, 0, term.t_ncol);
		}
	}
}

#if OPT_VIDEO_ATTRS
static void
updattrs(WINDOW *wp)
{
    AREGION *ap;
    int i;

    L_NUM start_wlnum, end_wlnum;
    LINEPTR lp;
    int rows;

    /*
     * Clear portion of virtual screen associated with window pointer
     * of all attributes.
     */
    /* FIXME: color; need to set to value indicating fg and bg for window */
    for (i = wp->w_toprow + wp->w_ntrows - 1; i >= wp->w_toprow; i--)
	set_vattrs(i, 0, 0, term.t_ncol);

    /*
     * No need to do any more work on this window if there are no
     * attributes.
     */
    if (wp->w_bufp->b_attribs == NULL)
	return;

    /*
     * Compute starting and ending line numbers for the window.  We
     * also fill in lmap which is used for mapping line numbers to
     * screen row numbers.
     */
    lp = wp->w_line.l;
    start_wlnum =
    end_wlnum = line_no(wp->w_bufp, lp);
    rows = wp->w_ntrows;
    lmap[end_wlnum - start_wlnum] = TopRow(wp);
    while ( (rows -= line_height(wp,lp)) > 0) {
	lp = lforw(lp);
	end_wlnum++;
	lmap[end_wlnum - start_wlnum] = TopRow(wp) + wp->w_ntrows - rows;
    }

    /*
     * Set current attributes in virtual screen associated with window
     * pointer.
     */
    for (ap = wp->w_bufp->b_attribs; ap != NULL;) {
	VIDEO_ATTR attr;
	C_NUM start_col, end_col;
	C_NUM rect_start_col = 0, rect_end_col = 0;
	L_NUM start_rlnum, end_rlnum, lnum, start_lnum, end_lnum;
	start_rlnum = line_no(wp->w_bufp, ap->ar_region.r_orig.l);
	end_rlnum = line_no(wp->w_bufp, ap->ar_region.r_end.l);

	/* Garbage collect empty visible regions */
	if (start_rlnum == end_rlnum
	 && VATTRIB(ap->ar_vattr) != 0
	 && ap->ar_region.r_orig.o >= ap->ar_region.r_end.o) {
	    AREGION *nap = ap->ar_next;
	    free_attrib(wp->w_bufp, ap);
	    ap = nap;
	    continue;
	}

	if (start_rlnum > start_wlnum) {
	    start_lnum = start_rlnum;
	    lp = ap->ar_region.r_orig.l;
	}
	else {
	    start_lnum = start_wlnum;
	    lp = wp->w_line.l;
	}
	end_lnum = (end_rlnum < end_wlnum) ? end_rlnum : end_wlnum;
	attr = ap->ar_vattr;
	if (ap->ar_shape == RECTANGLE) {
	    int n;
	    rect_start_col = mark2col(wp, ap->ar_region.r_orig);
	    rect_end_col   = mark2col(wp, ap->ar_region.r_end);
	    if (rect_end_col < rect_start_col) {
		    C_NUM col = rect_end_col;
		    rect_end_col = rect_start_col;
		    rect_start_col = col;
		    n = MARK2COL(wp, ap->ar_region.r_orig);
	    } else {
		    n = MARK2COL(wp, ap->ar_region.r_end);
	    }
	    if (rect_end_col < n)
		rect_end_col = n;
	}
	for (lnum = start_lnum; lnum <= end_lnum; lnum++, lp = lforw(lp)) {
	    int row, col;
	    if (ap->ar_shape == RECTANGLE) {
		start_col = rect_start_col;
	    } else if (lnum == start_rlnum) {
		start_col = mark2col(wp, ap->ar_region.r_orig);
	    } else {
		start_col = w_left_margin(wp) + nu_width(wp);
	    }

	    if (start_col < w_left_margin(wp))
		start_col = (lnum == start_rlnum)
			? w_left_margin(wp) + nu_width(wp)
			: w_left_margin(wp);

	    if (ap->ar_shape == RECTANGLE) {
		end_col = rect_end_col;
	    } else if (lnum == end_rlnum) {
		int n = mark2col(wp, ap->ar_region.r_end) - 1;
		end_col = offs2col(wp, ap->ar_region.r_end.l,
				   ap->ar_region.r_end.o - 1);
		if (end_col < n)
			end_col = n;
	    } else {
		end_col = offs2col(wp, lp, llength(lp));
#ifdef WMDLINEWRAP
		if (w_val(wp,WMDLINEWRAP)
		 && (end_col % term.t_ncol) == 0)
		    end_col--;	/* cannot highlight the newline */
#endif
	    }
	    row = lmap[lnum - start_wlnum];
#ifdef WMDLINEWRAP
	    if (w_val(wp,WMDLINEWRAP))
	    {
		for (col = start_col; col <= end_col; col++) {
		    int x = row + col / term.t_ncol;
		    if  (x < 0)
			continue;
		    if (x < mode_row(wp)) {
			int y = col % term.t_ncol;
			vscreen[x]->v_attrs[y] =
			    (vscreen[x]->v_attrs[y] | (attr & ~VAREV))
			    ^ (attr & VAREV);
		    }
		    else
			break;
		}
	    }
	    else
#endif
	    {
		if (end_col >= term.t_ncol)
		    end_col = term.t_ncol-1;
		for (col = start_col; col <= end_col; col++)
		    vscreen[row]->v_attrs[col] =
			(vscreen[row]->v_attrs[col] | (attr & ~VAREV))
			^ (attr & VAREV);
	    }
	}
	ap = ap->ar_next;
    }
}
#endif /* OPT_VIDEO_ATTRS */

/*
 * Translate offset (into a line's text) into the display-column, taking into
 * account the tabstop, sideways, number- and list-modes.
 */
int
offs2col(
WINDOW	*wp,
LINEPTR	lp,
C_NUM	offset)
{
	int	length = llength(lp);
	int	column = 0;
	int	tabs = tabstop_val(wp->w_bufp);
	int	list = w_val(wp,WMDLIST);
	int	left =
#ifdef WMDLINEWRAP	/* overrides left/right scrolling */
			w_val(wp,WMDLINEWRAP) ? 0 :
#endif
			w_val(wp,WVAL_SIDEWAYS);

	register C_NUM	n, c;

	/* this makes the how-much-to-select calculation easier above */
	if (offset < 0)
		return offset;

	if (lp == win_head(wp)) {
		column = 0;
	} else {
		for (n = w_left_margin(wp); (n < offset) && (n <= length); n++) {
			c = (n >= length) ? '\n' : lp->l_text[n];
			if (isPrint(c)) {
				column++;
			} else if (list || (c != '\t')) {
				column += (c & HIGHBIT) ? 4 : 2;
			} else if (c == '\t') {
				column = ((column / tabs) + 1) * tabs;
			}
		}
		column = column - left + nu_width(wp) + w_left_margin(wp);
	}
	return column;
}

/*
 * Translate a display-column (assuming an infinitely-wide display) into the
 * line's offset, taking into account the tabstop, sideways, number and list
 * modes.
 */
#if OPT_MOUSE || defined(WMDLINEWRAP)
int
col2offs(
WINDOW	*wp,
LINEPTR	lp,
C_NUM	col)
{
	int	tabs = tabstop_val(wp->w_bufp);
	int	list = w_val(wp,WMDLIST);
	int	left =
#ifdef WMDLINEWRAP	/* overrides left/right scrolling */
			w_val(wp,WMDLINEWRAP) ? 0 :
#endif
			w_val(wp,WVAL_SIDEWAYS);
	int	goal = col + left - nu_width(wp) - w_left_margin(wp);

	register C_NUM	n;
	register C_NUM	offset;
	register C_NUM	len	= llength(lp);
	register char	*text	= lp->l_text;

	if (lp == win_head(wp)) {
		offset = 0;
	} else {
		for (offset = w_left_margin(wp), n = 0;
			(offset < len) && (n < goal);
				offset++) {
			register int c = text[offset];
			if (isPrint(c)) {
				n++;
			} else if (list || (c != '\t')) {
				n += (c & HIGHBIT) ? 4 : 2;
			} else if (c == '\t') {
				n = ((n / tabs) + 1) * tabs;
			}
			if (n > goal)
				break;
		}
	}
	return offset;
}
#endif

/*
 * Compute the number of rows required for displaying a line.
 */
#ifdef WMDLINEWRAP
int
line_height(
WINDOW	*wp,
LINEPTR	lp)
{
	int hi = 1;
	if (w_val(wp,WMDLINEWRAP)) {
		int	len = llength(lp);
		if (len > 0) {
			int col = offs2col(wp,lp,len) - 1;
			if (ins_mode(wp) != FALSE
			 && lp == DOT.l
			 && len <= DOT.o) {
				col++;
				if (w_val(wp,WMDLIST))
					col++;
			} else if (w_val(wp,WMDLIST)) {
				col += 2;
			}
			hi = (col / term.t_ncol) + 1;
		}
	}
	return hi;
}
#endif

/*
 * Given a row on the screen, determines which window it belongs to.  Returns
 * null only for the message line.
 */
#if defined(WMDLINEWRAP) || OPT_MOUSE
WINDOW *
row2window (int row)
{
	register WINDOW *wp;

	for_each_visible_window(wp)
		if (row >= wp->w_toprow && row <= mode_row(wp))
			return wp;
	return 0;
}
#endif

/*
 * Highlight the requested portion of the screen.  We're mucking with the video
 * attributes on the line here, so this is NOT good code - it would be better
 * if there was an individual colour attribute per character, rather than per
 * row, but I didn't write the original code.  Anyway, hilite is called only
 * once so far, so it's not that big a deal.
 */
void
hilite(
int	row,		/* row to start highlighting */
int	colfrom,	/* column to start highlighting */
int	colto,		/* column to end highlighting */
int	on)		/* start highlighting */
{
#if !OPT_VIDEO_ATTRS
	register VIDEO *vp1 = vscreen[row];
#endif
#ifdef WMDLINEWRAP
	WINDOW	*wp = row2window(row);
	if (w_val(wp,WMDLINEWRAP)) {
		if (colfrom < 0)
			colfrom = 0;
		if (colfrom > term.t_ncol) {
			do {
				row++;
				colfrom -= term.t_ncol;
				colto   -= term.t_ncol;
				hilite(row, colfrom, colto, on);
			} while (colto > term.t_ncol);
			return;
		}
	}
#endif
	if (row < term.t_nrow-1 && (colfrom >= 0 || colto <= term.t_ncol)) {
		if (colfrom < 0)
			colfrom = 0;
		if (colto > term.t_ncol)
			colto = term.t_ncol;
#if OPT_VIDEO_ATTRS
		if (on) {
		    int col;
		    for (col=colfrom; col<colto; col++)
			vscreen[row]->v_attrs[col] |= VAREV;
		}
		else {
		    int col;
		    for (col=colfrom; col<colto; col++)
			vscreen[row]->v_attrs[col] &= ~VAREV;
		}
		vscreen[row]->v_flag |= VFCHG;
		updateline(row, 0, term.t_ncol);
#else /* OPT_VIDEO_ATTRS */
		if (on) {
			vp1->v_flag |= VFREQ;
		} else {
			vp1->v_flag &= ~VFREQ;
		}
		updateline(row, colfrom, colto);
#endif /* OPT_VIDEO_ATTRS */
	}
}

#if CAN_SCROLL
/* optimize simple scrolled screen region movements.
 * arg. chooses between looking for inserts or deletes.
 * original code by Roger Ove, for an early version of
 * MicroEMACS.  used by permission.
 */
static int
scrolls(int inserts)	/* returns true if it does something */
{
	struct	VIDEO *vpv;	/* virtual screen image */
	struct	VIDEO *vpp;	/* physical screen image */
	int	i, j, k ;
	int	rows, cols ;
	int	first, match, count, ptarget = 0, vtarget = 0;
	SIZE_T	end;
	int	longmatch, longcount;
	int	longinplace, inplace;	/* count of lines which are already
					   in the right place */
	int	from, to;

	if (term.t_scroll == null_t_scroll) /* can't scroll */
		return FALSE;

	rows = term.t_nrow -1;
	cols = term.t_ncol ;

	/* find first line that doesn't match */
	first = -1 ;
	for (i = 0; i < rows; i++) {
		if (!texttest(i,i)) {
			first = i;
			break;
		}
	}
	if (first < 0)
		return FALSE;		/* there isn't one */

	vpv = vscreen[first] ;
	vpp = PScreen(first) ;

	if (inserts) {
		/* determine types of potential scrolls */
		end = endofline(vpv->v_text,cols) ;
		if ( end == 0 )
			ptarget = first ;		/* newlines */
		else if ( memcmp(vpp->v_text, vpv->v_text, end) == 0 )
			ptarget = first + 1 ;	/* broken line newlines */
		else
			ptarget = first ;
		from = ptarget;
	} else {
		from = vtarget = first + 1 ;
	}

	/* can we find that text elsewhere ? */
	longmatch = -1;
	longcount = 0;
	longinplace = 0;
	for (i = from+1; i < rows; i++) {
		if (inserts ? texttest(i,from) : texttest(from,i) ) {
			match = i ;
			count = 1 ;
			inplace = texttest(match, match) ? 1 : 0;
			for (j=match+1, k=from+1; j<rows && k<rows; j++, k++) {
				if (inserts ? texttest(j,k) : texttest(k,j)) {
					count++ ;
					if (texttest(j,j))
						inplace++;
				}
				else
					break ;
			}
			if (longcount - longinplace < count - inplace) {
				longcount = count;
				longmatch = match;
				longinplace = inplace;
			}
		}
	}
	match = longmatch;
	count = longcount;

	if (!inserts) {
		/* full kill case? */
		if (match > 0 && texttest(first, match-1)) {
			vtarget-- ;
			match-- ;
			count++ ;
		}
	}

	if (match>0 && count>2) {		 /* got a scroll */
		/* move the count lines starting at ptarget to match */
		/* mlwrite("scrolls: move the %d lines starting at %d to %d",
						count,ptarget,match);
		*/
		if (inserts) {
			from = ptarget;
			to = match;
		} else {
			from = match;
			to = vtarget;
		}
#if OPT_PSCREEN
		/*
		 * Update lines _before_ the scroll so that they will
		 * be available for any updates which need to be done
		 * (due to a GraphicsExpose event in X11...these occur
		 * when scrolling a partially obscured window).  Note
		 * that in the typical case of scrolling a line or two
		 * that very few memory accesses are performed.  We
		 * mostly shuffle pointers around.
		 */
#define SWAP_PLINE(a, b) do { VIDEO *temp = pscreen[a];	\
			      pscreen[a] = pscreen[b];	\
			      pscreen[b] = temp; } one_time
#define CLEAR_PLINE(a)  do {						\
			    MARK_LINE_DIRTY(a);				\
			    for (j = 0; j < term.t_ncol; j++) {		\
				CELL_TEXT(a,j) = ' ';			\
				CELL_ATTR(a,j) = 0;			\
			    }						\
			  } one_time
		if (from < to) {
		    /* FIXME: color */
		    for (i = from; i < to; i++)
			CLEAR_PLINE(i+count);
		    for (i = count-1; i >= 0; i--)
			SWAP_PLINE(from+i, to+i);
		}
		else {
		    /* FIXME: color */
		    for (i = to; i < from; i++)
			CLEAR_PLINE(i);
		    for (i = 0; i < count; i++)
			SWAP_PLINE(from+i, to+i);
		}
#endif /* OPT_PSCREEN */
		scrscroll(from, to, count) ;
#if !OPT_PSCREEN
		for (i = 0; i < count; i++) {
			vpp = PScreen(to+i) ;
			vpv = vscreen[to+i];
			(void)memcpy(vpp->v_text, vpv->v_text, (SIZE_T)cols) ;
		}
#if OPT_VIDEO_ATTRS && !FRAMEBUF
#define SWAP_ATTR_PTR(a, b) do { VIDEO_ATTR *temp = pscreen[a]->v_attrs;  \
				 pscreen[a]->v_attrs = pscreen[b]->v_attrs; \
				 pscreen[b]->v_attrs = temp; } one_time
		if (from < to) {
		    /* FIXME: color */
		    for (i = from; i < to; i++)
			for (j = 0; j < term.t_ncol; j++)
			    CELL_ATTR(i+count,j) = 0;
		    for (i = count-1; i >= 0; i--)
			SWAP_ATTR_PTR(from+i, to+i);

		}
		else {
		    /* FIXME: color */
		    for (i = to; i < from; i++)
			for (j = 0; j < term.t_ncol; j++)
			    CELL_ATTR(i,j) = 0;
		    for (i = 0; i < count; i++)
			SWAP_ATTR_PTR(from+i, to+i);
		}
#undef SWAP_ATTR_PTR
#endif /* OPT_VIDEO_ATTRS */
		if (inserts) {
			from = ptarget;
			to = match;
		} else {
			from = vtarget+count;
			to = match+count;
		}
		for (i = from; i < to; i++) {
			char *txt;
			txt = PScreen(i)->v_text;
			for (j = 0; j < term.t_ncol; ++j)
				txt[j] = ' ';
			vscreen[i]->v_flag |= VFCHG;
		}
#endif /* !OPT_PSCREEN */
		return(TRUE) ;
	}
	return(FALSE) ;
}

/* move the "count" lines starting at "from" to "to" */
static void
scrscroll(int from, int to, int count)
{
	beginDisplay();
	ttrow = ttcol = -1;
	TTscroll(from,to,count);
	endofDisplay();
}

static int
texttest(		/* return TRUE on text match */
int	vrow,		/* virtual row */
int	prow)		/* physical row */
{
	struct	VIDEO *vpv = vscreen[vrow] ;	/* virtual screen image */
	struct	VIDEO *vpp = PScreen(prow)  ;	/* physical screen image */

	return (!memcmp(vpv->v_text, vpp->v_text, (SIZE_T)term.t_ncol)) ;
}

/* return the index of the first blank of trailing whitespace */
static int
endofline(char *s, int n)
{
	int	i;
	for (i = n - 1; i >= 0; i--)
		if (s[i] != ' ') return(i+1) ;
	return(0) ;
}

#endif /* CAN_SCROLL */


/* Update the extended line which the cursor is currently on at a column less
 * than the terminal width.  The line will be scrolled right or left to let the
 * user see where the cursor is.
 */
static int
updext(int col, int excess, int use_excess)
{
	register int rcursor;
	register int zero = nu_width(curwp);

	/* calculate what column the real cursor will end up in */
	if (!use_excess) {
		curcol = col;
		rcursor = (col % (term.t_ncol - term.t_margin));
	} else {
		rcursor = ((excess - 1) % term.t_scrsiz) + term.t_margin;
	}
	horscroll = col - rcursor;

	/* Scan through the line outputting characters to the virtual screen
	 * once we reach the left edge.  */
	vtmove(currow, -horscroll);	/* start scanning offscreen */
	vtset(DOT.l, curwp);

	/* truncate the virtual line */
	vteeol();

	horscroll = 0;

	if (use_excess || col != rcursor) { /* ... put a marker in column 1 */
		vscreen[currow]->v_text[zero] = MRK_EXTEND_LEFT;
	}
	vscreen[currow]->v_flag |= (VFEXT|VFCHG);
	return rcursor;
}



/*
 * Update a single line. This does not know how to use insert or delete
 * character sequences; we are using VT52 functionality. Update the physical
 * row and column variables. It does try an exploit erase to end of line.
 */
#if	FRAMEBUF
/*	UPDATELINE specific code for the IBM-PC and other compatibles */

static void
updateline(

int	row,		/* row of screen to update */
int	colfrom,	/* first column on screen */
int	colto)		/* last column on screen */

{
	register struct VIDEO *vp1 = vscreen[row];	/* virtual screen image */
	register int	req = (vp1->v_flag & VFREQ) == VFREQ;

#if	OPT_COLOR
	CurFcolor(vp1) = ReqFcolor(vp1);
	CurBcolor(vp1) = ReqBcolor(vp1);
#endif
#if OPT_VIDEO_ATTRS
	scwrite(row, colfrom, colto - colfrom,
		VideoText(vp1),
		VideoAttr(vp1),
		ReqFcolor(vp1),
		ReqBcolor(vp1));
#else	/* highlighting, anyway */
	scwrite(row, colfrom, colto - colfrom,
		VideoText(vp1),
		(VIDEO_ATTR *)0,
		req ? ReqBcolor(vp1) : ReqFcolor(vp1),
		req ? ReqFcolor(vp1) : ReqBcolor(vp1));
#endif
	vp1->v_flag &= ~(VFCHG | VFCOL); /* flag this line as updated */
	if (req)
		vp1->v_flag |= VFREV;
	else
		vp1->v_flag &= ~VFREV;
}

#else	/* !FRAMEBUF */
#if	OPT_PSCREEN
static void
updateline(
    int	row,		/* row of screen to update */
    int	colfrom,	/* col to start updating from */
    int	colto)		/* col to go to */
{
    register char *vc, *pc, *evc;
    register VIDEO_ATTR *va, *pa;
    int nchanges = 0;

    if ((vscreen[row]->v_flag & VFCHG) == 0)
	return;

    vc  = &vscreen[row]->v_text[colfrom];
    evc = &vscreen[row]->v_text[colto];
    va  = &vscreen[row]->v_attrs[colfrom];
    pc  = &CELL_TEXT(row,colfrom);
    pa  = &CELL_ATTR(row,colfrom);

    while (vc < evc) {
	if (*vc != *pc || VATTRIB(*va) != VATTRIB(*pa)) {
	    *pc = *vc;
	    *pa = *va | VADIRTY;
	    nchanges++;
	}
	vc++;
	pc++;
	va++;
	pa++;
    }

    if (nchanges > 0)
	MARK_LINE_DIRTY(row);
    vscreen[row]->v_flag &= ~(VFCHG | VFCOL); /* mark virtual line updated */
}

#else  /* !OPT_PSCREEN */

/*	UPDATELINE code for all other versions		*/

#define TTattr(a) TTrev(a) /* FIXME */

static void
updateline(

int	row,		/* row of screen to update */
int	colfrom,	/* first column on screen */
int	colto)		/* first column on screen */

{
	struct VIDEO *vp1 = vscreen[row];	/* virtual screen image */
	struct VIDEO *vp2 = PSCREEN[row];	/* physical screen image */
	register int xl = colfrom;
	register int xr = colto;
	register int xx;

	register char *cp1 = VideoText(vp1);
	register char *cp2 = VideoText(vp2);
	register int nbflag;	/* non-blanks to the right flag? */

#if OPT_VIDEO_ATTRS
	register VIDEO_ATTR *ap1 = VideoAttr(vp1);
	register VIDEO_ATTR *ap2 = VideoAttr(vp2);
	VIDEO_ATTR Blank = 0;	/* FIXME: Color? */
#else
	UINT rev;		/* reverse video flag */
	UINT req;		/* reverse video request flag */
#endif

#if !OPT_VIDEO_ATTRS
#if	OPT_COLOR
	TTforg(ReqFcolor(vp1));
	TTbacg(ReqBcolor(vp1));
#endif

#if	OPT_REVSTA || OPT_COLOR
	/* if we need to change the reverse video status of the
	   current line, we need to re-write the entire line     */
	rev = (vp1->v_flag & VFREV) == VFREV;
	req = (vp1->v_flag & VFREQ) == VFREQ;
	if ((rev != req)
#if	OPT_COLOR
	    || (CurFcolor(vp1) != ReqFcolor(vp1))
	    || (CurBcolor(vp1) != ReqBcolor(vp1))
#endif
			) {
		movecursor(row, colfrom);	/* Go to start of line. */
		/* set rev video if needed */
		if (req)
			TTrev(req);

		/* scan through the line and dump it to the screen and
		   the virtual screen array				*/
		for (; xl < colto; xl++) {
			TTputc(cp1[xl]);
			++ttcol;
			cp2[xl] = cp1[xl];
		}
		/* turn rev video off */
		if (req)
			TTrev(FALSE);

		/* update the needed flags */
		vp1->v_flag &= ~(VFCHG|VFCOL);
		if (req)
			vp1->v_flag |= VFREV;
		else
			vp1->v_flag &= ~VFREV;
#if	OPT_COLOR
		CurFcolor(vp1) = ReqFcolor(vp1);
		CurBcolor(vp1) = ReqBcolor(vp1);
#endif
		return;
	}
#else
	rev = FALSE;
#endif	/* OPT_REVSTA || OPT_COLOR */
#endif	/* !OPT_VIDEO_ATTRS */

	/* advance past any common chars at the left */
#if !OPT_VIDEO_ATTRS
	if (!rev)
#endif	/* !OPT_VIDEO_ATTRS */
		while (xl != colto
		    && cp1[xl] == cp2[xl]
#if OPT_VIDEO_ATTRS
		    && VATTRIB(ap1[xl]) == VATTRIB(ap2[xl])
#endif	/* OPT_VIDEO_ATTRS */
		      ) {
			++xl;
		}

/* This can still happen, even though we only call this routine on changed
 * lines. A hard update is always done when a line splits, a massive
 * change is done, or a buffer is displayed twice. This optimizes out most
 * of the excess updating. A lot of computes are used, but these tend to
 * be hard operations that do a lot of update, so I don't really care.
 */
	/* if both lines are the same, no update needs to be done */
	if (xl == colto) {
		vp1->v_flag &= ~VFCHG;
		return;
	}

	/* find out if there is a match on the right */
	nbflag = FALSE;

#if !OPT_VIDEO_ATTRS
	if (!rev)
#endif
		while (cp1[xr-1] == cp2[xr-1]
#if OPT_VIDEO_ATTRS
		    && VATTRIB(ap1[xr-1]) == VATTRIB(ap2[xr-1])
#endif
		 ) {
			--xr;
			/* Note if any nonblank in right match */
			if (cp1[xr] != ' '
#if OPT_VIDEO_ATTRS
			 || VATTRIB(ap1[xr] != Blank)
#endif
			  )
				nbflag = TRUE;
		}

	xx = xr;

	/* Erase to EOL ? */
	if (nbflag == FALSE
	 && eolexist == TRUE
#if	OPT_REVSTA && !OPT_VIDEO_ATTRS
	 && (req != TRUE)
#endif
	   ) {
		while ((xx != xl)
		    && cp1[xx-1] == ' '
#if OPT_VIDEO_ATTRS
		    && VATTRIB(ap1[xx-1]) == Blank
#endif
		)
			xx--;

		if ((xr - xx) <= 3)		/* Use only if erase is */
			xx = xr;		/* fewer characters. */
	}

	movecursor(row, xl - colfrom);		/* Go to start of line. */
#if OPT_VIDEO_ATTRS
	while (xl < xx) {
		register int j = xl;
		VIDEO_ATTR attr = VATTRIB(ap1[j]);
		while ((j < xx) && (attr == VATTRIB(ap1[j])))
			j++;
		TTattr(attr);
		for (; xl < j; xl++) {
			TTputc(cp1[xl]);
			++ttcol;
			cp2[xl] = cp1[xl];
			ap2[xl] = ap1[xl];
		}
	}
	TTattr(0);

	if (xx != xr) {				/* Erase. */
		TTeeol();
		for (; xl < xr; xl++) {
			if (cp2[xl] != cp1[xl]
			 || VATTRIB(ap2[xl]) != VATTRIB(ap1[xl]))
				ap2[xl] = ap1[xl];
			cp2[xl] = cp1[xl];
		}
	}
#else /* OPT_VIDEO_ATTRS */
#if	OPT_REVSTA
	TTrev(rev);
#endif

	for (; xl < xr; xl++) {		/* Ordinary. */
		TTputc(cp1[xl]);
		++ttcol;
		cp2[xl] = cp1[xl];
	}

	if (xx != xr) {		/* Erase. */
		TTeeol();
		for (; xl < xr; xl++) {
			cp2[xl] = cp1[xl];
		}
	}
#if	OPT_REVSTA
	TTrev(FALSE);
#endif
#endif /* OPT_VIDEO_ATTRS */
	vp1->v_flag &= ~(VFCHG|VFCOL);
	return;
}
#endif  /* OPT_PSCREEN(updateline) */
#endif	/* FRAMEBUF(updateline) */

/*
 * Redisplay the mode line for the window pointed to by the "wp".
 * modeline() is the only routine that has any idea of how the modeline is
 * formatted.  You can change the modeline format by hacking at this
 * routine.  Called by "update" any time there is a dirty window.
 */
#if OPT_MLFORMAT
static void
mlfs_prefix(
    char **fsp,
    char **msp,
    int lchar)
{
    register char *fs = *fsp;
    register char *ms = *msp;
    if (*fs == ':') {
	fs++;
	while (*fs && *fs != ':') {
	    if (*fs != '%')
		*ms++ = *fs++;
	    else {
		fs++;
		switch(*fs++) {
		    case EOS :
			fs--;
			break;
		    case '%' :
			*ms++ = '%';
			break;
		    case ':' :
			*ms++ = ':';
			break;
		    case '-' :
			*ms++ = (char)lchar;
			break;
		    default :
			*ms++ = '%';
			*ms++ = *(fs-1);
			break;
		}
	    }
	}
    }
    *fsp = fs;
    *msp = ms;
}

static void
mlfs_suffix(
    char **fsp,
    char **msp,
    int lchar)
{
    mlfs_prefix(fsp, msp, lchar);
    if (**fsp == ':')
	(*fsp)++;
}

static void
mlfs_skipfix(char **fsp)
{
    register char *fs = *fsp;
    if (*fs == ':') {
	for (fs++;*fs && *fs != ':'; fs++);
	if (*fs == ':')
	    fs++;
	for (;*fs && *fs != ':'; fs++);
	if (*fs == ':')
	    fs++;
    }
    *fsp = fs;
}
#endif /* OPT_MLFORMAT */

#define PutModename(format, name) { \
		if (ms != 0) { \
			ms = lsprintf(ms, format, \
				mcnt ? ' ' : '[', \
				name); \
		} \
		mcnt++; \
	}

#define PutMode(mode,name) \
	if (b_val(bp, mode)) PutModename("%c%s", name)

#if OPT_MAJORMODE
#define PutMajormode(bp) if (bp->majr != 0) PutModename("%c%smode", bp->majr->name)
#else
#define PutMajormode(bp) /*nothing*/
#endif

static int
modeline_modes(
BUFFER *bp,
char	**msptr)
{
	register char *ms = msptr ? *msptr : 0;
	register SIZE_T mcnt = 0;

#if CC_CANNOT_OFFSET_CASES
	PutMajormode(bp)
#if !OPT_MAJORMODE
	PutMode(MDCMOD,		"cmode")
#endif
#if OPT_ENCRYPT
	PutMode(MDCRYPT,	"crypt")
#endif
	PutMode(MDDOS,		"dos-style")
	PutMode(MDREADONLY,	"read-only")
	PutMode(MDVIEW,		"view-only")
#if OPT_LCKFILES
	PutMode(MDLOCKED,	"locked by")
#endif
#else
	static	const	struct {
		int   mode;
		const char *name;
	} table[] = {
#if !OPT_MAJORMODE
		{MDCMOD,    "cmode"},
#endif
#if OPT_ENCRYPT
		{MDCRYPT,   "crypt"},
#endif
		{MDDOS,     "dos-style"},
		{MDREADONLY,"read-only"},
		{MDVIEW,    "view-only"},
#if OPT_LCKFILES
		{MDLOCKED,  "locked by"},  /* keep this last */
#endif
	};
	register SIZE_T j;

	PutMajormode(bp)
	for (j = 0; j < TABLESIZE(table); j++) {
		PutMode(table[j].mode, table[j].name)
	}
#endif
#if OPT_LCKFILES
	if (ms != 0 && b_val(bp, MDLOCKED))
		ms = lsprintf(ms, " %s", b_val_ptr(bp,VAL_LOCKER));
#endif
	if (mcnt && ms)
		*ms++ = ']';
	if (b_is_changed(bp)) {
		if (ms != 0)
			ms = lsprintf(ms, "%s[modified]", mcnt ? " " : "");
		mcnt++;
	}
	if (kbd_mac_recording()) {
		if (ms != 0)
			ms = lsprintf(ms, "%s[recording]", mcnt ? " " : "");
		mcnt++;
	}
	if (ms != 0)
		*msptr = ms;
	return (mcnt != 0);
}

static int
modeline_show(
WINDOW *wp,
int lchar)
{
	register int ic = lchar;
	register BUFFER *bp = wp->w_bufp;

	if (b_val(bp, MDSHOWMODE)) {
#ifdef insertmode	/* insert mode is a trait for each window */
		if (wp->w_traits.insmode == INSERT)
			ic = 'I';
		else if (wp->w_traits.insmode == REPLACECHAR)
			ic = 'R';
		else if (wp->w_traits.insmode == OVERWRITE)
			ic = 'O';
#else			/* insertmode is a variable global to all windows */
		if (wp == curwp) {
			if (insertmode == INSERT)
				ic = 'I';
			else if (insertmode == REPLACECHAR)
				ic = 'R';
			else if (insertmode == OVERWRITE)
				ic = 'O';
		}
#endif /* !defined(insertmode) */
#if OPT_ICURSOR
		TTicursor(ic != lchar);
#endif
	}
	return ic;
}

static const char *
rough_position(WINDOW *wp)
{
	LINE *lp = wp->w_line.l;
	int rows = wp->w_ntrows;
	const char *msg = 0;

	while (rows-- > 0) {
		lp = lforw(lp);
		if (lp == win_head(wp)) {
			msg = "bot";
			break;
		}
	}
	if (lback(wp->w_line.l) == win_head(wp)) {
		if (msg) {
			if (wp->w_line.l == win_head(wp))
				msg = "emp";
			else
				msg = "all";
		} else {
			msg = "top";
		}
	}
	if (!msg)
		msg = "mid";
	return msg;
}

static void
modeline(WINDOW *wp)
{
#if OPT_MLFORMAT
    char *fs = modeline_format;
    int fc;
#endif
    char temp[NFILEN];
    char left_ms[NFILEN*2];
    char right_ms[NFILEN*2];
    char *ms;
    register int n;
    int lchar;
    int col;
    int right_len;
    int need_eighty_column_indicator = FALSE;
    register BUFFER *bp;

    TTcursor(FALSE);
    left_ms[0] = right_ms[0] = EOS;
    ms = left_ms;

    n = mode_row(wp);		/* Location. */
#if OPT_VIDEO_ATTRS
    {
	VIDEO_ATTR attr;
	if (wp == curwp)
	    attr = VAMLFOC;
	else
	    attr = VAML;
#if	OPT_REVSTA
#ifdef	GVAL_MCOLOR
	if (global_g_val(GVAL_MCOLOR) & VASPCOL)
	    attr |= VCOLORATTR(global_g_val(GVAL_MCOLOR) & 0xf);
	else
	    attr |= global_g_val(GVAL_MCOLOR);
#else
	    attr |= VAREV;
#endif
#endif
	vscreen[n]->v_flag |= VFCHG;
	set_vattrs(n, 0, attr, term.t_ncol);
    }
#else
    vscreen[n]->v_flag |= VFCHG | VFREQ | VFCOL;/* Redraw next time. */
#endif
#if	OPT_COLOR
    ReqFcolor(vscreen[n]) = gfcolor;
    ReqBcolor(vscreen[n]) = gbcolor;
#endif
    bp = wp->w_bufp;
    vtmove(n, 0);				/* Seek to right line. */
    if (wp == curwp) {				/* mark the current buffer */
	lchar = '=';
    } else {
#if	OPT_REVSTA
	if (revexist)
	    lchar = ' ';
	else
#endif
	    lchar = '-';
    }

#if OPT_MLFORMAT
    while (*fs) {
	if (*fs != '%')
	    *ms++ = *fs++;
	else {
	    fs++;
	    switch ((fc = *fs++)) {
		case EOS :			/* Null character ! */
		    fs--;
		    break;
		case '%' :
		case ':' :
		    *ms++ = *(fs-1);
		    break;
		case '|' :
		    need_eighty_column_indicator = TRUE;
		    break;
		case '-' :
		    *ms++ = (char)lchar;
		    break;
		case '=' :
		    *ms = EOS;
		    ms = right_ms;
		    break;
		case 'i' :			/* insert mode indicator */
		    *ms++ = (char)modeline_show(wp, lchar);
		    break;
		case 'b' :
		    ms = lsprintf(ms, "%s", bp->b_bname);
		    break;
		case 'm' :
		    if (modeline_modes(bp, (char **)0)) {
			mlfs_prefix(&fs, &ms, lchar);
			(void)modeline_modes(bp, &ms);
			mlfs_suffix(&fs, &ms, lchar);
		    }
		    else
			mlfs_skipfix(&fs);
		    break;
		case 'f' :
		case 'F' : {
		    char *p = 0;
		    if (bp->b_fname != 0
		     && (p = shorten_path(strcpy(temp,bp->b_fname), FALSE)) != 0
		     && *p
		     && !eql_bname(bp,p)
		     && (fc == 'f' ? !is_internalname(p)
				   : is_internalname(p))) {
			mlfs_prefix(&fs, &ms, lchar);
			for (; *p == ' '; p++);
			ms = lsprintf(ms, "%s", p);
			mlfs_suffix(&fs, &ms, lchar);
		    }
		    else
			mlfs_skipfix(&fs);
		    break;
		}
#ifdef WMDRULER
		case 'l' :		/* line number */
		case 'c' :		/* column number */
		case 'p' :		/* percentage */
		case 'L' :		/* number of lines in buffer */

		    if (w_val(wp,WMDRULER) && !is_empty_buf(wp->w_bufp)) {
			int val = 0;
			switch (fc) {
			    case 'l' : val = wp->w_ruler_line; break;
			    case 'L' : val = line_count(wp->w_bufp); break;
			    case 'c' : val = wp->w_ruler_col; break;
			    case 'p' : val = wp->w_ruler_line*100
					     / line_count(wp->w_bufp); break;
			}
			mlfs_prefix(&fs, &ms, lchar);
			ms = lsprintf(ms, "%d", val);
			mlfs_suffix(&fs, &ms, lchar);
		    }
		    else
			mlfs_skipfix(&fs);
		    break;

#endif
		case 'S' :
		    if (
#ifdef WMDRULER
			!w_val(wp, WMDRULER) ||
#endif
			is_empty_buf(wp->w_bufp)) {
			mlfs_prefix(&fs, &ms, lchar);
			ms = lsprintf(ms, " %s ", rough_position(wp));
			mlfs_suffix(&fs, &ms, lchar);
		    }
		    else
			mlfs_skipfix(&fs);
		    break;
		default :
		    *ms++ = '%';
		    *ms++ = *(fs-1);
		    break;
	    }
	}
    }
#else	/* hard-coded format */
    ms = lsprintf(ms, "%c%c%c %s ",
	lchar, modeline_show(wp, lchar), lchar, bp->b_bname);
    if (modeline_modes(bp, &ms))
	*ms++ = ' ';
    if (bp->b_fname != 0
    && (shorten_path(strcpy(temp,bp->b_fname), FALSE))
    && !eql_bname(bp,temp)) {
	if (is_internalname(temp)) {
	    for (n = term.t_ncol - (13 + strlen(temp) + (int)(ms - left_ms));
			n > 0; n--)
		*ms++ = lchar;
	} else {
	    ms = lsprintf(ms, "is");
	}
	ms = lsprintf(ms, " %s ", temp);
    }
#ifdef WMDRULER
    if (w_val(wp, WMDRULER))
	(void)lsprintf(right_ms, " (%d,%d) %3p",
		wp->w_ruler_line, wp->w_ruler_col, lchar);
    else
#endif
     (void) lsprintf(right_ms, " %s %3p", rough_position(wp), lchar);
#endif /* OPT_MLFORMAT */

    *ms++ = EOS;
    right_len = strlen(right_ms);
    vtputsn(left_ms, term.t_ncol);
    for (n = term.t_ncol - strlen(left_ms) - right_len; n > 0; n--)
	vtputc(lchar);
    vtcol = term.t_ncol - right_len;
    if (vtcol < 0) {
	n = -vtcol;
	vtcol = 0;
    }
    else
	n = 0;
    vtputsn(right_ms+n, term.t_ncol - vtcol);
    if (need_eighty_column_indicator) {		/* mark column 80 */
	int left = -nu_width(wp);
#ifdef WMDLINEWRAP
	if (!w_val(wp,WMDLINEWRAP))
#endif
	 left += w_val(wp,WVAL_SIDEWAYS);
	n = term.t_ncol + left;
	col = 80 - left;

	if ((n > 80) && (col >= 0) && (vtgetc(col) == lchar)) {
	    vtcol = col;
	    vtputc('|');
	}
    }
    TTcursor(TRUE);
}

void
upmode(void)	/* update all the mode lines */
{
	register WINDOW *wp;

	for_each_window(wp)
		wp->w_flag |= WFMODE;
}

/*
 * Recompute the given buffer. Save/restore its modes and position information
 * so that a redisplay will show as little change as possible.
 */
#if	OPT_UPBUFF
typedef	struct	{
	WINDOW	*wp;
	struct VAL w_vals[MAX_W_VALUES];
	int	top;
	int	line;
	int	col;
	} SAVEWIN;

static	SAVEWIN	*recomp_tbl;
static	ALLOC_T	recomp_len;

static void
recompute_buffer(BUFFER *bp)
{
	register WINDOW *wp;
	register SAVEWIN *tbl;

	struct VAL b_vals[MAX_B_VALUES];
	ALLOC_T	num = 0;
	BUFFER *savebp = curbp;
	WINDOW *savewp = curwp;
	int	mygoal = curgoal;

	if (!b_val(bp,MDUPBUFF)) {
		b_clr_obsolete(bp);
		return;
	}
	if (recomp_len < bp->b_nwnd) {
		recomp_len = bp->b_nwnd + 1;
		recomp_tbl = (recomp_tbl != 0)
			? typereallocn(SAVEWIN,recomp_tbl,recomp_len)
			: typeallocn(SAVEWIN,recomp_len);
		if (recomp_tbl == 0) {
			recomp_len = 0;
			return;
		}
	}
	tbl = recomp_tbl;

	/* remember where we are, to reposition */
	/* ...in case line is deleted from buffer-list */
	relisting_b_vals = 0;
	relisting_w_vals = 0;
	if (curbp == bp) {
		relisting_b_vals = b_vals;
	} else {
		curbp = bp;
		curwp = bp2any_wp(bp);
	}
	for_each_visible_window(wp) {
		if (wp->w_bufp == bp) {
			if (wp == savewp)
				relisting_w_vals = tbl[num].w_vals;
			curwp = wp;	/* to make 'getccol()' work */
			curbp = curwp->w_bufp;
			tbl[num].wp   = wp;
			tbl[num].top  = line_no(bp, wp->w_line.l);
			tbl[num].line = line_no(bp, wp->w_dot.l);
			tbl[num].col  = getccol(FALSE);
			save_vals(NUM_W_VALUES, global_w_values.wv,
				tbl[num].w_vals, wp->w_values.wv);
			if (++num >= recomp_len)
				break;
		}
	}
	curwp = savewp;
	curbp = savebp;

	save_vals(NUM_B_VALUES, global_b_values.bv, b_vals, bp->b_values.bv);
	(bp->b_upbuff)(bp);
	copy_mvals(NUM_B_VALUES, bp->b_values.bv, b_vals);

	/* reposition and restore */
	while (num-- != 0) {
		curwp = wp = tbl[num].wp;
		curbp = curwp->w_bufp;
		(void)gotoline(TRUE, tbl[num].top);
		wp->w_line.l = wp->w_dot.l;
		wp->w_line.o = 0;
		if (tbl[num].line != tbl[num].top)
			(void)gotoline(TRUE, tbl[num].line);
		(void)gocol(tbl[num].col);
		wp->w_flag |= WFMOVE;
		copy_mvals(NUM_W_VALUES, wp->w_values.wv, tbl[num].w_vals);
	}
	curwp = savewp;
	curbp = savebp;
	curgoal = mygoal;
	b_clr_obsolete(bp);
	relisting_b_vals = 0;
	relisting_w_vals = 0;
}
#endif	/* OPT_UPBUFF */

/*
 * Send a command to the terminal to move the hardware cursor to row "row"
 * and column "col". The row and column arguments are origin 0. Optimize out
 * random calls. Update "ttrow" and "ttcol".
 */
void
movecursor(int row, int col)
{
	beginDisplay();
	if ((row!=ttrow || col!=ttcol)
	 && (row >= 0 && row < term.t_nrow)
	 && (col >= 0 && col < term.t_ncol))
	{
		ttrow = row;
		ttcol = col;
		TTmove(row, col);
	}
	endofDisplay();
}

void
bottomleft(void)
{
	movecursor(term.t_nrow-1, 0);
}

/*
 * Erase the message line. This is a special routine because the message line
 * is not considered to be part of the virtual screen. It always works
 * immediately; the terminal buffer is flushed via a call to the flusher.
 */
void
mlerase(void)
{
	beginDisplay();
	kbd_erase_to_end(0);
	kbd_flush();
	endofDisplay();
}

static char *mlsavep;

void
mlsavec(int c)
{
	if (mlsavep - mlsave < NSTRING-1) {
		*mlsavep++ = (char)c;
		*mlsavep = EOS;
	}
}

/*
 * Write a message into the message line only if appropriate.
 */
/* VARARGS1 */
void
mlwrite(const char *fmt, ...)
{
	va_list ap;
	/* if we are not currently echoing on the command line, abort this */
	if (global_b_val(MDTERSE) || kbd_replaying(FALSE) || discmd == FALSE) {
		bottomleft();
		return;
	}
	va_start(ap,fmt);
	mlmsg(fmt,&ap);
	va_end(ap);
}

/*	Put a string out to the message line regardless of the
	current $discmd setting. This is needed when $debug is TRUE
	and for the write-message and clear-message-line commands
	Also used for most errors, to be sure they're seen.
*/
/* VARARGS1 */
void
mlforce(const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	mlmsg(fmt,&ap);
	va_end(ap);
}

/* VARARGS1 */
void
mlprompt(const char *fmt, ...)
{
	va_list ap;
	int osgarbf = sgarbf;
	if (discmd == FALSE) {
		bottomleft();
		return;
	}
	sgarbf = FALSE;
	va_start(ap,fmt);
	mlmsg(fmt,&ap);
	va_end(ap);
	sgarbf = osgarbf;
}

/* VARARGS */
void
dbgwrite(const char *fmt, ...)
{
	char temp[80];

	va_list ap;	/* ptr to current data field */
	va_start(ap,fmt);
	lsprintf(temp, "[press ^G to continue] %s", fmt);
	mlmsg(temp,&ap);
	va_end(ap);
	beginDisplay();
	while (TTgetc() != '\007')
		;
	endofDisplay();
}

/*
 * Do the real message-line work.  Keep track of the physical cursor
 * position. A small class of printf like format items is handled.
 * Set the "message line" flag TRUE.
 */
static void
mlmsg(const char *fmt, va_list *app)
{
	static	int	recur;
	int	end_at;
	int	do_crlf = (strchr(fmt, '\n') != 0
			|| strchr(fmt, '\r') != 0);

	if (recur++) {
		/*EMPTY*/;
	} else if (sgarbf) {
		/* then we'll lose the message on the next update(),
		 * so save it now */
		mlsavep = mlsave;
#if	OPT_POPUP_MSGS
		if (global_g_val(GMDPOPUP_MSGS) || (curwp == 0)) {
			TRACE(("mlmsg popup_msgs #1 for '%s'\n", fmt))
			popup_msgs();
			msg_putc('\n');
			dfoutfn = msg_putc;
		} else
#endif
		  dfoutfn = mlsavec;
		dofmt(fmt,app);
	} else {
		beginDisplay();

		kbd_expand = -1;
#if	OPT_POPUP_MSGS
		if (global_g_val(GMDPOPUP_MSGS)) {
			TRACE(("mlmsg popup_msgs #2 for '%s'\n", fmt))
			popup_msgs();
			if (mlsave[0] == EOS) {
				msg_putc('\n');
				dfoutfn = msg_putc;
			} else {
				dfoutfn = kbd_putc;
			}
		} else
#endif
		  dfoutfn = kbd_putc;

		if (*fmt != '\n') {
			kbd_erase_to_end(0);
			dofmt(fmt,app);
			kbd_expand = 0;

			/* if we can, erase to the end of screen */
			end_at = wminip->w_dot.o;
			kbd_erase_to_end(end_at);
			mlsave[0] = EOS;
			kbd_flush();
		}
		if (do_crlf) {
			kbd_openup();
		}
		endofDisplay();
	}
	recur--;
}

/*
 * Do the equivalent of 'perror()' on the message line
 */
void
mlerror(const char *s)
{
#if HAVE_STRERROR
	if (errno > 0)
		mlwarn("[Error %s: %s]", s, strerror(errno));
	else
		mlwarn("[Error %s: unknown system error %d]", s, errno);
#else
#if HAVE_SYS_ERRLIST
	if (errno > 0 && errno < sys_nerr)
		mlwarn("[Error %s: %s]", s, sys_errlist[errno]);
	else
		mlwarn("[Error %s: unknown system error %d]", s, errno);
#else
	mlwarn("[Error %s, errno=%d]", s, errno);
#endif /* HAVE_SYS_ERRLIST */
#endif /* HAVE_STRERROR */
}

/*
 * Emit a warning message (with alarm)
 */
/* VARARGS1 */
void
mlwarn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	mlmsg(fmt,&ap);
	va_end(ap);
	kbd_alarm();
}

/*
 * Local sprintf -- similar to standard libc, but
 *  returns pointer to null character at end of buffer, so it can
 *  be called repeatedly, as in:
 *	cp = lsprintf(cp, fmt, args...);
 *
 */

static	char *lsp;

static void
lspputc(int c)
{
	*lsp++ = (char)c;
}

/* VARARGS1 */
char *
lsprintf(char *buf, const char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);

	lsp = buf;
	dfoutfn = lspputc;

	dofmt(fmt,&ap);
	va_end(ap);

	*lsp = EOS;
	return lsp;
}


/*
 * Buffer printf -- like regular printf, but puts characters
 *	into the BUFFER.
 */
void
bputc(int c)
{
	if (c == '\n')
		(void)lnewline();
	else
		(void)linsert(1,c);
}

/* printf into curbp, at DOT */
/* VARARGS */
void
bprintf(const char *fmt, ...)
{
	va_list ap;

	dfoutfn = bputc;

	va_start(ap,fmt);
	dofmt(fmt,&ap);
	va_end(ap);

}

#if defined( SIGWINCH) && ! DISP_X11
/* ARGSUSED */
SIGT
sizesignal (int ACTUAL_SIG_ARGS GCC_UNUSED)
{
	int w, h;
	int old_errno = errno;

	getscreensize (&w, &h);

	if ((h > 1 && h != term.t_nrow) || (w > 1 && w != term.t_ncol))
		newscreensize(h, w);

	setup_handler(SIGWINCH, sizesignal);
	errno = old_errno;
	SIGRET;
}
#endif

void
newscreensize (int h, int w)
{
	/* do the change later */
	if (im_displaying
#if OPT_WORKING
	|| !i_displayed
#endif
	) {
		chg_width = w;
		chg_height = h;
		return;
	}
	chg_width = chg_height = 0;
	if ((h > term.t_mrow) || (w > term.t_mcol)) {
		int or, oc;
		or = term.t_mrow;
		oc = term.t_mcol;
		term.t_mrow = h;
		term.t_mcol = w;
		if (!vtinit()) { /* allocation failure */
			term.t_mrow = or;
			term.t_mcol = oc;
			return;
		}
	}
	if (!newlength(TRUE,h) || !newwidth(TRUE,w))
		return;

	(void)update(TRUE);
}

#if OPT_WORKING
/*
 * Start the timer that controls the "working..." message.
 */
static void
start_working(void)
{
	setup_handler(SIGALRM,imworking);
	(void)alarm(1);
	im_timing = TRUE;
}

/*
 * When we stop the timer, we should cleanup the "working..." message.
 */
static void
stop_working(void)
{
	if (mpresf) {	/* erase leftover working-message */
		int	save_row = ttrow;
		int	save_col = ttcol;
		kbd_overlay(0);
		kbd_flush();
		movecursor(save_row, save_col);
		TTflush();
	}
}

/*
 * Displays alternate
 *	"working..." and
 *	"...working"
 * at the end of the message line if it has been at least a second since
 * displaying anything or waiting for keyboard input.  The cur_working and
 * max_working values are used in 'slowreadf()' to show the progress of reading
 * large files.
 */

/*ARGSUSED*/
SIGT
imworking (int ACTUAL_SIG_ARGS GCC_UNUSED)
{
	static	const	char *const msg[] = {"working", "..."};
	static	int	flip;
	static	int	skip;

	signal_was = SIGALRM;	/* remember this was an alarm */

	if (no_working) /* brute force, for debugging */
		return;

	/* (if GMDWORKING is _not_ set, or MDTERSE is set, we're allowed
	 * to erase, but not to write.  and if we do erase, we don't
	 * reschedule the alarm, since setting the mode will call us
	 * again to start things up)
	 */

	if (im_displaying || !i_displayed) { /* look at the semaphore first! */
		/*EMPTY*/;
	} else if (im_waiting(-1)) {
		im_timing = FALSE;
		stop_working();
		return;
	} else if (ShowWorking()) {
		if (skip) {
			skip = FALSE;
		} else {
#if DISP_X11
			x_working();
#else
			char	result[20];
			result[0] = EOS;
			if (cur_working != 0
			 && cur_working != old_working) {
				char	temp[20];
				int	len = cur_working > 999999L ? 10 : 6;

				old_working = cur_working;
				strcat(result, right_num(temp, len, cur_working));
				if (len == 10)
					/*EMPTY*/;
				else if (max_working != 0) {
					strcat(result, " ");
					strcat(result, right_num(temp, 2,
						(100 * cur_working) / max_working));
					strcat(result, "%");
				} else
					strcat(result, " ...");
			} else {
				strcat(result, msg[ flip]);
				strcat(result, msg[!flip]);
			}
			kbd_overlay(result);
			kbd_flush();
#endif
		}
	} else {
		stop_working();
		skip = TRUE;
		return;
	}
	start_working();
	flip = !flip;
}
#endif	/* OPT_WORKING */

/*
 * Maintain a flag that records whether we're waiting for keyboard input.  As a
 * side-effect, restart the 'working' timer if we see that it's been made
 * inactive while we were waiting.
 */
int
im_waiting(int flag)
{
	static int waiting;
	if (flag >= 0) {	/* TRUE or FALSE set, negative used to query */
		waiting = flag;
#if OPT_WORKING
		if (!waiting && !im_timing && ShowWorking())
			start_working();
#endif
	}
	return waiting;
}

#if defined(SIGWINCH) || OPT_WORKING
/*
 * Set the semaphore so that we don't try to do I/O while we're being
 * interrupted.
 */
void
beginDisplay (void)
{
	im_displaying++;
}

/*
 * Reset the semaphore.
 */
void
endofDisplay (void)
{
	if (im_displaying)
		im_displaying--;
}
#endif

#if	OPT_PSCREEN
/* Most of the code in this section is for making the message line work
 * right...it shouldn't be called to display the rest of the screen.
 */
static int psc_row;
static int psc_col;

#define SWAP_INT(x,y) \
	do { (x) = (x)+(y); (y) = (x)-(y); (x) = (x)-(y); } one_time
#define SWAP_VT_PSC \
	do { SWAP_INT(vtcol, psc_col); SWAP_INT(vtrow, psc_row); } one_time

OUTC_DCL
psc_putchar(OUTC_ARGS)
{
    if (c == '\b') {
	if (psc_col > 0)
	    psc_col--;
    }
    else {
	SWAP_VT_PSC;
	vtputc(c);
	vscreen[vtrow]->v_flag |= VFCHG;
	SWAP_VT_PSC;
    }
    OUTC_RET c;
}

void
psc_flush(void)
{
    updateline(term.t_nrow-1, 0, term.t_ncol);
    TTpflush();
}

void
psc_move(int row, int col)
{
    psc_row = row;
    psc_col = col;
}

void
psc_eeol(void)
{
    if (ttrow >= 0 && ttrow < term.t_nrow && ttcol >= 0) {
	VIDEO_ATTR *vp = &vscreen[ttrow]->v_attrs[ttcol];
	char *cp = &vscreen[ttrow]->v_text[ttcol];
	char *cplim = &vscreen[ttrow]->v_text[term.t_ncol];
	vscreen[ttrow]->v_flag |= VFCHG;
	while (cp < cplim) {
	    *vp++ = 0;
	    *cp++ = ' ';
	}
    }
}

void
psc_eeop(void)
{
    int saverow = ttrow;
    int savecol = ttcol;
    while (ttrow < term.t_nrow) {
	psc_eeol();
	ttrow++;
	ttcol = 0;
    }
    ttrow = saverow;
    ttcol = savecol;
}

/* ARGSUSED */
void
psc_rev(UINT huh GCC_UNUSED)
{
    /* do nothing */
}

#endif	/* OPT_PSCREEN */

/* For memory-leak testing (only!), releases all display storage. */
#if NO_LEAKS
void	vt_leaks(void)
{
	vtfree();
#if OPT_UPBUFF
	FreeIfNeeded(recomp_tbl);
#endif
}
#endif
