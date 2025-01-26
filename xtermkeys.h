/*
 * $Id: xtermkeys.h,v 1.20 2025/01/26 11:54:25 tom Exp $
 *
 * Function-key definitions and modifiers used for xterm.  This is a header
 * file to simplify sharing between the termcap/curses drivers.
 */

#define DATA(tc,ti,code) { CAPNAME(tc,ti), code, NULL }
/* *INDENT-OFF* */
static struct {
    const char *capname;
    int code;
    const char * result;
} keyseqs[] = {
    /* Arrow keys */
    DATA( "ku","kcuu1",	KEY_Up ),
    DATA( "kd","kcud1",	KEY_Down ),
    DATA( "kr","kcuf1",	KEY_Right ),
    DATA( "kl","kcub1",	KEY_Left ),
    /* other cursor-movement */
    DATA( "kh","khome",	KEY_Home ),
    DATA( "kH","kll",	KEY_End ),
    DATA( "@7","kend",	KEY_End ),
    DATA( "kB","kcbt",	KEY_BackTab ),		/* back-tab */
    /* page scroll */
    DATA( "kN","knp",	KEY_Next ),
    DATA( "kP","kpp",	KEY_Prior ),
    /* editing */
    DATA( "kI","kich1",	KEY_Insert ),
    DATA( "kD","kdch1",	KEY_Delete ),
    DATA( "@0","kfnd",	KEY_Find ),
    DATA( "*6","kslt",	KEY_Select ),
    /* command */
    DATA( "%1","khlp",	KEY_Help ),
    /* function keys */
    DATA( "k1","kf1",	KEY_F1 ),
    DATA( "k2","kf2",	KEY_F2 ),
    DATA( "k3","kf3",	KEY_F3 ),
    DATA( "k4","kf4",	KEY_F4 ),
    DATA( "k5","kf5",	KEY_F5 ),
    DATA( "k6","kf6",	KEY_F6 ),
    DATA( "k7","kf7",	KEY_F7 ),
    DATA( "k8","kf8",	KEY_F8 ),
    DATA( "k9","kf9",	KEY_F9 ),
    DATA( "k;","kf10",	KEY_F10 ),
    DATA( "F1","kf11",	KEY_F11 ),
    DATA( "F2","kf12",	KEY_F12 ),
    DATA( "F3","kf13",	KEY_F13 ),
    DATA( "F4","kf14",	KEY_F14 ),
    DATA( "F5","kf15",	KEY_F15 ),
    DATA( "F6","kf16",	KEY_F16 ),
    DATA( "F7","kf17",	KEY_F17 ),
    DATA( "F8","kf18",	KEY_F18 ),
    DATA( "F9","kf19",	KEY_F19 ),
    DATA( "FA","kf20",	KEY_F20 ),
    DATA( "FB","kf21",	KEY_F21 ),
    DATA( "FC","kf22",	KEY_F22 ),
    DATA( "FD","kf23",	KEY_F23 ),
    DATA( "FE","kf24",	KEY_F24 ),
    DATA( "FF","kf25",	KEY_F25 ),
    DATA( "FG","kf26",	KEY_F26 ),
    DATA( "FH","kf27",	KEY_F27 ),
    DATA( "FI","kf28",	KEY_F28 ),
    DATA( "FJ","kf29",	KEY_F29 ),
    DATA( "FK","kf30",	KEY_F30 ),
    DATA( "FL","kf31",	KEY_F31 ),
    DATA( "FM","kf32",	KEY_F32 ),
    DATA( "FN","kf33",	KEY_F33 ),
    DATA( "FO","kf34",	KEY_F34 ),
    DATA( "FP","kf35",	KEY_F35 ),
    /* shifted keys */
    DATA( "#1","kHLP", 0 ),
    DATA( "*0","kFND", 0 ),
    DATA( "*7","kEND", 0 ),
    DATA( "*4","kDC",  0 ),
    DATA( "#3","kIC",  0 ),
    DATA( "%c","kNXT", 0 ),
    DATA( "%e","kPRV", 0 ),
    DATA( "kF","kind", 0 ),
    DATA( "#2","kHOM", 0 ),
    DATA( "#4","kLFT", 0 ),
    DATA( "%i","kRIT", 0 ),
    DATA( "kR","kri",  0 ),
};
#undef DATA
/* *INDENT-ON* */

