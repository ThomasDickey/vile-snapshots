/*	INPUT:	Various input routines for vile
 *		written by Daniel Lawrence	5/9/86
 *		variously munged/massaged/relayered/slashed/burned
 *			since then. -pgf
 *
 *	term.getc()	raw 8-bit key from terminal driver.
 *
 *	sysmapped_c()	single "keystroke" -- may have SPEC bit, if it was
 *			a sytem-mapped function key.  calls term.getc().  these
 *			system-mapped keys will never map to a multi-char
 *			sequence.  the routine does have storage, to hold
 *			keystrokes gathered "in error".
 *
 *	tgetc()		fresh, pushed back, or recorded output of result of
 *			sysmapped_c() (i.e. dotcmd and the keyboard macros
 *			are recordedand played back at this level).  this is
 *			only called from mapgetc() in map.c
 *
 *	mapped_c()	(map.c) worker routine which will return a mapped
 *			or non-mapped character from the mapping engine.
 *			determines correct map, and uses its own pushback
 *			buffers on top of calls to tgetc() (see mapgetc/
 *			mapungetc).
 *
 *	mapped_keystroke() applies user-specified maps to user's input.
 *			correct map used depending on mode (insert, command,
 *			message-line).
 *
 *	keystroke()	returns pushback from mappings, the results of
 *			previous calls to mapped_keystroke().
 *
 *	keystroke8()	as above, but masks off any "wideness", i.e. SPEC bits.
 *
 *	keystroke_raw() as above, but recording is forced even if
 *			sysmapped_c() returns intrc. (old "tgetc(TRUE)")
 *
 *	kbd_seq()	the vile prefix keys (^X,^A,#) are checked for, and
 *			appropriate key pairs are turned into CTLA|c, CTLX|c,
 *			SPEC|c.
 *
 *
 *	term.typahead()	  true if a key is avail from term.getc().
 *	sysmapped_c_avail() "  if a key is avail from sysmapped_c() or below.
 *	tgetc_avail()     true if a key is avail from tgetc() or below.
 *	keystroke_avail() true if a key is avail from keystroke() or below.
 *
 * $Id: input.c,v 1.380 2025/01/26 17:03:16 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"

#define	DEFAULT_REG	-1

#define INFINITE_LOOP_COUNT 1200

typedef struct _kstack {
    struct _kstack *m_link;
    int m_save;			/* old value of 'kbdmacactive'          */
    int m_indx;			/* index identifying this macro         */
    int m_rept;			/* the number of times to execute the macro */
    ITBUFF *m_kbdm;		/* the macro-text to execute            */
    ITBUFF *m_dots;		/* workspace for "." command            */
} KSTACK;

/*--------------------------------------------------------------------------*/
static void finish_kbm(void);

static int kbdmacactive;	/* current mode */
static size_t kbdmaclength;	/* effective recording length */
static KSTACK *KbdStack;	/* keyboard/@-macros that are replaying */
static ITBUFF *KbdMacro;	/* keyboard macro, recorded     */
static int last_eolchar;	/* records last eolchar-match in 'kbd_string' */

/*--------------------------------------------------------------------------*/

/*
 * Returns a pointer to the buffer that we use for saving text to replay with
 * the "." command.
 */
static ITBUFF *
TempDot(int init)
{
    static ITBUFF *tmpcmd;	/* dot commands, 'til we're sure */

    if (kbdmacactive == PLAY) {
	if (init)
	    (void) itb_init(&(KbdStack->m_dots), esc_c);
	return KbdStack->m_dots;
    }
    if (init || (tmpcmd == NULL))
	(void) itb_init(&tmpcmd, esc_c);
    return tmpcmd;
}

/*
 * Dummy function to use when 'kbd_string()' does not handle automatic completion
 */
/*ARGSUSED*/
int
no_completion(DONE_ARGS)
{
    (void) flags;
    (void) c;
    (void) buf;
    (void) pos;
    return FALSE;
}

/*
 * Complete names in a shell command using the filename-completion.  We'll have
 * to scan back to the beginning of the appropriate name since that module
 * expects only one name in a buffer.
 *
 * We only do shell-completion if filename-completion is configured.
 */
#if COMPLETE_FILES
static int doing_shell;
int
shell_complete(DONE_ARGS)
{
    int status = FALSE;
    size_t len = *pos;
    size_t base;
    size_t first = 0;

    TRACE(("shell_complete %d:'%s'\n", (int) *pos, buf));
    if (buf != NULL) {
	if (isShellOrPipe(buf))
	    first++;

	for (base = len; base > first;) {
	    base--;
	    if (isSpace(buf[base]) && (first || doing_shell)) {
		base++;
		break;
	    } else if (buf[base] == '$'
		       && (base == 0
			   || (!isEscaped(buf + base)))) {
		break;
	    }
	}
	len -= base;
	status = path_completion(flags, c, buf + base, &len);
	*pos = len + base;

    }
    return status;
}
#endif /* COMPLETE_FILES */

int
read_msgline(int flag)
{
    int old_value = reading_msg_line;
    reading_msg_line = flag;
    return old_value;
}

/*
 * Ask a yes or no question in the message line. Return either TRUE, FALSE, or
 * ABORT. The ABORT status is returned if the user bumps out of the question
 * with an esc_c. Used any time a confirmation is required.
 */
int
mlyesno(const char *prompt)
{
    int result = ABORT;
    char c;			/* input character */

    /* in case this is right after a shell escape */
    if (update(TRUE)) {
	int old_reading = read_msgline(TRUE);
	for_ever {
	    mlforce("%s [y/n]? ", prompt);
	    c = (char) keystroke();	/* get the response */

	    if (ABORTED(c)) {	/* Bail out! */
		break;
	    }

	    if (c == 'y' || c == 'Y') {
		result = TRUE;
		break;
	    }

	    if (c == 'n' || c == 'N') {
		result = FALSE;
		break;
	    }
	}
	read_msgline(old_reading);
    }
    return result;
}

/*
 * Ask a simple question in the message line.  Return the single char response
 * if it was one of the valid responses.
 */
int
mlquickask(const char *prompt, const char *respchars, int *cp)
{
    int result = ABORT;

    if (update(TRUE)) {
	int old_reading = read_msgline(TRUE);
	for_ever {
	    mlforce("%s ", prompt);
	    *cp = keystroke();	/* get the response */

	    if (ABORTED(*cp)) {	/* Bail out! */
		break;
	    }

	    if (vl_index(respchars, *cp)) {
		result = TRUE;
		break;
	    }

	    kbd_alarm();
	}
	read_msgline(old_reading);
    }
    return result;
}

/*
 * Prompt for a named-buffer (i.e., "register")
 */
int
mlreply_reg(const char *prompt,
	    char *cbuf,		/* 2-char buffer for register+eol */
	    int *retp,		/* => the register-name */
	    int at_dft)		/* default-value (e.g., for "@@" command) */
{
    int status;
    int c;

    regs_kbd_default = (at_dft != -1);
    if (clexec || isnamedcmd) {
	TBUFF *result = NULL;
	tb_scopy(&result, cbuf);
	if ((status = kbd_reply(prompt, &result, eol_history, '\n',
				regs_kbd_options(), regs_completion)) != TRUE) {
	    tb_free(&result);
	    return status;
	}
	strcpy(cbuf, tb_values(result));
	tb_free(&result);
	c = cbuf[0];
    } else {
	c = keystroke();
	if (ABORTED(c))
	    return ABORT;
    }

    if (c == UNAME_KCHR && regs_kbd_default) {
	c = at_dft;
    } else if (reg2index(c) < 0) {
	mlwarn("[Invalid register name]");
	return FALSE;
    }

    *retp = c;
    return TRUE;
}

/*
 * Prompt for a register-name and/or line-count (e.g., for the ":yank" and
 * ":put" commands).  The register-name, if given, is first.
 */
int
mlreply_reg_count(const char *cmd,	/* command-name, for clarity */
		  int state,	/* negative=register, positive=count, zero=either */
		  int *retp,	/* returns the register-index or line-count */
		  int *next)	/* returns 0/1=register, 2=count */
{
    int status;
    char prompt[80];
    char expect[80];
    char *buffer;
    TBUFF *result = NULL;

    *expect = EOS;
    if (state <= 0)
	(void) strcat(expect, " register");
    if (state == 0)
	(void) strcat(expect, " or");
    if (state >= 0) {
	(void) strcat(expect, " line-count");
    }

    (void) lsprintf(prompt, "Specify%s (%s): ", expect, cmd);
    tb_scopy(&result, "");
    status = kbd_reply(prompt, &result, eol_history, '\n',
		       regs_kbd_options(), regs_completion);

    if (status == TRUE) {
	buffer = tb_values(result);
	if (state <= 0
	    && isAlpha(buffer[0])
	    && buffer[1] == EOS
	    && (*retp = reg2index(*buffer)) >= 0) {
	    *next = isUpper(*buffer) ? 1 : 0;
	} else if (state >= 0
		   && string_to_number(buffer, retp)
		   && *retp) {
	    *next = 2;
	} else {
	    mlforce("[Expected%s]", expect);
	    kbd_alarm();
	    status = ABORT;
	}
    }
    tb_free(&result);
    return status;
}

/*
 * Write a prompt into the message line, then read back a response.  Keep track
 * of the physical position of the cursor.  If we are in a keyboard macro throw
 * the prompt away, and return the remembered response.  This lets macros run
 * at full speed.  The reply is always terminated by a carriage return.  Handle
 * erase, kill, and abort keys.
 */
int
mlreply(const char *prompt, char *buf, UINT bufn)
{
    return kbd_string(prompt, buf, (size_t) bufn, '\n', KBD_NORMAL, no_completion);
}

int
mlreply2(const char *prompt, TBUFF **buf)
{
    return kbd_string2(prompt, buf, '\n', KBD_NORMAL, no_completion);
}

