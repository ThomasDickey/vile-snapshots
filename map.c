/*
 * map.c	-- map and map! interface routines
 *	Original interface by Otto Lind, 6/3/93
 *	Additional map and map! support by Kevin Buettner, 9/17/94
 *
 * $Header: /users/source/archives/vile.vcs/RCS/map.c,v 1.80 1998/04/09 21:16:45 kev Exp $
 *
 */

#include "estruct.h"
#include "edef.h"

/*
 * Picture for struct maprec
 * -------------------------
 *
 * Assume the following mappings...
 *
 * map za abc
 * map zb def
 * map q  quit
 *
 * These may be represented by the following picture...
 *
 *   |
 *   v
 * +---+--------+---+---+   +---+--------+---+---+
 * | z |  NULL  | o | o-+-->| q | "quit" | 0 | 0 |
 * +---+--------+-|-+---+   +---+--------+---+---+
 *                |
 *		  v
 *              +---+--------+---+---+   +---+--------+---+---+
 *              | a | "abc"  | 0 | o-+-->| b | "def"  | 0 | 0 |
 *              +---+--------+---+---+   +---+--------+---+---+
 *
 * where the pertinent fields are as follows:
 *
 * +----+-----+-------+-------+
 * | ch | srv | dlink | flink |
 * +----+-----+-------+-------+
 *
 * When matching a character sequence, we follow dlink when we've found a
 * matching character.  We change the character to be matched to the next
 * character in the sequence.  If the character doesn't match, we stay at
 * the same level (with the same character) and follow flink.
 *
 */

struct maprec {
	int		ch;		/* character to match		*/
	UINT		flags;		/* flags word			*/
	struct maprec *	dlink;		/* Where to go to match the	*/
					/*   next character in a multi-	*/
					/*   character sequence.	*/
	struct maprec *	flink;		/* Where to try next if match	*/
					/*   against current character 	*/
					/*   is unsuccessful		*/
	int		irv;		/* system defined mapping: The	*/
					/*   (wide) character code to	*/
					/*   replace a matched sequence by */
	char          *	srv;		/* user defined mapping: This	*/
					/*   is the string to replace a	*/
					/*   matched sequence by	*/
};

#define MAPF_SYSTIMER	0x01
#define MAPF_USERTIMER	0x02
#define MAPF_TIMERS	0x03
#define MAPF_NOREMAP	0x04

static int suppress_sysmap;

static struct maprec *map_command = NULL;
static struct maprec *map_insert = NULL;
static struct maprec *map_syskey = NULL;
static struct maprec *abbr_map = NULL;

static	int	map_common(struct maprec **mpp, const char *bufname, UINT remapflag);
static	int	unmap_common(struct maprec **mpp, const char *bufname);
static	void	addtomap(struct maprec **mpp, const char * ks, int kslen, UINT flags, int irv, char * srv);
static	int	delfrommap(struct maprec **mpp, const char * ks);

static	int	abbr_getc (void);
static	int	abbr_c_avail (void);

static	int	mapgetc (void);

typedef	int (*AvailFunc) (void);
typedef	int (*GetFunc) (void);
typedef	int (*StartFunc) (void);

static	int	maplookup(int c, ITBUFF **outp, struct maprec *mp, GetFunc get, AvailFunc avail, StartFunc start, int suffix);

#if !OPT_UPBUFF
#define relist_mappings(name)
#endif

#if OPT_SHOW_MAPS
#define MAPS_PREFIX 12

