/*
 * X11 text-output, based on
 *	X11 support, Dave Lemke, 11/91
 *	X Toolkit support, Kevin Buettner, 2/94
 *
 * $Header: /users/source/archives/vile.vcs/RCS/x11plain.c,v 1.3 2013/03/06 10:53:03 tom Exp $
 *
 */

/*
 * Widget set selection.
 *
 * You must have exactly one of the following defined
 *
 *    NO_WIDGETS	-- Use only Xlib and X toolkit (Xt)
 *    ATHENA_WIDGETS	-- Use Xlib, Xt, and Xaw widget set
 *    MOTIF_WIDGETS	-- Use Xlib, Xt, and Motif widget set
 *
 * We derive/set from the configure script some flags that allow intermediate
 * configurations between NO_WIDGETS and ATHENA_WIDGETS
 *
 *    KEV_WIDGETS
 *    OPT_KEV_DRAGGING
 *    OPT_KEV_SCROLLBARS
 */

#include <x11vile.h>

static XFontStruct *
alternate_font(Display * dpy, TextWindow win, const char *weight, const char *slant)
{
    char *newname, *np, *op;
    int cnt;
    XFontStruct *fsp = NULL;

    if (win->fontname == NULL
	|| win->fontname[0] != '-'
	|| (newname = castalloc(char, strlen(win->fontname) + 32))
	== NULL)
	  return NULL;

    /* copy initial two fields */
    for (cnt = 3, np = newname, op = win->fontname; *op && cnt > 0;) {
	if (*op == '-')
	    cnt--;
	*np++ = *op++;
    }
    if (!*op)
	goto done;

    /* substitute new weight and slant as appropriate */
#define SUBST_FIELD(field)				\
    do {						\
	if ((field) != NULL) {				\
	    const char *fp = (field);			\
	    if (nocase_eq(*fp, *op))			\
		goto done;				\
	    while ((*np++ = *fp++))			\
		;					\
	    *(np-1) = '-';				\
	    while (*op && *op++ != '-')			\
		;					\
	}						\
	else {						\
	    while (*op && (*np++ = *op++) != '-')	\
		;					\
	}						\
	if (!*op)					\
	    goto done;					\
    } one_time

    SUBST_FIELD(weight);
    SUBST_FIELD(slant);
#undef SUBST_FIELD

    /* copy rest of name */
    while ((*np++ = *op++)) {
	;			/*nothing */
    }

    TRACE(("x11:alternate_font(weight=%s, slant=%s)\n -> %s\n",
	   NONNULL(weight),
	   NONNULL(slant), newname));

    if ((fsp = XLoadQueryFont(dpy, newname)) != NULL) {
	win->left_ink = win->left_ink || (fsp->min_bounds.lbearing < 0);
	win->right_ink = win->right_ink
	    || (fsp->max_bounds.rbearing > win->char_width);
	TRACE(("...found left:%d, right:%d\n",
	       win->left_ink,
	       win->right_ink));
    }

  done:
    free(newname);
    return fsp;

}

static GC
get_color_gc(Display * dpy, TextWindow win, int n, Bool normal)
{
    ColorGC *data;

    assert(n >= 0 && n < NCOLORS);

    if (n < 0 || n >= NCOLORS)
	n = 0;			/* shouldn't happen */
    data = (normal
	    ? &(win->fore_color[n])
	    : &(win->back_color[n]));
    if (win->screen_depth == 1) {
	data->gc = (normal
		    ? win->textgc
		    : win->reversegc);
    } else if (data->reset) {
	XGCValues gcvals;
	ULONG gcmask;

	gcmask = GCForeground | GCBackground | GCFont | GCGraphicsExposures;
	if (win->bg_follows_fg) {
	    gcvals.foreground = (normal
				 ? win->colors_fg[n]
				 : win->colors_bg[n]);
	    gcvals.background = (normal
				 ? win->colors_bg[n]
				 : win->colors_fg[n]);
	} else {
	    if (normal) {
		gcvals.foreground = win->colors_fg[n];
		gcvals.background = win->bg;
	    } else {
		gcvals.foreground = win->bg;
		gcvals.background = win->colors_fg[n];
	    }
	}
	if (gcvals.foreground == gcvals.background) {
	    gcvals.foreground = normal ? win->fg : win->bg;
	    gcvals.background = normal ? win->bg : win->fg;
	}
	gcvals.font = win->pfont->fid;
	gcvals.graphics_exposures = False;

	TRACE(("get_color_gc(%d,%s) %#lx/%#lx\n",
	       n,
	       normal ? "fg" : "bg",
	       (long) gcvals.foreground,
	       (long) gcvals.background));

	if (data->gc == 0)
	    data->gc = XCreateGC(dpy, DefaultRootWindow(dpy), gcmask, &gcvals);
	else
	    XChangeGC(dpy, data->gc, gcmask, &gcvals);

	TRACE(("... gc %#lx\n", (long) data->gc));
	data->reset = False;
    }
    return data->gc;
}