/* as above, but don't do anything to backslashes */
int
mlreply_no_bs(const char *prompt, char *buf, UINT bufn)
{
    int s;
#if COMPLETE_FILES
    doing_shell = TRUE;
    init_filec(FILECOMPLETION_BufName);
#endif
    s = kbd_string(prompt,
		   buf,
		   (size_t) bufn,
		   '\n',
		   KBD_EXPAND | KBD_SHPIPE, shell_complete);
#if COMPLETE_FILES
    doing_shell = FALSE;
#endif
    return s;
}

/* as above, but neither expand nor do anything to backslashes */
int
mlreply_no_opts(const char *prompt, char *buf, UINT bufn)
{
    return kbd_string(prompt, buf, (size_t) bufn, '\n', 0, no_completion);
}

/* the numbered buffer names increment each time they are referenced */
void
incr_dot_kregnum(void)
{
    if (dotcmdactive == PLAY) {
	int c = itb_peek(dotcmd);
	if (isDigit(c) && c < '9')
	    itb_stuff(dotcmd, ++c);
    }
}

/*
 * Record a character for "." commands
 */
static void
record_dot_char(int c)
{
    if (dotcmdactive == RECORD) {
	ITBUFF *tmp = TempDot(FALSE);
	(void) itb_append(&tmp, c);
    }
}

/*
 * Record a character for kbd-macros
 */
static void
record_kbd_char(int c)
{
    if (dotcmdactive != PLAY && kbdmacactive == RECORD)
	(void) itb_append(&KbdMacro, c);
}

/* if we should preserve this input, do so */
static void
record_char(int c)
{
    record_dot_char(c);
    record_kbd_char(c);
}

/* get the next character of a replayed '.' or macro */
int
get_recorded_char(int eatit)	/* consume the character? */
{
    int c = -1;
    ITBUFF *buffer;

    if (dotcmdactive == PLAY) {

	if (interrupted()) {
	    dotcmdactive = 0;
	    return intrc;
	} else {

	    if (!itb_more(buffer = dotcmd)) {
		if (!eatit) {
		    if (dotcmdrep > 1)
			return itb_get(buffer, (size_t) 0);
		} else {	/* at the end of last repetition?  */
		    if (--dotcmdrep < 1) {
			dotcmdactive = 0;
			(void) dotcmdbegin();
			/* immediately start recording
			 * again, just in case.
			 */
		    } else {
			/* reset the macro to the
			 * beginning for the next rep.
			 */
			itb_first(buffer);
		    }
		}
	    }

	    /* if there is some left... */
	    if (itb_more(buffer)) {
		if (eatit)
		    c = itb_next(buffer);
		else
		    c = itb_peek(buffer);
		return c;
	    }
	}
    }

    if (kbdmacactive == PLAY) {

	if (interrupted()) {
	    while (kbdmacactive == PLAY)
		finish_kbm();
	    return intrc;
	} else {

	    if (!itb_more(buffer = KbdStack->m_kbdm)) {
		if (--(KbdStack->m_rept) >= 1)
		    itb_first(buffer);
		else
		    finish_kbm();
	    }

	    if (kbdmacactive == PLAY) {
		buffer = KbdStack->m_kbdm;
		if (eatit)
		    record_dot_char(c = itb_next(buffer));
		else
		    c = itb_peek(buffer);
	    }
	}
    }

    return c;
}

void
unkeystroke(int c)
{
    mapungetc((int) ((UINT) c | NOREMAP));
}

int
mapped_keystroke(void)
{
    return lastkey = mapped_c(DOMAP, MAP_UNQUOTED);
}

int
keystroke(void)
{
    return lastkey = mapped_c(NODOMAP, MAP_UNQUOTED);
}

int
keystroke8(void)
{
    int c;
    for_ever {
	c = mapped_c(NODOMAP, MAP_UNQUOTED);
	if ((c & ~0xff) == 0)
	    return lastkey = c;
	kbd_alarm();
    }
}

int
keystroke_raw8(void)
{
    int c;
    for_ever {
	c = mapped_c(NODOMAP, MAP_QUOTED);
	if ((c & ~0xff) == 0)
	    return lastkey = c;
	kbd_alarm();
    }
}

static int
mapped_keystroke_raw(void)
{
    return lastkey = mapped_c(DOMAP, MAP_QUOTED);
}

int
keystroke_avail(void)
{
    return mapped_c_avail();
}

static ITBUFF *tgetc_ungottenchars = NULL;
static int tgetc_ungotcnt = 0;

void
tungetc(int c)
{
    (void) itb_append(&tgetc_ungottenchars, c);
    tgetc_ungotcnt++;
}

int
tgetc_avail(void)
{
    return tgetc_ungotcnt > 0 ||
	get_recorded_char(FALSE) != -1 ||
	sysmapped_c_avail();
}

int
tgetc(int quoted)
{
    int c;			/* character read from terminal */

    TRACE((T_CALLED "tgetc\n"));
    if (tgetc_ungotcnt > 0) {
	tgetc_ungotcnt--;
	c = itb_last(tgetc_ungottenchars);
    } else if ((c = get_recorded_char(TRUE)) == -1) {
	/* read a character from the terminal */
	not_interrupted();
	if (setjmp(read_jmp_buf)) {
	    c = kcod2key((UINT) intrc);
	    TRACE(("setjmp/getc:%c (%#x)\n", c, c));
#if defined(BUG_LINUX_2_0_INTR) && DISP_TERMCAP
	    /*
	     * Linux bug (observed with kernels 1.2.13 & 2.0.0):
	     * when interrupted, the _next_ character that
	     * getchar() returns will be the same as the last
	     * character before the interrupt.  Eat it.
	     */
	    (void) term.getc();
#endif
	} else {
	    (void) im_waiting(TRUE);
	    do {		/* if it's sysV style signals,
				   we want to try again, since this
				   must not have been SIGINT, but
				   was probably SIGWINCH */
		c = sysmapped_c();
	    } while (c == -1);
	}
	(void) im_waiting(FALSE);
	if (quoted || (c != kcod2key((UINT) intrc)))
	    record_char(c);
    }

    /* return the character read from the terminal */
#if OPT_TRACE
    {
	char temp[NSTRING];
	TRACE(("tgetc(%s) = %s(%#x)\n", (quoted
					 ? "quoted"
					 : "unquoted"),
	       kcod2prc(c, temp), c));
    }
#endif
    returnCode(c);
}

/*
 * Get a command sequence (multiple keystrokes) from the keyboard.  Process all
 * applicable prefix keys.  Set lastcmd for commands which want stuttering.
 */
int
kbd_seq(void)
{
    int c;			/* fetched keystroke */

    int prefix = 0;		/* accumulate prefix */

    c = mapped_keystroke();

    if (c == cntl_a) {
	prefix = CTLA;
	c = keystroke();
    } else if (c == cntl_x) {
	prefix = CTLX;
	c = keystroke();
    } else if (c == poundc) {
	prefix = SPEC;
	c = keystroke();
    }

    c |= prefix;

    /* otherwise, just return it */
    return (lastcmd = c);
}

/*
 * Get a command-key, suppressing the mapping
 */
int
kbd_seq_nomap(void)
{
    unkeystroke(keystroke());
    return kbd_seq();
}

/*
 * Check for special cases which override character-classes when getting
 * data from the screen.
 */
int
adjust_chartype(CHARTYPE *mask)
{
    int whole_line = FALSE;

#if OPT_EVAL
    if (vl_get_it_all) {
	whole_line = TRUE;
	if (*mask == 0)		/* special case for $line variable */
	    *mask = ~(*mask);
    } else
#endif
	if (b_val(curbp, MDTAGWORD)
	    && *mask == vl_ident) {
	/* if from gototag(), grab from the beginning of the string */
	whole_line = TRUE;
    } else if (b_is_directory(curbp)
	       && *mask == SCREEN_STRING) {
	whole_line = TRUE;
	*mask = (CHARTYPE) ~0;
    }
    return whole_line;
}

/*
 * Get a string consisting of inclchartype characters starting from the current
 * position.  If inclchartype is 0, return everything to end of line.  If
 * whole_line is true, allow the string to start before the current position.
 */
static int
read_by_ctype(char *buf, size_t bufn, CHARTYPE inclchartype, int whole_line)
{
    static TBUFF *temp;
    int rc;
    size_t len;

#if OPT_CURTOKENS
    if (b_val(curbp, VAL_CURSOR_TOKENS) == CT_REGEX) {
	if (!(b_is_directory(curbp) && whole_line))
	    return FALSE;
    }
#endif

    TRACE((T_CALLED "read_by_ctype(incl=%#lx, whole=%d)\n",
	   (ULONG) inclchartype, whole_line));

    if ((rc = vl_ctype2tbuff(&temp, inclchartype, whole_line)) == TRUE) {
	if ((len = tb_length(temp)) >= bufn)
	    len = bufn - 1;
	strncpy0(buf, tb_values(temp), len + 1);
    } else {
	*buf = EOS;
    }
    TRACE(("result %s\n", buf));
    returnCode(rc);
}

#if OPT_CURTOKENS
static int
read_by_regex(char *buf, size_t bufn, REGEXVAL * rexp, int whole_line)
{
    static TBUFF *temp;
    int rc = FALSE;
    size_t len;

    if (b_val(curbp, VAL_CURSOR_TOKENS) != CT_CCLASS && rexp != NULL) {
	TRACE((T_CALLED "read_by_regex(incl=%s)\n", rexp->pat));
	*buf = EOS;

	if (*rexp->pat != EOS && rexp->reg != NULL) {
	    if ((rc = vl_regex2tbuff(&temp, rexp, whole_line)) == TRUE) {
		if ((len = tb_length(temp)) >= bufn)
		    len = bufn - 1;
		strncpy0(buf, tb_values(temp), len + 1);
	    }
	}
	TRACE(("result %s\n", buf));
    }
    returnCode(rc);
}
#else
#define read_by_regex(buf, bufn, rexp, whole_line) FALSE
#endif