/*ARGSUSED*/
static void
makemaplist(int dummy GCC_UNUSED, void *mapp)
{
    TBUFF *lhsstr = 0;
    struct maprec **lhsstack = 0;
    struct maprec *mp = (struct maprec *) mapp;
    int footnote = 0;
    ALLOC_T depth = 0;
    ALLOC_T maxdepth;
    ALLOC_T i;

    lhsstr = tb_init(&lhsstr, 0);
    lhsstack = typeallocn(struct maprec *, maxdepth = NSTRING);
    for_ever {
	if (mp) {
	    const char *remapnote;
	    char *mapstr;
	    char esc_seq[10];	/* FIXME */
	    tb_put(&lhsstr, depth, mp->ch);
	    if (depth+1 >= maxdepth)
		lhsstack = typereallocn(struct maprec *, lhsstack, maxdepth *= 2);
	    lhsstack[depth++] = mp->flink;

	    mapstr = (char *)0;
	    if (mp->srv) {
		mapstr = mp->srv;
	    } else if (mp->irv != -1) {
		(void)kcod2escape_seq(mp->irv, esc_seq);
		mapstr = esc_seq;
	    }
	    if (mapstr) {
    		    if (mapp && (struct maprec *)mapp == abbr_map) {
			    /* the abbr map is stored inverted */
			    for (i = depth; i != 0; )
				bputc(tb_values(lhsstr)[--i]);
		    } else {
			    if (mp->flags & MAPF_NOREMAP) {
				remapnote = "(n)";
				footnote++;
			    } else {
				remapnote = "   ";
			    }
			    bprintf("%s ", remapnote);
			    for (i = 0; i < depth; i++)
				bputc(tb_values(lhsstr)[i]);
		    }
		    bprintf("\t%s\n", mapstr);
	    }
	    mp = mp->dlink;
	}
	else if (depth != 0)
	    mp = lhsstack[--depth];
	else
	    break;
    }
    if (footnote) {
	bprintf("[(n) means never remap]\n");
    }
    tb_free(&lhsstr);
    free((char *)lhsstack);
}

static int
show_mapped_chars(const char *bname)
{
	struct maprec *mp;
	if (strcmp(bname, MAP_BufName) == 0)
		mp = map_command;
	else if (strcmp(bname, MAPBANG_BufName) == 0)
		mp = map_insert;
	else if (strcmp(bname, ABBR_BufName) == 0)
		mp = abbr_map;
	else if (strcmp(bname, SYSMAP_BufName) == 0)
		mp = map_syskey;
	else
		return FALSE;
	return liststuff(bname, FALSE, makemaplist, 0, (void *)mp);
}

#if OPT_UPBUFF
static int
show_Mappings(BUFFER *bp)
{
	b_clr_obsolete(bp);
	return show_mapped_chars(bp->b_bname);
}

#undef relist_mappings

static void
relist_mappings(const char * bufname)
{
    update_scratch(bufname, show_Mappings);
}
#endif	/* OPT_UPBUFF */

#endif	/* OPT_SHOW_MAPS */

/*
** set a map for the character/string combo
*/
/* ARGSUSED */
int
map(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return map_common(&map_command, MAP_BufName, 0);
}

/* ARGSUSED */
int
map_bang(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return map_common(&map_insert, MAPBANG_BufName, 0);
}

/* ARGSUSED */
int
noremap(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return map_common(&map_command, MAP_BufName, MAPF_NOREMAP);
}

/* ARGSUSED */
int
noremap_bang(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return map_common(&map_insert, MAPBANG_BufName, MAPF_NOREMAP);
}

/* ARGSUSED */
int
abbrev(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return map_common(&abbr_map, ABBR_BufName, MAPF_NOREMAP);
}

#if OPT_SHOW_MAPS
/* ARGSUSED */
int
sysmap(int f GCC_UNUSED, int n GCC_UNUSED)
{
	return show_mapped_chars(SYSMAP_BufName);
}
#endif

static int
map_common(struct maprec **mpp, const char *bufname, UINT remapflag)
{
    int	 status;
    static TBUFF *kbuf;
    static TBUFF *val;
    int len;

#if OPT_SHOW_MAPS
    if (end_named_cmd()) {
	return show_mapped_chars(bufname);
    }
#endif
    tb_scopy(&kbuf, "");
    status = kbd_reply("change this string: ", &kbuf, eol_history,
			' ', KBD_NOMAP|KBD_NOEVAL, no_completion);
    if (status != TRUE)
	return status;

    hst_glue(' ');
    tb_scopy(&val, "");
    if (!clexec) {
	    status = kbd_reply("to this new string: ", &val, eol_history,
			'\n', KBD_NOMAP, no_completion);
    } else {
	    (void)macliteralarg(&val); /* consume to end of line */
	    status = tb_length(val) > 1;
    }
    if (status != TRUE)
	return status;

    len = tb_length(kbuf) - 1;
    if ((*mpp && *mpp == abbr_map) || (strcmp(bufname, ABBR_BufName) == 0)) {
	/* reverse the lhs */
	int i;
	char t;
	char *s = tb_values(kbuf);
	for (i = 0; i < len/2; i++) {
	    t = s[len-i-1];
	    s[len-i-1] = s[i];
	    s[i] = t;
	}
    }

    addtomap(mpp, tb_values(kbuf), len, MAPF_USERTIMER|remapflag, -1, tb_values(val));
    relist_mappings(bufname);
    return TRUE;
}

