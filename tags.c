/* Look up vi-style tags in the file "tags".
 *	Invoked either by ":ta routine-name" or by "^]" while sitting
 *	on a string.  In the latter case, the tag is the word under
 *	the cursor.
 *	written for vile: Copyright (c) 1990, 1995 by Paul Fox
 *
 * $Header: /users/source/archives/vile.vcs/RCS/tags.c,v 1.89 1998/04/28 10:18:57 tom Exp $
 *
 */
#include	"estruct.h"
#include        "edef.h"

#if OPT_TAGS

#if OPT_TAGS_CMPL
typedef struct {
	/* FIXME: we could make next-tag work faster if we also hash the
	 * line-pointers for each key.
	 */
	char	*bi_key;
	} TAGS_DATA;

#define BI_DATA TAGS_DATA
#include	"btree.h"

#endif

#define	UNTAG	struct	untag
	UNTAG {
	char *u_fname;
	L_NUM u_lineno;
	UNTAG *u_stklink;
#if OPT_SHOW_TAGS
	char	*u_templ;
#endif
};

#define TAGHITS	struct	taghits
	TAGHITS	{
	TAGHITS	*link;
	LINE	*tag;	/* points to tag-buffer line */
	LINE	*hit;	/* points to corresponding line in source-file */
};

static	LINE *	cheap_tag_scan(LINEPTR oldlp, char *name, SIZE_T taglen);
static	LINE *	cheap_buffer_scan(BUFFER *bp, char *patrn, int dir);
static	BUFFER *gettagsfile(int n, int *endofpathflagp, int *did_read);
static	int	popuntag(char *fname, L_NUM *linenop);
static	void	pushuntag(char *fname, L_NUM lineno, char *tag);
static	int	tag_search(char *tag, int taglen, int initial);
static	void	tossuntag (void);

static	TAGHITS*tag_hits;
static	UNTAG *	untaghead;
static	char	tagname[NFILEN+2];  /* +2 since we may add a tab later */

#if OPT_SHOW_TAGS
#  if OPT_UPBUFF
static	int	update_tagstack ( BUFFER *bp );
#  endif
#endif	/* OPT_SHOW_TAGS */

#if OPT_TAGS_CMPL

static BI_NODE*
new_tags (BI_DATA *a)
{
	BI_NODE *p = typecalloc(BI_NODE);
	p->value = *a;
	BI_KEY(p) = strmalloc(a->bi_key);
	return p;
}

static void
old_tags (BI_NODE *a)
{
	free(BI_KEY(a));
	free(a);
}

static void
dpy_tags (BI_NODE *a GCC_UNUSED, int level GCC_UNUSED)
{
#if OPT_TRACE
	while (level-- > 0)
		TRACE((". "));
	TRACE(("%s (%d)\n", BI_KEY(a), a->balance));
#endif
}

static BI_TREE tags_tree = { new_tags, old_tags, dpy_tags };

/* Parse the identifier out of the given line and store it in the binary tree */
static void
store_tag(LINE *lp)
{
	char	my_name[sizeof(tagname)];
	size_t	len, got;
	int	c;

	if (llength(lp) > 0) {
		len = llength(lp);
		for (got = 0; got < len; got++) {
			c = lgetc(lp, got);
			if (!isident(c))
				break;
			my_name[got] = c;
		}
		my_name[got] = EOS;
		if (got) {
			BI_DATA temp;
#ifdef MDTAGIGNORECASE
			if (b_val(curbp,MDTAGIGNORECASE))
				mklower(my_name);
#endif
			temp.bi_key = my_name;
			btree_insert(&tags_tree, &temp);
		}
	}
}

