/*	This file is for functions having to do with key bindings,
 *	descriptions, help commands and startup file.
 *
 *	written 11-feb-86 by Daniel Lawrence
 *
 * $Header: /users/source/archives/vile.vcs/RCS/bind.c,v 1.239 2001/02/15 22:41:54 tom Exp $
 *
 */

#ifdef _WIN32
# include <windows.h>
#endif

#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"
#include	"nefsms.h"

#define BI_DATA NBST_DATA
#include	"btree.h"

#define SHORT_CMD_LEN 4		/* command names longer than this are preferred
				   over those shorter.  e.g. display "quit"
				   instead of "q" if possible */

extern const int nametblsize;

#if OPT_CASELESS
#define Strcmp(s,d)      cs_strcmp(case_insensitive, s, d)
#define StrNcmp(s,d,len) cs_strncmp(case_insensitive, s, d, len)
#else
#define Strcmp(s,d)      strcmp(s, d)
#define StrNcmp(s,d,len) strncmp(s, d, len)
#endif

#if OPT_REBIND
#define	isSpecialCmd(k) \
		( (k == &f_cntl_a_func)\
		||(k == &f_cntl_x_func)\
		||(k == &f_poundc_func)\
		||(k == &f_reptc_func)\
		||(k == &f_esc_func)\
		)

static int key_to_bind(const CMDFUNC * kcmd);
static int update_binding_list(BUFFER *bp);
static void makebindlist(LIST_ARGS);
#endif /* OPT_REBIND */

#if OPT_NAMEBST
static int kbd_complete_bst(unsigned flags, int c, char *buf, unsigned *pos);
#else
#define kbd_complete_bst(flags, c, buf, pos) \
	kbd_complete(flags, c, buf, pos, \
			(const char *)&nametbl[0], sizeof(nametbl[0]))
#endif /* OPT_NAMEBST */

#if OPT_EVAL || OPT_REBIND
static const NTAB *fnc2ntab(const CMDFUNC * cfp);
static int prc2kcod(const char *kk);
#endif

BINDINGS dft_bindings =
{
    asciitbl
    ,kbindtbl
#if OPT_REBIND
    ,kbindtbl
#endif
};

static const CMDFUNC *ins_table[N_chars];
BINDINGS ins_bindings =
{
    ins_table
    ,kbindtbl
#if OPT_REBIND
    ,kbindtbl
#endif
};

static const CMDFUNC *cmd_table[N_chars];
BINDINGS cmd_bindings =
{
    cmd_table
    ,kbindtbl
#if OPT_REBIND
    ,kbindtbl
#endif
};

static const CMDFUNC *sel_table[N_chars];
BINDINGS sel_bindings =
{
    sel_table
    ,kbindtbl
#if OPT_REBIND
    ,kbindtbl
#endif
};

#if OPT_REBIND
static BINDINGS *bindings_to_describe = &dft_bindings;
#endif

static void kbd_puts(const char *s);

/*----------------------------------------------------------------------------*/

#if OPT_NAMEBST
static BI_NODE *
new_namebst(BI_DATA * a)
{
    BI_NODE *p = typecalloc(BI_NODE);
    p->value = *a;
    if (!(a->n_flags & NBST_READONLY))
	BI_KEY(p) = strmalloc(a->bi_key);
    return p;
}

static void
old_namebst(BI_NODE * a)
{
    if (!(a->value.n_flags & NBST_READONLY)) {
	CMDFUNC *cmd = TYPECAST(CMDFUNC, a->value.n_cmd);
	if (cmd != 0) {
#if OPT_ONLINEHELP
	    if (cmd->c_help)
		free(TYPECAST(char, cmd->c_help));
#endif
#if OPT_MACRO_ARGS
	    if (cmd->c_args)
		free(cmd->c_args);
#endif
	    free(cmd);
	}
	free(TYPECAST(char, BI_KEY(a)));
    }
    free(TYPECAST(char, a));
}

static void
dpy_namebst(BI_NODE * a GCC_UNUSED, int level GCC_UNUSED)
{
#if OPT_TRACE
    while (level-- > 0)
	TRACE((". "));
    TRACE(("%p -> %s (%d)\n", a, BI_KEY(a), a->balance));
#endif
}

#define BI_DATA0 {{0}, 0, {0,0,0}}
#define BI_TREE0 0, 0, BI_DATA0
static BI_TREE namebst =
{new_namebst, old_namebst, dpy_namebst, BI_TREE0};
static BI_TREE redefns =
{new_namebst, old_namebst, dpy_namebst, BI_TREE0};
#endif /* OPT_NAMEBST */

/*----------------------------------------------------------------------------*/

int
no_such_function(const char *fnp)
{
    mlforce("[No such function \"%s\"]", fnp != 0 ? fnp : "");
    return FALSE;
}

/* give me some help!!!! bring up a buffer and read the help file into it */
/* ARGSUSED */
int
vl_help(int f GCC_UNUSED, int n GCC_UNUSED)
{
    BUFFER *bp;			/* buffer pointer to help */
    char *hname;
    int alreadypopped;

    /* first check if we are already here */
    bp = bfind(HELP_BufName, BFSCRTCH);
    if (bp == NULL)
	return FALSE;

    if (bp->b_active == FALSE) {	/* never been used */
	hname = cfg_locate(helpfile, FL_ANYWHERE | FL_READABLE);
	if (!hname) {
	    mlforce("[Sorry, can't find the help information]");
	    (void) zotbuf(bp);
	    return (FALSE);
	}
	alreadypopped = (bp->b_nwnd != 0);
	/* and read the stuff in */
	if (readin(hname, 0, bp, TRUE) == FALSE ||
	    popupbuff(bp) == FALSE) {
	    (void) zotbuf(bp);
	    return (FALSE);
	}
	set_bname(bp, HELP_BufName);
	set_rdonly(bp, hname, MDVIEW);

	make_local_b_val(bp, MDIGNCASE);	/* easy to search, */
	set_b_val(bp, MDIGNCASE, TRUE);
	b_set_scratch(bp);
	b_set_recentlychanged(bp);
	if (!alreadypopped)
	    shrinkwrap();
    }

    if (!swbuffer(bp))
	return FALSE;

    if (help_at >= 0) {
	if (!gotoline(TRUE, help_at))
	    return FALSE;
	mlwrite("[Type '1G' to return to start of help information]");
	help_at = -1;		/* until zotbuf is called, we let normal
				   DOT tracking keep our position */
    }
    return TRUE;
}

/*
 * translate a 10-bit key-binding to the table-pointer
 */
static KBIND *
kcode2kbind(BINDINGS * bs, int code)
{
    KBIND *kbp;

#if OPT_REBIND
    for (kbp = bs->kb_extra; kbp != bs->kb_special; kbp = kbp->k_link) {
	if (kbp->k_code == code)
	    return kbp;
    }
#endif
    for (kbp = bs->kb_special; kbp->k_cmd; kbp++) {
	if (kbp->k_code == code)
	    return kbp;
    }
    return 0;
}

#if OPT_REBIND

#if OPT_TERMCHRS

	/* NOTE: this table and the corresponding initializations should be
	 * generated by 'mktbls'.  In any case, the table must be sorted to use
	 * name-completion on it.
	 */
static const struct {
    const char *name;
    int *value;
    char how_to;
} TermChrs[] = {
		/* *INDENT-OFF* */
		{"backspace",		&backspc,	's'},
		{"interrupt",		&intrc,		's'},
		{"line-kill",		&killc,		's'},
		{"mini-edit",		&editc,		's'},
		{"name-complete",	&name_cmpl,	0},
		{"quote-next",		&quotec,	0},
		{"start-output",	&startc,	's'},
		{"stop-output",		&stopc,		's'},
		{"suspend",		&suspc,		's'},
		{"test-completions",	&test_cmpl,	0},
		{"word-kill",		&wkillc,	's'},
		{0,                     0,              0}
		/* *INDENT-ON* */

};

/*----------------------------------------------------------------------------*/

/* list the current chrs into the current buffer */
/* ARGSUSED */
static void
makechrslist(int dum1 GCC_UNUSED, void *ptr GCC_UNUSED)
{
    int i;
    char temp[NLINE];

    bprintf("--- Terminal Character Settings %*P\n", term.cols - 1, '-');
    for (i = 0; TermChrs[i].name != 0; i++) {
	bprintf("\n%s = %s",
		TermChrs[i].name,
		kcod2prc(*(TermChrs[i].value), temp));
    }
}

/*
 * Find a special-character definition, given the name
 */
static int
chr_lookup(const char *name)
{
    int j;
    for (j = 0; TermChrs[j].name != 0; j++)
	if (!strcmp(name, TermChrs[j].name))
	    return j;
    return -1;
}

/*
 * The 'chr_complete()' and 'chr_eol()' functions are invoked from
 * 'kbd_reply()' to setup the mode-name completion and query displays.
 */
static int
chr_complete(int c, char *buf, unsigned *pos)
{
    return kbd_complete(0, c, buf, pos, (const char *) &TermChrs[0],
			sizeof(TermChrs[0]));
}

static int
/*ARGSUSED*/
chr_eol(
	   const char *buffer GCC_UNUSED,
	   unsigned cpos GCC_UNUSED,
	   int c GCC_UNUSED,
	   int eolchar GCC_UNUSED)
{
    return isSpace(c);
}

#if OPT_UPBUFF
/* ARGSUSED */
static int
update_termchrs(BUFFER *bp GCC_UNUSED)
{
    return show_termchrs(FALSE, 1);
}
#endif