/*
** remove map entry, restore saved CMDFUNC for key
*/
/* ARGSUSED */
int
unmap(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return unmap_common(&map_command, MAP_BufName);
}

/* ARGSUSED */
int
unmap_bang(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return unmap_common(&map_insert, MAPBANG_BufName);
}

/* ARGSUSED */
int
unmap_system(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return unmap_common(&map_syskey, SYSMAP_BufName);
}

/* ARGSUSED */
int
unabbr(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return unmap_common(&abbr_map, ABBR_BufName);
}

static int
unmap_common(struct maprec **mpp, const char *bufname)
{
    int	 status;
    static TBUFF *kbuf;

    /* otherwise it'll be mapped, and not found when looked up */
    if (mpp && mpp == &map_syskey)
    	suppress_sysmap = TRUE;

    tb_scopy(&kbuf, "");
    status = kbd_reply("unmap string: ", &kbuf, eol_history,
			' ', KBD_NOMAP, no_completion);
    suppress_sysmap = FALSE;
    if (status != TRUE)
	return status;

    if ((*mpp && *mpp == abbr_map) || (strcmp(bufname, ABBR_BufName) == 0)) {
	/* reverse the lhs */
	int i;
	char t;
    	int len = tb_length(kbuf) - 1;
	char *s = tb_values(kbuf);
	for (i = 0; i < len/2; i++) {
	    t = s[len-i-1];
	    s[len-i-1] = s[i];
	    s[i] = t;
	}
    }

    if (delfrommap(mpp, tb_values(kbuf)) != TRUE) {
	mlforce("[Sequence not mapped]");
	return FALSE;
    }
    relist_mappings(bufname);
    return TRUE;
}

/* addtosysmap is used to initialize the system default function key map
*/
void
addtosysmap(const char * seq, int seqlen, int code)
{
    addtomap(&map_syskey, seq, seqlen, MAPF_SYSTIMER,
    			code, (char *)0);
}

static void
addtomap(
    struct maprec **mpp,
    const char * ks,
    int         kslen,
    UINT        flags,
    int		irv,
    char *	srv)
{
    struct maprec *mp = NULL;

    if (ks == 0 || kslen == 0)
	return;

    while (*mpp && kslen) {
	mp = *mpp;
	mp->flags |= flags;
	if (char2int(*ks) == mp->ch) {
	    mpp = &mp->dlink;
	    ks++;
	    kslen--;
	}
	else
	    mpp = &mp->flink;
    }

    while (kslen) {
	mp = typealloc(struct maprec);
	if (mp == 0)
	    break;
	*mpp = mp;
	mp->dlink = mp->flink = NULL;
	mp->ch = char2int(*ks++);
	mp->srv = NULL;
	mp->flags = flags;
	mp->irv = -1;
	mpp = &mp->dlink;
	kslen--;
    }

    if (irv != -1)
	mp->irv = irv;
    if (srv) {
	if (mp->srv)
	    free(mp->srv);
	mp->srv = strmalloc(srv);
    }
    mp->flags = flags;
}

static int
delfrommap(struct maprec **mpp, const char * ks)
{
    struct maprec **save_m = mpp;
    struct maprec ***mstk = 0;
    const char *save_k = ks;
    int depth = 0;
    int pass;

    if (ks == 0 || *ks == 0)
	return FALSE;

    for (pass = 0; pass < 2; pass++) {
	mpp   = save_m;
	ks    = save_k;
	depth = 0;
	while (*mpp && *ks) {
	    if (pass)
		mstk[depth] = mpp;
	    if ((*mpp)->ch == char2int(*ks)) {
		mpp = &(*mpp)->dlink;
		ks++;
		depth++;
	    }
	    else
		mpp = &(*mpp)->flink;
	}

	if (*ks)
	    return FALSE;		/* not in map */
	if (!pass)
	    mstk = typecallocn(struct maprec **, depth+1);
    }

    depth--;
    if ((*mstk[depth])->srv) {
	free((*mstk[depth])->srv);
	(*mstk[depth])->srv = NULL;
    } else if ((*mstk[depth])->irv != -1) {
	(*mstk[depth])->irv = -1;
    } else {
	free((char *)mstk);
	return FALSE;
    }

    for (; depth >= 0; depth--) {
	struct maprec *mp = *mstk[depth];
	if (mp->irv == -1 && mp->dlink == NULL && mp->srv == NULL) {
	    *mstk[depth] = mp->flink;
	    if (depth > 0 && (*mstk[depth-1])->dlink == mp)
		(*mstk[depth-1])->dlink = NULL;
	    free((char *)mp);
	}
	else
	    break;
    }
    free((char *)mstk);
    return TRUE;
}