/* check if the binary-tree is up-to-date.  If not, rebuild it. */
static const char **
init_tags_cmpl (char *buf, unsigned cpos)
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
		if (b_val(curbp,MDTAGIGNORECASE) != my_tcase) {
			my_tcase = b_val(curbp,MDTAGIGNORECASE);
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
		for (tf_num = 0; ; tf_num++) {
			bp = gettagsfile(tf_num, &done, &flag);
			if (done || bp == 0)
				break;
			(void)bsizes(bp);
			obsolete = flag || (bp->b_linecount != 0);
			if (obsolete)
				break;
		}
	}

	if (obsolete) {
		btree_freeup(&tags_tree);

		for (tf_num = 0; ; tf_num++) {
			bp = gettagsfile(tf_num, &done, &flag);
			if (done || bp == 0)
				break;
			for_each_line(lp,bp)
				store_tag(lp);
		}

		TRACE(("stored %d tags entries\n", tags_tree.count))
	}

	return btree_parray(&tags_tree, buf, cpos);
}

static int
tags_completion(int c, char *buf, unsigned *pos)
{
	register unsigned cpos = *pos;
	int status = FALSE;
	const char **nptr;

	kbd_init();		/* nothing to erase */
	buf[cpos] = EOS;	/* terminate it for us */

	if ((nptr = init_tags_cmpl(buf, cpos)) != 0) {
		status = kbd_complete(FALSE, c, buf, pos, (char *)nptr, sizeof(*nptr));
		free((char *)nptr);
	}
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

	TRACE(("mark_tag_hit %s:%d\n", curbp->b_bname, line_no(curbp, hit)))
	for (p = tag_hits; p != 0; p = p->link) {
		if (p->hit == hit) {
			TRACE(("... mark_tag_hit TRUE\n"))
			return (p->tag == tag) ? ABORT : TRUE;
		}
	}

	p = typecalloc(TAGHITS);
	p->link  = tag_hits;
	p->tag   = tag;
	p->hit   = hit;
	tag_hits = p;

	TRACE(("... mark_tag_hit FALSE\n"))
	return FALSE;
}

/*
 * Discard the list of tag-hits when we're about to begin a new tag-search.
 */
static void
free_tag_hits(void)
{
	TAGHITS *p;
	while ((p = tag_hits) != 0) {
		tag_hits = p->link;
		free(p);
	}
}

