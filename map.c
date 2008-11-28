/*
 * map.c	-- map and map! interface routines
 *	Original interface by Otto Lind, 6/3/93
 *	Additional map and map! support by Kevin Buettner, 9/17/94
 *
 * $Header: /users/source/archives/vile.vcs/RCS/map.c,v 1.116 2008/11/27 14:03:17 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"

#if DISP_NTWIN
# define NTWIN_CURSOR_MINWAIT 550	/* msec */
#endif

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

#define MAPREC struct maprec
MAPREC {
    int ch;
    /* character to match */
    UINT flags;
    /* flags word */
    MAPREC *dlink;
    /* Where to go to match the next character in a multi-character sequence.
     */
    MAPREC *flink;
    /* Where to try next if match against current character is unsuccessful.
     */
    int irv;
    /* system defined mapping:  The (wide) character code to replace a matched
     * sequence by.
     */
    char *srv;
    /* user defined mapping:  This is the string to replace a matched sequence
     * by.
     */
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

typedef int (*AvailFunc) (void);
typedef int (*GetFunc) (void);
typedef int (*StartFunc) (void);

#if !OPT_SHOW_MAPS
#define relist_mappings(name)
#endif

#define NUMKEYSTR (KBLOCK / 2)

#ifdef DOKEYLOG
int do_keylog = 1;
#endif

#if OPT_SHOW_MAPS
#define MAPS_PREFIX 12

/*ARGSUSED*/
static void
makemaplist(int dummy GCC_UNUSED, void *mapp)
{
    TBUFF *lhsstr = 0;
    struct maprec **lhsstack;
    struct maprec *mp = (struct maprec *) mapp;
    int footnote = 0;
    size_t depth = 0;
    size_t maxdepth;
    size_t i;
    char *the_lhs_string;

    lhsstr = tb_init(&lhsstr, 0);

    lhsstack = typeallocn(struct maprec *, maxdepth = NSTRING);
    if (lhsstack == 0) {
	no_memory("makemaplist");
	return;
    }

    for_ever {
	if (mp) {
	    const char *remapnote;
	    char *mapstr;
	    char esc_seq[NSTRING];

	    tb_put(&lhsstr, depth, mp->ch);
	    if (depth + 1 >= maxdepth) {
		maxdepth *= 2;
		lhsstack = typereallocn(struct maprec *, lhsstack, maxdepth);
		if (lhsstack == 0) {
		    no_memory("makemaplist");
		    return;
		}
	    }
	    lhsstack[depth++] = mp->flink;

	    mapstr = (char *) 0;
	    if (mp->srv) {
		mapstr = mp->srv;
	    } else if (mp->irv != -1) {
		(void) kcod2escape_seq(mp->irv, esc_seq, sizeof(esc_seq));
		mapstr = esc_seq;
	    }
	    if (mapstr && (the_lhs_string = tb_values(lhsstr)) != 0) {
		if (mapp && (struct maprec *) mapp == abbr_map) {
		    /* the abbr map is stored inverted */
		    if (the_lhs_string != 0)
			for (i = depth; i != 0;)
			    bputc(the_lhs_string[--i]);
		} else {
		    if (mp->flags & MAPF_NOREMAP) {
			remapnote = "(n)";
			footnote++;
		    } else {
			remapnote = "   ";
		    }
		    bprintf("%s ", remapnote);
		    bputsn_xcolor(the_lhs_string, depth, XCOLOR_STRING);
		}
		bputc('\t');
		bputsn_xcolor(mapstr, -1, XCOLOR_STRING);
		bputc('\n');
	    }
	    mp = mp->dlink;
	} else if (depth != 0)
	    mp = lhsstack[--depth];
	else
	    break;
    }
    if (footnote) {
	bprintf("[(n) means never remap]\n");
    }
    tb_free(&lhsstr);
    free((char *) lhsstack);
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
    return liststuff(bname, FALSE, makemaplist, 0, (void *) mp);
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
relist_mappings(const char *bufname)
{
    update_scratch(bufname, show_Mappings);
}
#endif /* OPT_UPBUFF */

#endif /* OPT_SHOW_MAPS */

static int
addtomap(struct maprec **mpp,
	 const char *ks,
	 int kslen,
	 UINT flags,
	 int irv,
	 char *srv)
{
    int status = TRUE;
    struct maprec *mp = NULL;

    if (ks != 0 && kslen != 0) {

	TRACE2(("addtomap %s\n", visible_buff(ks, kslen, FALSE)));
	while (*mpp && kslen) {
	    mp = *mpp;
	    mp->flags |= flags;
	    if (CharOf(*ks) == mp->ch) {
		mpp = &mp->dlink;
		ks++;
		kslen--;
	    } else
		mpp = &mp->flink;
	}

	while (kslen) {
	    mp = typealloc(struct maprec);
	    if (mp == 0) {
		status = no_memory("addtomap");
		break;
	    }
	    *mpp = mp;
	    mp->dlink = mp->flink = NULL;
	    TRACE2(("...adding %#x\n", CharOf(*ks)));
	    mp->ch = CharOf(*ks++);
	    mp->srv = NULL;
	    mp->flags = flags;
	    mp->irv = -1;
	    mpp = &mp->dlink;
	    kslen--;
	}

	if (mp != 0) {
	    if (irv != -1)
		mp->irv = irv;
	    if (srv) {
		if (mp->srv)
		    free(mp->srv);
		mp->srv = strmalloc(srv);
		TRACE2(("...addtomap %s\n", visible_buff(srv, strlen(srv), FALSE)));
	    }
	    mp->flags = flags;
	}
    }
    return status;
}

static int
delfrommap(struct maprec **mpp, const char *ks, int length)
{
    struct maprec **save_m = mpp;
    struct maprec ***mstk = 0;
    int depth = 0;
    int pass;
    int n;

    if (ks == 0 || length == 0)
	return FALSE;

    for (pass = 0; pass < 2; pass++) {
	mpp = save_m;
	depth = 0;
	n = 0;
	while (*mpp && n < length) {
	    if (pass)
		mstk[depth] = mpp;
	    if ((*mpp)->ch == CharOf(ks[n])) {
		mpp = &(*mpp)->dlink;
		n++;
		depth++;
	    } else
		mpp = &(*mpp)->flink;
	}

	if (n < length)
	    return FALSE;	/* not in map */
	if (!pass) {
	    mstk = typecallocn(struct maprec **, depth + 1);
	    if (mstk == 0)
		return no_memory("delfrommap");
	}
    }

    depth--;
    if ((*mstk[depth])->srv) {
	free((*mstk[depth])->srv);
	(*mstk[depth])->srv = NULL;
    } else if ((*mstk[depth])->irv != -1) {
	(*mstk[depth])->irv = -1;
    } else {
	free((char *) mstk);
	return FALSE;
    }

    for (; depth >= 0; depth--) {
	struct maprec *mp = *mstk[depth];
	if (mp->irv == -1 && mp->dlink == NULL && mp->srv == NULL) {
	    *mstk[depth] = mp->flink;
	    if (depth > 0 && (*mstk[depth - 1])->dlink == mp)
		(*mstk[depth - 1])->dlink = NULL;
	    free((char *) mp);
	} else
	    break;
    }
    free((char *) mstk);
    return TRUE;
}

/* do map translations.
 *	C is first char to begin mapping on
 *	OUTP is an ITBUFF in which to put the result
 *	MP is the map in which to look
 *	GET is a routine to use to get the next character
 *	AVAIL is the routine that tells if GET can return something now
 * returns number of characters matched
 */
static int
maplookup(int c,
	  ITBUFF ** outp,
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
     * We don't want to delay for a user-specified :map!  starting with
     * poundc since it's likely that the mapping is happening on behalf of
     * a function key.  (It's so the user can ":map! #1 foobar" but still be
     * able to insert a '#' character normally.)  If they've changed poundc
     * so it's not something one normally inserts, then it's okay to delay
     * on it.
     */
    use_sys_timing = (insertmode && c == poundc &&
		      (isPrint(poundc) || isSpace(poundc)));

    unmatched = itb_init(&unmatched, 0);
    itb_append(&unmatched, c);
    count++;

    TRACE2(("maplookup unmatched:%s\n", itb_visible(unmatched)));
    matchedcnt = 0;
    while (mp != 0) {
	TRACE2(("... c=%#x, mp->ch=%#x\n", c, mp->ch));
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
		if ((*start) ()) {
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
	    if (!(*avail) ()) {

		/* give it a little extra time... */
		int timer = 0;
#if DISP_NTWIN
		int cursor_state = 0;
#endif

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
#if DISP_NTWIN
		if (timer > NTWIN_CURSOR_MINWAIT) {
		    /*
		     * editor waits for a potentially long time for the next
		     * character, during which the cursor will be invisible; so
		     * turn it back on.  unfortunately, the cursor won't blink
		     * because there's no thread driving the message pump.
		     */
		    cursor_state = winvile_cursor_state(TRUE, FALSE);
		}
#endif
		catnap(timer, TRUE);
#if DISP_NTWIN
		if (timer > NTWIN_CURSOR_MINWAIT)	/* restore cursor state */
		    (void) winvile_cursor_state(cursor_state, FALSE);
#endif

		if (!(*avail) ())
		    break;
	    }

	    if ((c = (*get) ()) < 0)
		break;

	    itb_append(&unmatched, c & ~REMAPFLAGS);
	    count++;

	} else
	    mp = mp->flink;
    }

    if (itb_values(unmatched) == 0) {
	matchedcnt = 0;
	no_memory("maplookup");
    } else if (had_start && (rmp != 0)) {
	/* unget the unmatched suffix */
	while (suffix && (count > 0)) {
	    (void) itb_append(outp, itb_values(unmatched)[--count]);
	    TRACE2(("...1 ungetting %#x\n", itb_values(unmatched)[count]));
	}
	/* unget the mapping and elide correct number of recorded chars */
	if (rmp->srv) {
	    UINT remapflag;
	    char *cp;
	    /* cp = rmp->srv + cnt; */
	    for (cp = rmp->srv; *cp; cp++) ;
	    if (rmp->flags & MAPF_NOREMAP)
		remapflag = NOREMAP;
	    else
		remapflag = 0;
	    while (cp > rmp->srv) {
		(void) itb_append(outp, CharOf(*--cp) | remapflag);
		TRACE2(("...2 ungetting %#x|%#x\n", CharOf(*cp), remapflag));
	    }
	} else {
	    (void) itb_append(outp, rmp->irv);
	    TRACE2(("...3 ungetting %#x\n", rmp->irv));
	}
    } else {			/* didn't find a match */
	while (count > 0) {
	    (void) itb_append(outp, itb_values(unmatched)[--count]);
	    TRACE2(("...4 ungetting %#x\n", itb_values(unmatched)[count]));
	}
	matchedcnt = 0;
    }
    itb_free(&unmatched);
    TRACE2(("...maplookup %d:%s\n", matchedcnt, itb_visible(*outp)));
    return matchedcnt;
}

static void
reverse_abbr(struct maprec **mpp, const char *bufname, TBUFF *kbuf)
{
    if ((*mpp && *mpp == abbr_map) || (strcmp(bufname, ABBR_BufName) == 0)) {
	/* reverse the lhs */
	int i;
	char t;
	int len = tb_length0(kbuf);
	char *s = tb_values(kbuf);
	for (i = 0; i < len / 2; i++) {
	    t = s[len - i - 1];
	    s[len - i - 1] = s[i];
	    s[i] = t;
	}
    }
}

static int
map_common(struct maprec **mpp, const char *bufname, UINT remapflag)
{
    int status;
    static TBUFF *kbuf;
    static TBUFF *val;

#if OPT_SHOW_MAPS
    if (end_named_cmd()) {
	return show_mapped_chars(bufname);
    }
#endif
    tb_scopy(&kbuf, "");
    status = kbd_reply("change this string: ", &kbuf, eol_history,
		       ' ', KBD_NOMAP | KBD_NOEVAL, no_completion);
    if (status != TRUE)
	return status;

    hst_glue(' ');
    tb_scopy(&val, "");
    if (!clexec) {
	status = kbd_reply("to this new string: ", &val, eol_history,
			   '\n', KBD_NOMAP, no_completion);
    } else {
	(void) mac_literalarg(&val);	/* consume to end of line */
	status = tb_length(val) > 1;
    }
    if (status != TRUE)
	return status;

    reverse_abbr(mpp, bufname, kbuf);

    status = addtomap(mpp, tb_values(kbuf), tb_length0(kbuf),
		      MAPF_USERTIMER | remapflag, -1, tb_values(val));
    relist_mappings(bufname);
    return status;
}

static int
unmap_common(struct maprec **mpp, const char *bufname)
{
    int status;
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

    reverse_abbr(mpp, bufname, kbuf);

    if (delfrommap(mpp, tb_values(kbuf), tb_length0(kbuf)) != TRUE) {
	mlforce("[Sequence not mapped]");
	return FALSE;
    }
    relist_mappings(bufname);
    return TRUE;
}

/*
 * set a map for the character/string combo
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

/*
 * remove map entry, restore saved CMDFUNC for key
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

/* addtosysmap is used to initialize the system default function key map
*/
void
addtosysmap(const char *seq, int seqlen, int code)
{
    addtomap(&map_syskey, seq, seqlen, MAPF_SYSTIMER, code, (char *) 0);
    relist_mappings(SYSMAP_BufName);
}

void
delfromsysmap(const char *seq, int seqlen)
{
    delfrommap(&map_syskey, seq, seqlen);
    relist_mappings(SYSMAP_BufName);
}

#define INPUT_FROM_TTGET 1
#define INPUT_FROM_MAPGETC 2

static ITBUFF *sysmappedchars = NULL;
static ITBUFF *mapgetc_ungottenchars = NULL;
static int mapgetc_ungotcnt = 0;

static void
save_keystroke(int c)
{
    KILL *kp;
    KILLREG *kr = &kbs[KEYST_KREG];

    beginDisplay();
#ifdef DOKEYLOG
    if (do_keylog) {
	static int keyfd = -1;
	static char *tfilenam;
	if (!tfilenam)
	    tfilenam = tempnam("/tmp/vilekeylogs", "vilek");
	if (tfilenam) {
	    if (keyfd < 0)
		keyfd = open(tfilenam, O_CREAT | O_WRONLY, 0600);
	    if (keyfd >= 0)
		write(keyfd, &c, 1);
	}
    }
#endif
    if (kr->kbufh == NULL) {
	kr->kbufh = typealloc(KILL);
	kr->kused = 0;
    }
    if (kr->kbufh != NULL) {

	kp = kr->kbufp = kr->kbufh;
	kp->d_next = NULL;

	kp->d_chunk[kr->kused++] = (UCHAR) c;
	if (kr->kused >= NUMKEYSTR * 2) {	/* time to dump the oldest half */
	    (void) memcpy((kp->d_chunk),
			  (&kp->d_chunk[NUMKEYSTR / 2]),
			  NUMKEYSTR / 2);
	    kr->kused = NUMKEYSTR / 2;
	}
    }
    endofDisplay();
}

/* These two wrappers are provided because at least one pcc-based compiler
 * balks at passing term.getch or term.typahead as a function pointer.
 */
static int
normal_getc(void)
{
    int c = term.getch();
    TRACE(("normal/getc:%c (%#x)\n", c, c));
    save_keystroke(c);
    return c;
}

static int
normal_typeahead(void)
{
    return (term.typahead());
}

static int
normal_start(void)
{
    return TRUE;
}

int
sysmapped_c(void)
{
    int c;

    /* still some left? */
    if (itb_more(sysmappedchars))
	return itb_last(sysmappedchars);

    if ((c = term.getch()) == -1)
	return c;		/* see comment in ttgetc */

    TRACE(("mapped/getc:%c (%#x)\n", c, c));

    save_keystroke(c);

    if (suppress_sysmap)
	return c;

    /* will push back on sysmappedchars successful, or not */
    (void) maplookup(c, &sysmappedchars, map_syskey,
		     normal_getc, normal_typeahead, normal_start, TRUE);

    return itb_last(sysmappedchars);
}

int
sysmapped_c_avail(void)
{
    return itb_more(sysmappedchars) || (!mapgetc_ungotcnt && term.typahead());
}

/*
 * Make the assumption that no input will magically appear (un)available to
 * tgetc in between a mapungetc and the next mapgetc.  Hence characters can't
 * be ungotten onto the wrong buffer (exception is the last tgot char might be
 * mapungot onto the map buffer.  This is OK (if assumption holds) because the
 * next character will be gotten from this buffer.
 */
void
mapungetc(int c)
{
    TRACE2(("mapungetc %#x\n", c));
    if (tgetc_avail()) {
	tungetc(c);
    } else {
	(void) itb_append(&mapgetc_ungottenchars, c);
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
	++infloopcount;
	if (infloopcount > global_g_val(GVAL_MAPLENGTH)
	    && infloopcount > 25) {
	    (void) itb_init(&mapgetc_ungottenchars, esc_c);
	    mapgetc_ungotcnt = 0;
	    mlforce("[Infinite loop detected in %s sequence]",
		    (insertmode) ? "map!" : "map");
	    catnap(1000, FALSE);	/* FIXME: be sure message gets out */
	    return esc_c | NOREMAP;
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

void
map_drain(void)
{
    int ch;

    while (mapped_ungotc_avail()) {
	ch = mapgetc();
	TRACE(("map_drain...\n"));
	(void) ch;
    }
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
    if ((c & SPEC)
#if OPT_KEY_MODIFY
	&& !(c & mod_KEY)	/* we don't do this special case */
#endif
	) {
	mapungetc(kcod2key(c));
	c = poundc;
	speckey = TRUE;
    }

    do {
	(void) itb_init(&mappedchars, esc_c);

	matched = maplookup(c,
			    &mappedchars,
			    mp,
			    mapgetc,
			    mapped_c_avail,
			    mapped_c_start,
			    TRUE);

	while (itb_more(mappedchars)) {
	    mapungetc(itb_next(mappedchars));
	}

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
	     ((remap && !(c & NOREMAP)) || (c & YESREMAP)));

    return c & ~REMAPFLAGS;

}

static int abbr_curr_off;
static int abbr_search_lim;

static int
abbr_getc(void)
{
    if (abbr_curr_off <= abbr_search_lim)
	return -1;		/* won't match anything in the tree */
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
	/*
	 * We need to check the char in front of the match.  If it's a similar
	 * type to the first char of the match, i.e. both idents, or both
	 * non-idents, we do nothing.  If whitespace precedes either ident or
	 * non-ident, the match is good.
	 */
	int first, prev;
	first = lgetc(DOT.l, abbr_curr_off);
	prev = lgetc(DOT.l, abbr_curr_off - 1);
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
    (void) itb_init(&abbr_chars, esc_c);
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
	ldel_bytes((B_COUNT) matched, FALSE);
	while (status && itb_more(abbr_chars))
	    status = inschar(itb_last(abbr_chars), backsp_limit_p);
    }
    itb_free(&abbr_chars);
    return;
}

#if NO_LEAKS
static void
free_maprec(struct maprec **p)
{
    struct maprec *q;
    if ((q = *p) != 0) {
	free_maprec(&(q->flink));
	free_maprec(&(q->dlink));
	FreeAndNull(q->srv);
	*p = 0;
	free((char *) q);
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
#endif /* NO_LEAKS */