#define INPUT_FROM_TTGET 1
#define INPUT_FROM_MAPGETC 2

static ITBUFF *sysmappedchars = NULL;

/* these two wrappers are provided because at least one pcc-based
	compiler balks at passing TTgetc or TTtypahead as a function pointer */

static int
normal_getc(void)
{
	int c = TTgetc();
	TRACE(("normal/getc:%c (%#x)\n", c, c))
	return c;
}

static int
normal_typeahead(void)
{
	return(TTtypahead());
}

static int
normal_start(void)
{
	return TRUE;
}

#define NUMKEYSTR (KBLOCK / 2)

static void
save_keystroke(int c)
{
	KILL *kp;
	KILLREG *kr = &kbs[KEYST_KREG];

	if (kr->kbufh == NULL) {
		kr->kbufh = typealloc(KILL);
		kr->kused = 0;
	}
	if (kr->kbufh == NULL)
		return;

	kp = kr->kbufp = kr->kbufh;
	kp->d_next = NULL;

	kp->d_chunk[kr->kused++] = (UCHAR)c;
	if (kr->kused >= NUMKEYSTR * 2) { /* time to dump the oldest half */
		(void)memcpy(
			(char *)(kp->d_chunk),
			(char *)(&kp->d_chunk[NUMKEYSTR / 2]),
			NUMKEYSTR / 2);
		kr->kused = NUMKEYSTR / 2;
	}


}

#if DOKEYLOG
int do_keylog = 1;
#endif

int
sysmapped_c(void)
{
    int c;

    /* still some left? */
    if (itb_more(sysmappedchars))
	return itb_last(sysmappedchars);

    c = TTgetc();
    TRACE(("mapped/getc:%c (%#x)\n", c, c))

#if DOKEYLOG
    if (do_keylog) {
	static int keyfd = -1;
	static char *tfilenam;
	if (!tfilenam)
		tfilenam = tempnam("/tmp/vilekeylogs", "vilek");
	if (tfilenam) {
		if (keyfd < 0)
			keyfd = open(tfilenam, O_CREAT|O_WRONLY, 0600);
		if (keyfd >= 0)
			write(keyfd, &c, 1);
	}
    }
#endif
    save_keystroke(c);

    if (suppress_sysmap)
    	return c;

    /* will push back on sysmappedchars successful, or not */
    (void)maplookup(c, &sysmappedchars, map_syskey,
    		normal_getc, normal_typeahead, normal_start, TRUE);

    return itb_last(sysmappedchars);
}

int
sysmapped_c_avail(void)
{
    return itb_more(sysmappedchars) || TTtypahead();
}


static ITBUFF *mapgetc_ungottenchars = NULL;
static int mapgetc_ungotcnt = 0;

/* make the assumption that no input will magically appear
 * (un)available to tgetc in between a mapungetc and the next mapgetc. 
 * Hence characters can't be ungotten onto the wrong buffer (exception
 * is the last tgot char might be mapungot onto the map buffer.  This
 * is OK (if assumption holds) because the next character will be
 * gotten from this buffer.
 */

void
mapungetc(int c)
{
    if (tgetc_avail()) {
	tungetc(c);
    } else {
	(void)itb_append(&mapgetc_ungottenchars, c);
	mapgetc_ungotcnt++;
    }
}

static int infloopcount;
static int mapgetc_raw_flag;

static int
mapgetc(void)
{
    UINT remapflag;
    if (global_g_val(GMDREMAP))
    	remapflag = 0;
    else
    	remapflag = NOREMAP;

    if (!tgetc_avail() && mapgetc_ungotcnt > 0) {
	    if (infloopcount++ > global_g_val(GVAL_MAPLENGTH)) {
		(void)itb_init(&mapgetc_ungottenchars, abortc);
		mapgetc_ungotcnt = 0;
		mlforce("[Infinite loop detected in %s sequence]",
			    (insertmode) ? "map!" : "map");
		catnap(1000,FALSE);  /* FIXX: be sure message gets out */
		return abortc|NOREMAP;
	    }
	    mapgetc_ungotcnt--;
	    return itb_last(mapgetc_ungottenchars) | remapflag;
    }
    infloopcount = 0;
    return tgetc(mapgetc_raw_flag);
}