/* ARGSUSED */
int
set_termchrs(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int s, j;
    static TBUFF *name;
    int c;

    /* get the table-entry */
    tb_scopy(&name, "");
    if ((s = kbd_reply("Terminal setting: ", &name, chr_eol,
		       ' ', 0, chr_complete)) == TRUE) {

	j = chr_lookup(tb_values(name));
	switch (TermChrs[j].how_to) {
	case 's':
	default:
	    c = key_to_bind((CMDFUNC *) 0);
	    if (c < 0)
		return (FALSE);
	    *(TermChrs[j].value) = c;
	    break;
	}
	update_scratch(TERMINALCHARS_BufName, update_termchrs);
    }
    return s;
}

/* ARGSUSED */
int
show_termchrs(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return liststuff(TERMINALCHARS_BufName, FALSE, makechrslist, 0, (void *) 0);
}
#endif /* OPT_TERMCHRS */

static int
mac_token2kcod(int check)
{
    static TBUFF *tok;
    char *value;
    int c;

    if ((value = mac_tokval(&tok)) == 0) {
	c = -1;
    } else {
	c = prc2kcod(value);

	if (c < 0 && check)
	    mlforce("[Illegal key-sequence \"%s\"]", value);
    }

    return c;
}

/*
 * Prompt-for and return the key-code to bind.
 */
static int
key_to_bind(const CMDFUNC * kcmd)
{
    char outseq[NLINE];		/* output buffer for keystroke sequence */
    int c;

    mlprompt("...to keyboard sequence (type it exactly): ");

    /* if running a command line, get a token rather than keystrokes */
    if (clexec) {
	c = mac_token2kcod(FALSE);
    } else {
	/* perhaps we only want a single key, not a sequence */
	/*      (see more comments below) */
	if (isSpecialCmd(kcmd))
	    c = keystroke();
	else
	    c = kbd_seq();
    }

    if (c >= 0) {
	/* change it to something we can print as well */
	outseq[0] = EOS;
	if (vl_msgs)
	    kbd_puts(kcod2prc(c, outseq));
	hst_append_s(outseq, FALSE);
    } else {
	mlforce("[Not a proper key-sequence]");
    }
    return c;
}

static int
unbindchar(BINDINGS * bs, int c)
{
    KBIND *kbp;			/* pointer into the command table */
    KBIND *skbp;		/* saved pointer into the command table */

    /* if it's a simple character, it will be in the normal[] array */
    if (!isSpecial(c)) {
	if (bs->kb_normal[c]) {
	    bs->kb_normal[c] = 0;
	    return TRUE;
	}
	return FALSE;
    }

    /* check first entry in kb_extra table */
    kbp = skbp = bs->kb_extra;
    if (kbp->k_code == c) {
	bs->kb_extra = kbp->k_link;
	free(TYPECAST(char, kbp));
	return TRUE;
    }

    /* check kb_extra codes */
    while (kbp != bs->kb_special) {
	if (kbp->k_code == c) {
	    /* relink previous */
	    skbp->k_link = kbp->k_link;
	    free(TYPECAST(char, kbp));
	    return TRUE;
	}

	skbp = kbp;
	kbp = kbp->k_link;
    }

    /* nope, check special codes */
    for (skbp = 0; kbp->k_cmd; kbp++) {
	if (!skbp && kbp->k_code == c)
	    skbp = kbp;
    }

    /* not found */
    if (!skbp)
	return FALSE;

    --kbp;			/* backup to the last legit entry */
    if (skbp != kbp) {
	/* copy the last entry to the current one */
	skbp->k_code = kbp->k_code;
	skbp->k_cmd = kbp->k_cmd;
    }

    /* null out the last one */
    kbp->k_code = 0;
    kbp->k_cmd = 0;

    return TRUE;
}

/*
 * removes a key from the table of bindings
 */
static int
unbind_any_key(BINDINGS * bs)
{
    int c;			/* command key to unbind */
    char outseq[NLINE];		/* output buffer for keystroke sequence */

    /* prompt the user to type in a key to unbind */
    mlprompt("Unbind this key sequence: ");

    /* get the command sequence to unbind */
    if (clexec) {
	if ((c = mac_token2kcod(TRUE)) < 0)
	    return FALSE;
    } else {
	c = kbd_seq();
	if (c < 0) {
	    mlforce("[Not a bindable key-sequence]");
	    return (FALSE);
	}
    }

    if (vl_msgs)
	kbd_puts(kcod2prc(c, outseq));

    if (unbindchar(bs, c) == FALSE) {
	mlforce("[Key not bound]");
	return (FALSE);
    }
    update_scratch(BINDINGLIST_BufName, update_binding_list);
    return (TRUE);
}

/* ARGSUSED */
int
unbindkey(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return unbind_any_key(&dft_bindings);
}

/* ARGSUSED */
int
unbind_i_key(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return unbind_any_key(&ins_bindings);
}

/* ARGSUSED */
int
unbind_c_key(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return unbind_any_key(&cmd_bindings);
}

/* ARGSUSED */
int
unbind_s_key(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return unbind_any_key(&sel_bindings);
}

/*
 * Prefix-keys can be only bound to one value. This procedure tests the
 * argument 'kcmd' to see if it is a prefix key, and if so, unbinds the
 * key, and sets the corresponding global variable to the new value.
 * The calling procedure will then do the binding per se.
 */
static void
reset_prefix(int c, const CMDFUNC * kcmd, BINDINGS * bs)
{
    if (isSpecialCmd(kcmd)) {
	int j;
	/* search for an existing binding for the prefix key */
	if ((j = fnc2kcod(kcmd)) >= 0)
	    (void) unbindchar(bs, j);
	/* reset the appropriate global prefix variable */
	if (kcmd == &f_cntl_a_func)
	    cntl_a = c;
	if (kcmd == &f_cntl_x_func)
	    cntl_x = c;
	if (kcmd == &f_poundc_func)
	    poundc = c;
	if (kcmd == &f_reptc_func)
	    reptc = c;
	if (kcmd == &f_esc_func)
	    esc_c = c;
    }
}

/*
 * Bind a command-function pointer to a given key-code (saving the old
 * value of the function-pointer via an pointer given by the caller).
 */
static int
install_bind(int c, const CMDFUNC * kcmd, BINDINGS * bs)
{
    KBIND *kbp;			/* pointer into a binding table */

    if (c < 0)
	return FALSE;		/* not a legal key-code */

    /* if the function is a prefix key, i.e. we're changing the definition
       of a prefix key, then they typed a dummy function name, which
       has been translated into a dummy function pointer */
    reset_prefix(-1, kcod2fnc(bs, c), bs);
    reset_prefix(c, kcmd, bs);

    if (!isSpecial(c)) {
	bs->kb_normal[c] = TYPECAST(CMDFUNC, kcmd);
    } else if ((kbp = kcode2kbind(bs, c)) != 0) {	/* change it in place */
	kbp->k_cmd = kcmd;
    } else {
	if ((kbp = typealloc(KBIND)) == 0) {
	    return no_memory("Key-Binding");
	}
	kbp->k_link = bs->kb_extra;
	kbp->k_code = c;	/* add keycode */
	kbp->k_cmd = kcmd;	/* and func pointer */
	bs->kb_extra = kbp;
    }
    update_scratch(BINDINGLIST_BufName, update_binding_list);
    return (TRUE);
}

/* ARGSUSED */
static int
bind_any_key(BINDINGS * bs)
{
    const CMDFUNC *kcmd;
    char cmd[NLINE];
    char *fnp;

    /* prompt the user to type in a key to bind */
    /* and get the function name to bind it to */
    fnp = kbd_engl("Bind function whose full name is: ", cmd);

    if (fnp == NULL || (kcmd = engl2fnc(fnp)) == NULL) {
	return no_such_function(fnp);
    }

    return install_bind(key_to_bind(kcmd), kcmd, bs);
}

/*
 * bindkey:	add a new key to the key binding table
 */

/* ARGSUSED */
int
bindkey(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return bind_any_key(&dft_bindings);
}

/* ARGSUSED */
int
bind_i_key(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return bind_any_key(&ins_bindings);
}

/* ARGSUSED */
int
bind_c_key(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return bind_any_key(&cmd_bindings);
}

/* ARGSUSED */
int
bind_s_key(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return bind_any_key(&sel_bindings);
}

/* describe bindings bring up a fake buffer and list the key bindings
		   into it with view mode			*/

/* remember whether we last did "apropos" or "describe-bindings" */
static char *last_lookup_string;
static CMDFLAGS last_whichcmds;
static int append_to_binding_list;

/* ARGSUSED */
static int
update_binding_list(BUFFER *bp GCC_UNUSED)
{
    return liststuff(BINDINGLIST_BufName, append_to_binding_list,
		     makebindlist, (int) last_whichcmds, (void *) last_lookup_string);
}

static int
describe_any_bindings(char *lookup, int whichcmd)
{
    last_lookup_string = lookup;
    last_whichcmds = whichcmd;

    return update_binding_list((BUFFER *) 0);
}

/* ARGSUSED */
int
desbind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_any_bindings((char *) 0, 0);
}

static int
describe_alternate_bindings(BINDINGS * bs)
{
    int code;
    bindings_to_describe = bs;
    code = describe_any_bindings((char *) 0, 0);
    bindings_to_describe = &dft_bindings;
    return code;
}

int
des_i_bind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_alternate_bindings(&ins_bindings);
}

int
des_c_bind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_alternate_bindings(&cmd_bindings);
}

int
des_s_bind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_alternate_bindings(&sel_bindings);
}

/* ARGSUSED */
int
desmotions(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_any_bindings((char *) 0, MOTION);
}

/* ARGSUSED */
int
desopers(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_any_bindings((char *) 0, OPER);
}

/* lookup function by substring */
/* ARGSUSED */
int
dessubstr(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int s;
    static char substring[NSTRING];

    s = mlreply("Apropos string: ", substring, sizeof(substring));
    if (s != TRUE)
	return (s);

    return describe_any_bindings(substring, 0);
}

static char described_cmd[NLINE + 1];	/* string to match cmd names to */

