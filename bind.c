/*	This file is for functions having to do with key bindings,
 *	descriptions, help commands and startup file.
 *
 *	written 11-feb-86 by Daniel Lawrence
 *
 * $Id: bind.c,v 1.384 2025/01/26 17:07:24 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"
#include	"nefsms.h"
#include	"nename.h"

#define BI_DATA NBST_DATA
#include	"btree.h"

#define SHORT_CMD_LEN 4		/* command names longer than this are preferred
				   over those shorter.  e.g. display "quit"
				   instead of "q" if possible */

extern const int nametbl_size;

#define Strcmp(s,d)      cs_strcmp(case_insensitive, s, d)
#define StrNcmp(s,d,len) cs_strncmp(case_insensitive, s, d, len)

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
static void makebindkeyslist(LIST_ARGS);
#endif /* OPT_REBIND */

#if OPT_NAMEBST
static int kbd_complete_bst(unsigned flags, int c, char *buf, size_t *pos);
#else
#define kbd_complete_bst(params) \
	kbd_complete(params, \
			(const char *)&nametbl[0], sizeof(nametbl[0]))
#endif /* OPT_NAMEBST */

#if OPT_EVAL || OPT_REBIND
static int fnc2ntab(NTAB * result, const CMDFUNC * cfp);
static int prc2kcod(const char *kk);
#endif

BINDINGS dft_bindings =
{
    BINDINGLIST_BufName
    ,asciitbl
    ,kbindtbl
#if OPT_REBIND
    ,kbindtbl
#endif
};

static const CMDFUNC *ins_table[N_chars];
BINDINGS ins_bindings =
{
    INS_BINDINGS_BufName
    ,ins_table
    ,kbindtbl
#if OPT_REBIND
    ,kbindtbl
#endif
};

static const CMDFUNC *cmd_table[N_chars];
BINDINGS cmd_bindings =
{
    CMD_BINDINGS_BufName
    ,cmd_table
    ,kbindtbl
#if OPT_REBIND
    ,kbindtbl
#endif
};

static const CMDFUNC *sel_table[N_chars];
BINDINGS sel_bindings =
{
    SEL_BINDINGS_BufName
    ,sel_table
    ,kbindtbl
#if OPT_REBIND
    ,kbindtbl
#endif
};

static struct {
    const char *name;
    BINDINGS *binding;
} bindings_by_name[] = {

    {
	"command", &cmd_bindings
    },
    {
	"default", &dft_bindings
    },
    {
	"insert", &ins_bindings
    },
    {
	"select", &sel_bindings
    },
};

static UINT which_current;

#if OPT_REBIND
static BINDINGS *bindings_to_describe = &dft_bindings;
#endif

static void kbd_puts(const char *s);

/*----------------------------------------------------------------------------*/

BINDINGS *
vl_get_binding(const char *name)
{
    BINDINGS *result = NULL;
    unsigned n;
    for (n = 0; n < TABLESIZE(bindings_by_name); ++n) {
	if (!strncmp(bindings_by_name[n].name, name, strlen(name))) {
	    result = bindings_by_name[n].binding;
	    break;
	}
    }
    return result;
}

/*----------------------------------------------------------------------------*/

#if OPT_NAMEBST
static void
old_namebst(BI_NODE * a)
{
    beginDisplay();
    if (!(a->value.n_flags & NBST_READONLY)) {
	CMDFUNC *cmd = TYPECAST(CMDFUNC, a->value.n_cmd);
	if (cmd != NULL) {
	    if ((cmd->c_flags & CMD_TYPE) != CMD_FUNC) {
#if OPT_ONLINEHELP
		if (cmd->c_help)
		    free(TYPECAST(char, cmd->c_help));
#endif
#if OPT_MACRO_ARGS
		if (cmd->c_args) {
		    int n;
		    for (n = 0; cmd->c_args[n].pi_text != NULL; ++n) {
			free(cmd->c_args[n].pi_text);
		    }
		    free(cmd->c_args);
		}
#endif
		free(cmd);
		free(TYPECAST(char, BI_KEY(a)));
	    }
	} else {
	    free(TYPECAST(char, BI_KEY(a)));
	}
    }
    free(a);
    endofDisplay();
}

static BI_NODE *
new_namebst(BI_DATA * a)
{
    BI_NODE *p;

    beginDisplay();
    if ((p = typecalloc(BI_NODE)) != NULL) {
	p->value = *a;
	if (!(a->n_flags & NBST_READONLY)) {
	    if ((BI_KEY(p) = strmalloc(a->bi_key)) == NULL) {
		old_namebst(p);
		p = NULL;
	    }
	}
    }
    endofDisplay();

    return p;
}

static void
dpy_namebst(BI_NODE * a GCC_UNUSED, int level GCC_UNUSED)
{
#if OPT_TRACE
    TRACE(("[%d]%s%p -> %s%s (%d)\n",
	   level, alloc_indent(level, '.'),
	   (void *) a,
	   (a->value.n_flags & NBST_READONLY)
	   ? "*"
	   : "",
	   BI_KEY(a), a->balance));
#endif
}

static void
xcg_namebst(BI_NODE * a, BI_NODE * b)
{
    BI_DATA temp = a->value;
    a->value = b->value;
    b->value = temp;
}

#define BI_DATA0 {{NULL}, 0, {NULL,NULL,0}}
#define BI_TREE0 0, 0, BI_DATA0

static BI_TREE namebst =
{new_namebst, old_namebst, dpy_namebst, xcg_namebst, BI_TREE0};

static BI_TREE glbsbst =
{new_namebst, old_namebst, dpy_namebst, xcg_namebst, BI_TREE0};

static BI_TREE redefns =
{new_namebst, old_namebst, dpy_namebst, xcg_namebst, BI_TREE0};

/*----------------------------------------------------------------------------*/
/*
 * Name-completion for global commands uses a different table (a subset) from
 * the normal command-table.  Provide a way to select that table.
 */

static BI_TREE *
bst_pointer(UINT which)
{
    BI_TREE *result;

    if (which == GLOBOK) {
	result = &glbsbst;
    } else {
	result = &namebst;
    }
    return result;
}

static BI_TREE *
bst_current(void)
{
    return bst_pointer(which_current);
}
#endif /* OPT_NAMEBST */

/*----------------------------------------------------------------------------*/

int
no_such_function(const char *fnp)
{
    mlforce("[No such function \"%s\"]", NONNULL(fnp));
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

    TRACE((T_CALLED "vl_help()\n"));

    /* first check if we are already here */
    bp = bfind(HELP_BufName, BFSCRTCH);
    if (bp == NULL)
	returnCode(FALSE);

    if (bp->b_active == FALSE) {	/* never been used */
	hname = cfg_locate(helpfile, LOCATE_HELP);
	if (!hname) {
	    mlforce("[Sorry, can't find the help information]");
	    (void) zotbuf(bp);
	    returnCode(FALSE);
	}
	alreadypopped = (bp->b_nwnd != 0);
	/* and read the stuff in */
	if (readin(hname, 0, bp, TRUE) != TRUE ||
	    popupbuff(bp) == FALSE) {
	    (void) zotbuf(bp);
	    returnCode(FALSE);
	}
	set_bname(bp, HELP_BufName);
	set_rdonly(bp, hname, MDVIEW);

	set_local_b_val(bp, MDIGNCASE, TRUE);	/* easy to search, */
	b_set_scratch(bp);
	b_set_recentlychanged(bp);
	if (!alreadypopped)
	    shrinkwrap();
    }

    if (!swbuffer(bp))
	returnCode(FALSE);

    if (help_at >= 0) {
	if (!vl_gotoline(help_at))
	    returnCode(FALSE);
	mlwrite("[Type '1G' to return to start of help information]");
	help_at = -1;		/* until zotbuf is called, we let normal
				   DOT tracking keep our position */
    }
    returnCode(TRUE);
}

/*
 * translate a 10-bit key-binding to the table-pointer
 */
