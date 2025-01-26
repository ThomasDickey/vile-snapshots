/*
 * Look up vi-style tags in the file "tags".
 *	Invoked either by ":ta routine-name" or by "^]" while sitting
 *	on a string.  In the latter case, the tag is the word under
 *	the cursor.
 *	written for vile.
 *
 * Copyright (c) 1990, 1995-2019,2025 by Paul Fox and Thomas E. Dickey
 *
 * $Id: tags.c,v 1.156 2025/01/26 14:48:55 tom Exp $
 */
#include "estruct.h"
#include "edef.h"

#if OPT_TAGS

#if OPT_TAGS_CMPL
typedef struct {
    /* FIXME: we could make next-tag work faster if we also hash the
     * line-pointers for each key.
     */
    char *bi_key;
} TAGS_DATA;

#define BI_DATA TAGS_DATA
#include "btree.h"

#endif

#define UNTAG struct untag
UNTAG {
    char *u_fname;
    L_NUM u_lineno;
    C_NUM u_colno;
    UNTAG *u_stklink;
#if OPT_SHOW_TAGS
    char *u_templ;
#endif
};

#define TAGHITS struct taghits
TAGHITS {
    TAGHITS *link;
    LINE *tag;			/* points to tag-buffer line */
    LINE *hit;			/* points to corresponding line in source-file */
};

static TAGHITS *tag_hits = NULL;
static UNTAG *untaghead = NULL;
static char tagname[NFILEN + 2];	/* +2 since we may add a tab later */

#if OPT_SHOW_TAGS
#  if OPT_UPBUFF
static int update_tagstack(BUFFER *bp);
#  endif
#endif /* OPT_SHOW_TAGS */

/*
 * return (in buf) the Nth whitespace
 *	separated word in "path", counting from 0
 */
static void
nth_name(char *buf, const char *path, int n)
{
    while (n-- > 0) {
	path = skip_cblanks(path);
	path = skip_ctext(path);
    }
    path = skip_cblanks(path);
    while (*path && !isSpace(*path))
	*buf++ = *path++;
    *buf = EOS;
}

static BUFFER *
gettagsfile(int n, int *endofpathflagp, int *did_read)
{
#ifdef MDCHK_MODTIME
    time_t current;
#endif
    char *tagsfile;
    BUFFER *tagbp;
    char tagbufname[NBUFN];
    char tagfilename[NFILEN];

    *endofpathflagp = FALSE;
    *did_read = FALSE;

    (void) lsprintf(tagbufname, TAGFILE_BufName, n + 1);

    /* is the buffer around? */
    if ((tagbp = find_b_name(tagbufname)) == NULL) {
	char *tagf = global_b_val_ptr(VAL_TAGS);

	nth_name(tagfilename, tagf, n);
	if (!doglob(tagfilename)
	    || tagfilename[0] == EOS) {
	    *endofpathflagp = TRUE;
	    return NULL;
	}

	/* look up the tags file */
	tagsfile = cfg_locate(tagfilename, LOCATE_TAGS);

	/* if it isn't around, don't sweat it */
	if (tagsfile == NULL) {
	    return NULL;
	}

	/* find the pointer to that buffer */
	if ((tagbp = bfind(tagbufname, BFINVS)) == NULL) {
	    mlforce("[Can't create tags buffer]");
	    return NULL;
	}

	if (readin(tagsfile, FALSE, tagbp, FALSE) != TRUE) {
	    zotbuf(tagbp);
	    return NULL;
	}
	set_tagsmode(tagbp);
	*did_read = TRUE;
    }
#ifdef MDCHK_MODTIME
    /*
     * Re-read the tags buffer if we are checking modification-times and find
     * that the tags file's been changed.
     */
    if (b_val(tagbp, MDCHK_MODTIME)
	&& get_modtime(tagbp, &current)
	&& tagbp->b_modtime != current) {
	if (!*did_read) {
	    if (readin(tagbp->b_fname, FALSE, tagbp, FALSE) != TRUE) {
		zotbuf(tagbp);
		return NULL;
	    }
	    set_tagsmode(tagbp);
	    *did_read = TRUE;
	}
	set_modtime(tagbp, tagbp->b_fname);
    }
#endif
    b_set_invisible(tagbp);
    return tagbp;
}