/*
 * Obtain a buffer (or file) name from the screen.
 * It may contain wildcards that we can glob-expand.
 *
 * If it is not a directory-buffer, we provide a choice between regular
 * expressions and character classes.  Directory-buffers are guaranteed
 * to have one filename per line.
 */
int
screen_to_bname(char *buf, size_t bufn)
{
    int rc;
    CHARTYPE mask = SCREEN_STRING;
    int whole_line = adjust_chartype(&mask);

    TRACE((T_CALLED "screen_to_bname\n"));
#if OPT_CURTOKENS
    if (b_is_directory(curbp))
	rc = FALSE;
    else
	rc = read_by_regex(buf, bufn, get_buf_fname_expr(curbp), whole_line);
    if (rc == FALSE)
#endif
	rc = read_by_ctype(buf, bufn, mask, whole_line);
    returnCode(rc);
}

/*
 * Obtain $pathname from the screen.
 */
int
screen_to_fname(char *buf, size_t bufn)
{
    int rc = FALSE;
    CHARTYPE mask = vl_pathn;
    int whole_line = adjust_chartype(&mask);

    TRACE((T_CALLED "screen_to_fname\n"));
    rc = read_by_regex(buf, bufn, b_val_rexp(curbp, VAL_PATHNAME_EXPR), whole_line);
    if (rc == FALSE)
	rc = read_by_ctype(buf, bufn, mask, whole_line);

    returnCode(rc);
}

/*
 * Obtain $identifier from the screen.
 */
int
screen_to_ident(char *buf, size_t bufn)
{
    int rc = FALSE;
    CHARTYPE mask = vl_ident;
    int whole_line = adjust_chartype(&mask);

    TRACE((T_CALLED "screen_to_ident\n"));
    rc = read_by_regex(buf, bufn, b_val_rexp(curbp, VAL_IDENTIFIER_EXPR), whole_line);
    if (rc == FALSE)
	rc = read_by_ctype(buf, bufn, mask, whole_line);
    returnCode(rc);
}

/*
 * Get a string consisting of inclchartype characters starting from the current
 * position.  If inclchartype is 0, return everything to end of line.  If
 * whole_line is true, allow the string to start before the current position.
 */
int
vl_ctype2tbuff(TBUFF **result, CHARTYPE inclchartype, int whole_line)
{
    int okay;
    int i = 0;
    MARK save_dot;
    int first_ch = -1;
    int last_ch = -1;
    int first_off = -1;
    int save_off;
    int last_off = -1;
    int at_end = 0;

    TRACE((T_CALLED "vl_ctype2tbuff(incl=%#lx, whole=%d)\n",
	   (ULONG) inclchartype, whole_line));

    save_dot = DOT;

    /*
     * If we're processing the whole line, find the beginning of the token
     * by searching to the left.
     */
    if (whole_line
	&& DOT.o > 0
	&& HasCType(inclchartype, CharAtDot())) {
	while (DOT.o > 0) {
	    backchar_to_bol(TRUE, 1);
	    if (!HasCType(inclchartype, CharAtDot())) {
		forwchar_to_eol(TRUE, 1);
		break;
	    }
	}
    }

    /*
     * Search for the start of the token, and then the end of the token.
     */
    first_off = DOT.o;
    while (!is_at_end_of_line(DOT)) {
	last_ch = CharAtDot();
	if (first_ch < 0)
	    first_ch = last_ch;
#if OPT_WIDE_CTYPES
	if (i == 0) {
	    if (inclchartype & vl_scrtch) {
		if (first_ch != SCRTCH_LEFT[0])
		    inclchartype &= ~vl_scrtch;
	    }
	    if (inclchartype & vl_shpipe) {
		if (first_ch != SHPIPE_LEFT[0])
		    inclchartype &= ~vl_shpipe;
	    }
	}

	/* allow "[!command]" */
	if ((inclchartype & vl_scrtch)
	    && (i == 1)
	    && (last_ch == SHPIPE_LEFT[0])) {
	    /*EMPTY */ ;
	} else if ((inclchartype & vl_pathn)
		   && !HasCType(vl_pathn, last_ch)
		   && (inclchartype == vl_pathn)) {
	    /* guard against things like "[Buffer List]" on VMS */
	    break;
	} else
#endif
	if (inclchartype && !HasCType(inclchartype, last_ch)) {
	    break;
	}
	if (!forwchar_to_eol(TRUE, 1)) {
	    at_end = 1;
	    break;
	}
	i++;
#if OPT_WIDE_CTYPES
	if (inclchartype & vl_scrtch) {
	    if ((inclchartype & vl_pathn)
		&& HasCType(vl_pathn, CharAtDot())) {
		continue;
	    }
	    if (last_ch == SCRTCH_RIGHT[0]) {
		break;
	    }
	}
#endif
    }
    last_off = DOT.o + at_end;
    if (!inclchartype)
	last_off += len_record_sep(curbp);

#if OPT_WIDE_CTYPES
#if OPT_VMS_PATH
    if (inclchartype & vl_pathn) {
	;			/* override conflict with "[]" */
    } else
#endif
    if (inclchartype & vl_scrtch) {
	if (last_ch != SCRTCH_RIGHT[0]) {
	    last_off = first_off - 1;
	}
    }
#endif

    tb_init(result, EOS);
    save_off = first_off;
    while (first_off < last_off && first_off < llength(DOT.l)) {
	tb_append(result, lgetc(DOT.l, first_off));
	++first_off;
    }
    tb_append(result, EOS);
    DOT = save_dot;

    okay = (tb_length(*result) != 0);

    /*
     * Set side-effect variable $get-offset and $get-length, for scripts.
     */
#if OPT_EVAL
    if (okay) {
	vl_get_offset = (C_NUM) save_off;
	vl_get_length = ((C_NUM) tb_length(*result)) - 1;
	if (vl_get_at_dot) {
	    if ((vl_get_offset + vl_get_length <= DOT.o) ||
		(vl_get_offset > DOT.o)) {
		vl_get_offset = -1;
		vl_get_length = -1;
	    }
#if OPT_MULTIBYTE
	    else if (b_is_utfXX(curbp)) {
		int offset = chars_to_bol(DOT.l, vl_get_offset);
		int length = count_chars(DOT.l, vl_get_offset, vl_get_length);
		vl_get_offset = offset;
		vl_get_length = length;
	    }
#endif
	}
    } else {
	vl_get_offset = -1;
	vl_get_length = -1;
    }
#else
    (void) save_off;
#endif

    returnCode(okay);
}

#if OPT_CURTOKENS
static void
vl_regex2tbuff_best(TBUFF **result, regexp * exp)
{
    C_NUM given = DOT.o;
    C_NUM length;
    C_NUM offset;
    char *line_text = lvalue(DOT.l);

    vl_get_offset = -1;
    vl_get_length = -1;
    while (given >= 0) {
	if (lregexec(exp, DOT.l, given, llength(DOT.l), FALSE)) {
	    offset = (C_NUM) (exp->startp[0] - line_text);
	    length = (C_NUM) (exp->endp[0] - exp->startp[0]);
	    if ((length > vl_get_length)
		&& (offset + length > DOT.o)
		&& (offset <= DOT.o || !vl_get_at_dot)) {
		vl_get_offset = offset;
		vl_get_length = length;
	    }
	}
	--given;
    }
    if (vl_get_length > 0) {
	tb_bappend(result, line_text + vl_get_offset, (size_t) vl_get_length);
	tb_append(result, EOS);
    }
}

static void
vl_regex2tbuff_dot(TBUFF **result, regexp * exp)
{
    C_NUM start = DOT.o;
    C_NUM given = DOT.o;
    C_NUM length;
    C_NUM offset;
    char *line_text = lvalue(DOT.l);

    vl_get_offset = -1;
    vl_get_length = -1;
    while (given >= 0) {
	if (lregexec(exp, DOT.l, given, llength(DOT.l), FALSE)) {
	    offset = (C_NUM) (exp->startp[0] - line_text);
	    length = (C_NUM) (exp->endp[0] - exp->startp[0]);
	    if (offset <= DOT.o) {
		if ((length > vl_get_length || vl_get_offset > DOT.o)
		    && (offset + length > DOT.o)) {
		    vl_get_offset = offset;
		    vl_get_length = length;
		}
	    } else if ((length > vl_get_length) && (offset + length >= DOT.o)) {
		vl_get_offset = offset;
		vl_get_length = length;
	    }
	}
	if ((vl_get_offset >= 0
	     && vl_get_offset <= DOT.o
	     && vl_get_offset + vl_get_length > DOT.o)) {
	    vl_get_length -= (start - given);
	    vl_get_offset = start;
	    break;
	}
	--given;
    }
    if (vl_get_length > 0) {
	tb_bappend(result, line_text + vl_get_offset, (size_t) vl_get_length);
	tb_append(result, EOS);
    }
}

/*
 * Get a string matching the given expression, which includes the current
 * position.
 *
 * The "whole_line" parameter selects one of two cases for matching:
 *
 * 1. true, try successively smaller offsets, looking only for the best match.
 * This is used for tags and directory-buffers.
 *
 * 2. false, try smaller offsets, constrained by preferring to match a string
 * including dot.  This is used for matching $identifier
 */
int
vl_regex2tbuff(TBUFF **result, REGEXVAL * rexp, int whole_line)
{
    regexp *exp = rexp->reg;

    TRACE((T_CALLED "vl_regex2tbuff(pat=%s, whole=%d)\n",
	   NonNull(rexp->pat), whole_line));

    tb_init(result, EOS);

    if (rexp->pat != NULL && rexp->pat[0] && rexp->reg) {
	if (whole_line) {
	    vl_regex2tbuff_best(result, exp);
	} else {
	    vl_regex2tbuff_dot(result, exp);
	}
    }
#if OPT_MULTIBYTE
    if (vl_get_offset >= 0 && vl_get_length > 0 && b_is_utfXX(curbp)) {
	int offset = chars_to_bol(DOT.l, vl_get_offset);
	int length = count_chars(DOT.l, vl_get_offset, vl_get_length);
	vl_get_offset = offset;
	vl_get_length = length;
    }
#endif

    returnCode(tb_length(*result) != 0);
}
#endif /* OPT_CURTOKENS */