#if OPT_KEY_MODIFY
/* *INDENT-OFF* */
static const struct {
    int code;
    int mask;
} xterm_mods[] = {
    { 2, mod_KEY | mod_SHIFT },
    { 3, mod_KEY | mod_ALT },
    { 4, mod_KEY | mod_ALT | mod_SHIFT },
    { 5, mod_KEY | mod_CTRL },
    { 6, mod_KEY | mod_CTRL | mod_SHIFT },
    { 7, mod_KEY | mod_CTRL | mod_ALT },
    { 8, mod_KEY | mod_CTRL | mod_ALT | mod_SHIFT }
};

/*
 * Shifted cursor- and editing-keypad keys may be supported in termcap or
 * terminfo.  There are a few ambiguities, e.g., the relationship of kcud1
 * and kind, which depend on a particular convention.
 */
static const struct {
    const char *normal;
    const char *shifted;
} shifted_keys[] = {
    { CAPNAME("%1", "khlp"),  CAPNAME("#1", "kHLP") },
    { CAPNAME("@0", "kfnd"),  CAPNAME("*0", "kFND") },
    { CAPNAME("@7", "kend"),  CAPNAME("*7", "kEND") },
    { CAPNAME("kD", "kdch1"), CAPNAME("*4", "kDC") },
    { CAPNAME("kI", "kich1"), CAPNAME("#3", "kIC") },
    { CAPNAME("kN", "knp"),   CAPNAME("%c", "kNXT") },
    { CAPNAME("kP", "kpp"),   CAPNAME("%e", "kPRV") },
    { CAPNAME("kd", "kcud1"), CAPNAME("kF", "kind") },
    { CAPNAME("kh", "khome"), CAPNAME("#2", "kHOM") },
    { CAPNAME("kl", "kcub1"), CAPNAME("#4", "kLFT") },
    { CAPNAME("kr", "kcuf1"), CAPNAME("%i", "kRIT") },
    { CAPNAME("ku", "kcuu1"), CAPNAME("kR", "kri") },
};

#if 0
static const struct {
    const char *normal;
    const char *extbase;
} extended_keys[] = {
};
#endif
/* *INDENT-ON* */

static const char *
skip_csi(const char *string)
{
    const char *result = string;

    if (!strncmp(string, "\033[", (size_t) 2))
	result = string + 2;
    else if (!strncmp(string, "\233", (size_t) 1))
	result = string + 1;

    return result;
}

static int
is_csi(const char *string)
{
    return (skip_csi(string) != string);
}

static int
has_param(const char *string)
{
    return (strchr(string, ';') != NULL);
}

static int
format_modified(char *target, const char *source, int length, int which)
{
    int result = length + 2 + (xterm_mods[which].code > 9);
    const char *params = skip_csi(source);

    if (isDigit(*params)) {
	sprintf(target, "%.*s;%d%s",
		length - 1,
		source,
		xterm_mods[which].code,
		source + length - 1);
    } else {
	++result;
	sprintf(target, "%.*s1;%d%s",
		(int) (params - source),
		source,
		xterm_mods[which].code,
		params);
    }
    return result;
}

static void
add_modified_key(const char *source, int len, int code, int modify)
{
    if (source != NULL && len > 0) {
	if (modify < 0) {
	    delfromsysmap(source, len);
	} else {
	    addtosysmap(source, len, code);
	}
    }
}

static void
add_fkey(const char *string, int length, int code, int modify)
{
    char buffer[80];
    unsigned n;

    if (modify) {
	if (length >= 3
	    && (length + 2) < (int) sizeof(buffer)
	    && length == (int) strlen(string)
	    && is_csi(string)
	    && !has_param(string)) {
	    for (n = 0; n < TABLESIZE(xterm_mods); ++n) {
		int lenn = format_modified(buffer, string, length, (int) n);
		add_modified_key(buffer, lenn, code | xterm_mods[n].mask, modify);
	    }
	}
    } else {
	addtosysmap(string, length, code);
    }
}
#else
#define add_fkey(string, length, code, modify) addtosysmap(string, length, code)
#endif