static KBIND *
kcode2kbind(const BINDINGS * bs, int code)
{
    KBIND *kbp;

    TRACE((T_CALLED "kcode2kbind(%s, %#x)\n", bs->bufname, code));
#if OPT_REBIND
    for (kbp = bs->kb_extra; kbp != bs->kb_special; kbp = kbp->k_link) {
	if (kbp->k_code == code) {
	    TRACE(("...found in extra-bindings\n"));
	    returnPtr(kbp);
	}
    }
#endif
    for (kbp = bs->kb_special; kbp->k_cmd; kbp++) {
	if (kbp->k_code == code) {
	    TRACE(("...found in special-bindings\n"));
	    returnPtr(kbp);
	}
    }
    returnPtr(NULL);
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
		{NULL,                  NULL,           0}
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

    bprintf("--- Terminal Character Settings ");
    bpadc('-', term.cols - DOT.o);
    bputc('\n');

    for (i = 0; TermChrs[i].name != NULL; i++) {
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
    if (name != NULL) {
	for (j = 0; TermChrs[j].name != NULL; j++)
	    if (!strcmp(name, TermChrs[j].name))
		return j;
    }
    return -1;
}

/*
 * The 'chr_complete()' and 'chr_eol()' functions are invoked from
 * 'kbd_reply()' to setup the mode-name completion and query displays.
 */
static int
chr_complete(DONE_ARGS)
{
    return kbd_complete(PASS_DONE_ARGS, (const char *) &TermChrs[0],
			sizeof(TermChrs[0]));
}

static int
/*ARGSUSED*/
chr_eol(EOL_ARGS)
{
    (void) buffer;
    (void) cpos;
    (void) eolchar;

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
		       ' ', 0, chr_complete)) == TRUE
	&& (j = chr_lookup(tb_values(name))) >= 0) {
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

    if ((value = mac_unquotedarg(&tok)) == NULL) {
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
	if (isSpecialCmd(kcmd)) {
	    c = keystroke();
	} else {
	    c = kbd_seq();
	}
    }

    if (c >= 0) {
	/* change it to something we can print as well */
	outseq[0] = EOS;
	if (vl_msgs)
	    kbd_puts(kcod2prc(c, outseq));
	hst_append_s(outseq, FALSE, TRUE);
    } else {
	mlforce("[Not a proper key-sequence]");
    }
    return c;
}

static int
free_KBIND(KBIND ** oldp, KBIND * newp)
{
    *oldp = newp->k_link;

    beginDisplay();
    free(newp);
    endofDisplay();

    return TRUE;
}

static int
unbindchar(BINDINGS * bs, int c)
{
    KBIND *kbp;			/* pointer into the command table */
    KBIND *skbp;		/* saved pointer into the command table */

    /* if it's a simple character, it will be in the normal[] array */
    if (is8Bits(c)) {
	if (bs->kb_normal[CharOf(c)]) {
	    bs->kb_normal[CharOf(c)] = NULL;
	    return TRUE;
	}
	return FALSE;
    }

    /* check first entry in kb_extra table */
    kbp = skbp = bs->kb_extra;
    if (kbp->k_code == c) {
	return free_KBIND(&(bs->kb_extra), kbp);
    }

    /* check kb_extra codes */
    while (kbp != bs->kb_special) {
	if (kbp->k_code == c) {
	    /* relink previous */
	    return free_KBIND(&(skbp->k_link), kbp);
	}

	skbp = kbp;
	kbp = kbp->k_link;
    }

    /* nope, check special codes */
    for (skbp = NULL; kbp->k_cmd; kbp++) {
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
    kbp->k_cmd = NULL;

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
    update_scratch(bs->bufname, update_binding_list);
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

    TRACE(("install_bind(%#x, %s(%s), %s)\n",
	   c, TRACE_CMDFUNC(kcmd), TRACE_BINDINGS(bs)));

    if (c < 0)
	return FALSE;		/* not a legal key-code */

    /* if the function is a prefix key, i.e. we're changing the definition
       of a prefix key, then they typed a dummy function name, which
       has been translated into a dummy function pointer */
    reset_prefix(-1, kcod2fnc(bs, c), bs);
    reset_prefix(c, kcmd, bs);

    if (is8Bits(c)) {
	bs->kb_normal[CharOf(c)] = TYPECAST(CMDFUNC, kcmd);
    } else if ((kbp = kcode2kbind(bs, c)) != NULL) {	/* change it in place */
	kbp->k_cmd = kcmd;
    } else {

	beginDisplay();
	kbp = typealloc(KBIND);
	endofDisplay();

	if (kbp == NULL) {
	    return no_memory("Key-Binding");
	}
	kbp->k_link = bs->kb_extra;
	kbp->k_code = c;	/* add keycode */
	kbp->k_cmd = kcmd;	/* and func pointer */
	bs->kb_extra = kbp;
    }
    update_scratch(bs->bufname, update_binding_list);
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
    fnp = kbd_engl("Bind function whose full name is: ", cmd, 0);

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
static int last_whichmode;
static int append_to_binding_list;

/* ARGSUSED */
static int
update_binding_list(BUFFER *bp GCC_UNUSED)
{
    return liststuff(bindings_to_describe->bufname, append_to_binding_list,
		     makebindlist, (int) last_whichcmds, (void *) last_lookup_string);
}

static int
describe_any_bindings(char *lookup, CMDFLAGS whichcmd)
{
    last_lookup_string = lookup;
    last_whichcmds = whichcmd;

    return update_binding_list((BUFFER *) 0);
}

/* ARGSUSED */
int
desbind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_any_bindings((char *) 0, (CMDFLAGS) 0);
}

/* ARGSUSED */
static int
update_bindkeys_list(BUFFER *bp GCC_UNUSED)
{
    return liststuff(bindings_to_describe->bufname, append_to_binding_list,
		     makebindkeyslist, (int) last_whichcmds, (void *) last_lookup_string);
}

static int
describe_all_bindings(char *lookup, CMDFLAGS whichcmd, int mode)
{
    last_lookup_string = lookup;
    last_whichcmds = whichcmd;
    last_whichmode = mode;

    return update_bindkeys_list((BUFFER *) 0);
}

/* ARGSUSED */
int
desall_bind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_all_bindings((char *) 0, (CMDFLAGS) 0, (f ? n : -1));
}

static int
describe_all_keys_bindings(BINDINGS * bs, int mode)
{
    int code;
    bindings_to_describe = bs;
    code = describe_all_bindings((char *) 0, (CMDFLAGS) 0, mode);
    bindings_to_describe = &dft_bindings;
    return code;
}

int
desall_i_bind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_all_keys_bindings(&ins_bindings, (f ? n : -1));
}

int
desall_c_bind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_all_keys_bindings(&cmd_bindings, (f ? n : -1));
}

int
desall_s_bind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_all_keys_bindings(&sel_bindings, (f ? n : -1));
}

static int
describe_alternate_bindings(BINDINGS * bs)
{
    int code;
    bindings_to_describe = bs;
    code = describe_any_bindings((char *) 0, (CMDFLAGS) 0);
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

/* ARGSUSED */
int
desglobals(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return describe_any_bindings((char *) 0, GLOBOK);
}

/* lookup function by substring */
/* ARGSUSED */
int
dessubstr(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int s;
    static char substring[NSTRING];

    s = mlreply("Apropos string: ", substring, (UINT) sizeof(substring));
    if (s != TRUE)
	return (s);

    return describe_any_bindings(substring, (CMDFLAGS) 0);
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
		   described_cmd + 1, 0);
    if (fnp == NULL || engl2fnc(fnp) == NULL) {
	s = no_such_function(fnp);
    } else {
	append_to_binding_list = TRUE;
	s = describe_any_bindings(described_cmd, (CMDFLAGS) 0);
	append_to_binding_list = FALSE;
    }
    return s;
}

/* FIXME: this table should be generated from cmdtbl, but excluding the
 * mouse and text pseudo-keys.  Note that the table isn't really
 * sorted alphabetically, so it would be hard to generate in a nice way.
 */
static void
make_key_names(int iarg GCC_UNUSED, void *varg GCC_UNUSED)
{
    static const struct {
	int code;
	const char *name;
    } table[] = {
	/* *INDENT-OFF* */
	{ KEY_Up,	   "KEY_Up" },
	{ KEY_Down,	   "KEY_Down" },
	{ KEY_Right,	   "KEY_Right" },
	{ KEY_Left,	   "KEY_Left" },
	{ 0,		   NULL },
	{ KEY_Delete,	   "KEY_Delete" },
	{ KEY_End,	   "KEY_End" },
	{ KEY_Find,	   "KEY_Find" },
	{ KEY_Help,	   "KEY_Help" },
	{ KEY_Home,	   "KEY_Home" },
	{ KEY_Insert,	   "KEY_Insert" },
	{ KEY_Menu,	   "KEY_Menu" },
	{ KEY_Next,	   "KEY_Next" },
	{ KEY_Prior,	   "KEY_Prior" },
	{ KEY_Select,	   "KEY_Select" },
	{ KEY_BackTab,	   "KEY_BackTab" },
	{ 0,		   NULL },
	{ KEY_F1,	   "KEY_F1" },
	{ KEY_F2,	   "KEY_F2" },
	{ KEY_F3,	   "KEY_F3" },
	{ KEY_F4,	   "KEY_F4" },
	{ KEY_F5,	   "KEY_F5" },
	{ KEY_F6,	   "KEY_F6" },
	{ KEY_F7,	   "KEY_F7" },
	{ KEY_F8,	   "KEY_F8" },
	{ KEY_F9,	   "KEY_F9" },
	{ KEY_F10,	   "KEY_F10" },
	{ KEY_F11,	   "KEY_F11" },
	{ KEY_F12,	   "KEY_F12" },
	{ KEY_F13,	   "KEY_F13" },
	{ KEY_F14,	   "KEY_F14" },
	{ KEY_F15,	   "KEY_F15" },
	{ KEY_F16,	   "KEY_F16" },
	{ KEY_F17,	   "KEY_F17" },
	{ KEY_F18,	   "KEY_F18" },
	{ KEY_F19,	   "KEY_F19" },
	{ KEY_F20,	   "KEY_F20" },
	{ KEY_F21,	   "KEY_F21" },
	{ KEY_F22,	   "KEY_F22" },
	{ KEY_F23,	   "KEY_F23" },
	{ KEY_F24,	   "KEY_F24" },
	{ KEY_F25,	   "KEY_F25" },
	{ KEY_F26,	   "KEY_F26" },
	{ KEY_F27,	   "KEY_F27" },
	{ KEY_F28,	   "KEY_F28" },
	{ KEY_F29,	   "KEY_F29" },
	{ KEY_F30,	   "KEY_F30" },
	{ KEY_F31,	   "KEY_F31" },
	{ KEY_F32,	   "KEY_F32" },
	{ KEY_F33,	   "KEY_F33" },
	{ KEY_F34,	   "KEY_F34" },
	{ KEY_F35,	   "KEY_F35" },
	{ 0,		   NULL },
	{ KEY_KP_F1,	   "KEY_KP_F1" },
	{ KEY_KP_F2,	   "KEY_KP_F2" },
	{ KEY_KP_F3,	   "KEY_KP_F3" },
	{ KEY_KP_F4,	   "KEY_KP_F4" },
	/* *INDENT-ON* */

    };
    unsigned n;
    char temp[80];

    bprintf("Coding for function and cursor-keys (your terminal may not support all):\n");
    for (n = 0; n < TABLESIZE(table); n++) {
	bputc('\n');
	if (table[n].code != 0)
	    bprintf("%s\t%s", kcod2prc(table[n].code, temp), table[n].name);
    }
}