/* lookup up function by name */
/* ARGSUSED */
int
desfunc(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int s;
    char *fnp;

    /* force an exact match by ourstrstr() later on from makefuncdesc() */
    described_cmd[0] = '^';

    fnp = kbd_engl("Describe function whose full name is: ",
		   described_cmd + 1);
    if (fnp == NULL || engl2fnc(fnp) == NULL) {
	s = no_such_function(fnp);
    } else {
	append_to_binding_list = TRUE;
	s = describe_any_bindings(described_cmd, 0);
	append_to_binding_list = FALSE;
    }
    return s;
}

/* lookup up function by key */
/* ARGSUSED */
static int
prompt_describe_key(BINDINGS * bs)
{
    int c;			/* character to describe */
    char outseq[NSTRING];	/* output buffer for command sequence */
    const NTAB *nptr;		/* name table pointer */
    int s;

    /* prompt the user to type us a key to describe */
    mlprompt("Describe the function bound to this key sequence: ");

    /* check to see if we are executing a command line */
    if (clexec) {
	if ((c = mac_token2kcod(TRUE)) < 0)
	    return (FALSE);
    } else {
	c = kbd_seq_nomap();
	if (c < 0) {
	    mlforce("[Not a bindable key-sequence]");
	    return (FALSE);
	}
    }

    (void) kcod2prc(c, outseq);
    hst_append_s(outseq, FALSE);	/* cannot replay this, but can see it */

    /* find the function bound to the key */
    if ((nptr = fnc2ntab(kcod2fnc(bs, c))) == NULL) {
	mlwrite("Key sequence '%s' is not bound to anything.",
		outseq);
	return TRUE;
    }

    /* describe it */
    described_cmd[0] = '^';
    (void) strcpy(described_cmd + 1, nptr->n_name);
    append_to_binding_list = TRUE;
    s = describe_any_bindings(described_cmd, 0);
    append_to_binding_list = FALSE;

    mlwrite("Key sequence '%s' is bound to function \"%s\"",
	    outseq, nptr->n_name);

    return s;
}

int
deskey(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return prompt_describe_key(&dft_bindings);
}

int
des_i_key(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return prompt_describe_key(&ins_bindings);
}

int
des_c_key(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return prompt_describe_key(&cmd_bindings);
}

int
des_s_key(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return prompt_describe_key(&sel_bindings);
}

/* returns a name in double-quotes */
static char *
quoted(char *dst, const char *src)
{
    return strcat(strcat(strcpy(dst, "\""), src), "\"");
}

/* returns the number of columns used by the given string */
static unsigned
converted_len(char *buffer)
{
    unsigned len = 0, c;
    while ((c = *buffer++) != EOS) {
	if (c == '\t')
	    len |= 7;
	len++;
    }
    return len;
}

/* force the buffer to a tab-stop if needed */
static char *
to_tabstop(char *buffer)
{
    unsigned cpos = converted_len(buffer);
    if (cpos & 7)
	(void) strcat(buffer, "\t");
    return skip_string(buffer);
}

static void
quote_and_pad(char *dst, const char *src)
{
    quoted(dst, src);
    if (converted_len(dst) >= 32)
	strcat(dst, "\t");
    else
	while (converted_len(dst) < 32)
	    strcat(dst, "\t");
}

/* convert a key binding, padding to the next multiple of 8 columns */
static void
convert_kcode(int c, char *buffer)
{
    (void) kcod2prc(c, to_tabstop(buffer));
}

static void
convert_cmdfunc(BINDINGS * bs, const CMDFUNC * cmd, char *outseq)
{
    int i;
    KBIND *kbp;

    /* look in the simple ascii binding table first */
    for (i = 0; i < N_chars; i++)
	if (bs->kb_normal[i] == cmd)
	    convert_kcode(i, outseq);

    /* then look in the multi-key table */
#if OPT_REBIND
    for (kbp = bs->kb_extra; kbp != bs->kb_special; kbp = kbp->k_link) {
	if (kbp->k_cmd == cmd)
	    convert_kcode(kbp->k_code, outseq);
    }
#endif
    for (kbp = bs->kb_special; kbp->k_cmd; kbp++)
	if (kbp->k_cmd == cmd)
	    convert_kcode(kbp->k_code, outseq);
}

#if OPT_ONLINEHELP
static int
show_onlinehelp(const CMDFUNC * cmd)
{
    char outseq[NLINE];		/* output buffer for text */
    const char *text = cmd->c_help;

    if (text && *text) {
	if (*text == SQUOTE)
	    text++;
	(void) lsprintf(outseq, "  (%s %s )",
			(cmd->c_flags & MOTION) ? " motion: " :
			(cmd->c_flags & OPER) ? " operator: " : "",
			text);
    } else {
	(void) lsprintf(outseq, "  ( no help for this command )");
    }
    if (!addline(curbp, outseq, -1))
	return FALSE;
    if (cmd->c_flags & GLOBOK) {
	if (!addline(curbp, "  (may follow global command)", -1))
	    return FALSE;
    }
#if OPT_MACRO_ARGS
    if (cmd->c_args != 0) {
	int i;
	for (i = 0; cmd->c_args[i].pi_type != PT_UNKNOWN; i++) {
	    (void) lsprintf(outseq, "  ( $%d = %s )", i + 1,
			    (cmd->c_args[i].pi_text != 0)
			    ? cmd->c_args[i].pi_text
			    : choice_to_name(fsm_paramtypes_choices,
					     cmd->c_args[i].pi_type));
	    if (!addline(curbp, outseq, -1))
		return FALSE;
	}
    }
#endif
    return TRUE;
}
#endif

#if OPT_NAMEBST
struct bindlist_data {
    UINT mask;			/* oper/motion mask */
    int min;			/* minimum key length */
    char *apropos;		/* key check */
    BINDINGS *bs;
};

static int
btree_walk(BI_NODE * node, int (*func) (BI_NODE *, const void *),
	   const void *data)
{
    if (node) {
	if (btree_walk(BI_LEFT(node), func, data))
	    return 1;

	if (BI_KEY(node))
	    func(node, data);

	if (btree_walk(BI_RIGHT(node), func, data))
	    return 1;
    }

    return 0;
}

static int
clearflag_func(BI_NODE * n, const void *d GCC_UNUSED)
{
    n->value.n_flags &= (UCHAR) (~NBST_DONE);
    return 0;
}

static int
addsynonym_func(BI_NODE * node, const void *d)
{
    const CMDFUNC *func = (const CMDFUNC *) d;
    char outseq[NLINE];		/* output buffer for text */

    if (node->value.n_cmd == func &&
	!(node->value.n_flags & NBST_DONE)) {
	strcpy(outseq, "  or\t");
	quoted(outseq + 5, BI_KEY(node));
	if (!addline(curbp, outseq, -1))
	    return 1;
    }

    return 0;
}

static int
makebind_func(BI_NODE * node, const void *d)
{
    const struct bindlist_data *data = (const struct bindlist_data *) d;
    char outseq[NLINE];		/* output buffer for keystroke sequence */
    const CMDFUNC *cmd = node->value.n_cmd;

    /* has this been listed? */
    if (node->value.n_flags & NBST_DONE)
	return 0;

    /* are we interested in this type of command? */
    if (data->mask && !(cmd->c_flags & data->mask))
	return 0;

    /* try to avoid alphabetizing by the real short names */
    if (data->min && (int) strlen(BI_KEY(node)) <= data->min)
	return 0;

    /* failed lookup by substring? */
    if (data->apropos && !ourstrstr(BI_KEY(node), data->apropos, TRUE))
	return 0;

    /* add in the command name */
    quote_and_pad(outseq, BI_KEY(node));
    convert_cmdfunc(data->bs, cmd, outseq);

    /* dump the line */
    if (!addline(curbp, outseq, -1))
	return 1;

    node->value.n_flags |= NBST_DONE;

    /* add synonyms */
    btree_walk(&namebst.head, addsynonym_func, cmd);

#if OPT_ONLINEHELP
    if (!show_onlinehelp(cmd))
	return FALSE;
#endif
    /* blank separator */
    if (!addline(curbp, "", -1))
	return 1;

    return 0;
}

/* build a binding list (limited or full) */
/* ARGSUSED */
static void
makebindlist(int whichmask, void *mstring)
{
    struct bindlist_data data;

    data.mask = whichmask;
    data.min = SHORT_CMD_LEN;
    data.apropos = (char *) mstring;
    data.bs = bindings_to_describe;

    /* let us know this is in progress */
    mlwrite("[Building binding list]");

    /* clear the NBST_DONE flag */
    btree_walk(&namebst.head, clearflag_func, 0);

    /* create binding list */
    if (btree_walk(&namebst.head, makebind_func, &data))
	return;

    /* catch entries with no synonym > SHORT_CMD_LEN */
    data.min = 0;
    if (btree_walk(&namebst.head, makebind_func, &data))
	return;

    mlerase();			/* clear the message line */
}
#else /* OPT_NAMEBST */
/* fully describe a function into the current buffer, given a pointer to
 * its name table entry */
static int
makefuncdesc(int j, char *listed)
{
    int i;
    const CMDFUNC *cmd = nametbl[j].n_cmd;
    char outseq[NLINE];		/* output buffer for keystroke sequence */

    /* add in the command name */
    quote_and_pad(outseq, nametbl[j].n_name);
    convert_cmdfunc(bs, cmd, outseq);

    /* dump the line */
    if (!addline(curbp, outseq, -1))
	return FALSE;

    /* then look for synonyms */
    (void) strcpy(outseq, "  or\t");
    for (i = 0; nametbl[i].n_name != 0; i++) {
	/* if it's the one we're on, skip */
	if (i == j)
	    continue;
	/* if it's already been listed, skip */
	if (listed[i])
	    continue;
	/* if it's not a synonym, skip */
	if (nametbl[i].n_cmd != cmd)
	    continue;
	(void) quoted(outseq + 5, nametbl[i].n_name);
	if (!addline(curbp, outseq, -1))
	    return FALSE;
    }

#if OPT_ONLINEHELP
    if (!show_onlinehelp(cmd))
	return FALSE;
#endif
    /* blank separator */
    if (!addline(curbp, "", -1))
	return FALSE;

    return TRUE;
}