#if OPT_TAGS_CMPL

static void
old_tags(BI_NODE * a)
{
    beginDisplay();
    FreeIfNeeded(BI_KEY(a));
    FreeIfNeeded(TYPECAST(char, a));
    endofDisplay();
}

static BI_NODE *
new_tags(BI_DATA * a)
{
    BI_NODE *p;

    beginDisplay();
    if ((p = typecalloc(BI_NODE)) != NULL) {
	p->value = *a;
	if ((BI_KEY(p) = strmalloc(a->bi_key)) == NULL) {
	    old_tags(p);
	    p = NULL;
	}
    }
    endofDisplay();

    return p;
}

/*ARGSUSED*/
static void
dpy_tags(BI_NODE * a GCC_UNUSED, int level GCC_UNUSED)
{
#if OPT_TRACE
    while (level-- > 0)
	TRACE((". "));
    TRACE(("%s (%d)\n", BI_KEY(a), a->balance));
#endif
}

static void
xcg_tags(BI_NODE * a, BI_NODE * b)
{
    BI_DATA temp = a->value;
    a->value = b->value;
    b->value = temp;
}

#define BI_DATA0 {{NULL}, 0, {NULL}}
#define BI_TREE0 0, 0, BI_DATA0
static BI_TREE tags_tree =
{new_tags, old_tags, dpy_tags, xcg_tags, BI_TREE0};

/* Parse the identifier out of the given line and store it in the binary tree */
static void
store_tag(LINE *lp)
{
    char my_name[sizeof(tagname)];
    size_t len, got;
    int c;

    if (llength(lp) > 0) {
	len = (size_t) llength(lp);
	for (got = 0; got < len; got++) {
	    if (got >= sizeof(tagname) - 2) {
		return;		/* ignore super-long identifiers */
	    }
	    c = lgetc(lp, got);
	    if (!isGraph(c))
		break;
	    my_name[got] = (char) c;
	}
	my_name[got] = EOS;
	if (got) {
	    BI_DATA temp;
#ifdef MDTAGIGNORECASE
	    if (b_val(curbp, MDTAGIGNORECASE))
		mklower(my_name);
#endif
	    temp.bi_key = my_name;
	    btree_insert(&tags_tree, &temp);
	}
    }
}

/* check if the binary-tree is up-to-date.  If not, rebuild it. */
static const char **
init_tags_cmpl(char *buf, size_t cpos)
{
    int tf_num;
    BUFFER *bp;
    LINE *lp;
    int done;
    int flag;
    int obsolete = (tags_tree.count == 0);

#ifdef MDTAGIGNORECASE
    /* If curbp's b_val(curbp,MDTAGIGNORECASE)) is different from the last
     * time we built the tree, obsolete the tree, since the keys changed.
     */
    {
	static int my_tcase = SORTOFTRUE;
	if (b_val(curbp, MDTAGIGNORECASE) != my_tcase) {
	    my_tcase = b_val(curbp, MDTAGIGNORECASE);
	    obsolete = TRUE;
	}
    }
#endif
    /*
     * Check if we've already loaded all of the tags buffers.  If not, we
     * know we should build the tree.  Also, if any aren't empty, we may
     * have loaded the buffer for some other reason than tags processing.
     */
    if (!obsolete) {
	for (tf_num = 0;; tf_num++) {
	    bp = gettagsfile(tf_num, &done, &flag);
	    if (!done && bp == NULL)
		continue;	/* More tag files to examine */
	    if (done || bp == NULL)
		break;
	    (void) bsizes(bp);
	    obsolete = flag || (bp->b_linecount != 0);
	    if (obsolete)
		break;
	}
    }

    if (obsolete) {
	btree_freeup(&tags_tree);

	for (tf_num = 0;; tf_num++) {
	    bp = gettagsfile(tf_num, &done, &flag);
	    if (!done && bp == NULL)
		continue;	/* More tag files to examine */
	    if (done || bp == NULL)
		break;
	    for_each_line(lp, bp)
		store_tag(lp);
	}

	TRACE(("stored %d tags entries\n", tags_tree.count));
    }

    return btree_parray(&tags_tree, buf, cpos);
}

