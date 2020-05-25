/*
 * Xft text-output, Thomas Dickey 2020
 *
 * $Id: xftplain.c,v 1.12 2020/05/25 09:46:56 tom Exp $
 *
 * Some of this was adapted from xterm, of course.
 */

#include <x11vile.h>

#define GetFcBool(pattern, what) \
    (FcPatternGetBool(pattern, what, 0, &fc_bool) == FcResultMatch)

#define GetFcInt(pattern, what) \
    (FcPatternGetInteger(pattern, what, 0, &fc_int) == FcResultMatch)

#define GetFcTxt(pattern, what) \
    (FcPatternGetString(pattern, what, 0, &fc_txt) == FcResultMatch)

#ifdef FC_COLOR
#define NormXftPattern \
	    XFT_FAMILY,     XftTypeString, "mono", \
	    FC_COLOR,       XftTypeBool,   FcFalse, \
	    FC_OUTLINE,     XftTypeBool,   FcTrue, \
	    XFT_SIZE,       XftTypeDouble, face_size
#else
#define NormXftPattern \
	    XFT_FAMILY,     XftTypeString, "mono", \
	    XFT_SIZE,       XftTypeDouble, face_size
#endif

#define BoldXftPattern(font) \
	    XFT_WEIGHT,     XftTypeInteger, XFT_WEIGHT_BOLD, \
	    XFT_CHAR_WIDTH, XftTypeInteger, (font)->max_advance_width

#define ItalXftPattern(font) \
	    XFT_SLANT,      XftTypeInteger, XFT_SLANT_ITALIC, \
	    XFT_CHAR_WIDTH, XftTypeInteger, (font)->max_advance_width

#define BtalXftPattern(font) \
	    XFT_WEIGHT,     XftTypeInteger, XFT_WEIGHT_BOLD, \
	    XFT_SLANT,      XftTypeInteger, XFT_SLANT_ITALIC, \
	    XFT_CHAR_WIDTH, XftTypeInteger, (font)->max_advance_width

static XVileFont *
alternate_font(Display *dpy, TextWindow win, const char *weight, const char *slant)
{
#ifdef XRENDERFONT
    (void) dpy;
    (void) win;
    (void) weight;
    (void) slant;
    fprintf(stderr, "%s:%d: not implemented\n", __FILE__, __LINE__);
    return NULL;
#else
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

#if XRENDERFONT
    (void) dpy;
    fprintf(stderr, "%s:%d: not implemented\n", __FILE__, __LINE__);
#else
    if ((fsp = XLoadQueryFont(dpy, newname)) != NULL) {
	win->left_ink = win->left_ink || (fsp->min_bounds.lbearing < 0);
	win->right_ink = win->right_ink
	    || (fsp->max_bounds.rbearing > win->char_width);
	TRACE(("...found left:%d, right:%d\n",
	       win->left_ink,
	       win->right_ink));
    }
#endif

  done:
    free(newname);
    return fsp;
#endif
}

static GC
get_color_gc(Display *dpy, TextWindow win, int n, Bool normal)
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
#if XRENDERFONT
	fprintf(stderr, "%s:%d: not implemented\n", __FILE__, __LINE__);
#else
	gcvals.font = win->pfont->fid;
#endif
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
#define MYSIZE 1024
    XftColor color;
    int y = text_y_pos(win, sr);
    int x = x_pos(win, sr);
    XftCharSpec buffer[MYSIZE];
    static XftDraw *renderDraw;

    fprintf(stderr, "really_draw [%d,%d] %s\n", sr, sc, visible_video_text(text, tlen));
    memset(&color, 0xff, sizeof(color)); /* FIXME - computed and cached in xterm */
    if (renderDraw == 0) {
	int scr = DefaultScreen(dpy);
	Visual *visual = DefaultVisual(dpy, scr);
	renderDraw = XftDrawCreate(dpy,
				   win->win, visual,
				   DefaultColormap(dpy, scr));
    }
    while (tlen > 0) {
	Cardinal n;

	for (n = 0; ((int) n < tlen) && (n < MYSIZE); ++n) {
	    buffer[n].ucs4 = text[n];
	    buffer[n].x = (short) (x + win->char_width * n);
	    buffer[n].y = (short) (y);
	}

	XftDrawCharSpec(renderDraw,
			&color,
			win->pfont,
			buffer,
			(int) n);

	tlen -= MYSIZE;
	if (tlen > 0) {
	    text += MYSIZE;
	    sc += MYSIZE;
	}
    }