/* build a binding list (limited or full) */
/* ARGSUSED */
static void
makebindlist(int whichmask, void *mstring)
{
    int pass;
    int j;
    int ok = TRUE;		/* reset if out-of-memory, etc. */
    char *listed = typecallocn(char, (ALLOC_T) nametblsize);

    if (listed == 0) {
	(void) no_memory(BINDINGLIST_BufName);
	return;
    }

    /* let us know this is in progress */
    mlwrite("[Building binding list]");

    /* build the contents of this window, inserting it line by line */
    for (pass = 0; pass < 2; pass++) {
	for (j = 0; nametbl[j].n_name != 0; j++) {

	    /* if we've already described this one, move on */
	    if (listed[j])
		continue;

	    /* are we interested in this type of command? */
	    if (whichmask && !(nametbl[j].n_cmd->c_flags & whichmask))
		continue;

	    /* try to avoid alphabetizing by the real short names */
	    if (pass == 0 && (int) strlen(nametbl[j].n_name) <= SHORT_CMD_LEN)
		continue;

	    /* if we are executing an apropos command
	       and current string doesn't include the search string */
	    if (mstring
		&& !ourstrstr(nametbl[j].n_name, (char *) mstring, TRUE))
		continue;

	    ok = makefuncdesc(j, listed);
	    if (!ok)
		break;

	    listed[j] = TRUE;	/* mark it as already listed */
	}
    }

    if (ok)
	mlerase();		/* clear the message line */
    free(listed);
}
#endif /* OPT_NAMEBST */

#endif /* OPT_REBIND */

#if OPT_REBIND || OPT_EVAL
/* much like the "standard" strstr, but if the substring starts
	with a '^', we discard it and force an exact match.
	returns 1-based offset of match, or 0.  */
int
ourstrstr(
	     const char *haystack,
	     const char *needle,
	     int anchor)
{
    if (anchor && *needle == '^') {
	return strcmp(needle + 1, haystack) ? 0 : 1;
    } else {
	int nl = strlen(needle);
	int hl = strlen(haystack);
	const char *hp = haystack;
	while (hl >= nl && *hp) {
	    if (!strncmp(hp, needle, nl))
		return hp - haystack + 1;
	    hp++;
	    hl--;
	}
	return 0;
    }
}
#endif /* OPT_REBIND || OPT_EVAL */

static char *
locate_fname(char *dirname, char *fname, int mode)
{
    static char fullpath[NFILEN];	/* expanded path */

    if (dirname
	&& dirname[0] != EOS
	&& ffaccess(pathcat(fullpath, dirname, fname), mode))
	return (fullpath);

    return 0;
}

static char *
locate_file_in_list(char *list, char *fname, int mode)
{
    const char *cp;
    char *sp;
    char dirname[NFILEN];

    cp = list;
    while ((cp = parse_pathlist(cp, dirname)) != 0) {
	if ((sp = locate_fname(dirname, fname, mode)) != 0)
	    return sp;
    }
    return 0;
}

#if OPT_PATHLOOKUP
#if SYS_VMS
static char *
PATH_value(void)
{
    /* On VAX/VMS, the PATH environment variable is only the
     * current-dir.  Fake up an acceptable alternative.
     */

    static TBUFF *myfiles;
    char *tmp;

    if (!tb_length(myfiles)) {
	char mypath[NFILEN];

	(void) strcpy(mypath, prog_arg);
	if ((tmp = vms_pathleaf(mypath)) == mypath)
	    (void) strcpy(mypath,
			  current_directory(FALSE));
	else
	    *tmp = EOS;

	if (!tb_init(&myfiles, EOS)
	    || !tb_sappend(&myfiles, mypath)
	    || !tb_sappend(&myfiles,
			   ",SYS$SYSTEM:,SYS$LIBRARY:")
	    || !tb_append(&myfiles, EOS))
	    return NULL;
    }
    return tb_values(myfiles);
}
#else /* UNIX or MSDOS */
#define PATH_value() getenv("PATH")
#endif
#endif /* OPT_PATHLOOKUP */

/* config files that vile is interested in may live in various places,
 * and which may be constrained to various r/w/x modes.  the files may
 * be
 *  in the current directory,
 *  in the HOME directory,
 *  along a special "path" variable called "VILE_STARTUP_PATH",
 *  they may be in the directory where we found the vile executable itself,
 *  or they may be along the user's PATH.
 * in addition, they may be readable, writeable, or executable.
 */

char *
cfg_locate(char *fname, UINT which)
{
    char *sp;
    int mode = (which & (FL_EXECABLE | FL_WRITEABLE | FL_READABLE));

    /* take care of special cases */
    if (!fname || !fname[0] || isSpace(fname[0]))
	return NULL;
    else if (isShellOrPipe(fname))
	return fname;

    /* look in the current directory */
    if (which & FL_CDIR) {
	if (ffaccess(fname, mode)) {
	    return (fname);
	}
    }

    if ((which & FL_HOME)	/* look in the home directory */
	&&((sp = locate_fname(home_dir(), fname, mode)) != 0))
	return sp;

    if ((which & FL_EXECDIR)	/* look in vile's bin directory */
	&&((sp = locate_fname(exec_pathname, fname, mode)) != 0))
	return sp;

    if ((which & FL_STARTPATH)	/* look along "VILE_STARTUP_PATH" */
	&&(sp = locate_file_in_list(startup_path, fname, mode)) != 0)
	return sp;

    if (which & FL_PATH) {	/* look along "PATH" */
#if OPT_PATHLOOKUP
	if ((sp = locate_file_in_list(PATH_value(), fname, mode)) != 0)
	    return sp;
#endif /* OPT_PATHLOOKUP */

    }

    if ((which & FL_LIBDIR)	/* look along "VILE_LIBDIR_PATH" */
	&&(sp = locate_file_in_list(libdir_path, fname, mode)) != 0)
	return sp;

    return NULL;
}

#if OPT_SHOW_WHICH
static int
ask_which_file(char *fname)
{
    static TBUFF *last;

    return mlreply_file("Which file: ", &last,
			FILEC_READ | FILEC_PROMPT, fname);
}

static void
list_one_fname(char *fname, int mode)
{
    bprintf("\n%s\t%s", ffaccess(fname, mode) ? "*" : "", fname);
}

static void
list_which_fname(char *dirname, char *fname, int mode)
{
    char fullpath[NFILEN];	/* expanded path */

    if (dirname
	&& dirname[0] != EOS) {
	list_one_fname(SL_TO_BSL(
				    pathcat(fullpath, dirname, fname)),
		       mode);
    }
}

static void
list_which_file_in_list(char *list, char *fname, int mode)
{
    const char *cp;
    char dirname[NFILEN];

    cp = list;
    while ((cp = parse_pathlist(cp, dirname)) != 0) {
	list_which_fname(dirname, fname, mode);
    }
}

static void
list_which(LIST_ARGS)
{
    char *fname = (char *) ptr;
    UINT mode = (flag & (FL_EXECABLE | FL_WRITEABLE | FL_READABLE));

    /* take care of special cases */
    if (!fname || !fname[0] || isSpace(fname[0]))
	return;
    else if (isShellOrPipe(fname))
	return;

    bprintf("Show which %s-paths are tested for:\n\t%s\n(\"*\" marks found-files)\n",
	    (mode & FL_EXECABLE) ? "executable" : "source",
	    fname);

    /* look in the current directory */
    if (flag & FL_CDIR) {
	bprintf("\n$cwd");
	list_one_fname(fname, mode);
    }

    if (flag & FL_HOME) {	/* look in the home directory */
	bprintf("\n$HOME");
	list_which_fname(home_dir(), fname, mode);
    }

    if (flag & FL_EXECDIR) {	/* look in vile's bin directory */
	bprintf("\n$exec-path");
	list_which_fname(exec_pathname, fname, mode);
    }

    if (flag & FL_STARTPATH) {	/* look along "VILE_STARTUP_PATH" */
	bprintf("\n$startup-path");
	list_which_file_in_list(startup_path, fname, mode);
    }

    if (flag & FL_PATH) {	/* look along "PATH" */
#if OPT_PATHLOOKUP
	bprintf("\n$PATH");
	list_which_file_in_list(PATH_value(), fname, mode);
#endif /* OPT_PATHLOOKUP */

    }

    if (flag & FL_LIBDIR) {	/* look along "VILE_LIBDIR_PATH" */
	bprintf("\n$libdir-path");
	list_which_file_in_list(libdir_path, fname, mode);
    }
}

static int
show_which_file(char *fname, UINT mode, int f, int n)
{
    char *result;
    if ((result = cfg_locate(fname, mode)) != 0) {
	mlwrite("%s", result);
    }
    if (f)
	liststuff(WHICH_BufName, n > 2, list_which, mode, fname);
    return (result != 0);
}

int
which_source(int f, int n)
{
    int s;
    char fname[NFILEN];

    if ((s = ask_which_file(fname)) == TRUE) {
	s = show_which_file(fname, LOCATE_SOURCE, f, n);
    }
    return s;
}

int
which_exec(int f, int n)
{
    int s;
    char fname[NFILEN];

    if ((s = ask_which_file(fname)) == TRUE) {
	s = show_which_file(fname, LOCATE_EXEC, f, n);
    }
    return s;
}
#endif

/* translate a keycode to its binding-string */
char *
kcod2pstr(int c, char *seq)
{
    seq[0] = (char) kcod2escape_seq(c, &seq[1]);
    return seq;
}

/* Translate a 16-bit keycode to a string that will replay into the same
 * code.
 */