int
tags_completion(DONE_ARGS)
{
    size_t cpos = *pos;
    int status = FALSE;
    const char **nptr;

    kbd_init();			/* nothing to erase */
    buf[cpos] = EOS;		/* terminate it for us */

    beginDisplay();
    if ((nptr = init_tags_cmpl(buf, cpos)) != NULL) {
	status = kbd_complete(PASS_DONE_ARGS, (const char *) nptr, sizeof(*nptr));
	free(TYPECAST(char *, nptr));
    }
    endofDisplay();
    return status;
}
#else
#define tags_completion no_completion
#endif

/*
 * Record the places we've been to during a tag-search, so we'll not push the
 * stack just because there's repetition in the tags files.  Return true if
 * we've been here before.
 */
static int
mark_tag_hit(LINE *tag, LINE *hit)
{
    TAGHITS *p;

    TRACE(("mark_tag_hit %s:%d\n", curbp->b_bname, line_no(curbp, hit)));
    for (p = tag_hits; p != NULL; p = p->link) {
	if (p->hit == hit) {
	    TRACE(("... mark_tag_hit TRUE\n"));
	    return (p->tag == tag) ? ABORT : TRUE;
	}
    }

    beginDisplay();
    if ((p = typecalloc(TAGHITS)) != NULL) {
	p->link = tag_hits;
	p->tag = tag;
	p->hit = hit;
	tag_hits = p;
    }
    endofDisplay();

    TRACE(("... mark_tag_hit FALSE\n"));
    return FALSE;
}

/*
 * Discard the list of tag-hits when we're about to begin a new tag-search.
 */
static void
free_tag_hits(void)
{
    TAGHITS *p;

    beginDisplay();
    while ((p = tag_hits) != NULL) {
	tag_hits = p->link;
	free(p);
    }
    endofDisplay();
}

static void
free_untag(UNTAG * utp)
{
    if (utp != NULL) {
	beginDisplay();
	FreeIfNeeded(utp->u_fname);
#if OPT_SHOW_TAGS
	FreeIfNeeded(utp->u_templ);
#endif
	free(utp);
	endofDisplay();
    }
}

/* discard without returning anything */
static void
tossuntag(void)
{
    UNTAG *utp;

    if (untaghead != NULL) {
	utp = untaghead;
	untaghead = utp->u_stklink;
	free_untag(utp);
	update_scratch(TAGSTACK_BufName, update_tagstack);
    }
}

/*ARGSUSED*/
static void
pushuntag(char *fname, L_NUM lineno, C_NUM colno, char *tag)
{
    UNTAG *utp;

    (void) tag;

    beginDisplay();
    if ((utp = typecalloc(UNTAG)) != NULL) {

	if ((utp->u_fname = strmalloc(fname)) == NULL
#if OPT_SHOW_TAGS
	    || (utp->u_templ = strmalloc(tag)) == NULL
#endif
	    ) {
	    free_untag(utp);
	    return;
	}
#if OPT_VMS_PATH
	strip_version(utp->u_fname);
#endif

	utp->u_lineno = lineno;
	utp->u_colno = colno;
	utp->u_stklink = untaghead;
	untaghead = utp;
	update_scratch(TAGSTACK_BufName, update_tagstack);
    }
    endofDisplay();
}

static int
popuntag(char *fname, L_NUM * linenop, C_NUM * colnop)
{
    UNTAG *utp;

    if (untaghead) {
	utp = untaghead;
	untaghead = utp->u_stklink;
	(void) vl_strncpy(fname, utp->u_fname, NFILEN);
	*linenop = utp->u_lineno;
	*colnop = utp->u_colno;
	free_untag(utp);
	update_scratch(TAGSTACK_BufName, update_tagstack);
	return TRUE;
    }
    fname[0] = EOS;
    *linenop = 0;
    return FALSE;
}