/*
 * Returns the character that ended the last call on 'kbd_string()'
 */
int
end_string(void)
{
    return last_eolchar;
}

void
set_end_string(int c)
{
    last_eolchar = c;
}

/*
 * Returns an appropriate delimiter for /-commands, based on the end of the
 * last reply.  That is, in a command such as
 *
 *	:s/first/last/
 *
 * we will get prompts for
 *
 *	:s/	/-delimiter saved in 'end_string()'
 *	first/
 *	last/
 *
 * If a newline is used at any stage, subsequent delimiters are forced to a
 * newline.
 */
int
kbd_delimiter(void)
{
    int c = '\n';

    if (isnamedcmd) {
	int d = end_string();
	if (isPunct(d))
	    c = d;
    }
    return c;
}

/*
 * If we are NOT processing MSDOS pathnames, we may have literal backslashes in
 * pathnames and buffer names.  Escape those by doubling them.
 */
char *
add_backslashes2(char *text, const char *find)
{
    static TBUFF *temp = NULL;
    int n;

    if (text != NULL) {
	if (strpbrk(text, find) != NULL) {
	    temp = tb_init(&temp, EOS);
	    for (n = 0; text[n] != EOS; ++n) {
		if (vl_index(find, text[n]))
		    temp = tb_append(&temp, BACKSLASH);
		temp = tb_append(&temp, text[n]);
	    }
	    temp = tb_append(&temp, EOS);
	    if (tb_values(temp) != NULL)
		text = tb_values(temp);
	}
    }
    return text;
}

/*
 * If we are NOT processing MSDOS pathnames, we may have literal backslashes in
 * pathnames and buffer names.  Escape those by doubling them.
 */
char *
add_backslashes(char *text)
{
    return add_backslashes2(text, "\\");
}

/*
 * FIXME:
 * Special hacks to convert null-terminated string to/from TBUFF, for interface
 * with functions that still expect these.
 */
static void
StrToBuff(TBUFF *buf)
{
    char *str = tb_values(buf);
    if (str != NULL) {
	(buf)->tb_used = strlen(str);
    }
}

static void
BuffToStr(TBUFF *buf)
{
    char *str = tb_values(buf);
    if (str != NULL) {
	str[tb_length(buf)] = EOS;
    }
}

/*
 * Make sure that the buffer will have extra space when passing it to functions
 * that don't know it's a TBUFF.
 */
static char *
tbreserve(TBUFF **buf)
{
    char *result = tb_values(tb_alloc(buf, tb_length(*buf) + NSTRING));
    BuffToStr(*buf);
    return result;
}

/* turn \X into X */
static void
remove_backslashes(TBUFF *buf)
{
    char *cp = tb_values(buf);
    size_t s, d;

    if (cp != NULL) {
	for (s = d = 0; s < tb_length(buf);) {
	    if (cp[s] == BACKSLASH)
		s++;
	    cp[d++] = cp[s++];
	}

	buf->tb_used = d;
    }
}

/* count backslashes so we can tell at any point whether we have the current
 * position escaped by one.
 */
static size_t
countBackSlashes(TBUFF *buf, size_t len)
{
    char *buffer = tb_values(buf);
    size_t count;

    if (len && buffer != NULL && isEscaped(buffer + len)) {
	count = 1;
	while (count + 1 <= len &&
	       isEscaped(buffer + len - count))
	    count++;
    } else {
	count = 0;
    }
    return count;
}

static void
showChar(int c)
{
    int save_expand;

    if (!vl_echo)
	return;

    save_expand = kbd_expand;
    kbd_expand = 1;		/* show all controls */
    if (qpasswd)
	c = '*';
    kbd_putc(c);
    kbd_expand = save_expand;
}

static void
show1Char(int c)
{
    if (!vl_echo)
	return;
    showChar(c);
    kbd_flush();
}

#define isRangeCmd(s) \
	((s)[0] == SQUOTE || isDigit(s[0]))
#define editingRangeCmd(buf) \
	((buf) != NULL && isRangeCmd(buf))

/*
 * Macro result is true if the buffer is a normal :-command with a leading "!",
 * or is invoked from one of the places that supplies an implicit "!" for shell
 * commands.
 */
#define editingShellCmd(buf,options) \
	 (((options & KBD_EXPCMD) && (buf != NULL) && isShellOrPipe(buf)) \
	|| (options & KBD_SHPIPE))

/* expand a single character (only used on interactive input) */
static int
expandChar(TBUFF **buf,
	   size_t *position,
	   int c,
	   KBD_OPTIONS options)
{
    int cpos = (int) *position;
    char *cp;
    BUFFER *bp;
    char str[NFILEN];
    char *buffer = tb_values(*buf);
    int shell = editingShellCmd(buffer, options);
    int expand = ((options & KBD_EXPAND) || shell);
    int exppat = (options & KBD_EXPPAT) != 0;

    if (buffer == NULL)
	return FALSE;

    /* Are we allowed to expand anything? */
    if (!(expand || exppat || shell))
	return FALSE;

    /* Is this a character that we know about? */
    if (vl_index(global_g_val_ptr(GVAL_EXPAND_CHARS), c) == NULL)
	return FALSE;

    switch (c) {
    case EXPC_THIS:
    case EXPC_THAT:
	if (!expand)
	    return FALSE;

	bp = (c == EXPC_THIS) ? curbp : find_alt();
	if (bp == NULL) {
	    kbd_alarm();	/* complain a little */
	    return FALSE;	/* ...and let the user type it as-is */
	}

	cp = bp->b_fname;
	if (isInternalName(cp)) {
	    cp = bp->b_bname;
	} else if (!global_g_val(GMDEXPAND_PATH)) {
	    cp = shorten_path(vl_strncpy(str, cp, sizeof(str)), FALSE);
#if OPT_MSDOS_PATH
	    /* always use backslashes if invoking external prog */
	    if (shell)
		cp = SL_TO_BSL(cp);
#endif
	}

	break;

    case EXPC_TOKEN:
	if (!expand)
	    return FALSE;

	if (screen_to_fname(str, sizeof(str)))
	    cp = str;
	else
	    cp = NULL;

	break;

    case EXPC_RPAT:
	if (!exppat)
	    return FALSE;

	if (tb_length(replacepat))
	    cp = tb_values(replacepat);
	else
	    cp = NULL;

	break;

    case EXPC_SHELL:
	if (!shell)
	    return FALSE;

#ifdef only_expand_first_bang
	/* without this check, do as vi does -- expand '!'
	   to previous command anywhere it's typed */
	if (cpos > (buffer[0] == '!'))
	    return FALSE;
#endif
	cp = tb_values(tb_save_shell[!isShellOrPipe(buffer)]);
	if (cp != NULL && isShellOrPipe(cp))
	    cp++;		/* skip the '!' */
	break;

    default:
	return FALSE;
    }

    if (cp != NULL) {
	while ((c = *cp++) != EOS) {
	    tb_insert(buf, (size_t) cpos++, c);
	    showChar(c);
	}
	kbd_flush();
    }
    *position = (size_t) cpos;
    return TRUE;
}

/*
 * Returns true for the (presumably control-chars) that we use for line edits
 */
int
is_edit_char(int c)
{
    return (isreturn(c)
	    || isbackspace(c)
	    || (c == wkillc)
	    || (c == killc));
}

/*
 * Erases the response from the screen for 'kbd_string()'
 */
TBUFF *
kbd_kill_response(TBUFF *buffer, size_t *position, int c)
{
    char *buf = tb_values(buffer);
    TBUFF *tmp = NULL;
    int cpos = (int) *position;
    size_t mark = *position;

    if (buf != NULL) {
	tmp = tb_copy(&tmp, buffer);
	while (cpos > 0) {
	    cpos -= TbBytesBefore(tmp, cpos);
	    kbd_erase();
	    if (c == wkillc) {
		if (!isSpace(buf[cpos])) {
		    if (cpos > 0 && isSpace(buf[cpos - 1]))
			break;
		}
	    }

	    if (c != killc && c != wkillc)
		break;
	}
	if (vl_echo)
	    kbd_flush();

	*position = (size_t) cpos;
	buffer->tb_used = (size_t) cpos;
	if (mark < tb_length(tmp)) {
	    tb_bappend(&buffer, tb_values(tmp) + mark, tb_length(tmp) - mark);
	}
	tb_free(&tmp);
    }
    return buffer;
}

/*
 * Display the default response for 'kbd_string()', escaping backslashes if
 * necessary.
 */
int
kbd_show_response(TBUFF **dst,	/* string with escapes */
		  const char *src,	/* string w/o escapes */
		  size_t bufn,	/* # of chars we read from 'src[]' */
		  int eolchar,
		  KBD_OPTIONS options)
{
    size_t k;

    /* add backslash escapes in front of volatile characters */
    tb_init(dst, 0);

    if (src != NULL) {
	for (k = 0; k < bufn; k++) {
	    int c = src[k];

	    if ((c == BACKSLASH) || (c == eolchar && eolchar != '\n')) {
		if (options & KBD_QUOTES)
		    tb_append(dst, BACKSLASH);	/* add extra */
	    } else if (vl_index(global_g_val_ptr(GVAL_EXPAND_CHARS), c) != NULL) {
		if (c == EXPC_RPAT && !(options & KBD_EXPPAT))
		    /*EMPTY */ ;
		else if (c == EXPC_SHELL && !(options & KBD_SHPIPE))
		    /*EMPTY */ ;
		else if ((options & KBD_QUOTES)
			 && (options & KBD_EXPAND))
		    tb_append(dst, BACKSLASH);	/* add extra */
	    }
	    tb_append(dst, c);
	}
    }

    /* put out the default response, which is in the buffer */
    kbd_init();
    for (k = 0; k < tb_length(*dst); k++) {
	showChar(tb_values(*dst)[k]);
    }
    if (vl_echo)
	kbd_flush();
    return (int) tb_length(*dst);
}