int
des_keynames(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return liststuff(KEY_NAMES_BufName, FALSE, make_key_names, 0, (void *) 0);
}

/* lookup up function by key */
/* ARGSUSED */
static int
prompt_describe_key(BINDINGS * bs)
{
    int c;			/* character to describe */
    char outseq[NSTRING];	/* output buffer for command sequence */
    NTAB temp;			/* name table pointer */
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
    hst_append_s(outseq, FALSE, TRUE);	/* cannot replay this, but can see it */

    /* find the function bound to the key */
    if (!fnc2ntab(&temp, kcod2fnc(bs, c))) {
	mlwrite("Key sequence '%s' is not bound to anything.",
		outseq);
	return TRUE;
    }

    /* describe it */
    described_cmd[0] = '^';
    (void) vl_strncpy(described_cmd + 1, temp.n_name, sizeof(described_cmd) - 1);
    append_to_binding_list = TRUE;
    s = describe_any_bindings(described_cmd, (CMDFLAGS) 0);
    append_to_binding_list = FALSE;

    mlwrite("Key sequence '%s' is bound to function \"%s\"",
	    outseq, temp.n_name);

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
    char *result = NULL;
    if (dst != NULL) {
	result = strcat(strcat(strcpy(dst, "\""), src), "\"");
    } else {
	char temp[NLINE];
	bputsn_xcolor(quoted(temp, src), -1, XCOLOR_STRING);
    }
    return result;
}

/* returns the number of columns used by the given string */
static unsigned
converted_len(char *buffer)
{
    unsigned len = 0, c;
    while ((c = CharOf(*buffer++)) != EOS) {
	if (c == '\t')
	    len |= 7;
	len++;
    }
    return len;
}

static void
quote_and_pad(char *dst, const char *src)
{
    quoted(dst, src);
    if (dst != NULL) {
	if (converted_len(dst) >= 32)
	    strcat(dst, "\t");
	else
	    while (converted_len(dst) < 32)
		strcat(dst, "\t");
    } else {
	do {
	    bputc('\t');
	} while (getccol(FALSE) < 32);
    }
}

#if OPT_MENUS || OPT_NAMEBST
/* force the buffer to a tab-stop if needed */
static char *
to_tabstop(char *buffer)
{
    size_t len = strlen(buffer);
    unsigned cpos = converted_len(buffer);
    if (cpos & 7 || (len != 0 && !isBlank(buffer[len - 1])))
	(void) strcat(buffer, "\t");
    return skip_string(buffer);
}

