/* Look up vi-style tags in the file "tags".
 *	Invoked either by ":ta routine-name" or by "^]" while sitting
 *	on a string.  In the latter case, the tag is the word under
 *	the cursor.
 *	written for vile: Copyright (c) 1990, 1995 by Paul Fox
 *
 * $Header: /users/source/archives/vile.vcs/RCS/tags.c,v 1.80 1997/03/31 00:40:03 tom Exp $
 *
 */
#include	"estruct.h"
#include        "edef.h"

#if OPT_TAGS

#define	UNTAG	struct	untag
	UNTAG {
	char *u_fname;
	int u_lineno;
	UNTAG *u_stklink;
#if OPT_SHOW_TAGS
	char	*u_templ;
#endif
};

static	LINE *	cheap_tag_scan(LINEPTR oldlp, char *name, SIZE_T taglen);
static	LINE *	cheap_buffer_scan(BUFFER *bp, char *patrn, int dir);
static	BUFFER *gettagsfile(int n, int *endofpathflagp);
static	int	popuntag(char *fname, int *linenop);
static	void	pushuntag(char *fname, int lineno, char *tag);
static	int	tag_search(char *tag, int taglen, int initial);
static	void	tossuntag (void);

static	UNTAG *	untaghead = NULL;
static	char	tagname[NFILEN+2];  /* +2 since we may add a tab later */

#if OPT_SHOW_TAGS
#  if OPT_UPBUFF
static	int	update_tagstack ( BUFFER *bp );
#  endif
#endif	/* OPT_SHOW_TAGS */

/* ARGSUSED */
int
gototag(int f GCC_UNUSED, int n GCC_UNUSED)
{
	register int s;
	int taglen;

	if (clexec || isnamedcmd) {
	        if ((s=mlreply("Tag name: ", tagname, NFILEN)) != TRUE)
	                return (s);
		taglen = b_val(curbp,VAL_TAGLEN);
	} else {
		s = screen_string(tagname, NFILEN, _ident);
		taglen = 0;
	}
	if (s == TRUE)
		s = tag_search(tagname,taglen,TRUE);
	else
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
	if (tagname[0] != EOS)
		s = tag_search(tagname, global_b_val(VAL_TAGLEN), FALSE);
	return s;
}

int
cmdlinetag(const char *t)
{
	return tag_search(strncpy0(tagname, t, NFILEN),
		global_b_val(VAL_TAGLEN),
		TRUE);
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
	int lineno;
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
		tagbp = gettagsfile(tf_num, &nomore);
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
#if SMALLER
		register LINE *clp;
		lineno = 1;
	        for(clp = lforw(buf_head(curbp));
				clp != DOT.l; clp = lforw(clp))
			lineno++;
#else
		(void)bsizes(curbp);
		lineno = DOT.l->l_number;
#endif
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
		mlwrite("Tag \"%s\" in current buffer", tag);
		changedfile = FALSE;
	}

	/* it's an absolute move -- remember where we are */
	odot = DOT;

	tfp++;  /* skip the tab */
	if (tfp >= lplim) {
		mlforce("[Bad line in tags file.]");
		return FALSE;
	}
	if (isdigit(*tfp)) { /* then it's a line number */
		lineno = 0;
		while (isdigit(*tfp) && (tfp < lplim)) {
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
	/* if we moved, update the "last dot" mark */
	if (status == TRUE && !sameline(DOT, odot)) {
		curwp->w_lastdot = odot;
	}

	return status;
}

/*
 * return (in buf) the Nth whitespace
 *	separated word in "path", counting from 0
 */
static void
nth_name(char *buf, char *path, int n)
{
	while (n-- > 0) {
		while (*path &&  isspace(*path)) path++;
		while (*path && !isspace(*path)) path++;
	}
	while (*path &&  isspace(*path)) path++;
	while (*path && !isspace(*path)) *buf++ = *path++;
	*buf = EOS;
}


static BUFFER *
gettagsfile(int n, int *endofpathflagp)
{
#ifdef	MDCHK_MODTIME
	time_t current;
#endif
	const char *tagsfile;
	BUFFER *tagbp;
	char tagbufname[NBUFN];
	char tagfilename[NFILEN];

	*endofpathflagp = FALSE;

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
        }
#ifdef	MDCHK_MODTIME
	/*
	 * Re-read the tags buffer if we are checking modification-times and
	 * find that the tags file's been changed. We check the global mode
	 * value because it's too awkward to set the local mode value for a
	 * scratch buffer.
	 */
	else if (global_b_val(MDCHK_MODTIME)
	 && get_modtime(tagbp, &current)
	 && tagbp->b_modtime != current) {
		if (readin(tagbp->b_fname, FALSE, tagbp, FALSE) != TRUE) {
			return NULL;
		}
	 	set_modtime(tagbp, tagbp->b_fname);
	}
#endif
	b_set_invisible(tagbp);
	return tagbp;
}

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
		if (llength(lp) > namelen) {
			if (!strncmp(lp->l_text, name, namelen)) {
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
	return (result);
}

int
untagpop(int f, int n)
{
	int lineno;
	char fname[NFILEN];
	MARK odot;
	int s;

	if (!f) n = 1;
	while (n && popuntag(fname,&lineno))
		n--;
	if (lineno && fname[0]) {
		s = getfile(fname,FALSE);
		if (s == TRUE) {
			/* it's an absolute move -- remember where we are */
			odot = DOT;
			s = gotoline(TRUE,lineno);
			/* if we moved, update the "last dot" mark */
			if (s == TRUE && !sameline(DOT, odot)) {
				curwp->w_lastdot = odot;
			}
		}
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
pushuntag(char *fname, int lineno, char *tag)
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

	utp->u_lineno = lineno;
	utp->u_stklink = untaghead;
	untaghead = utp;
	update_scratch(TAGSTACK_BufName, update_tagstack);
}


static int
popuntag(char *fname, int *linenop)
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

#endif	/* OPT_TAGS */