/* check only for eol-character */
int
/*ARGSUSED*/
eol_null(EOL_ARGS)
{
    (void) buffer;
    (void) cpos;

    return (c == eolchar);
}

int
/*ARGSUSED*/
eol_history(EOL_ARGS)
{
    (void) buffer;
    (void) cpos;

    if (isPrint(eolchar) || eolchar == '\r') {
	if (c == eolchar || (eolchar == '\r' && c == '\n'))
	    return TRUE;
    }
    return FALSE;
}

/*
 * Store a one-level push-back of the shell command text. This allows simple
 * prompt/substitution of shell commands, while keeping the "!" and text
 * separate for the command decoder.
 */
static int pushed_back;
static int pushback_flg;
static char *pushback_ptr;

void
kbd_pushback(TBUFF *buf, int skip)
{
    static TBUFF *PushBack;
    char *buffer = tb_values(buf);	/* FIXME: could have embedded nulls */

    TRACE(("kbd_pushback(%s,%d)\n", tb_visible(buf), skip));
    if (macroize(&PushBack, buf, skip)) {
	pushed_back = TRUE;
	pushback_flg = clexec;
	pushback_ptr = execstr;
	clexec = TRUE;
	execstr = tb_values(PushBack);
	buffer[skip] = EOS;
	buf->tb_used = (size_t) skip;
    }
}

int
kbd_is_pushed_back(void)
{
    return clexec && pushed_back;
}

/* A generalized prompt/reply function which allows the caller to specify an
 * additional terminator as well as '\n'.  Both are accepted.  Assumes the
 * buffer already contains a valid (possibly null) string to use as the default
 * response.
 */
int
kbd_string(const char *prompt,	/* put this out first */
	   char *extbuf,	/* the caller's (possibly full) buffer */
	   size_t bufn,		/* the length of  " */
	   int eolchar,		/* char we can terminate on, in addition to '\n' */
	   KBD_OPTIONS options,	/* KBD_EXPAND/KBD_QUOTES, etc. */
	   int (*complete) (DONE_ARGS))		/* handles completion */
{
    int code;
    static TBUFF *temp;

    options &= ~KBD_0CHAR;	/* this func is for null-terminated strings! */
    tb_scopy(&temp, extbuf);
    code = kbd_reply(prompt, &temp, eol_history, eolchar, options, complete);
    if (bufn > tb_length(temp))
	bufn = tb_length(temp);
    if (bufn != 0)
	memcpy(extbuf, tb_values(temp), bufn);
    extbuf[bufn - 1] = EOS;
    return code;
}

int
kbd_string2(const char *prompt,	/* put this out first */
	    TBUFF **result,	/* the caller's (possibly full) buffer */
	    int eolchar,	/* char we can terminate on, in addition to '\n' */
	    KBD_OPTIONS options,	/* KBD_EXPAND/KBD_QUOTES, etc. */
	    int (*complete) (DONE_ARGS))	/* handles completion */
{
    return kbd_reply(prompt, result, eol_history, eolchar, options, complete);
}

/*
 * Prompt for strings, used for @"interactive" variables, and the &query,
 * &dquery, and &qpasswd functions.
 */
char *
user_reply(const char *prompt, const char *dft_val)
{
    static TBUFF *replbuf, *passwdbuf;

    TBUFF **ptb;
    int save_vl_msgs, save_clexec, status;

    save_vl_msgs = vl_msgs;
    vl_msgs = TRUE;

    save_clexec = clexec;
    clexec = FALSE;

    if (!qpasswd)
	ptb = &replbuf;
    else {
	/*
	 * Querying user for a password.  Store result in an alternate buffer,
	 * the value of which is _not_ reused (so that it can't be recalled via
	 * &query or &dquery or an @ var).
	 */
	if (passwdbuf)
	    tb_free(&passwdbuf);
	ptb = &passwdbuf;
    }

    if (isLegalVal(dft_val)) {
	TRACE(("user_reply, given default value %s\n", dft_val));
	tb_scopy(ptb, dft_val);
    }

    status = kbd_reply(prompt,
		       ptb,
		       eol_history,
		       '\n',
		       KBD_EXPAND | KBD_QUOTES,
		       no_completion);

    vl_msgs = save_vl_msgs;
    clexec = save_clexec;

    if (status == ABORT)
	return NULL;
    else			/* FIXME: assumes EOS added by kbd_reply */
	return tb_values(*ptb);
}

#define isMiniMotion(f) (((f) & MOTION) && ((f) & (MINIBUF)))

/*
 * We use the editc character to toggle between insert/command mode in the
 * minibuffer.  This is normally bound to ^G (the position command).
 */
static int
isMiniEdit(int c)
{
    const CMDFUNC *cfp = ((miniedit)
			  ? CommandKeyBinding(c)
			  : InsertKeyBinding(c));
    return ((c == editc) || miniedit || ((cfp != NULL) && isMiniMotion(cfp->c_flags)));
}

/*
 * Shift the minibuffer left/right to keep the cursor visible, as well as the
 * prompt, if that's feasible.
 */
static void
shiftMiniBuffer(int offs)
{
    static const int slack = 5;
    int shift = w_val(wminip, WVAL_SIDEWAYS);
    int adjust;
    KBD_DATA save;

    kbd_start(&save);

    adjust = offs - (shift + LastMsgCol - 1);
    if (adjust >= 0)
	mvrightwind(TRUE, 1 + adjust);
    else if (shift > 0 && (slack + adjust) < 0)
	mvleftwind(TRUE, slack - adjust);

    kbd_finish(&save);

    kbd_flush();
}

static int
fakeKeyCode(const CMDFUNC * f)
{
    return (miniedit
	    ? fnc2kcod(f)
	    : fnc2kins(f));
}

static int
reallyEditMiniBuffer(TBUFF **buf,
		     size_t *cpos,
		     int c,
#if OPT_MULTIBYTE
		     int recur,
#endif
		     int margin,
		     int quoted)
{
#if OPT_MULTIBYTE
#define editMiniBuffer(buf,cpos,c,margin,quoted) \
  reallyEditMiniBuffer(buf,cpos,c,FALSE,margin,quoted)
#else
#define editMiniBuffer(buf,cpos,c,margin,quoted) \
  reallyEditMiniBuffer(buf,cpos,c,margin,quoted)
#endif
    int edited = FALSE;
    const CMDFUNC *cfp;
    int save_insertmode = insertmode;
    KBD_DATA save;

    kbd_start(&save);
    ++no_minimsgs;

    TRACE((T_CALLED
	   "editMiniBuffer(%d:%s) called with c=%#x, miniedit=%d, dot=%d, quoted:%d\n",
	   (int) *cpos, tb_visible(*buf),
	   c, miniedit, DOT.o, quoted));

#if OPT_MULTIBYTE
    /*
     * Anything that does not fit into 8 bits has to be chopped up to pass it
     * into the command-interpreting and data-insertion.  Do this by converting
     * to UTF-8 and (if that does not map to a byte) recurring.
     *
     * As a special case, codes in 128..255 have to be converted to UTF-8, but
     * we want to do this only for the case where input is assumed to be in
     * UTF-8.  That makes it still usable for ASCII and Latin-1 editing.
     */
    if (global_g_val(GMDMINI_MAP) && !is8Bits(c) && b_is_utfXX(bminip) && !insertmode) {
	int c2 = map_to_command_char(c);
	if (c2 >= 0)
	    c = c2;
    }
    if (!recur
	&& ((char2int(c) >= (int) iBIT(MinCBits))
	    || (char2int(c) >= 128 && b_is_utfXX(bminip)))) {
	UCHAR temp[MAX_UTF8];
	int used = vl_conv_to_utf8(temp, (UINT) c, sizeof(temp));
	int n;

	for (n = 0; n < used; ++n) {
	    edited |= reallyEditMiniBuffer(buf, cpos, temp[n], TRUE, margin, quoted);
	}
    } else
#endif
    {
	cfp = ((miniedit)
	       ? CommandKeyBinding(c)
	       : InsertKeyBinding(c));

	/* Use editc (normally ^G) to toggle insert/command mode */
	if (c == editc && !quoted) {
	    miniedit = !miniedit;
	} else if ((miniedit || isSpecial(c))
		   && (cfp != NULL) && (cfp->c_flags & MINIBUF) != 0) {
	    C_NUM first = (C_NUM) * cpos + margin;
	    int old_clexec = clexec;
	    int old_named = isnamedcmd;
#if OPT_B_LIMITS
	    int old_margin = b_left_margin(bminip);
#endif
	    REGIONSHAPE old_shape = regionshape;

	    /*
	     * Reset flags that might cause a recursion into the prompt/reply
	     * code.
	     */
	    clexec = 0;
	    isnamedcmd = 0;

	    /*
	     * Set limits so we don't edit the prompt, and allow us to move the
	     * cursor with the arrow keys just past the end of line.
	     */
	    b_set_left_margin(bminip, margin);
	    DOT.o = llength(DOT.l);
	    lins_bytes(1, ' ');	/* pad the line so we can move */

	    DOT.o = first;
	    MK = DOT;
	    curwp->w_line = DOT;
	    regionshape = rgn_EXACT;	/* operdel(), etc., do not set this */
	    (void) execute(cfp, FALSE, 1);
	    insertmode = save_insertmode;
	    edited = TRUE;

	    if (llength(DOT.l) > margin)
		llength(DOT.l) -= 1;	/* strip the padding */
	    b_set_left_margin(bminip, old_margin);

	    clexec = old_clexec;
	    isnamedcmd = old_named;
	    regionshape = old_shape;

	    /*
	     * Cheat a little, since we may have used an alias for
	     * '$', which can set the offset just past the end of
	     * line.
	     */
	    if (DOT.o > llength(DOT.l)) {
		DOT.o = llength(DOT.l);
	    }
	    *cpos = ((size_t) DOT.o - (size_t) margin);

	    /*
	     * Copy the data back from the minibuffer into our working TBUFF.
	     */
	    tb_init(buf, EOS);
	    if (llength(DOT.l) > margin) {
		tb_bappend(buf,
			   lvalue(DOT.l) + margin,
			   (size_t) llength(DOT.l) - (size_t) margin);
	    }

	    /*
	     * Below are some workarounds for making it appear that we're doing the
	     * right thing for certain non-motion commands.  They allow us to revert to
	     * insert-mode (the default), moving the cursor first as expected.
	     */
	} else if (cfp == &f_insert) {
	    miniedit = !miniedit;
	} else if (miniedit && cfp == &f_insertbol) {
	    edited = editMiniBuffer(buf, cpos, fakeKeyCode(&f_firstnonwhite),
				    margin, quoted);
	    miniedit = FALSE;
	} else if (miniedit && cfp == &f_appendeol) {
	    edited = editMiniBuffer(buf, cpos, fakeKeyCode(&f_gotoeol),
				    margin, quoted);
	    miniedit = FALSE;
	} else if (miniedit && cfp == &f_append) {
	    edited = editMiniBuffer(buf, cpos, fakeKeyCode(&f_forwchar_to_eol),
				    margin, quoted);
	    miniedit = FALSE;
	} else if (isSpecial(c) || (miniedit && (cfp != NULL))) {
	    /*
	     * Reject other non-motion commands for now.  We haven't a good way to
	     * update the minibuffer if someone inserts a newline, so we couldn't
	     * implement insert except as a special case (see above).
	     */
	    kbd_alarm();
	} else {
	    /*
	     * If it is not a command, drop out of miniedit mode and append the
	     * character to the buffer.
	     */
	    LINE *lp = DOT.l;
	    miniedit = FALSE;
	    if (!vl_echo || qpasswd) {
		char tmp = (char) c;
		tb_bappend(buf, &tmp, (size_t) 1);
		if (qpasswd)
		    show1Char(c);
	    } else if (llength(lp) >= margin) {
		show1Char(c);
		tb_init(buf, EOS);
		tb_bappend(buf,
			   lvalue(lp) + margin,
			   (size_t) llength(lp) - (size_t) margin);
	    }
	    *cpos += 1;
	    edited = TRUE;
	}

	shiftMiniBuffer(DOT.o);

	if (*cpos > tb_length(*buf))
	    *cpos = tb_length(*buf);

	TRACE(("...editMiniBuffer(%d:%s) returns %d, miniedit=%d, dot=%d\n",
	       (int) *cpos, tb_visible(*buf),
	       edited, miniedit, DOT.o));
    }

    --no_minimsgs;

    kbd_finish(&save);
    kbd_flush();

    returnCode(edited);
}