#define DRAW_WITH(func,buffer,offset) \
	    func(dpy, win->win, fore_gc, \
		 (int) x_pos(win, sc) + offset, fore_yy, \
		 buffer, tlen)

static void
really_draw(Display * dpy,
	    TextWindow win,
	    GC fore_gc,
	    VIDEO_TEXT * text,
	    int tlen,
	    unsigned attr,
	    int sr,
	    int sc)
{
    int fore_yy = text_y_pos(win, sr);
    char buffer[BUFSIZ];
    XChar2b buffer2[sizeof(buffer)];

    while (tlen > 0) {
	Boolean wide = False;
	Cardinal n;

	for (n = 0; ((int) n < tlen) && (n < sizeof(buffer)); ++n) {
#if OPT_MULTIBYTE
	    if (text[n] >= 256) {
		wide = True;
		break;
	    }
#endif
	    buffer[n] = (char) text[n];
	}

	if (wide) {

	    for (n = 0; n < (Cardinal) tlen; ++n) {
		buffer2[n].byte2 = CharOf(text[n]);
		buffer2[n].byte1 = CharOf(text[n] >> 8);
	    }

	    DRAW_WITH(XDrawImageString16, buffer2, 0);
	    if (attr & VABOLD)
		DRAW_WITH(XDrawImageString16, buffer2, 1);
	} else {
	    DRAW_WITH(XDrawImageString, buffer, 0);
	    if (attr & VABOLD)
		DRAW_WITH(XDrawImageString, buffer, 1);
	}
	tlen -= (int) sizeof(buffer);
	if (tlen > 0) {
	    text += sizeof(buffer);
	    sc += (int) sizeof(buffer);
	}
    }
}

/*
 * The X protocol request for clearing a rectangle (PolyFillRectangle) takes
 * 20 bytes.  It will therefore be more expensive to switch from drawing text
 * to filling a rectangle unless the area to be cleared is bigger than 20
 * spaces.  Actually it is worse than this if we are going to switch
 * immediately to drawing text again since we incur a certain overhead
 * (16 bytes) for each string to be displayed.  This is how the value of
 * CLEAR_THRESH was computed (36 = 20+16).
 *
 * Kev's opinion:  If XDrawImageString is to be called, it is hardly ever
 * worth it to call XFillRectangle.  The only time where it will be a big
 * win is when the entire area to update is all spaces (in which case
 * XDrawImageString will not be called).  The following code would be much
 * cleaner, simpler, and easier to maintain if we were to just call
 * XDrawImageString where there are non-spaces to be written and
 * XFillRectangle when the entire region is to be cleared.
 */
#define	CLEAR_THRESH	36