/*
 * Returns TRUE if:
 *
 * a) pin-tagstack mode is enabled, and
 * b) 2 or more visible windows on screen, and
 * c) the current tag pop/push operation can be effected (pinned) in curwin.
 */
static int
pinned_tagstack(char *fname /* target of tag/push op */ )
{
    int pinned = FALSE;

    if (global_g_val(GMDPIN_TAGSTACK)) {
	BUFFER *bp;
	int nobufchg,		/* TRUE if a call to swbuffer_lfl() will not
				 * change/re-read current buffer.
				 */
	  wdwcnt;
	WINDOW *wp;

	/* Don't pin tagstack if less than two visible windows on screen. */
	wdwcnt = 0;
	for_each_visible_window(wp)
	    wdwcnt++;
	if (wdwcnt > 1) {
	    /*
	     * Try to display the results of this tag pop in the current
	     * window....
	     *
	     * Got an existing buffer for this filename?
	     */

	    if ((bp = find_b_file(fname)) != NULL) {
		/* yes, set the current window to this buffer. */

		nobufchg = (curbp == bp &&
			    DOT.l &&
			    curwp &&
			    curwp->w_bufp == bp &&
			    bp->b_active);

		if (swbuffer_lfl(bp, FALSE, TRUE) != FALSE) {
		    if (nobufchg) {
			/*
			 * in this case, DOT is changing to a new location in
			 * the same buffer.  if DOT isn't currently within the
			 * bounds of its window, updpos() will barf when
			 * update() is eventually invoked.  forestall this
			 * issue by forcing a window reframe.
			 */

			curwp->w_flag |= WFFORCE;
		    }
		    pinned = TRUE;
		}
	    }
	}
    }
    return (pinned);
}

/* Jump back to the given file, line#, and column#. */
static int
finish_pop(char *fname, L_NUM lineno, C_NUM colno)
{
    MARK odot;
    int s;

    if ((s = pinned_tagstack(fname)) == FALSE) {
	/* get a window open on the file */

	s = getfile(fname, FALSE);
    }
    if (s == TRUE) {
	/* it's an absolute move -- remember where we are */
	odot = DOT;
	s = vl_gotoline(lineno);
	/* if we moved, update the "last dot" mark */
	if (s == TRUE) {
	    gocol(colno);
	    if (!sameline(DOT, odot))
		curwp->w_flag = (USHORT) (curwp->w_flag & ~WFMOVE);
	    else
		curwp->w_lastdot = odot;
	}
    }
    return s;
}

#ifdef MDTAGIGNORECASE
typedef int (*CompareFunc) (const char *a, const char *b, size_t len);

static int
my_strncasecmp(const char *a, const char *b, size_t len)
{
    int aa = EOS, bb = EOS;

    while ((len != 0)
	   && ((aa = (isUpper(*a) ? toLower(*a) : *a)) != EOS)
	   && ((bb = (isUpper(*b) ? toLower(*b) : *b)) != EOS)
	   && (aa == bb)) {
	len--;
	a++;
	b++;
    }

    return aa - bb;
}
#endif

/*
 * Do exact/inexact lookup of an anchored string in a buffer.
 *	if taglen is 0, matches must be exact (i.e.  all
 *	characters significant).  if the user enters less than 'taglen'
 *	characters, this match must also be exact.  if the user enters
 *	'taglen' or more characters, only that many characters will be
 *	significant in the lookup.
 */
static LINE *
cheap_tag_scan(LINE *oldlp, char *name, size_t taglen)
{
    LINE *lp, *retlp;
    size_t namelen = strlen(name);
    int exact = (taglen == 0);
    int added_tab;
#ifdef MDTAGIGNORECASE
    CompareFunc compare = (b_val(curbp, MDTAGIGNORECASE)
			   ? my_strncasecmp
			   : strncmp);
#else
#define compare strncmp
#endif

    /* force a match of the tab delimiter if we're supposed to do
       exact matches or if we're searching for something shorter
       than the "restricted" length */
    if (exact || namelen < taglen) {
	name[namelen++] = '\t';
	name[namelen] = EOS;
	added_tab = TRUE;
    } else {
	added_tab = FALSE;
    }

    retlp = NULL;
    lp = lforw(oldlp);
    while (lp != oldlp) {
	if (llength(lp) > (int) namelen) {
	    if (!compare(lvalue(lp), name, namelen)) {
		retlp = lp;
		break;
	    }
	}
	lp = lforw(lp);
    }
    if (added_tab)
	name[namelen - 1] = EOS;
    return retlp;
}