/*
 * Read a character quoted, e.g., by ^V.  If we allow multiple characters (the
 * normal case), check if the character is the beginning of a number.  Decode
 * numbers in hex, octal or decimal.  If inscreen, show the progress on the
 * message line.
 */
int
read_quoted(int count, int inscreen)
{
    int c, digs, base, i, delta;
    unsigned value = 0;
    unsigned limit = 0xff;
    const char *str;

    TRACE((T_CALLED "read_quoted(%d,%d)\n", count, inscreen));

    i = digs = 0;

    c = keystroke_raw8();
    if (count <= 0)
	returnCode(c);

    /* accumulate digits for a character */
    if (isDigit(c)
#if OPT_MULTIBYTE
	|| (c == 'u')
	|| (c == 'U')
#endif
	|| (c == 'x')) {
	if (!inscreen) {
	    kbd_putc(c);
	    kbd_flush();
	}
#if OPT_MULTIBYTE
	if (c == 'u') {
	    c = '0';
	    digs = 5;		/* 4 plus the fake '0' */
	    base = 16;
	    str = "unicode (hex)";
	    limit = 0xffff;
	} else if (c == 'U') {
	    c = '0';
	    digs = 6;		/* 5 plus the fake '0' */
	    base = 10;
	    str = "unicode (dec)";
	    limit = 0xffff;
	} else
#endif
	if (c == '0') {
	    digs = 4;		/* including the leading '0' */
	    base = 8;
	    str = "octal";
	} else if (c == 'x') {
	    c = '0';
	    digs = 3;		/* including the leading 'x' */
	    base = 16;
	    str = "hex";
	} else {
	    digs = 3;
	    base = 10;
	    str = "decimal";
	}
	do {
	    if (isbackspace(c)) {
		value /= (UINT) base;
		if (--i < 0)
		    break;
	    } else {
		if ((c >= 'a') && (c <= 'f'))
		    delta = ('a' - 10);
		else if ((c >= 'A') && (c <= 'F'))
		    delta = ('A' - 10);
		else if (isDigit(c)
			 && (c >= '0') && (c <= (int) (base + '0')))
		    delta = '0';
		else
		    break;
		value = (value * (UINT) base) + (UINT) (c - delta);
		i++;
	    }

	    if (inscreen) {
		mlwrite("Enter %s digits... %d", str, value);
	    } else if (i > 1) {
		kbd_putc(c);
		kbd_flush();
	    }

	    if (i >= digs)
		break;
	    c = keystroke_raw8();
	} while (isbackspace(c) ||
		 (isDigit(c) && base >= 10) ||
		 (base == 8 && c < '8') ||
		 (base == 16 && (c >= 'a' && c <= 'f')) ||
		 (base == 16 && (c >= 'A' && c <= 'F')));
    }

    if (inscreen) {
	mlerase();
    } else {
	for (delta = i; delta > 0; delta--)
	    kbd_erase();
	kbd_flush();
    }

    if (c >= 0) {
	if (i == 0)		/* Did we start a number? */
	    returnCode(c);
	else if (i < digs)	/* any other character will be pushed back */
	    unkeystroke(c);
    }

    returnCode((i > 0) ? ((int) (value & limit)) : -1);
}

static int
may_complete(TBUFF *data, KBD_OPTIONS options)
{
    int result = ABORT;

    if ((options & KBD_MAYBEC) != 0) {
	result = SORTOFTRUE;
    } else if ((options & KBD_MAYBEC2) != 0
	       && tb_length(data) != 0
	       && (*tb_values(data) == '%')) {
	result = SORTOFTRUE;
    }
    return result;
}

/*
 * This undoes the quoting done by tb_enquote() in run_func(), etc.
 */
static void
copy_dequoting(TBUFF **p, const char *string)
{
    (void) tb_init(p, EOS);
    tb_scopy(p, string);
    tb_dequote(p);
    TRACE2(("copy_dequoting(%s) %s\n", string, tb_visible(*p)));
}

/*
 * Same as 'kbd_string()', except for adding the 'endfunc' parameter.
 *
 * Returns:
 *	ABORT - abort character given (usually ESC)
 *	SORTOFTRUE - backspace from empty-buffer
 *	TRUE - buffer is not empty
 *	FALSE - buffer is empty
 */