/* convert a key binding, padding to the next multiple of 8 columns */
static void
convert_kcode(int c, char *buffer)
{
    if (buffer != NULL) {
	(void) kcod2prc(c, to_tabstop(buffer));
    } else {
	char temp[NLINE];
	if (llength(DOT.l) > 0
	    && !isBlank(lvalue(DOT.l)[llength(DOT.l) - 1])) {
	    bputc('\t');
	}
	(void) kcod2prc(c, temp);
	bputsn_xcolor(temp, -1, XCOLOR_STRING);
    }
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
#endif /* OPT_MENUS || OPT_NAMEBST */

static int
add_newline(void)
{
    int rc = TRUE;
    if (buf_head(curbp) != DOT.l)
	rc = bputc('\n');
    return rc;
}

static int
add_line(const char *text)
{
    int rc = add_newline();
    if (rc)
	rc = bputsn(text, -1);
    return rc;
}

#if OPT_ONLINEHELP
static int
show_onlinehelp(const CMDFUNC * cmd)
{
    char outseq[NLINE];		/* output buffer for text */
    const char *text = cmd->c_help;
    CMDFLAGS flags = cmd->c_flags;

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
    if (!add_line(outseq))
	return FALSE;
#if OPT_PROCEDURES
    if ((cmd->c_flags & CMD_TYPE) == CMD_PROC)
	flags &= ~(UNDO | REDO);
#endif
#if OPT_PERL
    if ((cmd->c_flags & CMD_TYPE) == CMD_PERL)
	flags &= ~(UNDO | REDO);
#endif
    if (flags & (RANGE | UNDO | REDO | GLOBOK)) {
	const char *gaps = "";
	char *next;

	next = lsprintf(outseq, "  ( ");
	if (cmd->c_flags & (UNDO | REDO)) {
	    {
		next = lsprintf(next, "undoable");
		gaps = ", ";
	    }
	}
	if (cmd->c_flags & RANGE) {
	    next = lsprintf(next, "%saccepts range", gaps);
	    gaps = ", ";
	}
	if (cmd->c_flags & GLOBOK) {
	    next = lsprintf(next, "%smay follow global command", gaps);
	}
	(void) lsprintf(next, " )");
	if (!add_line(outseq))
	    return FALSE;
    }
#if OPT_MACRO_ARGS
    if (cmd->c_args != NULL) {
	int i;
	for (i = 0; cmd->c_args[i].pi_type != PT_UNKNOWN; i++) {
	    (void) lsprintf(outseq, "  ( $%d = %s )", i + 1,
			    (cmd->c_args[i].pi_text != NULL)
			    ? cmd->c_args[i].pi_text
			    : choice_to_name(&fsm_paramtypes_blist,
					     cmd->c_args[i].pi_type));
	    if (!add_line(outseq))
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
    clr_typed_flags(n->value.n_flags, UCHAR, NBST_DONE);
    return 0;
}

#define isShortCmd(s) ((int) strlen(s) <= SHORT_CMD_LEN)

static int
makebind_func(BI_NODE * node, const void *d)
{
    const struct bindlist_data *data = (const struct bindlist_data *) d;
    const CMDFUNC *cmd = node->value.n_cmd;
    int n;

    /* has this been listed? */
    if (node->value.n_flags & NBST_DONE)
	return 0;

    /* are we interested in this type of command? */
    if (data->mask && !(cmd->c_flags & data->mask))
	return 0;

    /* try to avoid alphabetizing by the real short names */
    if (data->min && isShortCmd(BI_KEY(node)))
	return 0;

    /* failed lookup by substring? */
    if (data->apropos && !ourstrstr(BI_KEY(node), data->apropos, TRUE))
	return 0;

    /* add in the command name */
    quote_and_pad(NULL, BI_KEY(node));
    convert_cmdfunc(data->bs, cmd, NULL);

    node->value.n_flags |= NBST_DONE;

    if (cmd->c_alias != NULL) {
	const char *top = BI_KEY(node);
	const char *name;
	for (n = 0; (name = cmd->c_alias[n]) != NULL; ++n) {
	    if (data->min) {
		if ((isShortCmd(name)
		     || ((strcmp(name, top) > 0)
			 && !isShortCmd(top)))
		    && add_newline()
		    && bputsn("  or\t", -1)) {
		    quoted(NULL, name);
		}
	    } else {
		if ((isShortCmd(name)
		     && isShortCmd(top)
		     && strcmp(name, top) > 0)
		    && add_newline()
		    && bputsn("  or\t", -1)) {
		    quoted(NULL, name);
		}
	    }
	}
    }
#if OPT_ONLINEHELP
    if (!show_onlinehelp(cmd))
	return FALSE;
#endif
    /* blank separator */
    if (!add_line(""))
	return 1;

    return 0;
}

/* build a binding list (limited or full) */
/* ARGSUSED */
static void
makebindlist(int whichmask, void *mstring)
{
    BI_TREE *my_bst = bst_current();
    struct bindlist_data data;

    data.mask = (UINT) whichmask;
    data.min = SHORT_CMD_LEN;
    data.apropos = (char *) mstring;
    data.bs = bindings_to_describe;

    /* let us know this is in progress */
    mlwrite("[Building binding list]");

    /* clear the NBST_DONE flag */
    btree_walk(&(my_bst->head), clearflag_func, NULL);

    /* create binding list */
    if (btree_walk(&(my_bst->head), makebind_func, &data))
	return;

    /* catch entries with no synonym > SHORT_CMD_LEN */
    data.min = 0;
    if (btree_walk(&(my_bst->head), makebind_func, &data))
	return;

    mlerase();			/* clear the message line */
}

/* build a binding list (limited or full) */
/* ARGSUSED */
static void
makebindkeyslist(int whichmask, void *mstring)
{
    char outseq[NSTRING];	/* output buffer for command sequence */
    NTAB temp;			/* name table pointer */
    int ch;
    int state;

    (void) mstring;

    /* let us know this is in progress */
    mlwrite("[Building binding list]");
    for (state = 0; state < 3; ++state) {
	int modify = 0;
	int found = 0;
	const char *what = "?";
	switch (state) {
	case 0:
	    what = "Normal keys";
	    break;
	case 1:
	    what = "^A-keys";
	    modify = CTLA;
	    break;
	case 2:
	    what = "^X-keys";
	    modify = CTLX;
	    break;
	}
	bprintf("%s-- %s", state ? "\n\n" : "", what);
	for (ch = 0; ch < N_chars; ++ch) {
	    (void) kcod2prc(ch + modify, outseq);
	    if (fnc2ntab(&temp, kcod2fnc(bindings_to_describe, ch + modify))) {
		if (whichmask && !(temp.n_cmd->c_flags & (CMDFLAGS) whichmask))
		    continue;
		bputc('\n');
		bprintf("%d\t%s\t", ch, outseq);
		quoted(NULL, temp.n_name);
		++found;
	    } else if (last_whichmode > 1) {
		bputc('\n');
		bprintf("%d\t%s", ch, outseq);
	    }
	}
	if (found) {
	    bprintf("\n-- %s total: %d", what, found);
	}
    }
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

    /* dump the line */
    if (!add_line(outseq))
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
	if (!add_line(outseq))
	    return FALSE;
    }

#if OPT_ONLINEHELP
    if (!show_onlinehelp(cmd))
	return FALSE;
#endif
    /* blank separator */
    if (!add_line(""))
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
    char *listed;

    beginDisplay();
    listed = typecallocn(char, (size_t) nametbl_size);
    endofDisplay();

    if (listed == 0) {
	(void) no_memory(bindings_to_describe->bufname);
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

    beginDisplay();
    free(listed);
    endofDisplay();
}
#endif /* OPT_NAMEBST */

#endif /* OPT_REBIND */

#if OPT_REBIND || OPT_EVAL
/* much like the "standard" strstr, but if the substring starts
	with a '^', we discard it and force an exact match.
	returns 1-based offset of match, or 0.  */
int
ourstrstr(const char *haystack, const char *needle, int anchor)
{
    if (anchor && *needle == '^') {
	return strcmp(needle + 1, haystack) ? 0 : 1;
    } else {
	size_t nl = strlen(needle);
	size_t hl = strlen(haystack);
	const char *hp = haystack;
	while (hl >= nl && *hp) {
	    if (!strncmp(hp, needle, nl))
		return (int) (hp - haystack + 1);
	    hp++;
	    hl--;
	}
	return 0;
    }
}
#endif /* OPT_REBIND || OPT_EVAL */

#if SYS_UNIX
/*
 * Only source a startup file on Unix if it (and the directory in which it
 * resides) happens to be in a directory owned by the current user (or root),
 * and neither is writable by other users.
 */
static int
is_our_file(char *fname, int check_owner)
{
    int status = FALSE;
    struct stat sb;

    if (stat(fname, &sb) == 0) {
	if ((sb.st_mode & 002) != 0) {
	    TPRINTF(("Ignoring world-writable \"%s\"\n", fname));
	} else if ((sb.st_mode & 020) != 0) {
	    TPRINTF(("Ignoring group-writable \"%s\"\n", fname));
	} else if (check_owner
		   && (sb.st_uid != getuid() && sb.st_uid != 0)) {
	    TPRINTF(("\"%s\" is not our %s\n",
		     fname, S_ISDIR(sb.st_mode) ? "directory" : "file"));
	} else {
	    status = TRUE;
	}
    } else {
	TPRINTF(("\"%s\": %s\n", fname, strerror(errno)));
    }
    return status;
}
#endif

static int
check_file_access(char *fname, UINT mode)
{
    int result = ffaccess(fname, mode);

    TRACE((T_CALLED "check_file_access(%s, %#x) = %d\n", fname, mode, result));
#if SYS_UNIX
#define CHK_OWNER (FL_HOME | FL_CDIR)
    if (result) {
	int doit = FALSE;
	int owner;
#ifdef GVAL_CHK_ACCESS
	int check;
#endif

	/*
	 * Suppress ownership check for absolute pathnames.
	 */
	if (is_abs_pathname(fname))
	    mode &= ~CHK_OWNER;

	/*
	 * Check ownership for current- and home-directories.
	 */
	owner = (mode < (CHK_OWNER * 2));

	if (mode & FL_EXECABLE) {
	    doit = TRUE;
	}
#ifdef GVAL_CHK_ACCESS
	else if ((check = global_g_val(GVAL_CHK_ACCESS)) != 0) {
	    /*
	     * The values for check-access mode are single bits.  But we treat
	     * those as an ordered list.  Hence, setting "home" implies we
	     * also check "current", since that is the first item in the list.
	     */
	    if ((mode & FL_ALWAYS) && !((UINT) check & FL_ALWAYS)) {
		doit = FALSE;
	    } else if ((mode
			& ((UINT) check * 2 - 1)
			& ~(FL_EXECABLE | FL_WRITEABLE | FL_READABLE)) != 0) {
		doit = TRUE;
	    }
	}
#endif

	if (doit && !(mode & FL_INSECURE)) {
	    char *dname = (char *) malloc(NFILEN + strlen(fname) + 10);
	    char *leaf;

	    if (dname != NULL) {
		leaf = pathleaf(lengthen_path(strcpy(dname, fname)));
		if ((leaf - 1) != dname)
		    *--leaf = EOS;
		if (!is_our_file(dname, owner) || !is_our_file(fname, owner)) {
		    mlforce("[Skipping '%s' (insecure permissions)]", fname);
		    result = ABORT;
		}
		free(dname);
	    }
	}
    }
#endif

    returnCode(result);
}

static char *
locate_fname(char *dir_name, char *fname, UINT mode)
{
    static char fullpath[NFILEN];	/* expanded path */

    if (!isEmpty(dir_name)
	&& check_file_access(pathcat(fullpath, dir_name, fname), mode) == TRUE)
	return (fullpath);

    return NULL;
}

static char *
locate_file_in_list(char *list, char *fname, UINT mode)
{
    const char *cp;
    char *sp;
    char dir_name[NFILEN];
    int first = TRUE;

    if ((cp = list) != NULL) {
	while ((cp = parse_pathlist(cp, dir_name, &first)) != NULL) {
	    if ((sp = locate_fname(dir_name, fname, mode)) != NULL)
		return sp;
	}
    }
    return NULL;
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

	(void) vl_strncpy(mypath, NONNULL(prog_arg), sizeof(mypath));
	if ((tmp = vms_pathleaf(mypath)) == mypath) {
	    (void) vl_strncpy(mypath, current_directory(FALSE), sizeof(mypath));
	} else {
	    *tmp = EOS;
	}

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
    UINT mode = (which & (FL_ALWAYS |
			  FL_EXECABLE |
			  FL_WRITEABLE |
			  FL_READABLE |
			  FL_INSECURE));

#define FL_BIT(name) ((which & FL_##name) ? " " #name : "")
    TRACE((T_CALLED "cfg_locate('%s',%s%s%s%s%s%s%s%s%s%s%s)\n", NonNull(fname),
	   FL_BIT(EXECABLE),
	   FL_BIT(WRITEABLE),
	   FL_BIT(READABLE),
	   FL_BIT(CDIR),
	   FL_BIT(HOME),
	   FL_BIT(EXECDIR),
	   FL_BIT(STARTPATH),
	   FL_BIT(PATH),
	   FL_BIT(LIBDIR),
	   FL_BIT(ALWAYS),
	   FL_BIT(INSECURE)));

    /* take care of special cases */
    if (!fname || !fname[0] || isSpace(fname[0]))
	returnString(NULL);
    else if (isShellOrPipe(fname))
	returnString(fname);

#if OPT_SHELL
    /* look in the current directory */
    if (look_in_cwd && (which & FL_CDIR)) {
	if (check_file_access(fname, FL_CDIR | mode) == TRUE) {
	    returnString(fname);
	}
    }

    if (look_in_home && (which & FL_HOME)	/* look in the home directory */
	&&((sp = locate_fname(home_dir(), fname, FL_HOME | mode)) != NULL))
	returnString(sp);
#endif

    if ((which & FL_EXECDIR)	/* look in vile's bin directory */
	&&((sp = locate_fname(exec_pathname, fname, FL_EXECDIR | mode)) != NULL))
	returnString(sp);

    if ((which & FL_STARTPATH)	/* look along "VILE_STARTUP_PATH" */
	&&(sp = locate_file_in_list(startup_path,
				    fname,
				    FL_STARTPATH | mode)) != NULL)
	returnString(sp);

    if (which & FL_PATH) {	/* look along "PATH" */
#if OPT_PATHLOOKUP
	if ((sp = locate_file_in_list(PATH_value(),
				      fname,
				      FL_PATH | mode)) != NULL)
	    returnString(sp);
#endif /* OPT_PATHLOOKUP */

    }

    if ((which & FL_LIBDIR)	/* look along "VILE_LIBDIR_PATH" */
	&&(sp = locate_file_in_list(libdir_path,
				    fname,
				    FL_LIBDIR | mode)) != NULL)
	returnString(sp);

    returnString(NULL);
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
list_one_fname(char *fname, UINT mode)
{
    int tag = check_file_access(fname, mode);

    bprintf("\n%s\t", ((tag == ABORT)
		       ? "?"
		       : ((tag == TRUE)
			  ? "*"
			  : "")));

#if OPT_HYPERTEXT
    if (tag == TRUE) {
	REGION myRegion;
	MARK myDot;
	TBUFF *myBuff = NULL;
	TBUFF *myFile = NULL;

	memset(&myRegion, 0, sizeof(myRegion));
	myRegion.r_orig = DOT;
	bprintf("%s", fname);
	myRegion.r_end = DOT;

	tb_scopy(&myBuff, "view ");
	path_quote(&myFile, fname);
	bsl_to_sl_inplace(tb_values(myFile));
	tb_sappend0(&myBuff, tb_values(myFile));

	myDot = DOT;
	attributeregion_in_region(&myRegion,
				  rgn_EXACT,
				  VAREV,
				  tb_values(myBuff));
	tb_free(&myBuff);
	tb_free(&myFile);
	DOT = myDot;
    } else
#endif
	bprintf("%s", fname);
}

static void
list_which_fname(char *dir_name, char *fname, UINT mode)
{
    char fullpath[NFILEN];	/* expanded path */

    if (!isEmpty(dir_name)) {
	list_one_fname(SL_TO_BSL(pathcat(fullpath, dir_name, fname)), mode);
    }
}

static void
list_which_file_in_list(char *list, char *fname, UINT mode)
{
    const char *cp;
    char dir_name[NFILEN];
    int first = TRUE;

    if ((cp = list) != NULL) {
	while ((cp = parse_pathlist(cp, dir_name, &first)) != NULL) {
	    list_which_fname(dir_name, fname, mode);
	}
    }
}

static void
list_which(LIST_ARGS)
{
    UINT uflag = (UINT) flag;
    char *fname = (char *) ptr;
    UINT mode = (uflag & (FL_EXECABLE | FL_WRITEABLE | FL_READABLE));

    /* take care of special cases */
    if (!fname || !fname[0] || isSpace(fname[0]))
	return;
    else if (isShellOrPipe(fname))
	return;

    bprintf("Show which %s-paths are tested for:\n\t%s\n(\"*\" marks found-files)\n",
	    (mode & FL_EXECABLE) ? "executable" : "source",
	    fname);

#if OPT_SHELL
    /* look in the current directory */
    if (look_in_cwd && (uflag & FL_CDIR)) {
	bprintf("\n$cwd");
	list_one_fname(fname, FL_CDIR | mode);
    }

    if (look_in_home && (uflag & FL_HOME)) {	/* look in the home directory */
	bprintf("\n$HOME");
	list_which_fname(home_dir(), fname, FL_HOME | mode);
    }
#endif

    if (uflag & FL_EXECDIR) {	/* look in vile's bin directory */
	bprintf("\n$exec-path");
	list_which_fname(exec_pathname, fname, FL_EXECDIR | mode);
    }

    if (uflag & FL_STARTPATH) {	/* look along "VILE_STARTUP_PATH" */
	bprintf("\n$startup-path");
	list_which_file_in_list(startup_path, fname, FL_STARTPATH | mode);
    }

    if (uflag & FL_PATH) {	/* look along "PATH" */
#if OPT_PATHLOOKUP
	bprintf("\n$PATH");
	list_which_file_in_list(PATH_value(), fname, FL_PATH | mode);
#endif /* OPT_PATHLOOKUP */

    }

    if (uflag & FL_LIBDIR) {	/* look along "VILE_LIBDIR_PATH" */
	bprintf("\n$libdir-path");
	list_which_file_in_list(libdir_path, fname, FL_LIBDIR | mode);
    }
}

static int
show_which_file(char *fname, UINT mode, int f, int n)
{
    char *result;
    if ((result = cfg_locate(fname, mode)) != NULL) {
	mlwrite("%s", result);
    }
    if (f)
	liststuff(WHICH_BufName, n > 2, list_which, (int) mode, fname);
    return (result != NULL);
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

/*
 * Translate a keycode to its binding-string.
 * seq[0] gets the length of the result.
 */
char *
kcod2pstr(int c, char *seq, int limit)
{
    seq[0] = (char) kcod2escape_seq(c, &seq[1], (size_t) limit - 1);
    return seq;
}

#if OPT_KEY_MODIFY
static const struct {
    const char *name;
    int code;
} key_modifiers[] = {

    {
	"Alt+", mod_ALT
    },
    {
	"Ctrl+", mod_CTRL
    },
    {
	"Shift+", mod_SHIFT
    },
};
#endif

#define ADD_KCODE(src) \
	if (((size_t) (ptr - base) + (len = strlen(src)) + need + 2) < limit) { \
	    strcpy(ptr, src); \
	    ptr += len; \
	}

/* Translate a 16-bit keycode to a string that will replay into the same
 * code.
 */
int
kcod2escape_seq(int c, char *ptr, size_t limit)
{
    char *base = ptr;
    char temp[20];
    size_t need;

#if OPT_MULTIBYTE
    int ch = (c & ((1 << MaxCBits) - 1));
    const char *s;

    if ((ch >= 256)
	&& (cmd_encoding >= enc_UTF8
	    || (cmd_encoding <= enc_AUTO && okCTYPE2(vl_wide_enc)))
	&& (s = vl_mb_to_utf8(ch)) != NULL) {
	vl_strncpy(temp, s, sizeof(temp));
    } else
#endif
    {
	temp[0] = (char) c;
	temp[1] = EOS;
    }
    need = strlen(temp);

    if (base != NULL && limit > (4 + need)) {
	if ((UINT) c & CTLA)
	    *ptr++ = (char) cntl_a;
	else if ((UINT) c & CTLX)
	    *ptr++ = (char) cntl_x;

#if OPT_KEY_MODIFY
	if ((UINT) c & mod_KEY) {
	    size_t len;
	    unsigned n;
	    for (n = 0; n < TABLESIZE(key_modifiers); ++n) {
		if (c & key_modifiers[n].code) {
		    ADD_KCODE(key_modifiers[n].name);
		}
	    }
#if SYS_WINNT
#define W32INSERT "Insert"
#define W32DELETE "Delete"
	    c &= mod_NOMOD;
	    if (c == KEY_Insert) {
		ADD_KCODE(W32INSERT);
		c = EOS;
	    } else if (c == KEY_Delete) {
		ADD_KCODE(W32DELETE);
		c = EOS;
	    }
#endif
	}
#endif
	if ((UINT) c & SPEC)
	    *ptr++ = (char) poundc;
	strcpy(ptr, temp);
	ptr += need;
    }
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

	tmp = NULL;

	if (c & HIGHBIT) {
	    *dst++ = 'M';
	    *dst++ = '-';
	    c &= ~HIGHBIT;
	}
	if (c == ' ') {
	    tmp = "<space>";
	} else if (isCntrl(c)) {
	    *dst++ = '^';
	    *dst = (char) tocntrl(c);
	} else {
	    *dst = (char) c;
	}

	if (tmp != NULL) {
	    while ((*dst++ = *tmp++) != EOS) {
		;
	    }
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
    int length;

    length = kcod2pstr(c, temp, (int) sizeof(temp))[0];
#if OPT_KEY_MODIFY
    if (((UINT) c & mod_KEY) != 0 && (length != 0)) {
	(void) strcpy(seq, temp + 1);
	if (length < (int) (1 + strlen(temp + 1) + (size_t) (CharOf(c) == 0))) {
	    (void) bytes2prc(seq + length - 1, temp + length, 1);
	}
    } else
#endif
	(void) bytes2prc(seq, temp + 1, length);
    TRACE2(("kcod2prc(%#x) ->%s\n", c, seq));
    return seq;
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
    for (kbp = bs->kb_special; kbp->k_cmd != NULL; kbp++) {
	if (kbp->k_cmd == f)
	    return kbp->k_code;
    }

    return -1;			/* none found */
}

/* kcod2fnc:  translate a 10-bit keycode to a function pointer */
/*	(look a key binding up in the binding table)		*/
const CMDFUNC *
kcod2fnc(const BINDINGS * bs, int c)
{
    const CMDFUNC *result = NULL;

    if (bs != NULL) {
	if (!is8Bits(c)) {
	    KBIND *kp = kcode2kbind(bs, c);
	    result = (kp != NULL) ? kp->k_cmd : NULL;
	} else {
	    result = bs->kb_normal[CharOf(c)];
	}
    }
    return result;
}

/* fnc2kcod: translate a function pointer to a keycode */
int
fnc2kcod(const CMDFUNC * f)
{
    return cmdfunc2keycode(&dft_bindings, f);
}

/* fnc2kins: translate a function pointer to an insert binding keycode */
int
fnc2kins(const CMDFUNC * f)
{
    return cmdfunc2keycode(&ins_bindings, f);
}

/* fnc2pstr: translate a function pointer to a pascal-string that a user
	could enter.  returns a pointer to a static array */
#if DISP_X11
char *
fnc2pstr(const CMDFUNC * f)
{
    int c;
    static char seq[40];

    c = fnc2kcod(f);

    if (c == -1)
	return NULL;

    return kcod2pstr(c, seq, (int) sizeof(seq));
}
#endif

#if OPT_EVAL || OPT_REBIND

#if OPT_NAMEBST
/*
 * Search for a matching CMDFUNC pointer, return the corresponding name.  We
 * use this rather than extracting from the buffer name, since it may be
 * ambiguous if the macro's name is longer than NBUFN-2.
 */
static int
match_cmdfunc(BI_NODE * node, const void *d)
{
    NTAB *result = (NTAB *) TYPECAST(void *, d);

    if (node->value.n_cmd == result->n_cmd) {
	result->n_name = (char *) TYPECAST(void *, node->value.bi_key);
	return 1;
    }
    return 0;
}
#endif

/*
 * Fill-in an NTAB with the data for the command.
 */
static int
fnc2ntab(NTAB * result, const CMDFUNC * cfp)
{
    int found = FALSE;

    if (cfp != NULL) {
#if OPT_NAMEBST
	result->n_name = NULL;
	result->n_cmd = cfp;
	btree_walk(&(bst_current()->head), match_cmdfunc, result);
	found = (result->n_name != NULL);
#else
	switch (cfp->c_flags & CMD_TYPE) {
	    const NTAB *nptr;

	case CMD_FUNC:		/* normal CmdFunc */
	    for (nptr = nametbl; nptr->n_cmd; nptr++) {
		if (nptr->n_cmd == cfp) {
		    /* if it's a long name, return it */
		    if ((int) strlen(nptr->n_name) > SHORT_CMD_LEN) {
			found = TRUE;
			*result = *nptr;
			break;
		    }
		    /* remember the first short name, in case there's
		       no long name */
		    if (!result) {
			*result = *nptr;
			found = SORTOFTRUE;
		    }
		}
	    }
	    break;

#if OPT_PROCEDURES
	case CMD_PROC:		/* named procedure */
	    result->n_name = CMD_U_BUFF(cfp)->b_bname;	/* not very good */
	    result->n_cmd = cfp;
	    found = TRUE;
	    break;
#endif

#if OPT_PERL
	case CMD_PERL:		/* perl subroutine */
	    result->n_name = "Perl subroutine";		/* FIXME - what name? */
	    result->n_cmd = cfp;
	    found = TRUE;
	    break;
#endif
	}
#endif /* OPT_NAMEBST */

	TRACE(("fnc2ntab(%s)\n", found ? result->n_name : ""));
    }
    return found;
}

/* fnc2engl: translate a function pointer to the english name for
		that function.  prefer long names to short ones.
*/
const char *
fnc2engl(const CMDFUNC * cfp)
{
    NTAB temp;
    return fnc2ntab(&temp, cfp) ? temp.n_name : NULL;
}

#endif

/* engl2fnc: match name to a function in the names table
	translate english name to function pointer
		 return any match or NULL if none
 */
const CMDFUNC *
engl2fnc(const char *fname)
{
#if OPT_NAMEBST
    BI_NODE *n = btree_pmatch(BI_RIGHT(&(bst_current()->head)), TRUE, fname);

    if (n != NULL)
	return n->value.n_cmd;
#else
    /* this runs 10 times faster for 'nametbl[]' */
    int lo, hi, cur;
    int r;
    size_t len = strlen(fname);

    if (len != 0) {

	/* scan through the table, returning any match */
	lo = 0;
	hi = nametbl_size - 2;	/* don't want last entry -- it's NULL */

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
    }
#endif /* OPT_NAMEBST */
    return NULL;
}

/*
 * This parser allows too many combinations; we will check for illegal
 * combinations after getting all of the information.  Still, it will not check
 * for odd things such as
 *	#-^X-a
 * since it is treated the same as
 *	^X-#-a
 */
#if OPT_EVAL || OPT_REBIND
static const char *
decode_prefix(const char *kk, UINT * prefix)
{
    UCHAR ch;
    int found;

    do {
	UINT bits = 0;
	size_t len = strlen(kk);

	if (len > 1) {
	    ch = (UCHAR) (kk[0]);
	    if (ch == cntl_a)
		bits = CTLA;
	    else if (ch == cntl_x)
		bits = CTLX;
	    else if (ch == poundc)
		bits = SPEC;
	    if (bits != 0) {
		kk++;
		if (len > 2 && *kk == '-')
		    kk++;
	    }
	}

	if (bits == 0 && len > 3 && *(kk + 2) == '-') {
	    if (*kk == '^') {
		ch = (UCHAR) (kk[1]);
		if (isCntrl(cntl_a) && ch == (UCHAR) toalpha(cntl_a))
		    bits = CTLA;
		if (isCntrl(cntl_x) && ch == (UCHAR) toalpha(cntl_x))
		    bits = CTLX;
		if (isCntrl(poundc) && ch == (UCHAR) toalpha(poundc))
		    bits = SPEC;
	    } else if (!strncmp(kk, "FN", (size_t) 2)) {
		bits = SPEC;
	    }
	    if (bits != 0)
		kk += 3;
	}

	found = FALSE;
	if (bits != 0) {
	    found = TRUE;
	    *prefix |= bits;
	}
    } while (found);
    return kk;
}

/* prc2kcod: translate printable code to 10 bit keycode */
static int
prc2kcod(const char *kk)
{
    UINT kcod;			/* code to return */
    UINT pref = 0;		/* key prefixes */

    kk = decode_prefix(kk, &pref);
    if ((pref & CTLA) && (pref & CTLX))
	return -1;

#if OPT_KEY_MODIFY
    {
	int found = FALSE;
	UINT last, len, n;
	char *s;
	char temp[NSTRING];

	while ((s = strchr(kk, '+')) != NULL) {
	    if ((len = (UINT) (s - kk) + 1) >= sizeof(temp))
		break;
	    if (s == kk || s[1] == '\n' || s[1] == '\0')
		break;
	    mklower(vl_strncpy(temp, kk, (size_t) len + 1));
	    if (isLower(temp[0]))
		temp[0] = (char) toUpper(temp[0]);
	    for (n = 0; n < TABLESIZE(key_modifiers); ++n) {
		if (!strncmp(key_modifiers[n].name, temp, (size_t) len)) {
		    found = TRUE;
		    pref |= ((UINT) key_modifiers[n].code | mod_KEY);
		    break;
		}
	    }
	    kk += len;
	}
	/*
	 * SPEC would be set after a modifier, e.g.,
	 *      control+#6
	 *      control+FN-6
	 * but it would be too confusing to allow
	 *      control+^A-x
	 *      FN-control+6
	 */
	if (found) {
	    last = pref;
	    pref = 0;
	    kk = decode_prefix(kk, &pref);
	    if ((pref & (CTLA | CTLX)) != 0
		|| (last & pref & SPEC) != 0)
		return -1;
	    pref |= last;
	}
    }
#endif

    if (strlen(kk) > 2 && !strncmp(kk, "M-", (size_t) 2)) {
	pref |= HIGHBIT;
	kk += 2;
    }

    if (*kk == '^' && kk[1] != EOS) {	/* control character */
	kcod = (UCHAR) kk[1];
	if (isLower(kcod))
	    kcod = (UINT) toUpper(kcod);
	kcod = tocntrl(kcod);
	kk += 2;
    } else {			/* any single char, control or not */
	kcod = (UCHAR) (*kk++);
    }

    if (*kk != EOS)		/* we should have eaten the whole thing */
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
	return error_val;

    englname = fnc2engl(DefaultKeyBinding(kcod));
    if (englname == NULL)
	englname = error_val;

    return englname;
}
#endif

/*
 * Get an english command name from the user.  If 'prompt' is a null pointer,
 * we will splice calls.
 */
char *
kbd_engl(const char *prompt, char *buffer, UINT which)
{
    char *result = NULL;
    if (kbd_engl_stat(prompt, buffer, which, 0) == TRUE)
	result = buffer;
    return result;
}

/* sound the alarm! */
#if OPT_TRACE
void
trace_alarm(const char *file, int line)
#else
void
kbd_alarm(void)
#endif
{
    TRACE(("BEEP (%s @%d)\n", file, line));
    TPRINTF(("BEEP\n"));

    if (!no_errs
	&& !clhide
	&& !quiet
	&& global_g_val(GMDERRORBELLS)) {
	term.beep();
	term.flush();
    }
    warnings++;
}

void
kbd_start(KBD_DATA * data)
{
    beginDisplay();

#if OPT_MULTIBYTE
    make_local_b_val(bminip, VAL_FILE_ENCODING);
    if (cmd_encoding == enc_AUTO) {
	set_b_val(bminip, VAL_FILE_ENCODING, b_val(curbp, VAL_FILE_ENCODING));
    } else {
	set_b_val(bminip, VAL_FILE_ENCODING, cmd_encoding);
    }
#endif

    data->save_bp = curbp;
    data->save_wp = curwp;
    data->save_mk = MK;

    curbp = bminip;
    curwp = wminip;
    MK = DOT;
}

void
kbd_finish(KBD_DATA * data)
{
    curbp = data->save_bp;
    curwp = data->save_wp;
    MK = data->save_mk;

    endofDisplay();
}

/* put a character to the keyboard-prompt */
int
kbd_putc(int c)
{
    KBD_DATA save;
    int status;

    kbd_start(&save);
    if ((kbd_expand <= 0) && isreturn(c)) {
	status = kbd_erase_to_end(0);
    } else {
	if ((kbd_expand < 0) && (c == '\t')) {
	    status = lins_bytes(1, ' ');
	} else {
	    status = lins_bytes(1, c);
	}
#ifdef VILE_DEBUG
	TRACE(("mini:%2d:%s\n", llength(DOT.l), lp_visible(DOT.l)));
#endif
    }
    kbd_finish(&save);

    return status;
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
    if (vl_echo && !clhide) {
	KBD_DATA save;

	kbd_start(&save);
	if (DOT.o > 0) {
	    backchar_to_bol(TRUE, 1);
	    ldel_chars((B_COUNT) 1, FALSE);
	}
#ifdef VILE_DEBUG
	TRACE(("MINI:%2d:%s\n", llength(DOT.l), lp_visible(DOT.l)));
#endif
	kbd_finish(&save);
    }
}

int
kbd_erase_to_end(int column)
{
    if (vl_echo && !clhide) {
	KBD_DATA save;

	kbd_start(&save);
	if (llength(DOT.l) > 0) {
	    DOT.o = column;
	    if (llength(DOT.l) > DOT.o)
		ldel_bytes((B_COUNT) (llength(DOT.l) - DOT.o), FALSE);
	    TRACE(("NULL:%2d:%s\n", llength(DOT.l), lp_visible(DOT.l)));
	}
	kbd_finish(&save);
    }

    return TRUE;
}

int
cs_strcmp(
	     int case_insensitive,
	     const char *s1,
	     const char *s2)
{
    if (case_insensitive)
	return stricmp(s1, s2);
    return strcmp(s1, s2);
}

int
cs_strncmp(
	      int case_insensitive,
	      const char *s1,
	      const char *s2,
	      size_t n)
{
    if (case_insensitive)
	return strnicmp(s1, s2, n);
    return strncmp(s1, s2, n);
}

/* definitions for name-completion */
#define	NEXT_DATA(p)	((p)+size_entry)
#define	PREV_DATA(p)	((p)-size_entry)

#define	THIS_NAME(p)	(*TYPECAST(const char *const,p))
#define	NEXT_NAME(p)	THIS_NAME(NEXT_DATA(p))

/*
 * Scan down until we no longer match the current input, or reach the end of
 * the symbol table.
 */
/*ARGSUSED*/
static const char *
skip_partial(
		int case_insensitive,
		char *buf,
		size_t len,
		const char *table,
		size_t size_entry)
{
    const char *next = NEXT_DATA(table);
    const char *sp;

    while ((sp = THIS_NAME(next)) != NULL) {
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
		size_t len,
		const char *table,
		size_t size_entry)
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
    size_t len;
    const char *table;
    size_t size_entry;
};

#ifdef lint
#define c2ComplRec(c) ((struct compl_rec *)0)
#else
#define c2ComplRec(c) ((struct compl_rec *)c)
#endif

/*ARGSUSED*/
static void
makecmpllist(int case_insensitive, void *cinfop)
{
    char *buf = c2ComplRec(cinfop)->buf;
    size_t len = c2ComplRec(cinfop)->len;
    const char *first = c2ComplRec(cinfop)->table;
    size_t size_entry = c2ComplRec(cinfop)->size_entry;
    const char *last = skip_partial(case_insensitive, buf, len, first, size_entry);
    const char *p;
    size_t maxlen;
    size_t slashcol;
    int cmpllen;
    int cmplcols;
    int cmplrows;
    int nentries;
    int i, j;

    for (p = NEXT_DATA(first), maxlen = strlen(THIS_NAME(first));
	 p != last;
	 p = NEXT_DATA(p)) {
	size_t l = strlen(THIS_NAME(p));
	if (l > maxlen)
	    maxlen = l;
    }

    slashcol = (size_t) (pathleaf(buf) - buf);
    if (slashcol != 0) {
	char b[NLINE];
	(void) strncpy(b, buf, slashcol);
	(void) strncpy(&b[slashcol], &(THIS_NAME(first))[slashcol],
		       (len - slashcol));
	b[slashcol + (len - slashcol)] = EOS;
	bprintf("Completions prefixed by %s:\n", b);
    }

    cmplcols = term.cols / (int) (maxlen - slashcol + 1);

    if (cmplcols == 0)
	cmplcols = 1;

    nentries = (int) ((size_t) (last - first) / size_entry);
    cmplrows = nentries / cmplcols;
    cmpllen = term.cols / cmplcols;
    if (cmplrows * cmplcols < nentries)
	cmplrows++;

    for (i = 0; i < cmplrows; i++) {
	for (j = 0; j < cmplcols; j++) {
	    int idx = cmplrows * j + i;
	    if (idx < nentries) {
		const char *s = (THIS_NAME(first
					   + ((size_t) idx * size_entry))
				 + slashcol);
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
		    size_t len,
		    const char *table,
		    size_t size_entry)
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
		      size_t len,
		      const char *table,
		      size_t size_entry)
{
    BUFFER *bp;

    if ((bp = find_b_name(COMPLETIONS_BufName)) == NULL)
	show_completions(case_insensitive, buf, len, table, size_entry);
    else {
	LINE *lp;
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
popdown_completions(const char *old_bname, WINDOW *old_wp)
{
    BUFFER *bp;
    if ((bp = find_b_name(COMPLETIONS_BufName)) != NULL) {
	zotwp(bp);
	if (strcmp(old_bname, curbp->b_bname)) {
	    if (!strcmp(old_wp->w_bufp->b_bname, old_bname)) {
		set_curwp(old_wp);
	    } else if ((bp = find_b_name(old_bname)) != NULL) {
		swbuffer(bp);
	    }
	}
    }
}
#endif /* OPT_POPUPCHOICE */

/*
 * Attempt to partial-complete the string, char at a time
 */
static size_t
fill_partial(
		int case_insensitive,
		char *buf,
		size_t pos,
		const char *first,
		const char *last,
		size_t size_entry)
{
    const char *p;
    size_t n = pos;
    const char *this_name = THIS_NAME(first);

    TRACE(("fill_partial(%d:%.*s) first=%s, last=%s\n",
	   (int) pos, (int) pos,
	   TRACE_NULL(buf),
	   TRACE_NULL(THIS_NAME(first)),
	   TRACE_NULL(THIS_NAME(last))));

    assert(buf != NULL);
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
	TRACE(("...added(%d:%c)\n", (int) n, buf[n]));

	/* scan through the candidates */
	for (p = first; p != last; p = NEXT_DATA(p)) {
	    if (StrNcmp(&THIS_NAME(p)[n], &buf[n], (size_t) 1) != 0) {
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
		TRACE(("...fill_partial %d\n", (int) n));
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
    if (wminip != NULL
	&& wminip->w_dot.l != NULL
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
kbd_complete(DONE_ARGS, const char *table, size_t size_entry)
{
    int case_insensitive = (flags & KBD_CASELESS) != 0;
    size_t cpos = *pos;
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

    if (nbp == NULL)
	return FALSE;

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
				(size_t) (NLINE - 1));
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
    if ((flags & (KBD_MAYBEC | KBD_MAYBEC2)) != 0) {
	status = (cpos != 0);
    } else {
	if (clexec) {
#if OPT_MODELINE
	    if (in_modeline < 2)
#endif
		mlwarn("[No match for '%s']", buf);
	} else {
	    kbd_alarm();
	}
	status = FALSE;
    }
    return status;
}

/*
 * Test a buffer to see if it looks like a shift-command, which may have
 * repeated characters (but they must all be the same).
 */
static int
is_shift_cmd(const char *buffer, size_t cpos)
{
    if (buffer != NULL) {
	int c = *buffer;
	if (isRepeatable(c)) {
	    while (--cpos != 0)
		if (*(++buffer) != c)
		    return FALSE;
	    return TRUE;
	}
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

#define ismostpunct(c) (isPunct(c) && (c) != '-' && (c) != '_')

int
eol_command(EOL_ARGS)
{
    int result = FALSE;

    if (is_shift_cmd(buffer, cpos) && (c == *buffer)) {
	/*
	 * Handle special case of repeated-character implying repeat-count
	 */
	result = TRUE;
    } else if ((cpos != 0) && isShellOrPipe(buffer)) {
	/*
	 * Shell-commands aren't complete until the line is complete.
	 */
	result = isreturn(c);
    } else if (c == eolchar) {
	result = TRUE;
    } else if (cpos != 0 && cpos < 3
	       && ((!ismostpunct(c)
		    && ismostpunct(buffer[cpos - 1]))
		   || ((c != '!' && ismostpunct(c))
		       && (buffer[cpos - 1] == '!'
			   || !ismostpunct(buffer[cpos - 1]))
		   )
	       )) {
	result = TRUE;
    }
    return result;
}

/*
 * This procedure is invoked from 'kbd_string()' to setup the command-name
 * completion and query displays.
 */
int
cmd_complete(DONE_ARGS)
{
    int status;
    if (buf != NULL && pos != NULL) {
#if OPT_HISTORY
	/*
	 * If the user scrolled back in 'edithistory()', the text may be a
	 * repeated-shift command, which won't match the command-table (e.g.,
	 * ">>>").
	 */
	if ((*pos > 1) && is_shift_cmd(buf, *pos)) {
	    size_t len = 1;
	    char tmp[NLINE];
	    tmp[0] = *buf;
	    tmp[1] = EOS;
	    status = cmd_complete(0, c, tmp, &len);
	} else
#endif
	if ((*pos != 0) && isShellOrPipe(buf)) {
#if COMPLETE_FILES
	    status = shell_complete(PASS_DONE_ARGS);
#else
	    status = isreturn(c);
	    if (c != NAMEC)
		unkeystroke(c);
#endif
	} else {
	    status = kbd_complete_bst(PASS_DONE_ARGS);
	}
    } else {
	status = ABORT;
    }
    return status;
}

int
kbd_engl_stat(const char *prompt, char *buffer, UINT which, int stated)
{
    KBD_OPTIONS kbd_flags = KBD_EXPCMD | KBD_NULLOK | ((NAMEC != ' ') ? 0 : KBD_MAYBEC);
    int code;
    static TBUFF *temp;
    size_t len = NLINE;

    which_current = which;

    tb_scopy(&temp, "");
    init_filec(FILECOMPLETION_BufName);
    kbd_flags |= (KBD_OPTIONS) stated;
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

    which_current = 0;
    return code;
}

#if OPT_NAMEBST
int
insert_namebst(const char *name, const CMDFUNC * cmd, int ro, UINT which)
{
    int result;

    TRACE2((T_CALLED "insert_namebst(%s,%s)\n", name, ro ? "ro" : "rw"));
    if (name != NULL) {
	BI_TREE *my_bst = bst_pointer(which);
	BI_DATA temp, *p;

	if ((p = btree_search(my_bst, name)) != NULL) {
	    if ((p->n_flags & NBST_READONLY)) {
		if (btree_insert(&redefns, p) == NULL) {
		    returnCode(FALSE);
		} else {
		    if (!btree_delete(my_bst, name)) {
			returnCode(FALSE);
		    }
		}
		mlwrite("[Redefining builtin '%s']", name);
	    } else {
		if (!delete_namebst(name, TRUE, TRUE, which)) {
		    returnCode(FALSE);
		}
	    }
	}

	temp.bi_key = name;
	temp.n_cmd = cmd;
	temp.n_flags = (UCHAR) (ro ? NBST_READONLY : 0);

	result = (btree_insert(my_bst, &temp) != NULL);
    } else {
	result = FALSE;
    }
    return2Code(result);
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
	    bs->kb_normal[i] = NULL;

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
delete_namebst(const char *name, int release, int redefining, UINT which)
{
    BI_TREE *my_bst = bst_pointer(which);
    BI_DATA *p = btree_search(my_bst, name);
    int code = TRUE;

    TRACE((T_CALLED "delete_namebst(%s,%d) %p\n", name, release, (void *) p));
    /* not a named procedure */
    if ((p = btree_search(my_bst, name)) != NULL) {

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
		beginDisplay();
		free(TYPECAST(char, p->n_cmd->c_help));
		free(TYPECAST(char, p->n_cmd));
		endofDisplay();
	    }
	    p->n_cmd = NULL;	/* ...so old_namebst won't free this too */
	}

	if ((code = btree_delete(my_bst, name)) == TRUE) {
	    if (!redefining) {
		if ((p = btree_search(&redefns, name)) != NULL) {
		    mlwrite("[Restoring builtin '%s']", name);
		    code = (btree_insert(my_bst, p) != NULL);
		    (void) btree_delete(&redefns, p->bi_key);
		}
	    }
	}
    }
    returnCode(code);
}

/*
 * If we're renaming a procedure to another "procedure" name (i.e., bracketed),
 * rename it in the name-completion table.  Otherwise, simply remove it from the
 * name-completions.
 */
int
rename_namebst(const char *oldname, const char *newname, UINT which)
{
    BI_DATA *prior;
    char name[NBUFN];

    /* not a named procedure */
    if ((prior = btree_search(bst_pointer(which), oldname)) == NULL)
	return TRUE;

    /* remove the entry if the new name is not a procedure (bracketed) */
    if (!is_scratchname(newname))
	return delete_namebst(oldname, TRUE, FALSE, which);

    /* add the new name */
    strip_brackets(name, newname);
    if ((insert_namebst(name,
			prior->n_cmd,
			prior->n_flags & NBST_READONLY,
			which)) != TRUE)
	return FALSE;

    /* delete the old (but don't free the data) */
    return delete_namebst(oldname, FALSE, FALSE, which);
}

int
search_namebst(const char *name, UINT which)
{
    return (btree_search(bst_pointer(which), name) != NULL);
}

/*
 * Build the initial name binary search tree.  Since the nametbl is sorted we
 * do this in a binary-search manner to get a balanced tree.
 */
void
build_namebst(const NTAB * nptr, int lo, int hi, UINT which)
{
    for (; lo < hi; lo++) {
	if (!insert_namebst(nptr[lo].n_name, nptr[lo].n_cmd, TRUE, which))
	    tidy_exit(BADEXIT);
    }
}

/*
 * This is invoked to find the closest name to complete from the current buffer
 * contents.
 */
static int
kbd_complete_bst(unsigned flags,
		 int c,		/* TESTC, NAMEC or isreturn() */
		 char *buf,
		 size_t *pos)
{
    size_t cpos = *pos;
    int status = FALSE;
    const char **nptr;

    (void) flags;

    kbd_init();			/* nothing to erase */
    buf[cpos] = EOS;		/* terminate it for us */

    if ((nptr = btree_parray(bst_current(), buf, cpos)) != NULL) {
	status = kbd_complete(0, c, buf, pos, (const char *) nptr,
			      sizeof(*nptr));
	beginDisplay();
	free(TYPECAST(char, nptr));
	endofDisplay();
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
    size_t i;
    int n;
    const CMDFUNC *cmd;
    static char outseq[NLINE];

    if ((n = blist_match(&blist_nametbl, bname)) >= 0) {
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
	if (install_bind(mod_KEY | KEY_Insert | mod_CTRL, cmd, &dft_bindings))
	    if ((cmd = engl2fnc("copy-to-clipboard-til")) != NULL)
		if (install_bind(mod_KEY | KEY_Insert | mod_ALT, cmd, &dft_bindings))
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

static void
free_ext_bindings(BINDINGS * bs)
{
    free_all_bindings(bs);
    FreeAndNull(bs->kb_special);
}
#endif
void
bind_leaks(void)
{
    btree_printf(&namebst);
#if OPT_REBIND
    free_all_bindings(&dft_bindings);
    free_ext_bindings(&ins_bindings);
    free_ext_bindings(&cmd_bindings);
    free_ext_bindings(&sel_bindings);
#endif
#if OPT_NAMEBST
    btree_freeup(&redefns);
    btree_freeup(&namebst);
    btree_freeup(&glbsbst);
#endif
}
#endif /* NO_LEAKS */