static LINE *
cheap_buffer_scan(BUFFER *bp, char *patrn, int dir)
{
    LINE *lp;
    LINE *result = NULL;
    regexp *exp;
    int ic = FALSE;

    if ((exp = regcomp(patrn, strlen(patrn), FALSE)) != NULL) {
#ifdef MDTAGIGNORECASE
	ic = b_val(bp, MDTAGIGNORECASE);
#endif

	TRACE(("cheap_buffer_scan '%s' %s\n",
	       patrn,
	       dir == FORWARD ? "fwd" : "bak"));

	for (lp = dir == FORWARD ? lforw(buf_head(bp)) : lback(buf_head(bp));
	     lp != buf_head(bp);
	     lp = dir == FORWARD ? lforw(lp) : lback(lp)) {
	    if (lregexec(exp, lp, 0, llength(lp), ic)) {
		result = lp;
		break;
	    }
	}

	beginDisplay();
	free(TYPECAST(char, exp));
	endofDisplay();
    }
    return (result);
}

static int
tag_search(char *tag, int taglen, int initial)
{
    /* variables for 'initial'=FALSE */
    static int tf_num;
    static char last_bname[NBUFN];
#ifdef MDCHK_MODTIME
    static time_t last_modtime;
#endif

    static TBUFF *srchpat;

    LINE *lp;
    size_t i;
    int status;
    char *tfp, *lplim;
    char tfname[NFILEN];
    int flag;
    L_NUM lineno;
    C_NUM colno;
    int changedfile;
    MARK odot;
    BUFFER *tagbp;
    int nomore;
    int gotafile = FALSE;
    int retried = FALSE;

    if (initial)
	tf_num = 0;
#ifdef MDCHK_MODTIME
    else {
	if ((tagbp = find_b_name(last_bname)) == NULL
	    || last_modtime != tagbp->b_modtime) {
	    initial = TRUE;
	    tf_num = 0;
	}
    }
#endif

    do {
	tagbp = gettagsfile(tf_num, &nomore, &flag);
	lp = NULL;
	if (nomore) {
	    if (gotafile) {
		if (initial || retried) {
		    mlwarn("[No such tag: \"%s\"]", tag);
		    return FALSE;
		}
		retried = TRUE;
		tf_num = 0;
		continue;
	    } else {
		mlforce("[No tags file available.]");
		return FALSE;
	    }
	}

	if (tagbp) {
	    lp = cheap_tag_scan((initial || retried
				 ? buf_head(tagbp)
				 : tagbp->b_dot.l),
				tag, (size_t) taglen);
	    gotafile = TRUE;
	}

	tf_num++;

    } while (lp == NULL);

    /* Save the position in the tags-file so "next-tag" will work */
    tf_num--;
    (void) vl_strncpy(last_bname, tagbp->b_bname, sizeof(last_bname));
    tagbp->b_dot.l = lp;
    tagbp->b_dot.o = 0;
#ifdef MDCHK_MODTIME
    last_modtime = tagbp->b_modtime;
#endif

    /* Parse the line from the tags-file */
    lplim = &lvalue(lp)[llength(lp)];
    tfp = lvalue(lp);
    while (tfp < lplim)
	if (*tfp++ == '\t')
	    break;
    if (*tfp == '\t') {		/* then it's a new-fangled NeXT tags file */
	tfp++;			/* skip the tab */
    }

    i = 0;
    if (b_val(curbp, MDTAGSRELTIV) && !is_slashc(*tfp)
#if OPT_MSDOS_PATH
	&& !is_msdos_drive(tfp)
#endif
	) {
	char *first = tagbp->b_fname;
	char *lastsl = pathleaf(tagbp->b_fname);
	while (lastsl != first)
	    tfname[i++] = *first++;
    }
    while (i < (sizeof(tfname) - 2) && tfp < lplim && *tfp != '\t') {
	tfname[i++] = *tfp++;
    }
    tfname[i] = EOS;

    if (tfp >= lplim) {
	mlforce("[Bad line in tags file.]");
	return FALSE;
    }

    if (curbp) {
	lineno = line_no(curbp, DOT.l);
	colno = getccol(FALSE);
	if (!isInternalName(curbp->b_fname))
	    pushuntag(curbp->b_fname, lineno, colno, tag);
	else
	    pushuntag(curbp->b_bname, lineno, colno, tag);
    }

    changedfile = (curbp == NULL || !same_fname(tfname, curbp, TRUE));
    if (changedfile)
	(void) doglob(tfname);
    if (!pinned_tagstack(tfname)) {
	if (changedfile) {
	    status = getfile(tfname, TRUE);
	    if (status != TRUE) {
		tossuntag();
		return status;
	    }
	}
    }

    /* it's an absolute move -- remember where we are */
    odot = DOT;

    tfp++;			/* skip the tab */
    if (tfp >= lplim) {
	mlforce("[Bad line in tags file.]");
	return FALSE;
    }
    if (isDigit(*tfp)) {	/* then it's a line number */
	lineno = 0;
	while (isDigit(*tfp) && (tfp < lplim)) {
	    lineno = 10 * lineno + *tfp - '0';
	    tfp++;
	}
	status = gotoline(TRUE, lineno);
	if (status != TRUE && !changedfile)
	    tossuntag();
    } else {
	int delim = *tfp;
	int quoted = FALSE;
	char *p;
	int dir;

	if (delim == '?') {
	    dir = REVERSE;
	} else {
	    dir = FORWARD;
	}
	p = ++tfp;		/* skip the "/" */

	/* we're on the '/', so look for the matching one */
	while (p < lplim) {
	    if (quoted) {
		quoted = FALSE;
	    } else if (*p == BACKSLASH) {
		quoted = TRUE;
	    } else if (*p == delim) {
		break;
	    }
	    p++;
	}
	if (p >= lplim) {
	    mlforce("[Bad pattern in tags file.]");
	    return FALSE;
	}

	if ((srchpat = tb_init(&srchpat, EOS)) == NULL
	    || (srchpat = tb_bappend(&srchpat, tfp, (size_t) (p - tfp))) == NULL
	    || (srchpat = tb_append(&srchpat, EOS)) == NULL)
	    return no_memory("tags");

	lp = cheap_buffer_scan(curbp, tb_values(srchpat), dir);
	if (lp == NULL) {
	    mlwarn("[Tag not present]");
	    if (!changedfile)
		tossuntag();
	    return FALSE;
	}
	DOT.l = lp;
	curwp->w_flag |= WFMOVE;
	(void) firstnonwhite(FALSE, 1);
	status = TRUE;
    }

    if (status == TRUE) {
	int s;
	if ((s = mark_tag_hit(tagbp->b_dot.l, DOT.l)) != FALSE) {
	    if (popuntag(tfname, &lineno, &colno)) {
		(void) finish_pop(tfname, lineno, colno);
	    }
	    return s;
	}

	/*
	 * If we've succeeded on a next-tags, adjust the stack so that
	 * it's all on the same level.  A tag-pop will return to the
	 * original position.
	 */
	if (!initial
	    && untaghead != NULL
	    && untaghead->u_stklink != NULL) {
	    UNTAG *p;
	    p = untaghead;
	    untaghead = p->u_stklink;
	    free_untag(p);
	}

	if (!changedfile)
	    mlwrite("Tag \"%s\" in current buffer", tag);

	/* if we moved, update the "last dot" mark */
	if (!sameline(DOT, odot)) {
	    curwp->w_lastdot = odot;
	}
    }

    return status;
}