int
kbd_reply(const char *prompt,	/* put this out first */
	  TBUFF **extbuf,	/* the caller's (possibly full) buffer */
	  int (*endfunc) (EOL_ARGS),	/* parsing with 'eolchar' delimiter */
	  int eolchar,		/* char we can terminate on, in addition to '\n' */
	  KBD_OPTIONS options,	/* KBD_EXPAND/KBD_QUOTES */
	  int (*complete) (DONE_ARGS))	/* handles completion */
{
    int c;
    int done;
    size_t cpos;		/* current position */
    int status;
    int shell;
    int range;
    int margin;
    size_t save_len;
    MARK saveMK;		/* FIXME: may be lost by name-completion swbuffer */

    int quotef;			/* are we quoting the next char? */
    size_t backslashes;		/* are we quoting the next expandable char? */
    UINT dontmap = (options & KBD_NOMAP);
    int firstch = TRUE;
    int lastch;
    size_t newpos;
    TBUFF *buf = NULL;
    const char *result;
    const char *old_bname = curbp->b_bname;
    WINDOW *old_wp = curwp;
    int old_reading;

    (void) old_wp;
    (void) old_bname;

    assert(extbuf != NULL);

    TRACE((T_CALLED "kbd_reply(prompt=%s, extbuf=%s, options=%#x)\n",
	   TRACE_NULL(prompt),
	   tb_visible(*extbuf),
	   options));
    TRACE(("clexec=%d, pushed_back=%d\n", clexec, pushed_back));

    saveMK = MK;
    miniedit = FALSE;
    set_end_string(EOS);	/* ...in case we don't set it elsewhere */

    /*
     * Check if the current value is the ERROR token.  If so, simply clear it.
     * Allowing the user to change the string to something like "ERROR_VALUE"
     * would be confusing and is not really necessary.
     */
    if (*extbuf != NULL
	&& (isErrorVal(tb_values(*extbuf))
	    || isTB_ERRS(*extbuf))) {
	tb_init(extbuf, EOS);
    }
    tb_unput(*extbuf);		/* FIXME: trim null */

    if (clexec) {
	int actual;

	/*
	 * A "^W!cat" (control/W followed by a shell command) will arrive at
	 * this point with "!" in extbuf, and the command in execstr.  Glue the
	 * two back together so they're seen as a shell command which should
	 * not be name-completed.
	 */
	if (tb_length(*extbuf) == 1
	    && isShellOrPipe(tb_values(*extbuf)))
	    options |= KBD_REGLUE | KBD_SHPIPE;

	if (tbreserve(extbuf) == NULL) {
	    returnCode(FALSE);
	}

	TRACE(("...getting token from: %s\n", str_visible0(execstr)));
	execstr = (dontmap
		   ? get_token1(execstr, extbuf, endfunc, eolchar, &actual)
		   : (((options & KBD_REGLUE) != 0 && pushed_back)
		      ? get_token2(execstr, extbuf, endfunc, eolchar, &actual)
		      : get_token(execstr, extbuf, endfunc, eolchar, &actual)));
	StrToBuff(*extbuf);	/* FIXME: get_token should use TBUFF */
	TRACE(("...got token, result:  %s\n", tb_visible(*extbuf)));
	status = (tb_length(*extbuf) != 0);
	if (status) {		/* i.e., we got some input */
#if !SMALLER
	    /* FIXME: may have nulls */
	    if ((options & KBD_LOWERC))
		(void) mklower(tb_values(*extbuf));
	    else if ((options & KBD_UPPERC))
		(void) mkupper(tb_values(*extbuf));
#endif
	    if (!(options & KBD_NOEVAL)) {
		buf = tb_copy(&buf, *extbuf);
		tb_append(&buf, EOS);	/* FIXME: for tokval() */
		result = tokval(tb_values(buf));

		/*
		 * Check for special return values from tokval().  It may be
		 * the result of an expression.  If an error was detected
		 * during evaluation, we will see the magic error_val.  That's
		 * a reason to stop here.
		 *
		 * If the result is a string, tokval() will return a quoted
		 * version of it, to avoid confusing recursive calls of
		 * run_func().  But we do not want quotes here, just the
		 * contents.  Dequote it if we started with a function and got
		 * a quoted string.
		 */
		if (isErrorVal(result)) {
		    status = ABORT;
		    (void) tb_scopy(extbuf, "");
		} else if (toktyp(tb_values(buf)) == TOK_FUNCTION &&
			   toktyp(result) == TOK_QUOTSTR) {
		    copy_dequoting(extbuf, result);
		} else {
		    (void) tb_scopy(extbuf, result);
		}
		tb_free(&buf);
		tb_unput(*extbuf);	/* trim the null */
	    }
	    if (complete != no_completion
		&& !editingShellCmd(tb_values(*extbuf), options)) {
		cpos =
		    newpos = tb_length(*extbuf);
		if ((options & KBD_MAYBEC2)
		    && tb_length(*extbuf) > 1
		    && (*tb_values(*extbuf) == '%')) {
		    status = TRUE;
		    tb_put(extbuf, cpos, EOS);
		} else if ((*complete) (options, NAMEC, tbreserve(extbuf), &newpos)) {
		    StrToBuff(*extbuf);
		} else {
		    status = may_complete(*extbuf, options);
		    tb_put(extbuf, cpos, EOS);
		}
	    }
	    /*
	     * Splice for multi-part command
	     */
	    if (actual != EOS) {
		set_end_string(actual);
	    } else {
		set_end_string('\n');
	    }
	}
	if (pushed_back && (*execstr == EOS)) {
	    pushed_back = FALSE;
	    clexec = pushback_flg;
	    execstr = pushback_ptr;
#if	OPT_HISTORY
	    if (!pushback_flg) {
		hst_append(*extbuf, EOS, TRUE);
	    }
#endif
	}
	if (tb_length(*extbuf) == 0
	    || tb_values(*extbuf)[tb_length(*extbuf) - 1] != EOS)
	    tb_append(extbuf, EOS);	/* FIXME */
	TRACE(("reply1:(status=%d, length=%d):%s\n", status,
	       (int) tb_length(*extbuf),
	       tb_visible(*extbuf)));
	MK = saveMK;
	returnCode(status);
    }

    quotef = FALSE;
    old_reading = read_msgline(TRUE);

    /* ask for the input string with the prompt */
    if (prompt != NULL)
	mlprompt("%s", prompt);

    /*
     * Use the length of the prompt (plus whatever led up to it) as the
     * left-margin for editing.  We'll automatically scroll the edited
     * field left/right within the margins.
     */
    margin = llength(wminip->w_dot.l);
    cpos = (size_t) kbd_show_response(&buf,
				      tb_values(*extbuf),
				      tb_length(*extbuf),
				      eolchar,
				      options);
    backslashes = 0;		/* by definition, there is an even
				   number of backslashes */
    c = -1;			/* initialize 'lastch' */

    for_ever {
	int EscOrQuo = (quotef ||
			((backslashes & 1) != 0));

	lastch = c;

	/*
	 * Get a character from the user. If not quoted, treat escape
	 * sequences as a single (16-bit) special character.
	 */
	if (quotef)
	    c = read_quoted(1, FALSE);
	else if (dontmap)
	    /* this looks wrong, but isn't.  no mapping will happen
	       anyway, since we're on the command line.  we want SPEC
	       keys to be expanded to #c, but no further.  this does
	       that */
	    c = mapped_keystroke_raw();
	else
	    c = keystroke();

#if SYS_WINNT
	/* make SHIFT+INSERT (clipboard paste) work from mini-buffer */
	if (c == (mod_KEY | KEY_Insert | mod_SHIFT)) {
	    const CMDFUNC *cfp = CommandKeyBinding(c);
	    if (cfp) {
		(cfp->cu.c_func) (FALSE, FALSE);
		c = 0;		/* is this important? */
		continue;
	    }
	}
#endif

	/* Ignore nulls if the calling function is not prepared to process
	 * them.  We want to be able to search for nulls, but must not use them
	 * in filenames, for example.  (We don't support name-completion with
	 * embedded nulls, either).
	 */
	if ((c == 0 && c != esc_c) && !(options & KBD_0CHAR))
	    continue;

	/* if we echoed ^V, erase it now */
	if (quotef) {
	    firstch = FALSE;
	    kbd_erase();
	    kbd_flush();
	}

	/* If it is a <ret>, change it to a <NL> */
	if (c == '\r' && !quotef)
	    c = '\n';

	/*
	 * If they hit the line terminator (i.e., newline or unescaped
	 * eolchar), finish up.
	 *
	 * Don't allow newlines in the string -- they cause real problems,
	 * especially when searching for patterns containing them -pgf
	 */
	done = FALSE;
	if (c == '\n') {
	    done = TRUE;
	} else if (!EscOrQuo
		   && !is_edit_char(c)
		   && !isMiniEdit(c)) {
	    BuffToStr(buf);
	    if ((*endfunc) (tb_values(buf), tb_length(buf), c, eolchar)) {
		done = TRUE;
	    }
	}

	if (complete != no_completion) {
	    kbd_unquery();
	    shell = editingShellCmd(tb_values(buf), options);
	    range = editingRangeCmd(tb_values(buf));
	    if (done && (options & KBD_NULLOK) && cpos == 0)
		/*EMPTY */ ;
	    else if ((done && (may_complete(buf, options) == ABORT))
		     || (!EscOrQuo
			 && !(shell && isPrint(c))
			 && (c == TESTC || c == NAMEC))) {
		/*
		 * If we're going to force name completion, put the cursor at
		 * the end of the line so the name completion code works
		 * properly.  Do this by erasing the remainder of the line and
		 * reentering it.
		 */
		if (cpos < tb_length(buf)) {
		    kbd_erase_to_end(wminip->w_dot.o);
		    while (cpos < tb_length(buf)) {
			show1Char(tb_values(buf)[cpos++]);
		    }
		}
		if ((shell || range) && isreturn(c)) {
		    /*EMPTY */ ;
		} else if ((*complete) (options, c, tbreserve(&buf), &cpos)) {
		    done = TRUE;
		    StrToBuff(buf);	/* FIXME */
		    if (c != NAMEC)	/* cancel the unget */
			(void) keystroke();
		} else {
		    StrToBuff(buf);	/* FIXME */
		    if (done) {	/* stay til matched! */
			kbd_unquery();
			(void) ((*complete) (options, TESTC,
					     tbreserve(&buf), &cpos));
		    }
		    firstch = FALSE;
		    shiftMiniBuffer(((int) cpos + margin));
		    continue;
		}
	    }
	}

	if (done) {
	    set_end_string(c);
	    if (options & KBD_QUOTES)
		remove_backslashes(buf);	/* take out quoters */

	    save_len = tb_length(buf);
	    hst_append(buf, eolchar, (int) (options & KBD_EXPCMD));
	    (void) tb_copy(extbuf, buf);
	    status = (tb_length(*extbuf) != 0);

	    /* If this is a shell command, push-back the actual text to
	     * separate the "!" from the command.  Note that the history
	     * command tries to do this already, but cannot for some special
	     * cases, e.g., the user types
	     *      :execute-named-command !ls -l
	     * which is equivalent to
	     *      :!ls -l
	     */
	    if (status == TRUE	/* ...we have some text */
		&& (options & KBD_EXPCMD)
#if	OPT_HISTORY
		&& (tb_length(buf) == save_len)		/* history didn't split it */
#endif
		&& isShellOrPipe(tb_values(buf))) {
		kbd_pushback(*extbuf, 1);
	    }
	    break;
	}
#if	OPT_HISTORY
	if (!EscOrQuo
	    && edithistory(&buf, &cpos, &c, options, endfunc, eolchar)) {
	    backslashes = countBackSlashes(buf, cpos);
	    firstch = TRUE;
	    continue;
	} else
#endif
	    /*
	     * If editc and esc_c are the same, don't abort unless the
	     * user presses it twice in a row.
	     */
	    if ((c != editc || c == lastch)
		&& ABORTED(c)
		&& !quotef
		&& !dontmap) {
	    tb_init(&buf, esc_c);
	    status = esc_func(FALSE, 1);
	    break;
	} else if ((isbackspace(c) ||
		    c == wkillc ||
		    c == killc) && !quotef) {

	    if (prompt == NULL && c == killc)
		cpos = 0;

	    if (cpos == 0) {
		if (prompt)
		    mlerase();
		if (isbackspace(c)) {	/* splice calls */
		    unkeystroke(c);
		    status = SORTOFTRUE;
		} else {
		    status = FALSE;
		}
		break;
	    }

	    kbd_kill_response(buf, &cpos, c);

	    /*
	     * If we backspaced to erase the buffer, it's ok to return to the
	     * caller, who would be the :-line parser, so we can do something
	     * reasonable with the address specification.
	     */
	    if (cpos == 0
		&& isbackspace(c)
		&& (options & KBD_STATED)) {
		status = SORTOFTRUE;
		break;
	    }
	    backslashes = countBackSlashes(buf, cpos);

	} else if (firstch == TRUE
		   && !quotef
		   && !isMiniEdit(c)
		   && !isSpecial(c)) {
	    /* clean the buffer on the first char typed */
	    unkeystroke(c);
	    kbd_kill_response(buf, &cpos, killc);
	    backslashes = countBackSlashes(buf, cpos);

	} else if (c == quotec && !quotef) {
	    quotef = TRUE;
	    show1Char(c);
	    continue;		/* keep firstch==TRUE */

	} else if (EscOrQuo || !expandChar(&buf, &cpos, c, options)) {
	    if (c == BACKSLASH)
		backslashes++;
	    else
		backslashes = 0;
	    quotef = FALSE;

#if !SMALLER
	    if (!isSpecial(c)) {
		if ((options & KBD_LOWERC)
		    && isUpper(c))
		    c = toLower(c);
		else if ((options & KBD_UPPERC)
			 && isLower(c))
		    c = toUpper(c);
	    }
#endif
	    if (!editMiniBuffer(&buf, &cpos, c, margin,
				quotef || (c != editc && (backslashes & 1)))) {
		continue;	/* keep firstch==TRUE */
	    }
	}
	firstch = FALSE;
    }
#if OPT_POPUPCHOICE
    popdown_completions(old_bname, old_wp);
#endif
    miniedit = FALSE;
    read_msgline(old_reading);
    tb_free(&buf);
    shiftMiniBuffer(0);

    TRACE(("reply:(status=%d, length=%d):%s\n", status,
	   (int) tb_length(*extbuf),
	   tb_visible(*extbuf)));
    tb_append(extbuf, EOS);	/* FIXME: this doesn't allow nulls */
    MK = saveMK;		/* FIXME: this should be in swbuffer? */
    returnCode(status);
}