#undef MYSIZE
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
	XVileFont *fsp = NULL;
	if ((attr & (VABOLD | VAITAL)) == (VABOLD | VAITAL)) {
	    if (!(win->fsrch_flags & FSRCH_BOLDITAL)) {
		if ((fsp = alternate_font(dpy, win, "bold", "i")) != NULL
		    || (fsp = alternate_font(dpy, win, "bold", "o")) != NULL)
		    win->pfont_boldital = fsp;
		win->fsrch_flags |= FSRCH_BOLDITAL;
	    }
	    if (win->pfont_boldital != NULL) {
#if XRENDERFONT
		fprintf(stderr, "%s:%d: not implemented\n", __FILE__, __LINE__);
#else
		XSetFont(dpy, fore_gc, win->pfont_boldital->fid);
#endif
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
#if XRENDERFONT
		fprintf(stderr, "%s:%d: not implemented\n", __FILE__, __LINE__);
#else
		XSetFont(dpy, fore_gc, win->pfont_ital->fid);
#endif
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
#if XRENDERFONT
		fprintf(stderr, "%s:%d: not implemented\n", __FILE__, __LINE__);
#else
		XSetFont(dpy, fore_gc, win->pfont_bold->fid);
#endif
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
    if (fontchanged) {
#if XRENDERFONT
	fprintf(stderr, "%s:%d: not implemented\n", __FILE__, __LINE__);
#else
	XSetFont(dpy, fore_gc, win->pfont->fid);
#endif
    }
}

/*
 * Given the Xft font metrics, determine the actual font size.  This is used
 * for each font to ensure that normal, bold and italic fonts follow the same
 * rule.
 */
static void
setRenderFontsize(TextWindow tw, XftFont *font, const char *tag)
{
    if (font != 0) {
	int width, height, ascent, descent;
#ifdef OPT_TRACE
	FT_Face face;
	FT_Size size;
	FT_Size_Metrics metrics;
	Boolean scalable;
	Boolean is_fixed;
	Boolean debug_xft = False;

	face = XftLockFace(font);
	size = face->size;
	metrics = size->metrics;
	is_fixed = FT_IS_FIXED_WIDTH(face);
	scalable = FT_IS_SCALABLE(face);
	XftUnlockFace(font);

	/* freetype's inconsistent for this sign */
	metrics.descender = -metrics.descender;

#define TR_XFT	   "Xft metrics: "
#define D_64(name) ((double)(metrics.name)/64.0)
#define M_64(a,b)  ((font->a * 64) != metrics.b)
#define BOTH(a,b)  D_64(b), M_64(a,b) ? "*" : ""

	debug_xft = (M_64(ascent, ascender)
		     || M_64(descent, descender)
		     || M_64(height, height)
		     || M_64(max_advance_width, max_advance));

	TRACE(("Xft font is %sscalable, %sfixed-width\n",
	       is_fixed ? "" : "not ",
	       scalable ? "" : "not "));

	if (debug_xft) {
	    TRACE(("Xft font size %d+%d vs %d by %d\n",
		   font->ascent,
		   font->descent,
		   font->height,
		   font->max_advance_width));
	    TRACE((TR_XFT "ascender    %6.2f%s\n", BOTH(ascent, ascender)));
	    TRACE((TR_XFT "descender   %6.2f%s\n", BOTH(descent, descender)));
	    TRACE((TR_XFT "height      %6.2f%s\n", BOTH(height, height)));
	    TRACE((TR_XFT "max_advance %6.2f%s\n", BOTH(max_advance_width, max_advance)));
	} else {
	    TRACE((TR_XFT "matches font\n"));
	}
#endif

	width = font->max_advance_width;
	height = font->height;
	ascent = font->ascent;
	descent = font->descent;

	/*
	 * An empty tag is used for the base/normal font.
	 */
	if (isEmpty(tag)) {
	    tw->char_width = width;
	    tw->char_height = height;
	    tw->char_ascent = ascent;
	    tw->char_descent = descent;
#ifdef TODO_XRENDERFONT
	    tw->left_ink = (pf->min_bounds.lbearing < 0);
	    tw->right_ink = (pf->max_bounds.rbearing > tw->char_width);
	    TRACE(("...success left:%d, right:%d\n", tw->left_ink, tw->right_ink));
#else
	    tw->left_ink = 0;
	    tw->right_ink = 0;
#endif
	}
    }
}

/*
 * from xterm, in turn adapted from xfd/grid.c
 */
static FcChar32
xft_last_char(XftFont *xft)
{
    FcChar32 this, last, next;
    FcChar32 map[FC_CHARSET_MAP_SIZE];
    int i;
    last = FcCharSetFirstPage(xft->charset, map, &next);
    while ((this = FcCharSetNextPage(xft->charset, map, &next)) != FC_CHARSET_DONE)
	last = this;
    last &= (FcChar32) ~ 0xff;
    for (i = FC_CHARSET_MAP_SIZE - 1; i >= 0; i--) {
	if (map[i]) {
	    FcChar32 bits = map[i];
	    last += (FcChar32) i *32 + 31;
	    while (!(bits & 0x80000000)) {
		last--;
		bits <<= 1;
	    }
	    break;
	}
    }
    return (FcChar32) last;
}

XVileFont *
xvileQueryFont(Display *dpy, TextWindow tw, const char *fname)
{
    XVileFont *pf;
    char fullname[1024];

    TRACE(("x11:query_font(%s)\n", fname));
    pf = NULL;
    if (TRUE) {
	XftPattern *pat;

	if ((pat = XftNameParse(fname)) != 0) {
	    XftPattern *match;
	    XftResult status;

#ifdef TODO_XRENDERFONT
	    /*
	     * xterm has this chunk, i.e., varargs list for each font,
	     * but XftPatternBuild==FcPatternBuild, and there's an alternative
	     * FcPatternVaBuild which would let me refactor this opening code.
	     */
	    norm.pattern = XftPatternDuplicate(pat);
	    XftPatternBuild(norm.pattern,
			    NormXftPattern,
			    (void *) 0);
#endif
	    FcConfigSubstitute(NULL, pat, FcMatchPattern);
	    XftDefaultSubstitute(dpy, DefaultScreen(dpy), pat);

	    match = FcFontMatch(NULL, pat, &status);
	    if (match != NULL) {
		int fc_int = -1;
		pf = XftFontOpenPattern(dpy, match);

#ifdef TODO_XRENDERFONT
		if (!GetFcInt(pat, FC_SPACING) || fc_int != FC_MONO) {
		    fprintf(stderr, "fc_int:%d\n", fc_int);
		    (void) fprintf(stderr,
				   "proportional font, things will be miserable\n");
		}
#else
		(void) fc_int;
#endif
		/*
		 * Xft is a layer over fontconfig.  The latter's documentation
		 * is composed of circular definitions which never get around
		 * to defining the format returned by FcNameUnparse.  The value
		 * returned via Xft has (also undocumented) the fontname that
		 * we want first in the buffer.
		 */
		if (XftNameUnparse(match, fullname, (int) sizeof(fullname))) {
		    char *colon = strchr(fullname, ':');
		    if (colon != NULL)
			*colon = '\0';
		    fname = fullname;
		}
	    }

#ifdef TODO_XRENDERFONT
	    /* xterm retains the pattern to (try to) make the bold/italic
	     * versions follow the same pattern as normal - should xvile? */
#endif
	    XftPatternDestroy(pat);
	}
    }
    if (pf != NULL) {
	/*
	 * Free resources associated with any presently loaded fonts.
	 */
	if (tw->pfont)
	    XftFontClose(dpy, tw->pfont);
	if (tw->pfont_bold) {
	    XftFontClose(dpy, tw->pfont_bold);
	    tw->pfont_bold = NULL;
	}
	if (tw->pfont_ital) {
	    XftFontClose(dpy, tw->pfont_ital);
	    tw->pfont_ital = NULL;
	}
	if (tw->pfont_boldital) {
	    XftFontClose(dpy, tw->pfont_boldital);
	    tw->pfont_boldital = NULL;
	}
	tw->fsrch_flags = 0;

	tw->pfont = pf;

	setRenderFontsize(tw, pf, NULL);

	x_set_fontname(tw, fullname);
#if OPT_MULTIBYTE
	x_set_font_encoding(xft_last_char(pf) > 255 ? enc_UTF8 : enc_8BIT);
#endif
    }
    return pf;
}