int
mapped_c_avail(void)
{
    return mapgetc_ungotcnt > 0 || tgetc_avail();
}

int
mapped_ungotc_avail(void)
{
    return mapgetc_ungotcnt > 0;
}

static int
mapped_c_start(void)
{
    return TRUE;
}

int
mapped_c(int remap, int raw)
{
    int c;
    int matched;
    struct maprec *mp;
    int speckey = FALSE;
    static ITBUFF *mappedchars = NULL;

    /* still some pushback left? */
    mapgetc_raw_flag = raw;
    c = mapgetc();

    if ((c & YESREMAP) == 0 && (!remap || (c & NOREMAP)))
	return (c & ~REMAPFLAGS);

    c &= ~REMAPFLAGS;

    if (reading_msg_line)
    	mp = 0;
    else if (insertmode)
    	mp = map_insert;
    else
    	mp = map_command;

    /* if we got a function key from the lower layers, turn it into '#c'
    	and see if the user remapped that */
    if (c & SPEC) {
	mapungetc(kcod2key(c));
	c = poundc;
	speckey = TRUE;
    }

    do {
	(void)itb_init(&mappedchars, abortc);

	matched = maplookup(c, &mappedchars, mp, mapgetc, mapped_c_avail, mapped_c_start, TRUE);


	while(itb_more(mappedchars))
	    mapungetc(itb_next(mappedchars));

	/* if the user has not mapped '#c', we return the wide code we got
	    in the first place.  unless they wanted it quoted.  then we
	    leave it as is */
	if (!raw && speckey && !matched) {
	    c = mapgetc() & ~REMAPFLAGS;
	    if (c != poundc)
		    dbgwrite("BUG: # problem in mapped_c");
	    return (mapgetc() & ~REMAPFLAGS) | SPEC;
	}

	c = mapgetc();

	if (!global_g_val(GMDREMAPFIRST))
		matched = FALSE;

	speckey = FALSE;

    } while (matched &&
    	((remap && !(c & NOREMAP)) || (c & YESREMAP)) );

    return c & ~REMAPFLAGS;

}

static int abbr_curr_off;
static int abbr_search_lim;

static int
abbr_getc(void)
{
    if (abbr_curr_off <= abbr_search_lim)
    	return -1; /* won't match anything in the tree */
    return lgetc(DOT.l, --abbr_curr_off) & 0xff;
}

static int
abbr_c_avail(void)
{
    return TRUE;
}

static int
abbr_c_start(void)
{
    if (abbr_curr_off > abbr_search_lim) {
	/* we need to check the char in front of the match.
	   if it's a similar type to the first char of the match,
	   i.e. both idents, or both non-idents, we do nothing.
	   if whitespace precedes either ident or non-ident, the
	   match is good.
	 */
	char first, prev;
	first = lgetc(DOT.l,abbr_curr_off);
	prev = lgetc(DOT.l,abbr_curr_off-1);
	if ((isident(first) && isident(prev)) ||
	    (!isident(first) && !(isident(prev) || isSpace(prev)))) {
		return FALSE;
	}
    }
    return TRUE;
}

void
abbr_check(int *backsp_limit_p)
{
    int matched;
    ITBUFF *abbr_chars = NULL;
    int status = TRUE;

    if (llength(DOT.l) < 1)
    	return;
    abbr_curr_off = DOT.o;
    abbr_search_lim = *backsp_limit_p;
    (void)itb_init(&abbr_chars, abortc);
    matched = maplookup(abbr_getc(), &abbr_chars, abbr_map,
    	abbr_getc, abbr_c_avail, abbr_c_start, FALSE);


    if (matched) {
	    /* there are still some conditions that have to be met by
	       the preceding chars, if any */
	    if (!abbr_c_start()) {
		itb_free(&abbr_chars);
	    	return;
	    }
	    DOT.o -= matched;
	    ldelete((B_COUNT)matched, FALSE);
	    while(status && itb_more(abbr_chars))
		status = inschar(itb_last(abbr_chars), backsp_limit_p);
    }
    itb_free(&abbr_chars);
    return;
}