int
kcod2escape_seq(
		   int c,
		   char *ptr)
{
    char *base = ptr;

    /* ...just for completeness */
    if (c & CTLA)
	*ptr++ = (char) cntl_a;
    else if (c & CTLX)
	*ptr++ = (char) cntl_x;
    else if (c & SPEC)
	*ptr++ = (char) poundc;
#if SYS_WINNT
#define ALTPLUS   "Alt+"
#define CTRLPLUS  "Ctrl+"
#define SHIFTPLUS "Shift+"
#define W32INSERT "Insert"
#define W32DELETE "Delete"
    else if (c & W32_KEY) {
	if (c & W32_SHIFT) {
	    strcpy(ptr, SHIFTPLUS);
	    ptr += sizeof(SHIFTPLUS) - 1;
	}
	if (c & W32_CTRL) {
	    strcpy(ptr, CTRLPLUS);
	    ptr += sizeof(CTRLPLUS) - 1;
	}
	if (c & W32_ALT) {
	    strcpy(ptr, ALTPLUS);
	    ptr += sizeof(ALTPLUS) - 1;
	}
	c &= W32_NOMOD;
	if (c == VK_INSERT) {
	    strcpy(ptr, W32INSERT);
	    ptr += sizeof(W32INSERT) - 1;
	} else if (c == VK_DELETE) {
	    strcpy(ptr, W32DELETE);
	    ptr += sizeof(W32DELETE) - 1;
	} else
	    *ptr++ = c;		/* Pickup <modifier>+...<single_char> */
	*ptr = EOS;
	return (int) (ptr - base);
    }
#endif
    *ptr++ = (char) c;
    *ptr = EOS;
    return (int) (ptr - base);
}

/* translates a binding string into printable form */
#if OPT_REBIND
static char *
bytes2prc(char *dst, char *src, int n)
{
    char *base = dst;
    int c;
    const char *tmp;

    for (; n != 0; dst++, src++, n--) {

	c = *src;

	tmp = 0;

	if (c & HIGHBIT) {
	    *dst++ = 'M';
	    *dst++ = '-';
	    c &= ~HIGHBIT;
	}
	if (c == ' ') {
	    tmp = "<space>";
	} else if (isCntrl(c)) {
	    *dst++ = '^';
	    *dst = tocntrl(c);
	} else {
	    *dst = (char) c;
	}

	if (tmp != 0) {
	    while ((*dst++ = *tmp++) != EOS) ;
	    dst -= 2;		/* point back to last nonnull */
	}

	if (n > 1) {
	    *++dst = '-';
	}
    }
    *dst = EOS;
    return base;
}

/* translate a 10-bit keycode to its printable name (like "M-j")  */
char *
kcod2prc(int c, char *seq)
{
    char temp[NSTRING];
    (void) kcod2pstr(c, temp);
#if SYS_WINNT
    if (c & W32_KEY) {
	/* Translation is complete, by defn. */

	strcpy(seq, temp + 1);
	return (seq);
    }
#endif
    return bytes2prc(seq, temp + 1, (int) *temp);
}
#endif

static int
cmdfunc2keycode(BINDINGS * bs, const CMDFUNC * f)
{
    KBIND *kbp;
    int c;

    for (c = 0; c < N_chars; c++)
	if (f == bs->kb_normal[c])
	    return c;

#if OPT_REBIND
    for (kbp = bs->kb_extra; kbp != bs->kb_special; kbp = kbp->k_link) {
	if (kbp->k_cmd == f)
	    return kbp->k_code;
    }
#endif
    for (kbp = bs->kb_special; kbp->k_cmd != 0; kbp++) {
	if (kbp->k_cmd == f)
	    return kbp->k_code;
    }

    return -1;			/* none found */
}

/* kcod2fnc:  translate a 10-bit keycode to a function pointer */
/*	(look a key binding up in the binding table)		*/
const CMDFUNC *
kcod2fnc(BINDINGS * bs, int c)
{
    if (isSpecial(c)) {
	KBIND *kp = kcode2kbind(bs, c);
	return (kp != 0) ? kp->k_cmd : 0;
    }
    return bs->kb_normal[c];
}

/* fnc2kcod: translate a function pointer to a keycode */
int
fnc2kcod(const CMDFUNC * f)
{
    return cmdfunc2keycode(&dft_bindings, f);
}

/* fnc2pstr: translate a function pointer to a pascal-string that a user
	could enter.  returns a pointer to a static array */
#if DISP_X11
char *
fnc2pstr(const CMDFUNC * f)
{
    int c;
    static char seq[10];

    c = fnc2kcod(f);

    if (c == -1)
	return NULL;

    return kcod2pstr(c, seq);
}
#endif

/* fnc2engl: translate a function pointer to the english name for
		that function.  prefer long names to short ones.
*/
#if OPT_EVAL || OPT_REBIND
static const NTAB *
fnc2ntab(const CMDFUNC * cfp)
{
    const NTAB *nptr;
    const NTAB *shortnptr = NULL;

    for (nptr = nametbl; nptr->n_cmd; nptr++) {
	if (nptr->n_cmd == cfp) {
	    /* if it's a long name, return it */
	    if ((int) strlen(nptr->n_name) > SHORT_CMD_LEN)
		return nptr;
	    /* remember the first short name, in case there's
	       no long name */
	    if (!shortnptr)
		shortnptr = nptr;
	}
    }
    return shortnptr;
}

static char *
fnc2engl(const CMDFUNC * cfp)
{
    const NTAB *nptr = fnc2ntab(cfp);
    return nptr ? nptr->n_name : 0;
}

#endif

/* engl2fnc: match name to a function in the names table
	translate english name to function pointer
		 return any match or NULL if none
 */
#define BINARY_SEARCH_IS_BROKEN 0
#if OPT_NAMEBST
const CMDFUNC *
engl2fnc(const char *fname)
{
    BI_NODE *n = btree_pmatch(BI_RIGHT(&namebst.head), TRUE, fname);

    if (n == NULL)
	return NULL;
    else
	return n->value.n_cmd;
}
#else
#if BINARY_SEARCH_IS_BROKEN	/* then use the old linear look-up */
const CMDFUNC *
engl2fnc(const char *fname)
{
    NTAB *nptr;			/* pointer to entry in name binding table */
    SIZE_T len = strlen(fname);

    if (len != 0) {		/* scan through the table, returning any match */
	nptr = nametbl;
	while (nptr->n_cmd != NULL) {
	    if (strncmp(fname, nptr->n_name, len) == 0)
		return nptr->n_cmd;
	    ++nptr;
	}
    }
    return NULL;
}
#else
/* this runs 10 times faster for 'nametbl[]' */
const CMDFUNC *
engl2fnc(const char *fname)
{
    int lo, hi, cur;
    int r;
    SIZE_T len = strlen(fname);

    if (len == 0)
	return NULL;

    /* scan through the table, returning any match */
    lo = 0;
    hi = nametblsize - 2;	/* don't want last entry -- it's NULL */

    while (lo <= hi) {
	cur = (lo + hi) >> 1;
	if ((r = strncmp(fname, nametbl[cur].n_name, len)) == 0) {
	    /* Now find earliest matching entry */
	    while (cur > lo
		   && strncmp(fname, nametbl[cur - 1].n_name, len) == 0)
		cur--;
	    return nametbl[cur].n_cmd;

	} else if (r > 0) {
	    lo = cur + 1;
	} else {
	    hi = cur - 1;
	}
    }
    return NULL;
}
#endif /* binary vs linear */
#endif /* OPT_NAMEBST */

/* prc2kcod: translate printable code to 10 bit keycode */
#if OPT_EVAL || OPT_REBIND
static int
prc2kcod(const char *kk)
{
    UINT kcod;			/* code to return */
    UINT pref = 0;		/* key prefixes */
    int len = strlen(kk);
    const UCHAR *k = (const UCHAR *) kk;

    if (len > 3 && *(k + 2) == '-') {
	if (*k == '^') {
	    if (isCntrl(cntl_a) && *(k + 1) == toalpha(cntl_a))
		pref = CTLA;
	    if (isCntrl(cntl_x) && *(k + 1) == toalpha(cntl_x))
		pref = CTLX;
	    if (isCntrl(poundc) && *(k + 1) == toalpha(poundc))
		pref = SPEC;
	} else if (!strncmp((const char *) k, "FN", (SIZE_T) 2)) {
	    pref = SPEC;
	}
	if (pref != 0)
	    k += 3;
    } else if (len > 2 && !strncmp((const char *) k, "M-", (SIZE_T) 2)) {
	pref = HIGHBIT;
	k += 2;
    } else if (len > 1) {
	if (*k == cntl_a)
	    pref = CTLA;
	else if (*k == cntl_x)
	    pref = CTLX;
	else if (*k == poundc)
	    pref = SPEC;
	if (pref != 0) {
	    k++;
	    if (len > 2 && *k == '-')
		k++;
	}
    }

    if (*k == '^' && *(k + 1) != EOS) {		/* control character */
	kcod = *(k + 1);
	if (isLower(kcod))
	    kcod = toUpper(kcod);
	kcod = tocntrl(kcod);
	k += 2;
    } else {			/* any single char, control or not */
	kcod = *k++;
    }

    if (*k != EOS)		/* we should have eaten the whole thing */
	return -1;

    return (int) (pref | kcod);
}
#endif

#if OPT_EVAL
/* translate printable code (like "M-r") to english command name */
const char *
prc2engl(const char *kk)
{
    const char *englname;
    int kcod;

    kcod = prc2kcod(kk);
    if (kcod < 0)
	return "ERROR";

    englname = fnc2engl(DefaultKeyBinding(kcod));
    if (englname == NULL)
	englname = "ERROR";

    return englname;
}
#endif

/*
 * Get an english command name from the user
 */
char *
kbd_engl(
	    const char *prompt,	/* null pointer to splice calls */
	    char *buffer)
{
    if (kbd_engl_stat(prompt, buffer, 0) == TRUE)
	return buffer;
    return NULL;
}