KBD_OPTIONS
tags_kbd_options(void)
{
    KBD_OPTIONS mode = KBD_NORMAL
#if OPT_TAGS_CMPL
    | KBD_MAYBEC
#endif
     ;
#ifdef MDTAGIGNORECASE
    if (b_val(curbp, MDTAGIGNORECASE))
	mode |= KBD_LOWERC;
#endif
    return mode;
}

/* ARGSUSED */
int
gototag(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int s;
    int taglen;

    if (clexec || isnamedcmd) {
	if ((s = kbd_string("Tag name: ",
			    tagname, sizeof(tagname),
			    '\n', tags_kbd_options(), tags_completion)) != TRUE)
	    return (s);
	taglen = b_val(curbp, VAL_TAGLEN);
    } else {
	s = screen_to_ident(tagname, sizeof(tagname));
	taglen = 0;
    }

    if (s == TRUE) {
#ifdef MDTAGIGNORECASE
	if (b_val(curbp, MDTAGIGNORECASE))
	    mklower(tagname);
#endif
	free_tag_hits();
	s = tag_search(tagname, taglen, TRUE);
    } else
	tagname[0] = EOS;
    return s;
}

/*
 * Continue a tag-search by looking for other references in the tags file.
 */
/*ARGSUSED*/
int
nexttag(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int s = FALSE;

    if (tagname[0] != EOS) {
	do {
	    s = tag_search(tagname, global_b_val(VAL_TAGLEN), FALSE);
	} while (s == SORTOFTRUE);
	if (s == ABORT)
	    mlwarn("[No more matches]");
    }
    return s;
}