/*
 * Termcap represents nulls as octal 200, which is ambiguous (ugh).  We only
 * want the exact value, i.e., an explicit null.  Fix it here.
 */
static int
replace_nulls(char *target, const char *source)
{
    int len = (int) strlen(source);
    int j;

    (void) strcpy(target, source);
#define TCAP_NULL 0200
    if (strchr(source, TCAP_NULL) != NULL) {
	for (j = 0; j < len; j++)
	    if (CharOf(target[j]) == TCAP_NULL)
		target[j] = '\0';
    }
    return len;
}

/*
 * Lookup the name, to see if we know about its shifted key.
 */
static void
add_shifted_key(unsigned i, int modify)
{
    char temp[BUFSIZ];
    unsigned j, k;
    int len;

    for (j = 0; j < TABLESIZE(shifted_keys); j++) {
	if (!strcmp(keyseqs[i].capname, shifted_keys[j].normal)) {
	    for (k = 0; k < TABLESIZE(keyseqs); k++) {
		if (!strcmp(keyseqs[k].capname, shifted_keys[j].shifted)) {
		    if (keyseqs[k].result != NULL) {
			len = replace_nulls(temp, keyseqs[k].result);
			add_modified_key(temp, len,
					 keyseqs[i].code |
					 xterm_mods[0].mask, modify);
		    }
		    break;
		}
	    }
	    break;
	}
    }
}

/*
 * (re)initialize the function key definitions from the termcap database.
 */
void
tcap_init_fkeys(void)
{
    /* *INDENT-OFF* */
    static const char * fallback_arrows[] = {
/*	"\033",		** VT52 (type-ahead checks make this not useful) */
	"\033O",	/* SS3 */
	"\033[",	/* CSI */
	"\217",	/* SS3 */
	"\233",	/* CSI */
    };
    /* *INDENT-ON* */

    unsigned i, j;
    int pass1, pass2, pass;

    if (i_am_xterm) {
	pass1 = global_g_val(GMDXTERM_FKEYS) ? 0 : -1;
	pass2 = global_g_val(GMDXTERM_FKEYS) ? 2 : 1;
    } else {
	pass1 = 0;
	pass2 = 1;
    }

    for (pass = pass1; pass < pass2; ++pass) {
	/*
	 * Provide fallback definitions for all ANSI/ISO/DEC cursor keys.
	 */
	for (i = 0; i < TABLESIZE(fallback_arrows); i++) {
	    for (j = 'A'; j <= 'D'; j++) {
		char temp[80];
		lsprintf(temp, "%s%c", fallback_arrows[i], (char) j);
		add_fkey(temp, (int) strlen(temp), (int) (SPEC | j), pass);
	    }
	}

#if SYS_OS2_EMX
	for (i = TABLESIZE(VIO_KeyMap); i--;) {
	    add_fkey(VIO_KeyMap[i].seq, 2, VIO_KeyMap[i].code, pass);
	}
#endif
	for (i = TABLESIZE(keyseqs); i--;) {
	    const char *seq = keyseqs[i].result;
	    char temp[BUFSIZ];

	    if (keyseqs[i].code != 0 && !NO_CAP(seq)) {
		int len;
		TRACE(("TGETSTR(%s) = %s\n", keyseqs[i].capname, str_visible(seq)));
#define DONT_MAP_DEL 1
#if DONT_MAP_DEL
		/* NetBSD, FreeBSD, etc. have the kD (delete) function key
		   defined as the DEL char.  i don't like this hack, but
		   until we (and we may never) have separate system "map"
		   and "map!" maps, we can't allow this -- DEL has different
		   semantics in insert and command mode, whereas KEY_Delete
		   has the same semantics (whatever they may be) in both.
		   KEY_Delete is the only non-motion system map, by the
		   way -- so the rest are benign in insert or command
		   mode.  */
		if (strcmp(seq, "\177") == 0)
		    continue;
#endif
		len = replace_nulls(temp, seq);
		if (pass && !is_csi(temp)) {
		    add_shifted_key(i, pass);
		} else {
		    add_fkey(temp, len, keyseqs[i].code, pass);
		}
	    }
	}
    }
}