/* sound the alarm! */
#if OPT_TRACE
void
trace_alarm(char *file, int line)
#else
void
kbd_alarm(void)
#endif
{
    TRACE(("BEEP (%s @%d)\n", file, line));

    if (!no_errs
	&& !clhide
	&& global_g_val(GMDERRORBELLS)) {
	term.beep();
	term.flush();
    }
    warnings++;
}

/* put a character to the keyboard-prompt */
void
kbd_putc(int c)
{
    BUFFER *savebp;
    WINDOW *savewp;
    MARK savemk;

    beginDisplay();
    savebp = curbp;
    savewp = curwp;
    curbp = bminip;
    curwp = wminip;
    savemk = MK;
    MK = DOT;
    if ((kbd_expand <= 0) && isreturn(c)) {
	kbd_erase_to_end(0);
    } else {
	if ((kbd_expand < 0) && (c == '\t')) {
	    (void) linsert(1, ' ');
	} else {
	    (void) linsert(1, c);
	}
	if (!is_header_line(DOT, curbp) && !is_at_end_of_line(DOT))
	    forwchar(TRUE, 1);	/* END OF LINE HACK */
#ifdef DEBUG
	TRACE(("mini:%2d:%s\n", llength(DOT.l), lp_visible(DOT.l)));
#endif
    }
    curbp = savebp;
    curwp = savewp;
    MK = savemk;
    endofDisplay();
}

/* put a string to the keyboard-prompt */
static void
kbd_puts(const char *s)
{
    while (*s)
	kbd_putc(*s++);
}

/* erase a character from the display by wiping it out */
void
kbd_erase(void)
{
    BUFFER *savebp;
    WINDOW *savewp;
    MARK savemk;

    if (!vl_echo || clhide)
	return;

    beginDisplay();
    savebp = curbp;
    savewp = curwp;
    curbp = bminip;
    curwp = wminip;
    savemk = MK;
    MK = DOT;
    if (DOT.o > 0) {
	DOT.o -= 1;
	ldelete(1, FALSE);
    }
#ifdef DEBUG
    TRACE(("MINI:%2d:%s\n", llength(DOT.l), lp_visible(DOT.l)));
#endif
    curbp = savebp;
    curwp = savewp;
    MK = savemk;
    endofDisplay();
}

void
kbd_erase_to_end(int column)
{
    BUFFER *savebp;
    WINDOW *savewp;
    MARK savemk;

    if (!vl_echo || clhide)
	return;

    beginDisplay();
    savebp = curbp;
    savewp = curwp;
    curbp = bminip;
    curwp = wminip;
    savemk = MK;
    MK = DOT;
    if (llength(DOT.l) > 0) {
	DOT.o = column;
	ldelete(llength(DOT.l) - DOT.o, FALSE);
	TRACE(("NULL:%2d:%s\n", llength(DOT.l), lp_visible(DOT.l)));
    }
    curbp = savebp;
    curwp = savewp;
    MK = savemk;
    endofDisplay();
}

#if OPT_CASELESS
static int
cs_strcmp(
	     int case_insensitive,
	     const char *s1,
	     const char *s2)
{
    if (case_insensitive)
	return stricmp(s1, s2);
    return strcmp(s1, s2);
}

static int
cs_strncmp(
	      int case_insensitive,
	      const char *s1,
	      const char *s2,
	      SIZE_T n)
{
    if (case_insensitive)
	return strnicmp(s1, s2, n);
    return strncmp(s1, s2, n);
}
#endif /* OPT_CASELESS */

/* definitions for name-completion */
#define	NEXT_DATA(p)	((p)+size_entry)
#define	PREV_DATA(p)	((p)-size_entry)

#ifdef	lint
static				/*ARGSUSED */
const char *
THIS_NAME(const char *p)
{
    return 0;
}
#else
#define	THIS_NAME(p)	(*TYPECAST(const char *const,p))
#endif
#define	NEXT_NAME(p)	THIS_NAME(NEXT_DATA(p))

/*
 * Scan down until we no longer match the current input, or reach the end of
 * the symbol table.
 */
/*ARGSUSED*/
static const char *
skip_partial(
		int case_insensitive GCC_UNUSED,
		char *buf,
		SIZE_T len,
		const char *table,
		SIZE_T size_entry)
{
    const char *next = NEXT_DATA(table);
    const char *sp;

    while ((sp = THIS_NAME(next)) != 0) {
	if (StrNcmp(buf, sp, len) != 0)
	    break;
	next = NEXT_DATA(next);
    }
    return next;
}

/*
 * Shows a partial-match.  This is invoked in the symbol table at a partial
 * match, and the user wants to know what characters could be typed next.
 * If there is more than one possibility, they are shown in square-brackets.
 * If there is only one possibility, it is shown in curly-braces.
 */
static void
show_partial(
		int case_insensitive,
		char *buf,
		SIZE_T len,
		const char *table,
		SIZE_T size_entry)
{
    const char *next = skip_partial(case_insensitive, buf, len, table, size_entry);
    const char *last = PREV_DATA(next);
    int c;

    if (THIS_NAME(table)[len] == THIS_NAME(last)[len]) {
	kbd_putc('{');
	while ((c = THIS_NAME(table)[len]) != 0) {
	    if (c == THIS_NAME(last)[len]) {
		kbd_putc(c);
		len++;
	    } else
		break;
	}
	kbd_putc('}');
    }
    if (next != NEXT_DATA(table)) {
	c = TESTC;		/* shouldn't be in the table! */
	kbd_putc('[');
	while (table != next) {
	    const char *sp = THIS_NAME(table);
	    if (c != sp[len]) {
		c = sp[len];
		kbd_putc(c ? c : '$');
	    }
	    table = NEXT_DATA(table);
	}
	kbd_putc(']');
    }
    kbd_flush();
}

#if OPT_POPUPCHOICE
/*
 * makecmpllist is called from liststuff to display the possible completions.
 */
struct compl_rec {
    char *buf;
    SIZE_T len;
    const char *table;
    SIZE_T size_entry;
};

#ifdef lint
#define c2ComplRec(c) ((struct compl_rec *)0)
#else
#define c2ComplRec(c) ((struct compl_rec *)c)
#endif

/*ARGSUSED*/
static void
makecmpllist(
		int case_insensitive,
		void *cinfop)
{
    char *buf = c2ComplRec(cinfop)->buf;
    SIZE_T len = c2ComplRec(cinfop)->len;
    const char *first = c2ComplRec(cinfop)->table;
    SIZE_T size_entry = c2ComplRec(cinfop)->size_entry;
    const char *last = skip_partial(case_insensitive, buf, len, first, size_entry);
    const char *p;
    SIZE_T maxlen;
    int slashcol;
    int cmpllen;
    int cmplcols;
    int cmplrows;
    int nentries;
    int i, j;

    for (p = NEXT_DATA(first), maxlen = strlen(THIS_NAME(first));
	 p != last;
	 p = NEXT_DATA(p)) {
	SIZE_T l = strlen(THIS_NAME(p));
	if (l > maxlen)
	    maxlen = l;
    }

    slashcol = (int) (pathleaf(buf) - buf);
    if (slashcol != 0) {
	char b[NLINE];
	(void) strncpy(b, buf, (SIZE_T) slashcol);
	(void) strncpy(&b[slashcol], &(THIS_NAME(first))[slashcol],
		       (len - slashcol));
	b[slashcol + (len - slashcol)] = EOS;
	bprintf("Completions prefixed by %s:\n", b);
    }

    cmplcols = term.cols / (maxlen - slashcol + 1);

    if (cmplcols == 0)
	cmplcols = 1;

    nentries = (int) (last - first) / size_entry;
    cmplrows = nentries / cmplcols;
    cmpllen = term.cols / cmplcols;
    if (cmplrows * cmplcols < nentries)
	cmplrows++;

    for (i = 0; i < cmplrows; i++) {
	for (j = 0; j < cmplcols; j++) {
	    int idx = cmplrows * j + i;
	    if (idx < nentries) {
		const char *s = THIS_NAME(first + (idx * size_entry)) + slashcol;
		if (j == cmplcols - 1)
		    bprintf("%s\n", s);
		else
		    bprintf("%*s", cmpllen, s);
	    } else {
		bprintf("\n");
		break;
	    }
	}
    }
}

/*
 * Pop up a window and show the possible completions.
 */
static void
show_completions(
		    int case_insensitive,
		    char *buf,
		    SIZE_T len,
		    const char *table,
		    SIZE_T size_entry)
{
    struct compl_rec cinfo;
    BUFFER *bp;
    int alreadypopped = 0;

    /*
     * Find out if completions buffer exists; so we can take the time to
     * shrink/grow the window to the latest size.
     */
    if ((bp = find_b_name(COMPLETIONS_BufName)) != NULL) {
	alreadypopped = (bp->b_nwnd != 0);
    }

    cinfo.buf = buf;
    cinfo.len = len;
    cinfo.table = table;
    cinfo.size_entry = size_entry;
    liststuff(COMPLETIONS_BufName, FALSE, makecmpllist, case_insensitive,
	      (void *) &cinfo);

    if (alreadypopped)
	shrinkwrap();

    (void) update(TRUE);
}

/*
 * Scroll the completions window wrapping around back to the beginning
 * of the buffer once it has been completely scrolled.  If the completions
 * buffer is missing for some reason, we will call show_completions to pop
 * it (back) up.
 */
static void
scroll_completions(
		      int case_insensitive,
		      char *buf,
		      SIZE_T len,
		      const char *table,
		      SIZE_T size_entry)
{
    BUFFER *bp = find_b_name(COMPLETIONS_BufName);
    if (bp == NULL)
	show_completions(case_insensitive, buf, len, table, size_entry);
    else {
	LINEPTR lp;
	swbuffer(bp);
	(void) gotoeos(FALSE, 1);
	lp = DOT.l;
	(void) forwhpage(FALSE, 1);
	if (lp == DOT.l)
	    (void) gotobob(FALSE, 0);
	(void) update(TRUE);
    }
}