/*
 * Begin recording the dot command macro.
 * Set up variables and return.
 * we use a temporary accumulator, in case this gets stopped prematurely
 */
int
dotcmdbegin(void)
{
    /* never record a playback */
    if (dotcmdactive != PLAY) {
	(void) TempDot(TRUE);
	dotcmdactive = RECORD;
	return TRUE;
    }
    return FALSE;
}

/*
 * Finish dot command, and copy it to the real holding area
 */
int
dotcmdfinish(void)
{
    if (dotcmdactive == RECORD) {
	ITBUFF *tmp = TempDot(FALSE);
	if (itb_length(tmp) == 0	/* keep the old value */
	    || itb_copy(&dotcmd, tmp) != NULL) {
	    itb_first(dotcmd);
	    dotcmdactive = 0;
	    return TRUE;
	}
    }
    return FALSE;
}

/* stop recording a dot command,
	probably because the command is not re-doable */
void
dotcmdstop(void)
{
    if (dotcmdactive == RECORD)
	dotcmdactive = 0;
}

/*
 * Execute the '.' command, by putting us in PLAY mode.
 * The command argument is the number of times to loop. Quit as soon as a
 * command gets an error. Return TRUE if all ok, else FALSE.
 */
int
dotcmdplay(int f, int n)
{
    if (!f)
	n = dotcmdarg ? dotcmdcnt : 1;
    else if (n < 0)
	return TRUE;

    if (f)			/* we definitely have an argument */
	dotcmdarg = TRUE;
    /* else
       leave dotcmdarg alone; */

    if (dotcmdactive || itb_length(dotcmd) == 0) {
	dotcmdactive = 0;
	dotcmdarg = FALSE;
	return FALSE;
    }

    if (n == 0)
	n = 1;

    dotcmdcnt = dotcmdrep = n;	/* remember how many times to execute */
    dotcmdactive = PLAY;	/* put us in play mode */
    itb_first(dotcmd);		/*    at the beginning */

    if (ukb != 0)		/* save our kreg, if one was specified */
	dotcmdkreg = ukb;
    else			/* use our old one, if it wasn't */
	ukb = dotcmdkreg;

    return TRUE;
}

/*
 * Test if we are replaying either '.' command, or keyboard macro.
 */
int
kbd_replaying(int match)
{
    if (dotcmdactive == PLAY) {
	/*
	 * Force a false-return if we are in insert-mode and have
	 * only one character to display.
	 */
	if (match
	    && insertmode == INSMODE_INS
	    && b_val(curbp, MDSHOWMAT)
	    && KbdStack == NULL
	    && (dotcmd->itb_last + 1 >= dotcmd->itb_used)) {
	    return FALSE;
	}
	return TRUE;
    }
    return (kbdmacactive == PLAY);
}

int
kbd_mac_recording(void)
{
    return (kbdmacactive == RECORD);
}

/*
 * If we're recording a keyboard macro, remember the length before the
 * start/stop function is called, so we can ignore the last few characters that
 * actually turn the recording off when replaying it.
 */
void
kbd_mac_check(void)
{
    if (kbdmacactive == RECORD) {
	kbdmaclength = itb_length(KbdMacro);
    }
}

/* translate the keyboard-macro into readable form */
void
get_kbd_macro(TBUFF **rp)
{
    char temp[80];
    size_t n, last, len;

    *rp = tb_init(rp, EOS);
    if ((last = itb_length(KbdMacro)) != 0) {
	for (n = 0; n < last; n++) {
	    len = (size_t) kcod2escape_seq(itb_get(KbdMacro, n),
					   temp,
					   sizeof(temp));
	    *rp = tb_bappend(rp, temp, len);
	}
    }
}

/* ARGSUSED */
int
kbd_mac_startstop(int f GCC_UNUSED, int n GCC_UNUSED)
{
    if (kbdmacactive) {
	if (kbdmacactive == RECORD) {
	    mlwrite("[Macro recording stopped]");
	    kbdmacactive = 0;
	    if (KbdMacro != NULL
		&& itb_length(KbdMacro) > kbdmaclength)
		KbdMacro->itb_used = kbdmaclength;
	    upmode();
	    return TRUE;
	}

	/* note that for PLAY, we do nothing -- that makes the
	 * '^X-)' at the of the recorded buffer a no-op during
	 * playback */
	return TRUE;
    } else {
	mlwrite("[Macro recording started]");
	kbdmacactive = RECORD;
	upmode();
	return (itb_init(&KbdMacro, esc_c) != NULL);
    }

}

/*
 * Execute a macro.
 * The command argument is the number of times to loop. Quit as soon as a
 * command gets an error. Return TRUE if all ok, else FALSE.
 */
/* ARGSUSED */
int
kbd_mac_exec(int f GCC_UNUSED, int n)
{
    if (!kbdmacactive) {
	if (n <= 0)
	    return TRUE;
	return start_kbm(n, DEFAULT_REG, KbdMacro);
    }

    mlforce("[Can't execute macro unless idle]");
    return FALSE;
}

/* ARGSUSED */
int
kbd_mac_save(int f GCC_UNUSED, int n GCC_UNUSED)
{
    TBUFF *p = NULL;
    size_t j, len;

    ksetup();
    get_kbd_macro(&p);
    if ((len = tb_length(p)) != 0) {
	for (j = 0; j < len; j++)
	    if (!kinsert(tb_get(p, j)))
		break;
    }
    tb_free(&p);
    kdone();
    mlwrite("[Keyboard macro saved in register %c.]", index2reg(ukb));
    return TRUE;
}

/*
 * Test if the given macro has already been started.
 */
int
kbm_started(int macnum, int force)
{
    if (force || (kbdmacactive == PLAY)) {
	KSTACK *sp;
	for (sp = KbdStack; sp != NULL; sp = sp->m_link) {
	    if (sp->m_indx == macnum) {
		while (kbdmacactive == PLAY)
		    finish_kbm();
		mlwarn("[Error: currently executing %s%c]",
		       macnum == DEFAULT_REG
		       ? "macro" : "register ",
		       index2reg(macnum));
		return TRUE;
	    }
	}
    }
    return FALSE;
}

/*
 * Start playback of the given keyboard command-string
 */
int
start_kbm(int n,		/* # of times to repeat */
	  int macnum,		/* register to execute */
	  ITBUFF * ptr)		/* data to interpret */
{
    KSTACK *sp = NULL;
    ITBUFF *tp = NULL;

    if (interrupted())
	return FALSE;

    if (kbdmacactive == RECORD && KbdStack != NULL)
	return TRUE;

    if (itb_length(ptr)
	&& (sp = typealloc(KSTACK)) != NULL
	&& itb_copy(&tp, ptr) != NULL) {

	/* make a copy of the macro in case recursion alters it */
	itb_first(tp);

	sp->m_save = kbdmacactive;
	sp->m_indx = macnum;
	sp->m_rept = n;
	sp->m_kbdm = tp;
	sp->m_link = KbdStack;

	KbdStack = sp;
	kbdmacactive = PLAY;	/* start us in play mode */

	/* save data for "." on the same stack */
	sp->m_dots = NULL;
	if (dotcmdactive == PLAY) {
	    dotcmd = NULL;
	    dotcmdactive = RECORD;
	}
	return (itb_init(&dotcmd, esc_c) != NULL
		&& itb_init(&(sp->m_dots), esc_c) != NULL);
    }
    free(sp);
    return FALSE;
}

/*
 * Finish a macro begun via 'start_kbm()'
 */
static void
finish_kbm(void)
{
    if (kbdmacactive == PLAY) {
	KSTACK *sp = KbdStack;

	kbdmacactive = 0;
	if (sp != NULL) {
	    kbdmacactive = sp->m_save;
	    KbdStack = sp->m_link;

	    itb_free(&(sp->m_kbdm));
	    itb_free(&(sp->m_dots));
	    free((char *) sp);
	}
    }
}