/* ARGSUSED */
int
gototag(int f GCC_UNUSED, int n GCC_UNUSED)
{
	register int s;
	int taglen;

	if (clexec || isnamedcmd) {
		UINT mode = KBD_NORMAL
#if OPT_TAGS_CMPL
			| KBD_MAYBEC
#endif
			;
#ifdef MDTAGIGNORECASE
			if (b_val(curbp,MDTAGIGNORECASE))
				mode |= KBD_LOWERC;
#endif
		if ((s = kbd_string("Tag name: ",
				tagname, sizeof(tagname),
				'\n', mode, tags_completion)) != TRUE)
	                return (s);
		taglen = b_val(curbp,VAL_TAGLEN);
	} else {
		s = screen_string(tagname, sizeof(tagname), _ident);
		taglen = 0;
	}

	if (s == TRUE) {
#ifdef MDTAGIGNORECASE
		if (b_val(curbp,MDTAGIGNORECASE))
			mklower(tagname);
#endif
		free_tag_hits();
		s = tag_search(tagname,taglen,TRUE);
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
	int	s = FALSE;
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
cmdlinetag(const char *t)
{
	return tag_search(strncpy0(tagname, t, NFILEN),
		global_b_val(VAL_TAGLEN),
		TRUE);
}

/*
 * Jump back to the given file & line.
 */
static int
finish_pop(char *fname, L_NUM lineno)
{
	MARK odot;
	int s;

	s = getfile(fname,FALSE);
	if (s == TRUE) {
		/* it's an absolute move -- remember where we are */
		odot = DOT;
		s = gotoline(TRUE, lineno);
		/* if we moved, update the "last dot" mark */
		if (s == TRUE) {
			if (!sameline(DOT, odot))
				curwp->w_flag &= ~WFMOVE;
			else
				curwp->w_lastdot = odot;
		}
	}
	return s;
}

static int
tag_search(char *tag, int taglen, int initial)
{
	/* variables for 'initial'=FALSE */
	static	int tf_num;
	static	char last_bname[NBUFN];
#ifdef	MDCHK_MODTIME
	static	time_t	last_modtime;
#endif

	static TBUFF *srchpat;

	register LINE *lp;
	register SIZE_T i;
	register int status;
	char *tfp, *lplim;
	char tfname[NFILEN];
	int flag;
	L_NUM lineno;
	int changedfile;
	MARK odot;
	BUFFER *tagbp;
	int nomore;
	int gotafile = FALSE;
	int retried = FALSE;

	if (initial)
		tf_num = 0;
#ifdef	MDCHK_MODTIME
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
		lp = 0;
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
			lp = cheap_tag_scan(
				initial || retried
				  ? buf_head(tagbp)
				  : tagbp->b_dot.l,
				tag, (SIZE_T)taglen);
			gotafile = TRUE;
		}

		tf_num++;

	} while (lp == NULL);

	/* Save the position in the tags-file so "next-tag" will work */
	tf_num--;
	(void)strcpy(last_bname, tagbp->b_bname);
	tagbp->b_dot.l = lp;
	tagbp->b_dot.o = 0;
#ifdef	MDCHK_MODTIME
	last_modtime = tagbp->b_modtime;
#endif

	/* Parse the line from the tags-file */
	lplim = &lp->l_text[lp->l_used];
	tfp = lp->l_text;
	while (tfp < lplim)
		if (*tfp++ == '\t')
			break;
	if (*tfp == '\t') { /* then it's a new-fangled NeXT tags file */
		tfp++;  /* skip the tab */
	}

	i = 0;
	if (b_val(curbp,MDTAGSRELTIV) && !is_slashc(*tfp)
#if OPT_MSDOS_PATH
	&& !is_msdos_drive(tfp)
#endif
	) {
		register char *first = tagbp->b_fname;
		char *lastsl = pathleaf(tagbp->b_fname);
		while (lastsl != first)
			tfname[i++] = *first++;
	}
	while (i < sizeof(tfname) && tfp < lplim && *tfp != '\t') {
		tfname[i++] = *tfp++;
	}
	tfname[i] = EOS;

	if (tfp >= lplim) {
		mlforce("[Bad line in tags file.]");
		return FALSE;
	}

	if (curbp && curwp) {
		lineno = line_no(curbp, DOT.l);
		if (!isInternalName(curbp->b_fname))
			pushuntag(curbp->b_fname, lineno, tag);
		else
			pushuntag(curbp->b_bname, lineno, tag);
	}

	if (curbp == NULL
	 || !same_fname(tfname, curbp, TRUE)) {
		(void) doglob(tfname);
		status = getfile(tfname,TRUE);
		if (status != TRUE) {
			tossuntag();
			return status;
		}
		changedfile = TRUE;
	} else {
		changedfile = FALSE;
	}

	/* it's an absolute move -- remember where we are */
	odot = DOT;

	tfp++;  /* skip the tab */
	if (tfp >= lplim) {
		mlforce("[Bad line in tags file.]");
		return FALSE;
	}
	if (isDigit(*tfp)) { /* then it's a line number */
		lineno = 0;
		while (isDigit(*tfp) && (tfp < lplim)) {
			lineno = 10*lineno + *tfp - '0';
			tfp++;
		}
		status = gotoline(TRUE,lineno);
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
		p = ++tfp;	/* skip the "/" */

		/* we're on the '/', so look for the matching one */
		while (p < lplim) {
			if (quoted) {
				quoted = FALSE;
			} else if (*p == '\\') {
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

		if ((srchpat = tb_init(&srchpat, EOS)) == 0
		 || (srchpat = tb_bappend(&srchpat, tfp, (ALLOC_T)(p - tfp))) == 0
		 || (srchpat = tb_append(&srchpat, EOS)) == 0)
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
		(void)firstnonwhite(FALSE,1);
		status = TRUE;
	}

	if (status == TRUE) {
		int s;
		if ((s = mark_tag_hit(tagbp->b_dot.l, DOT.l)) != FALSE) {
			if (popuntag(tfname, &lineno))
				(void) finish_pop(tfname, lineno);
			return s;
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
	while (*path && !isSpace(*path)) *buf++ = *path++;
	*buf = EOS;
}


static BUFFER *
gettagsfile(int n, int *endofpathflagp, int *did_read)
{
#ifdef	MDCHK_MODTIME
	time_t current;
#endif
	char *tagsfile;
	BUFFER *tagbp;
	char tagbufname[NBUFN];
	char tagfilename[NFILEN];

	*endofpathflagp = FALSE;
	*did_read = FALSE;

	(void)lsprintf(tagbufname, TAGFILE_BufName, n+1);

	/* is the buffer around? */
	if ((tagbp=find_b_name(tagbufname)) == NULL) {
		char *tagf = global_b_val_ptr(VAL_TAGS);

		nth_name(tagfilename, tagf, n);
		if (!doglob(tagfilename)
		 || tagfilename[0] == EOS) {
			*endofpathflagp = TRUE;
			return NULL;
		}

		/* look up the tags file */
		tagsfile = flook(tagfilename, FL_HERE|FL_READABLE);

		/* if it isn't around, don't sweat it */
		if (tagsfile == NULL)
		{
			return NULL;
		}

		/* find the pointer to that buffer */
		if ((tagbp=bfind(tagbufname, BFINVS)) == NULL) {
			mlforce("[Can't create tags buffer]");
			return NULL;
		}

		if (readin(tagsfile, FALSE, tagbp, FALSE) != TRUE) {
			return NULL;
		}
		*did_read = TRUE;
        }
#ifdef	MDCHK_MODTIME
	/*
	 * Re-read the tags buffer if we are checking modification-times and
	 * find that the tags file's been changed. We check the global mode
	 * value because it's too awkward to set the local mode value for a
	 * scratch buffer.
	 */
	if (global_b_val(MDCHK_MODTIME)
	 && get_modtime(tagbp, &current)
	 && tagbp->b_modtime != current) {
		if (!*did_read
		 && readin(tagbp->b_fname, FALSE, tagbp, FALSE) != TRUE) {
			return NULL;
		}
	 	set_modtime(tagbp, tagbp->b_fname);
		*did_read = TRUE;
	}
#endif
	b_set_invisible(tagbp);
	return tagbp;
}

#ifdef MDTAGIGNORECASE
static int my_strncasecmp(const char *a, const char *b, size_t len)
{
	int aa = EOS, bb = EOS;

	while ((len != 0)
	  &&   ((aa = (isUpper(*a) ? toLower(*a) : *a)) != EOS)
	  &&   ((bb = (isUpper(*b) ? toLower(*b) : *b)) != EOS)
	  &&   (aa == bb)) {
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
cheap_tag_scan(LINEPTR oldlp, char *name, SIZE_T taglen)
{
	register LINE *lp,*retlp;
	SIZE_T namelen = strlen(name);
	int exact = (taglen == 0);
	int added_tab;
#ifdef MDTAGIGNORECASE
	int (*compare)(const char *a, const char *b, size_t len)
		= b_val(curbp,MDTAGIGNORECASE)
		? my_strncasecmp
		: strncmp;
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
		if (llength(lp) > (int)namelen) {
			if (!compare(lp->l_text, name, namelen)) {
				retlp = lp;
				break;
			}
		}
		lp = lforw(lp);
	}
	if (added_tab)
		name[namelen-1] = EOS;
	return retlp;
}

static LINE *
cheap_buffer_scan(BUFFER *bp, char *patrn, int dir)
{
	register LINE *lp;
	register LINE *result = 0;
	regexp *exp = regcomp(patrn, FALSE);
#ifdef MDTAGIGNORECASE
	int savecase = ignorecase;
	if (b_val(bp,MDTAGIGNORECASE))
		ignorecase = TRUE;
#endif

	TRACE(("cheap_buffer_scan '%s' %s\n",
		patrn,
		dir == FORWARD ? "fwd" : "bak"))

	for (lp = dir == FORWARD ? lforw(buf_head(bp)) : lback(buf_head(bp));
		lp != buf_head(bp);
		lp = dir == FORWARD ? lforw(lp) : lback(lp))
	{
		if (lregexec(exp, lp, 0, llength(lp))) {
			result = lp;
			break;
		}
	}
	free(exp);
#ifdef MDTAGIGNORECASE
	ignorecase = savecase;
#endif
	return (result);
}

int
untagpop(int f, int n)
{
	L_NUM lineno;
	char fname[NFILEN];
	int s;

	if (!f) n = 1;
	while (n && popuntag(fname,&lineno))
		n--;
	if (lineno && fname[0]) {
		s = finish_pop(fname, lineno);
	} else {
		mlwarn("[No stacked un-tags]");
		s = FALSE;
	}
	return s;
}


static void
free_untag(UNTAG *utp)
{
	FreeIfNeeded(utp->u_fname);
#if OPT_SHOW_TAGS
	FreeIfNeeded(utp->u_templ);
#endif
	free((char *)utp);
}


/*ARGSUSED*/
static void
pushuntag(char *fname, L_NUM lineno, char *tag)
{
	UNTAG *utp;
	utp = typealloc(UNTAG);
	if (!utp)
		return;

	if ((utp->u_fname = strmalloc(fname)) == 0
#if OPT_SHOW_TAGS
	 || (utp->u_templ = strmalloc(tag)) == 0
#endif
	   ) {
		free_untag(utp);
		return;
	}
#if OPT_VMS_PATH
	strip_version(utp->u_fname);
#endif

	utp->u_lineno = lineno;
	utp->u_stklink = untaghead;
	untaghead = utp;
	update_scratch(TAGSTACK_BufName, update_tagstack);
}


static int
popuntag(char *fname, L_NUM *linenop)
{
	register UNTAG *utp;

	if (untaghead) {
		utp = untaghead;
		untaghead = utp->u_stklink;
		(void)strcpy(fname, utp->u_fname);
		*linenop = utp->u_lineno;
		free_untag(utp);
		update_scratch(TAGSTACK_BufName, update_tagstack);
		return TRUE;
	}
	fname[0] = EOS;
	*linenop = 0;
	return FALSE;

}

/* discard without returning anything */
static void
tossuntag(void)
{
	register UNTAG *utp;

	if (untaghead) {
		utp = untaghead;
		untaghead = utp->u_stklink;
		free_untag(utp);
		update_scratch(TAGSTACK_BufName, update_tagstack);
	}
}

#if OPT_SHOW_TAGS
/*ARGSUSED*/
static void
maketagslist (int value GCC_UNUSED, void *dummy GCC_UNUSED)
{
	register UNTAG *utp;
	register int	n;
	int	taglen = global_b_val(VAL_TAGLEN);
	char	temp[NFILEN];

	if (taglen == 0) {
		for (utp = untaghead; utp != 0; utp = utp->u_stklink) {
			n = strlen(utp->u_templ);
			if (n > taglen)
				taglen = n;
		}
	}
	if (taglen < 10)
		taglen = 10;

	bprintf("    %*s FROM line in file\n", taglen, "TO tag");
	bprintf("    %*p --------- %30p",      taglen, '-', '-');

	for (utp = untaghead, n = 0; utp != 0; utp = utp->u_stklink)
		bprintf("\n %2d %*s %8d  %s",
			++n,
			taglen, utp->u_templ,
			utp->u_lineno,
			shorten_path(strcpy(temp, utp->u_fname), TRUE));
}


#if OPT_UPBUFF
/* ARGSUSED */
static int
update_tagstack(BUFFER *bp GCC_UNUSED)
{
	return showtagstack(FALSE,1);
}
#endif

/*
 * Display the contents of the tag-stack
 */
/*ARGSUSED*/
int
showtagstack(int f, int n GCC_UNUSED)
{
	return liststuff(TAGSTACK_BufName, FALSE, maketagslist, f, (void *)0);
}
#endif	/* OPT_SHOW_TAGS */

#if NO_LEAKS
void tags_leaks (void)
{
	L_NUM lineno;
	char fname[NFILEN];

	free_tag_hits();
	while (popuntag(fname, &lineno))
		;
#if OPT_TAGS_CMPL
	btree_freeup(&tags_tree);
#endif
}
#endif

#endif	/* OPT_TAGS */