void
popdown_completions(void)
{
    BUFFER *bp;
    if ((bp = find_b_name(COMPLETIONS_BufName)) != NULL)
	zotwp(bp);
}
#endif /* OPT_POPUPCHOICE */

/*
 * Attempt to partial-complete the string, char at a time
 */
static SIZE_T
fill_partial(
		int case_insensitive GCC_UNUSED,
		char *buf,
		SIZE_T pos,
		const char *first,
		const char *last,
		SIZE_T size_entry)
{
    const char *p;
    SIZE_T n = pos;
    const char *this_name = THIS_NAME(first);

    TRACE(("fill_partial(%d:%.*s) first=%s, last=%s\n",
	   pos, (int) pos, buf, THIS_NAME(first), THIS_NAME(last)));

#if 0				/* case insensitive reply correction doesn't work reliably yet */
    if (!clexec && case_insensitive) {
	int spos = pos;

	while (spos > 0 && buf[spos - 1] != SLASHC) {
	    kbd_erase();
	    spos--;
	}
	while (spos < pos) {
	    kbd_putc(this_name[spos]);
	    spos++;
	}
    }
#endif

    if (first == last
	|| (first = NEXT_DATA(first)) == last) {
	TRACE(("...empty list\n"));
	return n;
    }

    for_ever {
	buf[n] = this_name[n];	/* add the next char in */
	buf[n + 1] = EOS;
	TRACE(("...added(%d:%c)\n", n, buf[n]));

	/* scan through the candidates */
	for (p = first; p != last; p = NEXT_DATA(p)) {
	    if (StrNcmp(&THIS_NAME(p)[n], &buf[n], 1) != 0) {
		buf[n] = EOS;
		if (n == pos
#if OPT_POPUPCHOICE
# if OPT_ENUM_MODES
		    && !global_g_val(GVAL_POPUP_CHOICES)
# else
		    && !global_g_val(GMDPOPUP_CHOICES)
# endif
#endif
		    )
		    kbd_alarm();
		kbd_flush();	/* force out alarm or partial completion */
		TRACE(("...fill_partial %d\n", n));
		return n;
	    }
	}

	if (!clexec)
	    kbd_putc(buf[n]);	/* add the character */
	n++;
    }
}

static int testcol;		/* records the column when TESTC is decoded */
#if OPT_POPUPCHOICE
/*
 * cmplcol is used to record the column number (on the message line) after
 * name completion.  Its value is used to decide whether or not to display
 * a completion list if the name completion character (tab) is pressed
 * twice in succession.  Once the completion list has been displayed, its
 * value will be changed to the additive inverse of the column number in
 * order to determine whether to scroll if tab is pressed yet again.  We
 * assume that 0 will never be a valid column number.  So long as we always
 * display some sort of prompt prior to reading from the message line, this
 * is a good assumption.
 */
static int cmplcol = 0;
#endif

/*
 * Initializes the name-completion logic
 */
void
kbd_init(void)
{
    testcol = -1;
}

/*
 * Returns the current length of the minibuffer
 */
int
kbd_length(void)
{
    if (wminip != 0
	&& wminip->w_dot.l != 0
	&& llength(wminip->w_dot.l) > 0)
	return llength(wminip->w_dot.l);
    return 0;
}

/*
 * Erases the display that was shown in response to TESTC
 */
void
kbd_unquery(void)
{
    beginDisplay();
#if OPT_POPUPCHOICE
    if (cmplcol != kbd_length() && -cmplcol != kbd_length())
	cmplcol = 0;
#endif
    if (testcol >= 0) {
	while (kbd_length() > testcol)
	    kbd_erase();
	kbd_flush();
	testcol = -1;
    }
    endofDisplay();
}

/*
 * This is invoked to find the closest name to complete from the current buffer
 * contents.
 */
int
kbd_complete(
		unsigned flags,	/* KBD_xxx */
		int c,		/* TESTC, NAMEC or isreturn() */
		char *buf,
		unsigned *pos,
		const char *table,
		SIZE_T size_entry)
{
    int case_insensitive = (flags & KBD_CASELESS) != 0;
    SIZE_T cpos = *pos;
    const char *nbp;		/* first ptr to entry in name binding table */
    int status = FALSE;
#if OPT_POPUPCHOICE
# if OPT_ENUM_MODES
    int gvalpopup_choices = global_g_val(GVAL_POPUP_CHOICES);
# else
    int gvalpopup_choices = global_g_val(GMDPOPUP_CHOICES);
# endif
#endif

    kbd_init();			/* nothing to erase */
    buf[cpos] = EOS;		/* terminate it for us */
    nbp = table;		/* scan for matches */

    while (THIS_NAME(nbp) != NULL) {
	if (StrNcmp(buf, THIS_NAME(nbp), strlen(buf)) == 0) {
	    testcol = kbd_length();
	    /* a possible match! exact? no more than one? */
#if OPT_POPUPCHOICE
	    if (!clexec && c == NAMEC && cmplcol == -kbd_length()) {
		scroll_completions(case_insensitive, buf, cpos, nbp, size_entry);
		return FALSE;
	    }
#endif
	    if (c == TESTC) {
		show_partial(case_insensitive, buf, cpos, nbp, size_entry);
	    } else if (Strcmp(buf, THIS_NAME(nbp)) == 0 ||	/* exact? */
		       NEXT_NAME(nbp) == NULL ||
		       StrNcmp(buf, NEXT_NAME(nbp), strlen(buf)) != 0) {
		/* exact or only one like it.  print it */
		if (!clexec) {
#if 0				/* case insensitive reply correction doesn't work reliably yet */
		    if (case_insensitive) {
			int spos = cpos;

			while (spos > 0 && buf[spos - 1] != SLASHC) {
			    kbd_erase();
			    spos--;
			}
			kbd_puts(THIS_NAME(nbp) + spos);
		    } else
#endif
			kbd_puts(THIS_NAME(nbp) + cpos);
		    kbd_flush();
		    testcol = kbd_length();
		}
		if (c != NAMEC)	/* put it back */
		    unkeystroke(c);
		/* return complete name */
		(void) strncpy0(buf, THIS_NAME(nbp),
				(SIZE_T) (NLINE - 1));
		*pos = strlen(buf);
#if OPT_POPUPCHOICE
		if (gvalpopup_choices != POPUP_CHOICES_OFF
		    && !clexec && (c == NAMEC))
		    status = FALSE;
		else
#endif
		    status = TRUE;
	    } else {
		/* try for a partial match against the list */
		*pos = fill_partial(case_insensitive, buf, cpos, nbp,
				    skip_partial(case_insensitive, buf,
						 cpos, nbp, size_entry),
				    size_entry);
		testcol = kbd_length();
	    }
#if OPT_POPUPCHOICE
# if OPT_ENUM_MODES
	    if (!clexec
		&& gvalpopup_choices != POPUP_CHOICES_OFF
		&& c == NAMEC
		&& *pos == cpos) {
		if (gvalpopup_choices == POPUP_CHOICES_IMMED
		    || cmplcol == kbd_length()) {
		    show_completions(case_insensitive, buf, cpos, nbp, size_entry);
		    cmplcol = -kbd_length();
		} else
		    cmplcol = kbd_length();
	    } else
		cmplcol = 0;
# else
	    if (!clexec && gvalpopup_choices
		&& c == NAMEC && *pos == cpos) {
		show_completions(case_insensitive, buf, cpos, nbp, size_entry);
		cmplcol = -kbd_length();
	    } else
		cmplcol = 0;
# endif
#endif
	    return status;
	}
	nbp = NEXT_DATA(nbp);
    }

#if OPT_POPUPCHOICE
    cmplcol = 0;
#endif
    buf[*pos = cpos] = EOS;
    if ((flags & (KBD_MAYBEC|KBD_MAYBEC2)) != 0) {
	return (cpos != 0);
    } else {
	if (clexec) {
	    mlwarn("[No match for '%s']", buf);		/* no match */
	} else {
	    kbd_alarm();
	}
	return FALSE;
    }
}

/*
 * Test a buffer to see if it looks like a shift-command, which may have
 * repeated characters (but they must all be the same).
 */
static int
is_shift_cmd(
		const char *buffer,
		unsigned cpos)
{
    int c = *buffer;
    if (isRepeatable(c)) {
	while (--cpos != 0)
	    if (*(++buffer) != c)
		return FALSE;
	return TRUE;
    }
    return FALSE;
}

/*
 * The following mess causes the command to terminate if:
 *
 *	we've got the eolchar
 *		-or-
 *	we're in the first few chars and we're switching from punctuation
 *	(i.e., delimiters) to non-punctuation (i.e., characters that are part
 *	of command-names), or vice-versa.  oh yeah -- '-' isn't punctuation
 *	today, and '!' isn't either, in one direction, at any rate.
 *	All this allows things like:
 *		: e#
 *		: e!%
 *		: !ls
 *		: q!
 *		: up-line
 *	to work properly.
 *
 *	If we pass this "if" with c != NAMEC, then c is ungotten below,
 *	so it can be picked up by the commands argument getter later.
 */

#define ismostpunct(c) (isPunct(c) && (c) != '-')

static int
eol_command(
	       const char *buffer,
	       unsigned cpos,
	       int c,
	       int eolchar)
{
    /*
     * Handle special case of repeated-character implying repeat-count
     */
    if (is_shift_cmd(buffer, cpos) && (c == *buffer))
	return TRUE;

    /*
     * Shell-commands aren't complete until the line is complete.
     */
    if ((cpos != 0) && isShellOrPipe(buffer))
	return isreturn(c);

    return (c == eolchar)
	|| (
	       cpos != 0 && cpos < 3
	       && ((!ismostpunct(c)
		    && ismostpunct(buffer[cpos - 1]))
		   || ((c != '!' && ismostpunct(c))
		       && (buffer[cpos - 1] == '!'
			   || !ismostpunct(buffer[cpos - 1]))
		   )
	       )
	);
}