void
xvileDraw(Display * dpy,
	  TextWindow win,
	  VIDEO_TEXT * text,
	  int len,
	  UINT attr,
	  int sr,
	  int sc)
{
    GC fore_gc;
    GC back_gc;
    int fore_yy = text_y_pos(win, sr);
    int back_yy = y_pos(win, sr);
    VIDEO_TEXT *p;
    int cc, tlen, i, startcol;
    int fontchanged = FALSE;

    if (attr == 0) {		/* This is the most common case, so we list it first */
	fore_gc = win->textgc;
	back_gc = win->reversegc;
    } else if ((attr & VACURS) && win->is_color_cursor) {
	fore_gc = win->cursgc;
	back_gc = win->revcursgc;
	attr &= ~VACURS;
    } else if (attr & VASEL) {
	fore_gc = win->selgc;
	back_gc = win->revselgc;
    } else if (attr & VAMLFOC) {
	fore_gc = back_gc = win->modeline_focus_gc;
    } else if (attr & VAML) {
	fore_gc = back_gc = win->modeline_gc;
    } else if (attr & (VACOLOR)) {
	int fg = ctrans[VCOLORNUM(attr)];
	int bg = (gbcolor == ENUM_FCOLOR) ? fg : ctrans[gbcolor];

	if (attr & (VAREV)) {
	    fore_gc = get_color_gc(dpy, win, fg, False);
	    back_gc = get_color_gc(dpy, win, bg, True);
	    attr &= ~(VAREV);
	} else {
	    fore_gc = get_color_gc(dpy, win, fg, True);
	    back_gc = get_color_gc(dpy, win, bg, False);
	}
    } else {
	fore_gc = win->textgc;
	back_gc = win->reversegc;
    }

    if (attr & (VAREV | VACURS)) {
	GC tmp_gc = fore_gc;
	fore_gc = back_gc;
	back_gc = tmp_gc;
    }

    if (attr & (VABOLD | VAITAL)) {
	XFontStruct *fsp = NULL;
	if ((attr & (VABOLD | VAITAL)) == (VABOLD | VAITAL)) {
	    if (!(win->fsrch_flags & FSRCH_BOLDITAL)) {
		if ((fsp = alternate_font(dpy, win, "bold", "i")) != NULL
		    || (fsp = alternate_font(dpy, win, "bold", "o")) != NULL)
		    win->pfont_boldital = fsp;
		win->fsrch_flags |= FSRCH_BOLDITAL;
	    }
	    if (win->pfont_boldital != NULL) {
		XSetFont(dpy, fore_gc, win->pfont_boldital->fid);
		fontchanged = TRUE;
		attr &= ~(VABOLD | VAITAL);	/* don't use fallback */
	    } else
		goto tryital;
	} else if (attr & VAITAL) {
	  tryital:
	    if (!(win->fsrch_flags & FSRCH_ITAL)) {
		if ((fsp = alternate_font(dpy, win, (char *) 0, "i")) != NULL
		    || (fsp = alternate_font(dpy, win, (char *) 0, "o"))
		    != NULL)
		    win->pfont_ital = fsp;
		win->fsrch_flags |= FSRCH_ITAL;
	    }
	    if (win->pfont_ital != NULL) {
		XSetFont(dpy, fore_gc, win->pfont_ital->fid);
		fontchanged = TRUE;
		attr &= ~VAITAL;	/* don't use fallback */
	    } else if (attr & VABOLD)
		goto trybold;
	} else if (attr & VABOLD) {
	  trybold:
	    if (!(win->fsrch_flags & FSRCH_BOLD)) {
		win->pfont_bold = alternate_font(dpy, win, "bold", NULL);
		win->fsrch_flags |= FSRCH_BOLD;
	    }
	    if (win->pfont_bold != NULL) {
		XSetFont(dpy, fore_gc, win->pfont_bold->fid);
		fontchanged = TRUE;
		attr &= ~VABOLD;	/* don't use fallback */
	    }
	}
    }

    /* break line into TextStrings and FillRects */
    p = text;
    cc = 0;
    tlen = 0;
    startcol = sc;
    for (i = 0; i < len; i++) {
	if (text[i] == ' ') {
	    cc++;
	    tlen++;
	} else {
	    if (cc >= CLEAR_THRESH) {
		tlen -= cc;
		really_draw(dpy, win, fore_gc, p, tlen, attr, sr, sc);
		p += tlen + cc;
		sc += tlen;
		XFillRectangle(dpy, win->win, back_gc,
			       x_pos(win, sc), back_yy,
			       (UINT) (cc * win->char_width),
			       (UINT) (win->char_height));
		sc += cc;
		tlen = 1;	/* starting new run */
	    } else
		tlen++;
	    cc = 0;
	}
    }
    if (cc >= CLEAR_THRESH) {
	tlen -= cc;
	really_draw(dpy, win, fore_gc, p, tlen, attr, sr, sc);
	sc += tlen;
	XFillRectangle(dpy, win->win, back_gc,
		       x_pos(win, sc), back_yy,
		       (UINT) (cc * win->char_width),
		       (UINT) (win->char_height));
    } else if (tlen > 0) {
	really_draw(dpy, win, fore_gc, p, tlen, attr, sr, sc);
    }
    if (attr & (VAUL | VAITAL)) {
	fore_yy += win->char_descent - 1;
	XDrawLine(dpy, win->win, fore_gc,
		  x_pos(win, startcol), fore_yy,
		  x_pos(win, startcol + len) - 1, fore_yy);
    }

    if (fontchanged)
	XSetFont(dpy, fore_gc, win->pfont->fid);
}
