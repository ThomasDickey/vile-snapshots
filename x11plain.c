/*
 * X11 text-output, based on
 *	X11 support, Dave Lemke, 11/91
 *	X Toolkit support, Kevin Buettner, 2/94
 *
 * $Id: x11plain.c,v 1.15 2021/11/23 21:19:09 tom Exp $
 */

#include <x11vile.h>

static XVileFont *
alternate_font(Display *dpy, TextWindow win, const char *weight, const char *slant)
{
    char *newname, *np, *op;
    int cnt;
    XVileFont *fsp = NULL;

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

#define DRAW_WITH(func,buffer,offset) \
	    func(dpy, win->win, fore_gc, \
		 (int) x_pos(win, sc) + offset, fore_yy, \
		 buffer, tlen)

static void
really_draw(Display *dpy,
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

static char *
x_get_font_atom_property(Display *dpy, XVileFont * pf, Atom atom)
{
    XFontProp *pp;
    int i;
    char *retval = NULL;

    for (i = 0, pp = pf->properties; i < pf->n_properties; i++, pp++)
	if (pp->name == atom) {
	    retval = XGetAtomName(dpy, pp->card32);
	    break;
	}
    return retval;
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
xvileDraw(Display *dpy,
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
	fore_gc = win->tt_info.gc;
	back_gc = win->rt_info.gc;
    } else if ((attr & VACURS) && win->is_color_cursor) {
	fore_gc = win->cc_info.gc;
	back_gc = win->rc_info.gc;
	attr &= ~VACURS;
    } else if (attr & VASEL) {
	fore_gc = win->ss_info.gc;
	back_gc = win->rs_info.gc;
    } else if (attr & VAMLFOC) {
	fore_gc = back_gc = win->mm_info.gc;
    } else if (attr & VAML) {
	fore_gc = back_gc = win->oo_info.gc;
    } else if (attr & (VACOLOR)) {
	int fg = ctrans[VCOLORNUM(attr)];
	int bg = (gbcolor == ENUM_FCOLOR) ? fg : ctrans[gbcolor];

	if (attr & (VAREV)) {
	    fore_gc = x_get_color_gc(win, fg, False)->gc;
	    back_gc = x_get_color_gc(win, bg, True)->gc;
	    attr &= ~(VAREV);
	} else {
	    fore_gc = x_get_color_gc(win, fg, True)->gc;
	    back_gc = x_get_color_gc(win, bg, False)->gc;
	}
    } else {
	fore_gc = win->tt_info.gc;
	back_gc = win->rt_info.gc;
    }

    if (attr & (VAREV | VACURS)) {
	GC tmp_gc = fore_gc;
	fore_gc = back_gc;
	back_gc = tmp_gc;
    }

    if (attr & (VABOLD | VAITAL)) {
	XVileFont *fsp = NULL;
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

XVileFont *
xvileQueryFont(Display *dpy, TextWindow tw, const char *fname)
{
    XVileFont *pf;

    TRACE(("x11:query_font(%s)\n", fname));
    if ((pf = XLoadQueryFont(dpy, fname)) != 0) {
	char *fullname = NULL;

	if (pf->max_bounds.width != pf->min_bounds.width) {
	    (void) fprintf(stderr,
			   "proportional font, things will be miserable\n");
	}

	/*
	 * Free resources associated with any presently loaded fonts.
	 */
	if (tw->pfont)
	    XFreeFont(dpy, tw->pfont);
	if (tw->pfont_bold) {
	    XFreeFont(dpy, tw->pfont_bold);
	    tw->pfont_bold = NULL;
	}
	if (tw->pfont_ital) {
	    XFreeFont(dpy, tw->pfont_ital);
	    tw->pfont_ital = NULL;
	}
	if (tw->pfont_boldital) {
	    XFreeFont(dpy, tw->pfont_boldital);
	    tw->pfont_boldital = NULL;
	}
	tw->fsrch_flags = 0;

	tw->pfont = pf;
	tw->char_width = pf->max_bounds.width;
	tw->char_height = pf->ascent + pf->descent;
	tw->char_ascent = pf->ascent;
	tw->char_descent = pf->descent;
	tw->left_ink = (pf->min_bounds.lbearing < 0);
	tw->right_ink = (pf->max_bounds.rbearing > tw->char_width);

	TRACE(("...success left:%d, right:%d\n", tw->left_ink, tw->right_ink));

	if ((fullname = x_get_font_atom_property(dpy, pf, GetAtom(FONT))) != NULL
	    && fullname[0] == '-') {
	    /*
	     * Good. Not much work to do; the name was available via the FONT
	     * property.
	     */
	    x_set_fontname(tw, fullname);
	    XFree(fullname);
	    TRACE(("...resulting FONT property font %s\n", tw->fontname));
	} else {
	    /*
	     * Woops, fully qualified name not available from the FONT property.
	     * Attempt to get the full name piece by piece.  Ugh!
	     */
	    char str[1024], *s;
	    if (fullname != NULL)
		XFree(fullname);

	    s = str;
	    *s++ = '-';

#define GET_ATOM_OR_STAR(atom)					\
    do {							\
	char *as;						\
	if ((as = x_get_font_atom_property(dpy, pf, (atom))) != NULL) { \
	    char *asp = as;					\
	    while ((*s++ = *asp++))				\
		;						\
	    *(s-1) = '-';					\
	    XFree(as);						\
	}							\
	else {							\
	    *s++ = '*';						\
	    *s++ = '-';						\
	}							\
    } one_time

#define GET_ATOM_OR_GIVEUP(atom)				\
    do {							\
	char *as;						\
	if ((as = x_get_font_atom_property(dpy, pf, (atom))) != NULL) { \
	    char *asp = as;					\
	    while ((*s++ = *asp++))				\
		;						\
	    *(s-1) = '-';					\
	    XFree(as);						\
	}							\
	else							\
	    goto piecemeal_done;				\
    } one_time

#define GET_LONG_OR_GIVEUP(atom)				\
    do {							\
	ULONG val;						\
	if (XGetFontProperty(pf, (atom), &val)) {		\
	    sprintf(s, "%ld", (long)val);			\
	    while (*s++ != '\0')				\
		;						\
	    *(s-1) = '-';					\
	}							\
	else							\
	    goto piecemeal_done;				\
    } one_time

	    GET_ATOM_OR_STAR(GetAtom(FOUNDRY));
	    GET_ATOM_OR_GIVEUP(XA_FAMILY_NAME);
	    GET_ATOM_OR_GIVEUP(GetAtom(WEIGHT_NAME));
	    GET_ATOM_OR_GIVEUP(GetAtom(SLANT));
	    GET_ATOM_OR_GIVEUP(GetAtom(SETWIDTH_NAME));
	    *s++ = '*';		/* ADD_STYLE_NAME */
	    *s++ = '-';
	    GET_LONG_OR_GIVEUP(GetAtom(PIXEL_SIZE));
	    GET_LONG_OR_GIVEUP(XA_POINT_SIZE);
	    GET_LONG_OR_GIVEUP(GetAtom(RESOLUTION_X));
	    GET_LONG_OR_GIVEUP(GetAtom(RESOLUTION_Y));
	    GET_ATOM_OR_GIVEUP(GetAtom(SPACING));
	    GET_LONG_OR_GIVEUP(GetAtom(AVERAGE_WIDTH));
	    GET_ATOM_OR_STAR(GetAtom(CHARSET_REGISTRY));
	    GET_ATOM_OR_STAR(GetAtom(CHARSET_ENCODING));
	    *(s - 1) = '\0';

#undef GET_ATOM_OR_STAR
#undef GET_ATOM_OR_GIVEUP
#undef GET_LONG_OR_GIVEUP

	    fname = str;
	  piecemeal_done:
	    /*
	     * We will either use the name which was built up piecemeal or
	     * the name which was originally passed to us to assign to
	     * the fontname field.  We prefer the fully qualified name
	     * so that we can later search for bold and italic fonts.
	     */
	    x_set_fontname(tw, fname);
	    TRACE(("...resulting piecemeal font %s\n", tw->fontname));
	}
#if OPT_MULTIBYTE
	/*
	 * max_byte1 is the maximum for the high-byte of 16-bit chars.
	 * If it is nonzero, this is not an 8-bit font.
	 */
	x_set_font_encoding(pf->max_byte1 ? enc_UTF8 : enc_8BIT);
#endif
    }
    return pf;
}