/*
 * This procedure is invoked from 'kbd_string()' to setup the command-name
 * completion and query displays.
 */
static int
cmd_complete(
		int c,
		char *buf,
		unsigned *pos)
{
    int status;
#if OPT_HISTORY
    /*
     * If the user scrolled back in 'edithistory()', the text may be a
     * repeated-shift command, which won't match the command-table (e.g.,
     * ">>>").
     */
    if ((*pos > 1) && is_shift_cmd(buf, *pos)) {
	unsigned len = 1;
	char tmp[NLINE];
	tmp[0] = *buf;
	tmp[1] = EOS;
	status = cmd_complete(c, tmp, &len);
    } else
#endif
    if ((*pos != 0) && isShellOrPipe(buf)) {
#if COMPLETE_FILES
	status = shell_complete(c, buf, pos);
#else
	status = isreturn(c);
	if (c != NAMEC)
	    unkeystroke(c);
#endif
    } else {
	status = kbd_complete_bst(0, c, buf, pos);
    }
    return status;
}

int
kbd_engl_stat(const char *prompt, char *buffer, int stated)
{
    UINT kbd_flags = KBD_EXPCMD | KBD_NULLOK | ((NAMEC != ' ') ? 0 : KBD_MAYBEC);
    int code;
    static TBUFF *temp;
    ALLOC_T len = NLINE;

    tb_scopy(&temp, "");
#if COMPLETE_FILES
    init_filec(FILECOMPLETION_BufName);
#endif
    kbd_flags |= stated;
    code = kbd_reply(
			prompt,	/* no-prompt => splice */
			&temp,	/* in/out buffer */
			eol_command,
			' ',	/* eolchar */
			kbd_flags,	/* allow blank-return */
			cmd_complete);
    if (len > tb_length(temp))
	len = tb_length(temp);
    strncpy0(buffer, tb_values(temp), len);
    return code;
}

#if OPT_NAMEBST
int
insert_namebst(const char *name, const CMDFUNC * cmd, int ro)
{
    BI_DATA temp, *p;

    if ((p = btree_search(&namebst, name)) != 0) {
	if ((p->n_flags & NBST_READONLY)) {
	    if (btree_insert(&redefns, p) == 0
		|| !btree_delete(&namebst, name)) {
		return FALSE;
	    }
	    mlwrite("[Redefining builtin '%s']", name);
	} else if (!delete_namebst(name, TRUE)) {
	    return FALSE;
	}
    }

    temp.bi_key = name;
    temp.n_cmd = cmd;
    temp.n_flags = ro ? NBST_READONLY : 0;

    return (btree_insert(&namebst, &temp) != 0);
}

static void
remove_cmdfunc_ref(BINDINGS * bs, const CMDFUNC * cmd)
{
#if OPT_REBIND
    int i;
    int redo;
    KBIND *kbp;

    /* remove ascii bindings */
    for (i = 0; i < N_chars; i++)
	if (bs->kb_normal[i] == cmd)
	    bs->kb_normal[i] = 0;

    /* then look in the multi-key table */
    do {
	redo = FALSE;
	for (kbp = bs->kb_extra; kbp != bs->kb_special; kbp = kbp->k_link)
	    if (kbp->k_cmd == cmd) {
		unbindchar(bs, kbp->k_code);
		redo = TRUE;
		break;
	    }
    } while (redo);

    do {
	redo = FALSE;
	for (kbp = bs->kb_special; kbp->k_cmd; kbp++)
	    if (kbp->k_cmd == cmd) {
		unbindchar(bs, kbp->k_code);
		redo = TRUE;
		break;
	    }
    } while (redo);
#endif
}

/*
 * Lookup a name in the binary-search tree, remove it if found
 */
int
delete_namebst(const char *name, int release)
{
    BI_DATA *p = btree_search(&namebst, name);
    int code;

    /* not a named procedure */
    if (!p)
	return TRUE;

    /* we may have to free some stuff */
    if (p && release) {
	remove_cmdfunc_ref(&dft_bindings, p->n_cmd);
	remove_cmdfunc_ref(&ins_bindings, p->n_cmd);
	remove_cmdfunc_ref(&cmd_bindings, p->n_cmd);

	/* free stuff */
#if OPT_PERL
	if (p->n_cmd->c_flags & CMD_PERL)
	    perl_free_sub(CMD_U_PERL(p->n_cmd));
#endif

	if (!(p->n_flags & NBST_READONLY)) {
	    free(TYPECAST(char, p->n_cmd->c_help));
	    free(TYPECAST(char, p->n_cmd));
	}
	p->n_cmd = 0;		/* ...so old_namebst won't free this too */
    }

    if ((code = btree_delete(&namebst, name)) == TRUE) {
	if ((p = btree_search(&redefns, name)) != 0) {
	    mlwrite("[Restoring builtin '%s']", name);
	    code = (btree_insert(&namebst, p) != 0);
	    (void) btree_delete(&redefns, p->bi_key);
	}
    }
    return code;
}

/*
 * If we're renaming a procedure to another "procedure" name (i.e., bracketed),
 * rename it in the name-completion table.  Otherwise, simply remove it from the
 * name-completions.
 */
int
rename_namebst(const char *oldname, const char *newname)
{
    BI_DATA *prior;
    char name[NBUFN];

    /* not a named procedure */
    if ((prior = btree_search(&namebst, oldname)) == 0)
	return TRUE;

    /* remove the entry if the new name is not a procedure (bracketed) */
    if (!is_scratchname(newname))
	return delete_namebst(oldname, TRUE);

    /* add the new name */
    strip_brackets(name, newname);
    if ((insert_namebst(name, prior->n_cmd,
			prior->n_flags & NBST_READONLY)) != TRUE)
	return FALSE;

    /* delete the old (but don't free the data) */
    return delete_namebst(oldname, FALSE);
}

int
search_namebst(const char *name)
{
    return (btree_search(&namebst, name) != 0);
}

/*
 * Build the initial name binary search tree.  Since the nametbl is sorted we
 * do this in a binary-search manner to get a balanced tree.
 */
void
build_namebst(const NTAB * nptr, int lo, int hi)
{
    for (; lo < hi; lo++)
	if (!insert_namebst(nptr[lo].n_name, nptr[lo].n_cmd, TRUE))
	    tidy_exit(BADEXIT);
}

/*
 * This is invoked to find the closest name to complete from the current buffer
 * contents.
 */
static int
kbd_complete_bst(
		    unsigned flags GCC_UNUSED,
		    int c,	/* TESTC, NAMEC or isreturn() */
		    char *buf,
		    unsigned *pos)
{
    unsigned cpos = *pos;
    int status = FALSE;
    const char **nptr;

    kbd_init();			/* nothing to erase */
    buf[cpos] = EOS;		/* terminate it for us */

    if ((nptr = btree_parray(&namebst, buf, cpos)) != 0) {
	status = kbd_complete(0, c, buf, pos, (const char *) nptr,
			      sizeof(*nptr));
	free(TYPECAST(char, nptr));
    } else
	kbd_alarm();
    return status;
}
#endif /* OPT_NAMEBST */

#if OPT_MENUS
/* FIXME: reuse logic from makefuncdesc() */
char *
give_accelerator(char *bname)
{
    size_t i, n;
    const CMDFUNC *cmd;
    static char outseq[NLINE];

    for (n = 0; nametbl[n].n_name != 0; n++) {
	if (!strcmp(nametbl[n].n_name, bname)) {
	    cmd = nametbl[n].n_cmd;

	    outseq[0] = '\0';
	    convert_cmdfunc(&dft_bindings, cmd, outseq);

	    /* Replace \t by ' ' */
	    for (i = 0; i < strlen(outseq); i++) {
		if (outseq[i] == '\t')
		    outseq[i] = ' ';
	    }

	    return outseq;
	}
    }

    return NULL;
}
#endif /* OPT_MENUS */

#if SYS_WINNT
/*
 * At least one user wants the ability to swap ALT+Insert and CTRL+Insert
 * (mappings that copy info to the clipboard).
 *
 * If [win]vile ever gives end users the ability to map commands using
 * ALT, CTRL, and SHIFT prefixes, then swapcbrdkeys() will likely be
 * removed.
 */
int
swapcbrdkeys(int f GCC_UNUSED, int n GCC_UNUSED)
{
    const CMDFUNC *cmd;
    int rc = FALSE;

    if ((cmd = engl2fnc("copy-unnamed-reg-to-clipboard")) != NULL)
	if (install_bind(W32_KEY | VK_INSERT | W32_CTRL, cmd, &dft_bindings))
	    if ((cmd = engl2fnc("copy-to-clipboard-til")) != NULL)
		if (install_bind(W32_KEY | VK_INSERT | W32_ALT, cmd, &dft_bindings))
		    rc = TRUE;

    if (!rc)
	mlforce("[swap failed]");
    return (rc);
}
#endif

#if NO_LEAKS

#if OPT_REBIND
static void
free_all_bindings(BINDINGS * bs)
{
    while (bs->kb_extra != bs->kb_special) {
	KBIND *kbp = bs->kb_extra;
	bs->kb_extra = kbp->k_link;
	free((char *) kbp);
    }
}
#endif
void
bind_leaks(void)
{
#if OPT_REBIND
    free_all_bindings(&dft_bindings);
    free_all_bindings(&ins_bindings);
    free_all_bindings(&cmd_bindings);
    free_all_bindings(&sel_bindings);
#endif
#if OPT_NAMEBST
    btree_freeup(&redefns);
    btree_freeup(&namebst);
#endif
}
#endif /* NO_LEAKS */