/* do map tranlations.
	C is first char to begin mapping on
	OUTP is an ITBUFF in which to put the result
	MP is the map in which to look
	GET is a routine to use to get the next character
	AVAIL is the routine that tells if GET can return something now
  returns number of characters matched
*/
static int
maplookup(
    int c,
    ITBUFF **outp,
    struct maprec *mp,
    GetFunc get,
    AvailFunc avail,
    StartFunc start,
    int suffix)
{
    struct maprec *rmp = NULL;
    ITBUFF *unmatched = 0;
    int matchedcnt;
    int use_sys_timing;
    int had_start = FALSE;
    register int count = 0;	/* index into 'unmatched[]' */

    /*
     * we don't want to delay for a user-specified :map!  starting with
     * poundc since it's likely that the mapping is happening on behalf of
     * a function key.  (it's so the user can ":map! #1 foobar" but still be
     * able to insert a '#' character normally.)  if they've changed poundc
     * so it's not something one normally inserts, then it's okay to delay
     * on it.
     */
    use_sys_timing = (insertmode && c == poundc &&
    				(isPrint(poundc) || isSpace(poundc)));

    unmatched = itb_init(&unmatched, 0);
    itb_append(&unmatched, c);
    count++;

    matchedcnt = 0;
    while (mp != 0) {
	if (c == mp->ch) {
	    if (mp->irv != -1 || mp->srv != NULL) {
		rmp = mp;
		matchedcnt += count;
		unmatched = itb_init(&unmatched, 0);
		count = 0;

		/* our code supports matching the longer of two maps one of
		 * which is a subset of the other.  vi matches the shorter
		 * one.
		 */
	        if ((*start)()) {
		    had_start = TRUE;
		    if (!global_g_val(GMDMAPLONGER)) {
			break;
		    }
		}
	    }

	    mp = mp->dlink;

	    if (!mp)
		break;

	    /* if there's no recorded input, and no user typeahead */
	    if (!(*avail)()) {

		/* give it a little extra time... */
		int timer = 0;

		/* we want to use the longer of the two timers */

		/* get the user timer.  it may be zero */
		if (!use_sys_timing && (mp->flags & MAPF_USERTIMER) != 0)
			timer = global_g_val(GVAL_TIMEOUTUSERVAL);

		/* if there was no user timer, or this is a system
			sequence, use the system timer if it's bigger */
		if (timer == 0 || (mp->flags & MAPF_SYSTIMER) != 0) {
			if (timer < global_g_val(GVAL_TIMEOUTVAL))
				timer = global_g_val(GVAL_TIMEOUTVAL);
		}

		catnap(timer,TRUE);

		if (!(*avail)())
		    break;
	    }

	    if ((c = (*get)()) < 0)
		break;

	    itb_append(&unmatched, c & ~REMAPFLAGS);
	    count++;

	}
	else
	    mp = mp->flink;
    }

    if (had_start && (rmp != 0)) {
	/* unget the unmatched suffix */
	while (suffix && (count > 0))
	    (void)itb_append(outp, itb_values(unmatched)[--count]);
	/* unget the mapping and elide correct number of recorded chars */
	if (rmp->srv) {
	    UINT remapflag;
	    char *cp;
	    /* cp = rmp->srv + cnt; */
	    for (cp = rmp->srv; *cp; cp++)
	    	;
	    if (rmp->flags & MAPF_NOREMAP)
		remapflag = NOREMAP;
	    else
		remapflag = 0;
	    while (cp > rmp->srv)
		(void)itb_append(outp, char2int(*--cp)|remapflag);
	}
	else {
	    (void)itb_append(outp, rmp->irv);
	}
    }
    else {	/* didn't find a match */
	while (count > 0)
	    (void)itb_append(outp, itb_values(unmatched)[--count]);
	matchedcnt = 0;
    }
    itb_free(&unmatched);
    return matchedcnt;
}

#if NO_LEAKS
static void
free_maprec(struct maprec **p)
{
	struct	maprec *q;
	if ((q = *p) != 0) {
		free_maprec(&(q->flink));
		free_maprec(&(q->dlink));
		FreeAndNull(q->srv);
		*p = 0;
		free((char *)q);
	}
}

void
map_leaks(void)
{
	free_maprec(&map_command);
	free_maprec(&map_insert);
	free_maprec(&map_syskey);
	free_maprec(&abbr_map);
}
#endif	/* NO_LEAKS */