int
untagpop(int f, int n)
{
    L_NUM lineno = 0;
    C_NUM colno = 0;
    char fname[NFILEN];
    int s;

    n = need_a_count(f, n, 1);

    while (n && popuntag(fname, &lineno, &colno))
	n--;
    if (lineno && fname[0]) {
	s = finish_pop(fname, lineno, colno);
    } else {
	mlwarn("[No stacked un-tags]");
	s = FALSE;
    }
    return s;
}

#if OPT_SHOW_TAGS
/*ARGSUSED*/
static void
maketagslist(int value GCC_UNUSED, void *dummy GCC_UNUSED)
{
    UNTAG *utp;
    int n;
    int taglen = global_b_val(VAL_TAGLEN);
    char temp[NFILEN];

    if (taglen == 0) {
	for (utp = untaghead; utp != NULL; utp = utp->u_stklink) {
	    n = (int) strlen(utp->u_templ);
	    if (n > taglen)
		taglen = n;
	}
    }
    if (taglen < 10)
	taglen = 10;

    bprintf("    %*s FROM line in file\n", taglen, "TO tag");
    bprintf("    ");
    bpadc('-', taglen);
    bprintf(" --------- ");
    bpadc('-', 30);

    for (utp = untaghead, n = 0; utp != NULL; utp = utp->u_stklink)
	bprintf("\n %2d %*s %8d  %s",
		++n,
		taglen, utp->u_templ,
		utp->u_lineno,
		shorten_path(vl_strncpy(temp, utp->u_fname, sizeof(temp)),
			     TRUE));
}

#if OPT_UPBUFF
/* ARGSUSED */
static int
update_tagstack(BUFFER *bp GCC_UNUSED)
{
    return showtagstack(FALSE, 1);
}
#endif

/*
 * Display the contents of the tag-stack
 */
/*ARGSUSED*/
int
showtagstack(int f, int n GCC_UNUSED)
{
    return liststuff(TAGSTACK_BufName, FALSE, maketagslist, f, (void *) 0);
}
#endif /* OPT_SHOW_TAGS */

#if NO_LEAKS
void
tags_leaks(void)
{
    L_NUM lineno;
    C_NUM colno;
    char fname[NFILEN];

    free_tag_hits();
    while (popuntag(fname, &lineno, &colno)) ;
#if OPT_TAGS_CMPL
    btree_freeup(&tags_tree);
#endif
}
#endif

#endif /* OPT_TAGS */
