/*
 * The routines in this file
 * handle the reading and writing of
 * disk files. All of details about the
 * reading and writing of the disk are
 * in "fileio.c".
 *
 * $Header: /users/source/archives/vile.vcs/RCS/file.c,v 1.287 2001/06/12 08:57:24 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"

#if SYS_WINNT
#include	<io.h>		/* for mktemp */
#include	<direct.h>	/* for mkdir */
#define	vl_mkdir(path,mode) mkdir(path)
#else
#define	vl_mkdir(path,mode) mkdir(path,mode)
#endif

#if CC_NEWDOSCC
#include	<io.h>
#endif

#if CC_CSETPP
#define isFileMode(mode) (mode & S_IFREG) == S_IFREG
#else
#define isFileMode(mode) (mode & S_IFMT) == S_IFREG
#endif

static int bp2swbuffer(BUFFER *bp, int ask_rerun, int lockfl);
static int kifile(char *fname);
static void readlinesmsg(int n, int s, const char *f, int rdo);

#if OPT_DOSFILES
/* give DOS the benefit of the doubt on ambiguous files */
# if CRLF_LINES
#  define MORETHAN >=
# else
#  define MORETHAN >
# endif
#endif

#if !SYS_MSDOS
static int quickreadf(BUFFER *bp, int *nlinep);
#endif

/*--------------------------------------------------------------------------*/

/* Returns the modification time iff the path corresponds to an existing file,
 * otherwise it returns zero.
 */
time_t
file_modified(char *path)
{
    struct stat statbuf;
    time_t the_time = 0;

    if (stat(SL_TO_BSL(path), &statbuf) >= 0
	&& isFileMode(statbuf.st_mode)) {
#if SYS_VMS
	the_time = statbuf.st_ctime;	/* e.g., creation-time */
#else
	the_time = statbuf.st_mtime;
#endif
    }
    return the_time;
}

#ifdef MDCHK_MODTIME
static int
PromptFileChanged(BUFFER *bp, char *fname, const char *question, int iswrite)
{
    int status = SORTOFTRUE;
    char prompt[NLINE];
    const char *remind, *again;
    time_t curtime;
    int modtimes_mismatch = FALSE;
    int fuids_mismatch = FALSE;
#ifdef CAN_CHECK_INO
    FUID curfuid;
#endif

    remind = again = "";

    if (isInternalName(bp->b_fname) || !bp->b_active)
	return SORTOFTRUE;

    if (b_val(bp, MDCHK_MODTIME)) {

	if (same_fname(fname, bp, FALSE)
	    && get_modtime(bp, &curtime)) {
	    time_t check_against;
	    if (iswrite) {
		check_against = bp->b_modtime;
		remind = "Reminder: ";
	    } else {
		if (bp->b_modtime_at_warn) {
		    check_against = bp->b_modtime_at_warn;
		    again = "again ";
		} else {
		    check_against = bp->b_modtime;
		}
	    }
	    modtimes_mismatch = (check_against != curtime);
	}
#ifdef CAN_CHECK_INO
	if (bp->b_fileuid.valid) {
	    int have_fuid;
	    FUID *check_against;
	    have_fuid = fileuid_get(bp->b_fname, &curfuid);
	    if (have_fuid) {
		if (iswrite) {
		    check_against = &bp->b_fileuid;
		    remind = "Reminder: ";
		} else {
		    if (bp->b_fileuid_at_warn.valid) {
			check_against =
			    &bp->b_fileuid_at_warn;
			again = "again ";
		    } else {
			check_against = &bp->b_fileuid;
		    }
		}
		fuids_mismatch = !fileuid_compare(
						     check_against, &curfuid);
	    }
	}
#endif /* CAN_CHECK_INO */
    }

    if (modtimes_mismatch || fuids_mismatch) {
	(void) lsprintf(prompt,
			"%sFile for \"%s\" has changed %son disk.  %s",
			remind, bp->b_bname, again, question);
	if ((status = mlyesno(prompt)) != TRUE)
	    mlerase();
	/* avoid reprompts */
	if (modtimes_mismatch)
	    bp->b_modtime_at_warn = curtime;
#ifdef CAN_CHECK_INO
	if (fuids_mismatch)
	    bp->b_fileuid_at_warn = curfuid;
#endif
    }

    return status;
}

int
ask_shouldchange(BUFFER *bp)
{
    int status;
    status = PromptFileChanged(bp, bp->b_fname, "Continue", FALSE);
    return (status == TRUE || status == SORTOFTRUE);
}

int
check_file_changed(BUFFER *bp, char *fn)
{
    int status = TRUE;

    if (PromptFileChanged(bp, fn, "Reread", FALSE) == TRUE) {
#if OPT_LCKFILES
	/* release own lock before read the file again */
	if (global_g_val(GMDUSEFILELOCK)) {
	    if (!b_val(curbp, MDLOCKED) && !b_val(curbp, MDVIEW))
		release_lock(fn);
	}
#endif
	status = readin(fn, TRUE, bp, TRUE);
    }
    return status;
}

static int
inquire_file_changed(BUFFER *bp, char *fn)
{
    register int status;
    if ((status = PromptFileChanged(bp, fn, "Write", TRUE)) != TRUE
	&& (status != SORTOFTRUE)) {
	mlforce("[Write aborted]");
	return FALSE;
    }
    return TRUE;
}

int
check_visible_files_changed(void)
{
    register WINDOW *wp;

    for_each_visible_window(wp)
	(void) check_file_changed(wp->w_bufp, wp->w_bufp->b_fname);
    return TRUE;
}
#endif /*  MDCHK_MODTIME */

#ifdef MDCHK_MODTIME
int
get_modtime(BUFFER *bp, time_t * the_time)
{
    if (isInternalName(bp->b_fname))
	*the_time = 0;
    else
	*the_time = file_modified(bp->b_fname);

    return (*the_time != 0);
}

void
set_modtime(BUFFER *bp, char *fn)
{
    time_t current;

    if (same_fname(fn, bp, FALSE) && get_modtime(bp, &current)) {
	bp->b_modtime = current;
	bp->b_modtime_at_warn = 0;
    }
}
#endif /* MDCHK_MODTIME */

#if SYS_UNIX || SYS_MSDOS
#define	CleanToPipe()	if (fileispipe) ttclean(TRUE)

static void
CleanAfterPipe(int Wrote)
{
    if (fileispipe == TRUE) {
	ttunclean();		/* may clear the screen as a side-effect */
	term.flush();
	if (Wrote)
	    pressreturn();
	sgarbf = TRUE;
    }
}

#else /* !SYS_UNIX */
#ifdef GMDW32PIPES

static void
CleanToPipe(void)
{
    if (fileispipe) {
	kbd_erase_to_end(0);
	kbd_flush();
	term.kclose();
    }
}

static void
CleanAfterPipe(int Wrote)
{
    if (fileispipe) {
	term.kopen();
	if (global_g_val(GMDW32PIPES)) {
	    if (Wrote)
		pressreturn();
	    sgarbf = TRUE;
	}
    }
}

#else /* !GMDW32PIPES */
#define	CleanToPipe()		term.kclose()
#define	CleanAfterPipe(f)	term.kopen()
#endif
#endif /* SYS_UNIX */

/*
 * On faster machines, a pipe-writer will tend to keep the pipe full. This
 * function is used by 'slowreadf()' to test if we've not done an update
 * recently even if this is the case.
 */
#if SYS_UNIX && OPT_SHELL
static int
slowtime(time_t * refp)
{
    int status = FALSE;

    if (fileispipe) {
	time_t temp = time((time_t *) 0);

	status = (!ffhasdata() || (temp != *refp));
	if (status)
	    *refp = temp;
    }
    return status;
}
#else
# if SYS_WINNT
#  define	slowtime(refp)	(fileispipe && !nowait_pipe_cmd && !ffhasdata())
# else
#  define	slowtime(refp)	(fileispipe && !ffhasdata())
# endif
#endif

int
no_such_file(const char *fname)
{
    mlwarn("[No such file \"%s\"]", fname);
    return FALSE;
}

#if OPT_VMS_PATH
static int
explicit_version(char *ver)
{
    if (*ver != EOS) {
	ver++;
	if (isDigit(*ver))
	    return TRUE;
    }
    return FALSE;
}
#endif /* OPT_VMS_PATH */

#if SYS_VMS
static char *
resolve_filename(BUFFER *bp)
{
    char temp[NFILEN];
    ch_fname(bp, fgetname(ffp, temp));
    markWFMODE(bp);
    return bp->b_fname;
}
#endif

/*
 * Returns true if the given filename is the same as that of the referenced
 * buffer.  The 'lengthen' parameter controls whether we assume the filename is
 * already in canonical form, since that may be an expensive operation to do in
 * a loop.
 */
int
same_fname(const char *fname, BUFFER *bp, int lengthen)
{
    int status = FALSE;
    char temp[NFILEN];

    if (fname == 0
	|| bp->b_fname == 0
	|| isInternalName(fname)
	|| isInternalName(bp->b_fname)) {
	return (status);
    }

    TRACE(("same_fname(\n\tfname=%s,\n\tbname=%s,\n\tlengthen=%d)\n",
	   fname, bp->b_bname, lengthen));

    if (lengthen)
	fname = lengthen_path(strcpy(temp, fname));

#if OPT_VMS_PATH
    /* ignore version numbers in this comparison unless both are given */
    if (is_vms_pathname(fname, FALSE)) {
	char *bname = bp->b_fname, *s = version_of(bname), *t = version_of(fname);

	if ((!explicit_version(s)
	     || !explicit_version(t))
	    && ((s - bname) == (t - fname))) {
	    status = !strncmp(fname, bname, (SIZE_T) (s - bname));
	    TRACE(("=>%d\n", status));
	    return (status);
	}
    }
#endif

    status = !strcmp(fname, bp->b_fname);
    TRACE(("=>%d\n", status));
    return (status);
}

/* support for "unique-buffers" mode -- file uids. */

int
fileuid_get(const char *fname, FUID * fuid)
{
#ifdef CAN_CHECK_INO
    struct stat sb;
    if (stat(fname, &sb) == 0) {
	fuid->ino = sb.st_ino;
	fuid->dev = sb.st_dev;
	fuid->valid = TRUE;
	return TRUE;
    }
#endif
    return FALSE;
}

void
fileuid_set(BUFFER *bp, FUID * fuid)
{
#ifdef CAN_CHECK_INO
    bp->b_fileuid = *fuid;	/* struct copy */
#endif
}

void
fileuid_invalidate(BUFFER *bp)
{
#ifdef CAN_CHECK_INO
    bp->b_fileuid.ino = 0;
    bp->b_fileuid.dev = 0;
    bp->b_fileuid.valid = FALSE;
    bp->b_fileuid_at_warn = bp->b_fileuid;
#endif
}

void
fileuid_set_if_valid(BUFFER *bp, const char *fname)
{
    FUID fuid;

    if (!same_fname(fname, bp, FALSE))
	return;

    if (fileuid_get(fname, &fuid))
	fileuid_set(bp, &fuid);
    else
	fileuid_invalidate(bp);
}

int
fileuid_compare(FUID * fuid1, FUID * fuid2)
{
#ifdef CAN_CHECK_INO
    return (fuid1->valid && fuid2->valid &&
	    fuid1->ino == fuid2->ino &&
	    fuid1->dev == fuid2->dev);
#else
    return (FALSE);		/* Doesn't really matter */
#endif
}

int
fileuid_same(BUFFER *bp, FUID * fuid)
{
#ifdef CAN_CHECK_INO
    return fileuid_compare(&bp->b_fileuid, fuid);
#else
    return (FALSE);		/* Doesn't really matter */
#endif
}

/*
 * Set the buffer-name based on the filename
 */
static void
set_buffer_name(BUFFER *bp)
{
    char bname[NBUFN];

    bp->b_bname[0] = EOS;	/* ...so 'unqname()' doesn't find me */
    makename(bname, bp->b_fname);
    unqname(bname);
    set_bname(bp, bname);
    updatelistbuffers();
    markWFMODE(bp);
}

static int
cannot_reread(void)
{
    mlforce("[Cannot reread]");
    return FALSE;
}

/*
 * Read a file into the current
 * buffer. This is really easy; all you do it
 * find the name of the file, and call the standard
 * "read a file into the current buffer" code.
 */
/* ARGSUSED */
int
fileread(int f GCC_UNUSED, int n GCC_UNUSED)
{
    register int s;
    char fname[NFILEN];

    if (more_named_cmd()) {
	if ((s = mlreply_file("Replace with file: ", (TBUFF **) 0,
			      FILEC_REREAD, fname)) != TRUE)
	    return s;
    } else if (b_is_temporary(curbp)) {
	return cannot_reread();
    } else if (!(global_g_val(GMDWARNREREAD) || curbp->b_fname[0] == EOS)
	       || ((s = mlyesno("Reread current buffer")) == TRUE)) {
	(void) strcpy(fname, curbp->b_fname);
	/* Check if we are rereading an unnamed-buffer if it is not
	 * associated with a file.
	 */
	if (curbp->b_fname[0] == EOS) {
	    if (eql_bname(curbp, STDIN_BufName)
		|| eql_bname(curbp, UNNAMED_BufName)) {
		s = bclear(curbp);
		if (s == TRUE)
		    mlerase();
		curwp->w_flag |= WFMODE | WFHARD;
		return s;
	    }
	    return cannot_reread();
	}
    } else {
	return FALSE;
    }

#if OPT_LCKFILES
    /* release own lock before read the replaced file */
    if (global_g_val(GMDUSEFILELOCK)) {
	if (!b_val(curbp, MDLOCKED) && !b_val(curbp, MDVIEW))
	    release_lock(curbp->b_fname);
    }
#endif

    /* we want no errors or complaints, so mark it unchanged */
    b_clr_changed(curbp);
    s = readin(fname, TRUE, curbp, TRUE);
    set_buffer_name(curbp);
    return s;
}

/*
 * Select a file for editing.
 * Look around to see if you can find the
 * file in another buffer; if you can find it
 * just switch to the buffer. If you cannot find
 * the file, create a new buffer, read in the
 * text, and switch to the new buffer.
 * This is ": e"
 */

void
set_last_file_edited(const char *f)
{
    tb_scopy(&lastfileedited, f);
}

int
set_files_to_edit(const char *prompt, int appflag)
{
    int status;
    BUFFER *bp, *bp_next;

    char fname[NFILEN];
    char *actual;
    BUFFER *firstbp = 0;

    if ((status = mlreply_file(prompt, &lastfileedited,
			       FILEC_READ | FILEC_EXPAND,
			       fname)) == TRUE) {
	while ((actual = filec_expand()) != 0) {
	    if ((bp = getfile2bp(actual, !clexec, FALSE)) == 0)
		break;
	    bp->b_flag |= BFARGS;	/* treat this as an argument */
	    if (firstbp == 0) {
		firstbp = bp;
		if (!appflag) {
		    for (bp = bheadp; bp; bp = bp_next) {
			bp_next = bp->b_bufp;
			if (bp != firstbp
			    && !b_is_temporary(bp))
			    kill_that_buffer(bp);
		    }
		}
	    }
	}
	if (firstbp != 0)
	    status = bp2swbuffer(firstbp, FALSE, TRUE);
    }
    return status;
}

/* ARGSUSED */
int
filefind(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return set_files_to_edit("Find file: ", TRUE);
}

/* ARGSUSED */
int
viewfile(int f GCC_UNUSED, int n GCC_UNUSED)
{				/* visit a file in VIEW mode */
    char fname[NFILEN];		/* file user wishes to find */
    register int s;		/* status return */
    char *actual;
    static TBUFF *last;

    if ((s = mlreply_file("View file: ", &last, FILEC_READ | FILEC_EXPAND,
			  fname)) == TRUE) {
	while ((actual = filec_expand()) != 0) {
	    if ((s = getfile(actual, FALSE)) != TRUE)
		break;
	    /* if we succeed, put it in view mode */
	    make_local_b_val(curwp->w_bufp, MDVIEW);
	    set_b_val(curwp->w_bufp, MDVIEW, TRUE);
	    markWFMODE(curwp->w_bufp);
	}
    }
    return s;
}

/*
 * Insert a file into the current
 * buffer. This is really easy; all you do it
 * find the name of the file, and call the standard
 * "insert a file into the current buffer" code.
 */
/* ARGSUSED */
int
insfile(int f GCC_UNUSED, int n GCC_UNUSED)
{
    register int s;
    char fname[NFILEN];
    static TBUFF *last;

    if (!calledbefore) {
	if ((s = mlreply_file("Insert file: ", &last,
			      FILEC_READ | FILEC_PROMPT, fname)) != TRUE)
	    return s;
    }
    if (ukb == 0)
	return ifile(fname, TRUE, (FILE *) 0);
    else
	return kifile(fname);
}

BUFFER *
getfile2bp(
	      const char *fname,	/* file name to find */
	      int ok_to_ask,
	      int cmdline)
{
    BUFFER *bp = cmdline ? NULL : find_b_name(fname);
    int s;
    char bname[NBUFN];		/* buffer name to put file */
    char nfname[NFILEN];	/* canonical form of 'fname' */
#ifdef CAN_CHECK_INO
    int have_fuid = FALSE;
    FUID fuid;
#endif

    /* user may have renamed buffer to look like filename */
    if (bp == NULL
	|| (strlen(fname) > (SIZE_T) NBUFN - 1)) {

	/* It's not already here by that buffer name.
	 * Try to find it assuming we're given the file name.
	 */
	(void) lengthen_path(strcpy(nfname, fname));
	if (is_internalname(nfname)) {
	    mlforce("[Buffer not found]");
	    return 0;
	}
#ifdef CAN_CHECK_INO
	if (global_g_val(GMDUNIQ_BUFS)) {
	    have_fuid = fileuid_get(nfname, &fuid);
	    if (have_fuid) {
		for_each_buffer(bp) {
		    /* is the same unique file */
		    if (fileuid_same(bp, &fuid))
			return bp;
		}
	    }
	}
#endif
	for_each_buffer(bp) {
	    /* is it here by that filename? */
	    if (same_fname(nfname, bp, FALSE)) {
		return bp;
	    }
	}

	/* it's not here */
	makename(bname, nfname);	/* New buffer name.     */
	/* make sure the buffer name doesn't exist */
	while ((bp = find_b_name(bname)) != NULL) {
	    if (!b_is_changed(bp) && is_empty_buf(bp) &&
		!ffexists(bp->b_fname)) {
		/* empty, unmodified, and non-existent --
		   then it's okay to re-use this buffer */
		bp->b_active = FALSE;
		ch_fname(bp, nfname);
		return bp;
	    }
	    /* make a new name if it conflicts */
	    unqname(bname);
	    if (!ok_to_ask || !global_g_val(GMDWARNRENAME))
		continue;
	    hst_glue(' ');
	    s = mlreply("Will use buffer name: ", bname, sizeof(bname));
	    if (s == ABORT)
		return 0;
	    if (s == FALSE || bname[0] == EOS)
		makename(bname, fname);
	}
	/* okay, we've got a unique name -- create it */
	if (bp == NULL && (bp = bfind(bname, 0)) == NULL) {
	    mlwarn("[Cannot create buffer]");
	    return 0;
	}
	/* switch and read it in. */
	ch_fname(bp, nfname);
    }
    return bp;
}

static int
bp2swbuffer(BUFFER *bp, int ask_rerun, int lockfl)
{
    register int s;

    if ((s = (bp != 0)) != FALSE) {
	if (bp->b_active) {
	    if (ask_rerun) {
		switch (mlyesno(
				   "Old command output -- rerun")) {
		case TRUE:
		    bp->b_active = FALSE;
		    break;
		case ABORT:
		    s = FALSE;
		    /* FALLTHRU */
		default:
		    mlerase();
		    break;
		}
	    } else {
		mlwrite("[Old buffer]");
	    }
	}
#if BEFORE
	/*
	 * The 'swbuffer()' function will invoke 'readin()', but it
	 * doesn't accept the 'lockfl' parameter, so we call it here.
	 */
/* added swbuffer_lfl() to take lockfl arg to get around this problem.  the
 * readin() was happening before a lot of the buffer info was set up, so a
 * user readhook could not use that info successfully.
 * if this change appears successful, the bp2readin routine (4 lines) can
 * be folded into swbuffer_lfl(), which is the only caller.  --pgf */
	if (!(bp->b_active))
	    s = bp2readin(bp, lockfl);
#endif
	if (s == TRUE)
	    s = swbuffer_lfl(bp, lockfl, FALSE);
	if (s == TRUE)
	    curwp->w_flag |= WFMODE | WFHARD;
    }

    return s;
}

int
no_file_found(void)
{
    mlforce("[No file found]");
    return FALSE;
}

int
no_file_name(const char *fname)
{
    if (is_internalname(fname)) {
	mlwarn("[No filename]");
	return TRUE;
    }
    return FALSE;
}

int
getfile(
	   char *fname,		/* file name to find */
	   int lockfl)
{				/* check the file for locks? */
    BUFFER *bp;

    if (fname == 0 || *fname == EOS)
	return no_file_found();

    /* if there are no path delimiters in the name, then the user
       is likely asking for an existing buffer -- try for that
       first */
    if ((bp = find_b_file(fname)) == 0
	&& ((strlen(fname) > (SIZE_T) NBUFN - 1)	/* too big to be a bname */
	    ||maybe_pathname(fname)	/* looks a lot like a filename */
	    ||(bp = find_b_name(fname)) == NULL)) {
	/* oh well.  canonicalize the name, and try again */
	bp = getfile2bp(fname, !clexec, FALSE);
	if (!bp)
	    return FALSE;
    }
    return bp2swbuffer(bp, isShellOrPipe(bp->b_fname), lockfl);
}

#if OPT_DOSFILES
/*
 * Scan a buffer to see if it contains more lines terminated by CR-LF than by
 * LF alone.  If so, set the DOS-mode to true, otherwise false.
 *
 * If the given buffer is one that we're sourcing (e.g., with ":so"), or
 * specified in initialization, we require that _all_ of the lines end with
 * ^M's before deciding that it is DOS-style.  That is to protect us from
 * accidentally trimming the ^M's from a :map command.
 */
static void
apply_dosmode(BUFFER *bp, int doslines, int unixlines)
{
    int result;

    if (bp->b_flag & BFEXEC) {
	if (doslines && !unixlines) {
	    result = TRUE;
	} else {
	    result = FALSE;
	}
    } else {
	result = (doslines MORETHAN unixlines);
    }
    /*
     * Make 'dos' and 'recordseparator' modes local, since this
     * transformation should apply only to the specified file.
     */
    make_local_b_val(bp, MDDOS);
    make_local_b_val(bp, VAL_RECORD_SEP);

    set_b_val(bp, MDDOS, result);
    set_b_val(bp, VAL_RECORD_SEP, result ? RS_CRLF : RS_LF);
}

static void
strip_if_dosmode(BUFFER *bp, int doslines, int unixlines)
{
    apply_dosmode(bp, doslines, unixlines);
    if (b_val(bp, MDDOS)) {	/* if it _is_ a dos file, strip 'em */
	register LINE *lp;
	TRACE(("stripping CR's for dosmode\n"));
	for_each_line(lp, bp) {
	    if (llength(lp) > 0 &&
		lgetc(lp, llength(lp) - 1) == '\r') {
		llength(lp)--;
	    }
	}
    }
}

static void
guess_dosmode(BUFFER *bp)
{
    int doslines = 0, unixlines = 0;
    register LINE *lp;

    /* first count 'em */
    for_each_line(lp, bp) {
	if (llength(lp) > 0 &&
	    lgetc(lp, llength(lp) - 1) == '\r') {
	    doslines++;
	} else {
	    unixlines++;
	}
    }
    /* then strip 'em */
    strip_if_dosmode(bp, doslines, unixlines);
    TRACE(("guess_dosmode %d\n", b_val(bp, MDDOS)));
}

/*
 * Forces the current buffer to be either 'dos' or 'unix' format.
 */
static void
explicit_dosmode(int flag)
{
    make_local_b_val(curbp, MDDOS);
    make_local_b_val(curbp, VAL_RECORD_SEP);

    set_b_val(curbp, MDDOS, flag);
    set_b_val(curbp, VAL_RECORD_SEP, flag ? RS_CRLF : RS_LF);
}

/*
 * Recompute the buffer size, redisplay the [Settings] buffer.
 */
static int
modified_dosmode(int flag)
{
    explicit_dosmode(flag);
    guess_dosmode(curbp);
    explicit_dosmode(flag);	/* ignore the guess - only want to strip CR's */
    set_record_sep(curbp, (RECORD_SEP) b_val(curbp, VAL_RECORD_SEP));
    curwp->w_flag |= WFMODE | WFHARD;
    return TRUE;
}

/*
 * Forces the current buffer to be in DOS-mode, stripping any trailing CR's.
 */
/*ARGSUSED*/
int
set_dosmode(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return modified_dosmode(TRUE);
}

/* as above, but forces unix-mode instead */
/*ARGSUSED*/
int
set_unixmode(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return modified_dosmode(FALSE);
}
#endif /* OPT_DOSFILES */

#if OPT_LCKFILES
static void
grab_lck_file(BUFFER *bp, char *fname)
{
    /* Write the lock */
    if (global_g_val(GMDUSEFILELOCK) &&
	!isShellOrPipe(fname) &&
	!b_val(bp, MDVIEW)) {
	char locker[100];

	if (!set_lock(fname, locker, sizeof(locker))) {
	    /* we didn't get it */
	    make_local_b_val(bp, MDVIEW);
	    set_b_val(bp, MDVIEW, TRUE);
	    make_local_b_val(bp, MDLOCKED);
	    set_b_val(bp, MDLOCKED, TRUE);
	    make_local_b_val(bp, VAL_LOCKER);
	    set_b_val_ptr(bp, VAL_LOCKER, strmalloc(locker));
	    markWFMODE(bp);
	}
    }
}
#endif

/*
 * Set the buffer's window traits to sane values after reading the buffer from
 * a file.  We have to do this, since we may copy the buffer traits back to the
 * window when making a buffer visible after source'ing it from a file.
 */
static void
init_b_traits(BUFFER *bp)
{
    bp->b_dot.l = lforw(buf_head(bp));
    bp->b_dot.o = 0;
    bp->b_wline = bp->b_dot;
    bp->b_lastdot = bp->b_dot;
#if WINMARK
    bp->b_mark = bp->b_dot;
#endif
#if OPT_MOUSE
    bp->b_wtraits.insmode = FALSE;
#endif
}

/*
 *	Read file "fname" into a buffer, blowing away any text
 *	found there.  Returns the final status of the read.
 */

/* ARGSUSED */
int
readin(
	  char *fname,		/* name of file to read */
	  int lockfl,		/* check for file locks? */
	  register BUFFER *bp,	/* read into this buffer */
	  int mflg)
{				/* print messages? */
    register WINDOW *wp;
    register int s;
    int nline;
#if	OPT_ENCRYPT
    int local_crypt = (bp != 0)
    && is_local_val(bp->b_values.bv, MDCRYPT);
#endif

    if (bp == 0)		/* doesn't hurt to check */
	return FALSE;

    if (*fname == EOS) {
	mlforce("BUG: readin called with NULL fname");
	return FALSE;
    }

    if ((s = bclear(bp)) != TRUE)	/* Might be old.    */
	return s;

#if	OPT_ENCRYPT
    /* bclear() gets rid of local flags */
    if (local_crypt) {
	make_local_b_val(bp, MDCRYPT);
	set_b_val(bp, MDCRYPT, TRUE);
    }
#endif

    b_clr_flags(bp, BFINVS | BFCHG);
    ch_fname(bp, fname);
    fname = bp->b_fname;	/* this may have been b_fname! */
#if OPT_DOSFILES
    make_local_b_val(bp, MDDOS);
    /* assume that if our OS wants it, that the buffer will have CRLF
     * lines.  this may change when the file is read, based on actual
     * line counts, below.  otherwise, if there's an error, or the
     * file doesn't exist, we will keep this default.
     */
    set_b_val(bp, MDDOS, CRLF_LINES);
#endif
    make_local_b_val(bp, MDNEWLINE);
    set_b_val(bp, MDNEWLINE, TRUE);	/* assume we've got it */

    if ((s = ffropen(fname)) == FIOERR) {	/* Hard file error.      */
	/* do nothing -- error has been reported,
	   and it will appear as empty buffer */
	/*EMPTY */ ;
    } else if (s == FIOFNF) {	/* File not found.      */
	if (mflg)
	    mlwrite("[New file]");
    } else {

	if (mflg) {
#define READING_FILE_FMT "[Reading %s]"

	    int outlen;
	    char tmp[NFILEN];

	    outlen = (term.cols - 1) -
		(sizeof(READING_FILE_FMT) - 3);
	    mlforce(READING_FILE_FMT,
		    path_trunc(fname, outlen, tmp, sizeof(tmp)));
#undef READING_FILE_FMT
	}
#if SYS_VMS
	if (!isInternalName(bp->b_fname))
	    fname = resolve_filename(bp);
#endif
	/* start reading */
	nline = 0;
#if OPT_WORKING
	max_working = cur_working = old_working = 0;
#endif
#if ! SYS_MSDOS
	if (fileispipe || (s = quickreadf(bp, &nline)) == FIOMEM)
#endif
	    s = slowreadf(bp, &nline);
#if OPT_WORKING
	cur_working = 0;
#endif
	if (s == FIOERR) {
	    /*EMPTY */ ;
	} else {

	    if (s == FIOBAD)	/* last line is incomplete */
		set_b_val(bp, MDNEWLINE, FALSE);
	    b_clr_changed(bp);
#if OPT_FINDERR
	    if (fileispipe == TRUE)
		set_febuff(bp->b_bname);
#endif
	    (void) ffclose();	/* Ignore errors.       */
	    if (mflg)
		readlinesmsg(nline, s, fname, ffronly(fname));

	    /* set view mode for read-only files */
	    if ((global_g_val(GMDRONLYVIEW) && ffronly(fname))) {
		make_local_b_val(bp, MDVIEW);
		set_b_val(bp, MDVIEW, TRUE);
	    }
	    /* set read-only mode for read-only files */
	    if (isShellOrPipe(fname) ||
		(global_g_val(GMDRONLYRONLY) &&
		 ffronly(fname))) {
		make_local_b_val(bp, MDREADONLY);
		set_b_val(bp, MDREADONLY, TRUE);
	    }

	    bp->b_active = TRUE;
	    bp->b_lines_on_disk = bp->b_linecount;
	}

    }

    /*
     * Set the majormode if the file's suffix matches.
     */
    infer_majormode(bp);

    for_each_window(wp) {
	if (wp->w_bufp == bp) {
	    init_window(wp, bp);
	}
    }
    imply_alt(fname, FALSE, lockfl);
    updatelistbuffers();

#if OPT_LCKFILES
    if (lockfl && s != FIOERR)
	grab_lck_file(bp, fname);
#endif
#if OPT_HOOKS
    if (s <= FIOEOF && (bp == curbp))
	run_readhook();
#endif
#ifdef GMDCD_ON_OPEN
    if (global_g_val(GMDCD_ON_OPEN) > 0)
	set_directory_from_file(bp);
#endif
    b_match_attrs_dirty(bp);
    return (s != FIOERR);
}

/*
 * Read the file into a given buffer, setting dot to the first line.
 */
int
bp2readin(BUFFER *bp, int lockfl)
{
    register int s = readin(bp->b_fname, lockfl, bp, TRUE);
    bp->b_active = TRUE;
    return s;
}

#if ! SYS_MSDOS
static int
quickreadf(register BUFFER *bp, int *nlinep)
{
    register UCHAR *textp;
    UCHAR *countp;
    L_NUM nlines;
    int incomplete = FALSE;
    B_COUNT len, nbytes;

#if	OPT_ENCRYPT
    int s;
    if ((s = vl_resetkey(curbp, bp->b_fname)) != TRUE)
	return s;
#endif

#if SYS_VMS
    /*
     * Note that this routine uses fseek() to position a file, which
     * doesn't work on VMS _unless_ the the file type is stream-access
     * or fixed-length record with no carriage control.  If the
     * target file can't meet these criteria, defer to slowreadf().
     */

    if (!vms_fseek_ok(bp->b_fname))
	return (FIOMEM);
#endif

    if ((len = ffsize()) < 0) {
	mlwarn("[Can't size file]");
	return FIOERR;
    }

    /* avoid malloc(0) problems down below; let slowreadf() do the work */
    if (len == 0)
	return FIOMEM;
#if OPT_WORKING
    max_working = len;
#endif
    /* leave an extra byte at the front, for the length of the first
       line.  after that, lengths go in place of the newline at
       the end of the previous line */
    bp->b_ltext = castalloc(UCHAR, (ALLOC_T) (len + 2));
    if (bp->b_ltext == NULL)
	return FIOMEM;

    if ((len = ffread((char *) &bp->b_ltext[1], len)) < 0) {
	FreeAndNull(bp->b_ltext);
	mlerror("reading");
	return FIOERR;
    }
#if OPT_ENCRYPT
    if (b_val(bp, MDCRYPT)
	&& bp->b_cryptkey[0]) {	/* decrypt the file */
	vl_setup_encrypt(bp->b_cryptkey);
	vl_encrypt_blok((char *) &bp->b_ltext[1], (UINT) len);
    }
#endif

    /* loop through the buffer, replacing all newlines with the
       length of the _following_ line */
    bp->b_ltext_end = bp->b_ltext + len + 1;
    countp = bp->b_ltext;
    textp = countp + 1;
    nbytes = len;
    nlines = 0;

    if (textp[len - 1] != '\n') {
	textp[len++] = '\n';
	set_b_val(bp, MDNEWLINE, FALSE);
    }

    while (len--) {
	if (*textp == '\n') {
	    if (textp - countp >= 255) {
		UCHAR *np;
#if OPT_WORKING
		max_working = bp->b_ltext_end - countp;
#endif
		len = (B_COUNT) (countp - bp->b_ltext);
		incomplete = TRUE;
		/* we'll re-read the rest later */
		if (len) {
		    ffseek(len);
		    np = castrealloc(UCHAR, bp->b_ltext, (ALLOC_T) len);
		} else {
		    np = NULL;
		}
		if (np == NULL) {
		    ffrewind();
		    FreeAndNull(bp->b_ltext);
		    return FIOMEM;
		}
		bp->b_ltext = np;
		bp->b_ltext_end = np + len + 1;
		nbytes = len;
		break;
	    }
	    *countp = textp - countp - 1;
	    countp = textp;
	    nlines++;
	}
	++textp;
    }

    if (nlines == 0) {
	ffrewind();
	FreeAndNull(bp->b_ltext);
	incomplete = TRUE;
    } else {
	/* allocate all of the line structs we'll need */
	bp->b_LINEs = typeallocn(LINE, nlines);
	if (bp->b_LINEs == NULL) {
	    FreeAndNull(bp->b_ltext);
	    ffrewind();
	    return FIOMEM;
	}
	bp->b_LINEs_end = bp->b_LINEs + nlines;
	bp->b_bytecount = nbytes;
	bp->b_linecount = nlines;
	b_set_counted(bp);

	/* loop through the buffer again, creating
	   line data structure for each line */
	{
	    register LINE *lp;
#if !SMALLER
	    L_NUM lineno = 0;
#endif
	    lp = bp->b_LINEs;
	    textp = bp->b_ltext;
	    while (lp != bp->b_LINEs_end) {
#if !SMALLER
		lp->l_number = ++lineno;
#endif
		lp->l_used = *textp;
		lp->l_size = *textp + 1;
		lp->l_text = (char *) textp + 1;
		set_lforw(lp, lp + 1);
		if (lp != bp->b_LINEs)
		    set_lback(lp, lp - 1);
		lsetclear(lp);
		lp->l_nxtundo = null_ptr;
#if OPT_LINE_ATTRS
		lp->l_attrs = NULL;
#endif
		lp++;
		textp += *textp + 1;
	    }
	    /*
	       if (textp != bp->b_ltext_end - 1)
	       mlforce("BUG: textp not equal to end %d %d",
	       textp,bp->b_ltext_end);
	     */
	    lp--;		/* point at last line again */

	    /* connect the end of the list */
	    set_lforw(lp, buf_head(bp));
	    set_lback(buf_head(bp), lp);

	    /* connect the front of the list */
	    set_lback(bp->b_LINEs, buf_head(bp));
	    set_lforw(buf_head(bp), bp->b_LINEs);
	}
	init_b_traits(bp);
    }

    *nlinep = nlines;

    if (incomplete)
	return FIOMEM;
#if OPT_DOSFILES
    if (global_b_val(MDDOS))
	guess_dosmode(bp);
#endif
    return b_val(bp, MDNEWLINE) ? FIOSUC : FIOBAD;
}

#endif /* ! SYS_MSDOS */

int
slowreadf(register BUFFER *bp, int *nlinep)
{
    int s;
    int len;
#if OPT_DOSFILES
    int doslines = 0, unixlines = 0;
#endif
#if SYS_UNIX || SYS_MSDOS || SYS_OS2 || SYS_WINNT	/* i.e., we can read from a pipe */
    USHORT flag = 0;
    int done_update = FALSE;
#endif
#if SYS_UNIX && OPT_SHELL
    time_t last_updated = time((time_t *) 0);
#endif
#if	OPT_ENCRYPT
    if ((s = vl_resetkey(curbp, curbp->b_fname)) != TRUE)
	return s;
#endif
    b_set_counted(bp);		/* make 'addline()' do the counting */
#if OPT_DOSFILES
    /*
     * There might be some pre-existing lines if quickreadf
     * read part of the file and then left the rest up to us.
     */
    make_local_b_val(bp, MDDOS);	/* keep it local, if not */
    if (global_b_val(MDDOS)) {
	register LINE *lp;
	for_each_line(lp, bp) {
	    if (llength(lp) > 0 &&
		lgetc(lp, llength(lp) - 1) == '\r') {
		doslines++;
	    } else {
		unixlines++;
	    }
	}
    }
#endif
    bp->b_lines_on_disk = 0;
    while ((s = ffgetline(&len)) <= FIOSUC) {
	bp->b_lines_on_disk += 1;
#if OPT_DOSFILES
	/*
	 * Strip CR's if we are reading in DOS-mode.  Otherwise,
	 * keep any CR's that we read.
	 */
	if (global_b_val(MDDOS)) {
	    if (len > 0 && fflinebuf[len - 1] == '\r') {
		doslines++;
	    } else {
		unixlines++;
	    }
	}
#endif
	if (addline(bp, fflinebuf, len) != TRUE) {
	    s = FIOMEM;
	    break;
	}
#if SYS_UNIX || SYS_MSDOS || SYS_OS2 || SYS_WINNT
	else {
	    /* reading from a pipe, and internal? */
	    if (slowtime(&last_updated)) {
		register WINDOW *wp;

		flag |= (WFEDIT | WFFORCE);

		if (!done_update || bp->b_nwnd > 1)
		    flag |= WFHARD;
		for_each_window(wp) {
		    if (wp->w_bufp == bp) {
			wp->w_line.l =
			    lforw(buf_head(bp));
			wp->w_dot.l =
			    lback(buf_head(bp));
			wp->w_dot.o = 0;
			wp->w_flag |= flag;
			wp->w_force = -1;
		    }
		}

		/* track changes in dosfile as lines arrive */
#if OPT_DOSFILES
		if (global_b_val(MDDOS))
		    apply_dosmode(bp, doslines, unixlines);
#endif
		curwp->w_flag |= WFMODE | WFKILLS;
		if (!update(TRUE)) {
		    s = FIOERR;
		    break;
		}
#if DISP_X11
		/* to pick up intrc if it's been hit */
		x_move_events();
#endif
		done_update = TRUE;
		flag = 0;
	    } else {
		flag |= WFHARD;
	    }

	}
#endif
	++(*nlinep);
	if (s == FIOBAD) {
	    set_b_val(bp, MDNEWLINE, FALSE);
	    break;
	}
    }
#if OPT_DOSFILES
    if (global_b_val(MDDOS)) {
	strip_if_dosmode(bp, doslines, unixlines);
    }
#endif
    init_b_traits(bp);
    return s;
}

/* utility routine for no. of lines read */
static void
readlinesmsg(int n, int s, const char *f, int rdo)
{
    char fname[NFILEN];
    const char *m;
    char *short_f = shorten_path(strcpy(fname, f), TRUE);
    switch (s) {
    case FIOBAD:
	m = "Incomplete line, ";
	break;
    case FIOERR:
	m = "I/O Error, ";
	break;
    case FIOMEM:
	m = "Not enough memory, ";
	break;
    case FIOABRT:
	m = "Aborted, ";
	break;
    default:
	m = "";
	break;
    }
    if (!global_b_val(MDTERSE))
	mlwrite("[%sRead %d line%s from \"%s\"%s]", m,
		n, PLURAL(n), short_f, rdo ? "  (read-only)" : "");
    else
	mlforce("[%s%d lines]", m, n);
}

static void
writelinesmsg(char *fn, int nline, B_COUNT nchar)
{
#define WRITE_FILE_FMT "[%s %s line%s %s char%s to \"%s\"]"

    if (!global_b_val(MDTERSE)) {
	const char *action;
	char *aname, tmp[NFILEN], strlines[128], strchars[128];
	int outlen, lines_len, chars_len;
	if ((aname = is_appendname(fn)) != 0) {
	    fn = aname;
	    action = "Appended";
	} else {
	    action = "Wrote";
	}
	sprintf(strlines, "%d", nline);
	sprintf(strchars, "%ld", nchar);
	lines_len = strlen(strlines);
	chars_len = strlen(strchars);
	outlen = (term.cols - 1) -
	    (
		(sizeof(WRITE_FILE_FMT) - 13) +
		strlen(action) +
		lines_len +
		chars_len +
		strlen(PLURAL(nline)) +
		strlen(PLURAL(nchar))
	    );
	mlforce(WRITE_FILE_FMT,
		action,
		strlines,
		PLURAL(nline),
		strchars,
		PLURAL(nchar),
		path_trunc(fn, outlen, tmp, sizeof(tmp)));
    } else {
	mlforce("[%d lines]", nline);
    }
#undef WRITE_FILE_FMT
}

/*
 * Take a (null-terminated) file name, and from it
 * fabricate a buffer name. This routine knows
 * about the syntax of file names on the target system.
 * I suppose that this information could be put in
 * a better place than a line of code.
 */

void
makename(char *bname, const char *fname)
{
    register char *fcp;
    register char *bcp;
    register int j;
    char temp[NFILEN];

    fcp = skip_string(strcpy(temp, fname));
#if OPT_VMS_PATH
    if (is_vms_pathname(temp, TRUE)) {
	(void) strcpy(bname, "NoName");
	return;
    }
    if (is_vms_pathname(temp, FALSE)) {
	for (; fcp > temp && !strchr(":]", fcp[-1]); fcp--) {
	    ;
	}
	(void) strncpy0(bname, fcp, NBUFN);
	strip_version(bname);
	(void) mklower(bname);
	return;
    }
#endif
    /* trim trailing whitespace */
    while (fcp != temp && (isBlank(fcp[-1])
#if SYS_UNIX || OPT_MSDOS_PATH	/* trim trailing slashes as well */
			   || is_slashc(fcp[-1])
#endif
	   ))
	*(--fcp) = EOS;
    fcp = temp;
    /* trim leading whitespace */
    while (isBlank(*fcp))
	fcp++;

#if     SYS_UNIX || SYS_MSDOS || SYS_VMS || SYS_OS2 || SYS_WINNT
    bcp = bname;
    if (isShellOrPipe(fcp)) {
	/* ...it's a shell command; bname is first word */
	*bcp++ = SCRTCH_LEFT[0];
	*bcp++ = SHPIPE_LEFT[0];
	do {
	    ++fcp;
	} while (isSpace(*fcp));
	(void) strncpy0(bcp, fcp, (SIZE_T) (NBUFN - (bcp - bname)));
	for (j = 4; (j < NBUFN) && isPrint(*bcp); j++) {
	    bcp++;
	}
	(void) strcpy(bcp, SCRTCH_RIGHT);
	return;
    }

    (void) strncpy0(bcp, pathleaf(fcp), (SIZE_T) (NBUFN - (bcp - bname)));

#if	SYS_UNIX
    /* UNIX filenames can have any characters (other than EOS!).  Refuse
     * (rightly) to deal with leading/trailing blanks, but allow embedded
     * blanks.  For this special case, ensure that the buffer name has no
     * blanks, otherwise it is difficult to reference from commands.
     */
    for (j = 0; j < NBUFN; j++) {
	if (*bcp == EOS)
	    break;
	if (isSpace(*bcp))
	    *bcp = '-';
	bcp++;
    }
#endif

#else /* !(SYS_UNIX||SYS_VMS||SYS_MSDOS) */

    bcp = skip_string(fcp);
    {
	register char *cp2 = bname;
	strcpy0(bname, bcp, NBUFN);
	cp2 = strchr(bname, ':');
	if (cp2)
	    *cp2 = EOS;
    }
#endif
}

void
unqname(			/* generate a unique name for a buffer */
	   char *name)
{				/* buffer name to make unique */
    register SIZE_T j;
    char newname[NBUFN * 2];
    char suffixbuf[NBUFN];
    int suffixlen;
    int adjust;
    int i = 0;
    SIZE_T k;

    j = strlen(name);
    if (j == 0)
	j = strlen(strcpy(name, "NoName"));

    /* check to see if this name is in use */
    strcpy(newname, name);
    adjust = is_scratchname(newname);
    while (find_b_name(newname) != NULL) {
	/* from "foo" create "foo-1" or "foo-a1b5" */
	/* from "thisisamuchlongernam" create
	   "thisisamuchlongern-1" or
	   "thisisamuchlong-a1b5" */
	/* that is, put suffix at end if it fits, or else
	   overwrite some of the name to make it fit */
	/* the suffix is in "base 36" */
	suffixlen = (int) (lsprintf(suffixbuf, "-%r", 36, ++i) - suffixbuf);
	k = NBUFN - 1 - suffixlen;
	if (j < k)
	    k = j;
	if (adjust) {
	    strcpy(&newname[k - 1], suffixbuf);
	    strcat(newname, SCRTCH_RIGHT);
	} else
	    strcpy(&newname[k], suffixbuf);
    }
    strncpy0(name, newname, NBUFN);
}

/*
 * Ask for a file name, and write the
 * contents of the current buffer to that file.
 */
int
filewrite(int f, int n)
{
    register int s;
    char fname[NFILEN];
    int forced = (f && n == SPECIAL_BANG_ARG);

    if (more_named_cmd()) {
	if ((s = mlreply_file("Write to file: ", (TBUFF **) 0,
			      forced ? FILEC_WRITE2
			      : FILEC_WRITE, fname)) != TRUE)
	    return s;
    } else
	(void) strcpy(fname, curbp->b_fname);

    if ((s = writeout(fname, curbp, forced, TRUE)) == TRUE)
	unchg_buff(curbp, 0);
    return s;
}

/*
 * Save the contents of the current
 * buffer in its associatd file.
 * Error if there is no remembered file
 * name for the buffer.
 */
/* ARGSUSED */
int
filesave(int f, int n)
{
    register int s;
    int forced = (f && n == SPECIAL_BANG_ARG);	/* then it was :w! */

    if (no_file_name(curbp->b_fname))
	return FALSE;

    if ((s = writeout(curbp->b_fname, curbp, forced, TRUE)) == TRUE)
	unchg_buff(curbp, 0);
    return s;
}

static void
setup_file_region(BUFFER *bp, REGION * rp)
{
    (void) bsizes(bp);		/* make sure we have current count */
    /* starting at the beginning of the buffer */
    rp->r_orig.l = lforw(buf_head(bp));
    rp->r_orig.o = 0;
    rp->r_size = bp->b_bytecount;
    rp->r_end = bp->b_line;
}

static int
actually_write(REGION * rp, char *fn, int msgf, BUFFER *bp, int forced)
{
    register int s;
    register LINE *lp;
    register int nline;
    register int i;
    B_COUNT nchar;
    const char *ending = get_record_sep(bp);
    int len_rs = strlen(ending);
    C_NUM offset = rp->r_orig.o;

    /* this is adequate as long as we cannot write parts of lines */
    int whole_file = (rp->r_orig.l == lforw(buf_head(bp)))
    && (rp->r_end.l == buf_head(bp));

#if OPT_HOOKS
    if (run_a_hook(&writehook)) {
	/*
	 * The write-hook may have modified the buffer.  Assume
	 * the worst, and reconstruct the region.
	 */
	(void) bsizes(bp);
	if (whole_file
	    || line_no(bp, rp->r_orig.l) > bp->b_linecount
	    || line_no(bp, rp->r_end.l) > bp->b_linecount) {
	    setup_file_region(bp, rp);
	} else {
	    DOT = rp->r_orig;
	    MK = rp->r_end;
	    (void) getregion(rp);
	}
	offset = rp->r_orig.o;
    }
#endif

#if	OPT_ENCRYPT
    if ((s = vl_resetkey(curbp, fn)) != TRUE)
	return s;
#endif

    /* open writes error message, if needed */
    if ((s = ffwopen(fn, forced)) != FIOSUC)
	return FALSE;

    /* tell us we're writing */
    if (msgf == TRUE)
	mlwrite("[Writing...]");

    if (isShellOrPipe(fn)) {
	beginDisplay();
	CleanToPipe();
    }

    lp = rp->r_orig.l;
    nline = 0;			/* Number of lines     */
    nchar = 0;			/* Number of chars     */

    /* first (maybe partial) line and succeeding whole lines */
    while ((rp->r_size + offset) >= line_length(lp)) {
	register C_NUM len = llength(lp) - offset;
	register char *text = lp->l_text + offset;

	/* If this is the last line (and no fragment will be written
	 * after the line), allow 'newline' mode to suppress the
	 * trailing newline.
	 */
	if ((rp->r_size -= line_length(lp)) <= 0
	    && !b_val(bp, MDNEWLINE))
	    ending = "";
	if ((s = ffputline(text, len, ending)) != FIOSUC)
	    goto out;

	++nline;
	nchar += line_length(lp);
	offset = 0;
	lp = lforw(lp);
    }

    /* last line (fragment) */
    if (rp->r_size > 0) {
	for (i = 0; i < rp->r_size; i++)
	    if ((s = ffputc(lgetc(lp, i))) != FIOSUC)
		goto out;
	nchar += rp->r_size;
	++nline;		/* it _looks_ like a line */
    }

  out:
    if (s == FIOSUC) {		/* No write error.      */
#if SYS_VMS
	if (same_fname(fn, bp, FALSE))
	    fn = resolve_filename(bp);
#endif
	s = ffclose();
	if (s == FIOSUC && msgf) {	/* No close error.      */
	    writelinesmsg(fn, nline, nchar);
	}
    } else {			/* Ignore close error   */
	(void) ffclose();	/* if a write error.    */
    }
    if (whole_file) {
	bp->b_linecount =
	    bp->b_lines_on_disk = nline;
    }

    if (isShellOrPipe(fn)) {
	CleanAfterPipe(TRUE);
	endofDisplay();
    }

    if (s != FIOSUC)		/* Some sort of error.      */
	return FALSE;

#ifdef MDCHK_MODTIME
    set_modtime(bp, fn);
#endif
    fileuid_set_if_valid(bp, fn);
    /*
     * If we've written the unnamed-buffer, rename it according to the file.
     * FIXME: maybe we should do this to all internal-names?
     */
    if (whole_file
	&& eql_bname(bp, UNNAMED_BufName)
	&& find_b_file(fn) == 0) {
	ch_fname(bp, fn);
	set_buffer_name(bp);
    }

    imply_alt(fn, whole_file, FALSE);
    return TRUE;
}

static int
file_protection(char *fn)
{
    int result = -1;
    struct stat sb;

    if (!isShellOrPipe(fn)) {
	if (stat(fn, &sb) == 0) {
	    if (isFileMode(sb.st_mode))
		result = sb.st_mode & 0777;
	}
    }
    return result;
}

static int
writereg(REGION * rp, const char *given_fn, int msgf, BUFFER *bp, int forced)
{
    int status;
    char fname[NFILEN], *fn;
    int protection = -1;

    if (no_file_name(given_fn)) {
	status = FALSE;
    } else if (!forced && b_val(bp, MDREADONLY)) {
	mlwarn("[Buffer mode is \"readonly\"]");
	status = FALSE;
    } else {
	if (isShellOrPipe(given_fn)
	    && bp->b_fname != 0
	    && !strcmp(given_fn, bp->b_fname)
	    && mlyesno("Are you sure (this was a pipe-read)") != TRUE) {
	    mlwrite("File not written");
	    status = FALSE;
	} else {
	    fn = lengthen_path(strcpy(fname, given_fn));
	    if (same_fname(fn, bp, FALSE) && b_val(bp, MDVIEW)) {
		mlwarn("[Can't write-back from view mode]");
		status = FALSE;
	    } else {
#if defined(MDCHK_MODTIME)
		if (!inquire_file_changed(bp, fn))
		    status = FALSE;
		else
#endif
		{
		    if (forced
			&& !ffaccess(fn, FL_WRITEABLE)
			&& (protection = file_protection(fn)) >= 0) {
			chmod(SL_TO_BSL(fn), protection | 0600);
		    }
		    status = actually_write(rp, fn, msgf, bp, forced);
		    if (protection > 0)
			chmod(SL_TO_BSL(fn), protection);
		}
	    }
	}
    }

    return status;
}

/*
 * Write a whole file
 */
int
writeout(const char *fn, BUFFER *bp, int forced, int msgf)
{
    REGION region;

    setup_file_region(bp, &region);

    return writereg(&region, fn, msgf, bp, forced);
}

/*
 * Write the currently-selected region (i.e., the range of lines from DOT to
 * MK, inclusive).
 */
int
writeregion(void)
{
    REGION region;
    int status;
    char fname[NFILEN];

    if (end_named_cmd()) {
	if (mlyesno("Okay to write [possible] partial range") != TRUE) {
	    mlwrite("Range not written");
	    return FALSE;
	}
	(void) strcpy(fname, curbp->b_fname);
    } else {
	if ((status = mlreply_file("Write region to file: ",
				   (TBUFF **) 0, FILEC_WRITE | FILEC_PROMPT,
				   fname)) != TRUE)
	    return status;
    }
    if ((status = getregion(&region)) == TRUE)
	status = writereg(&region, fname, TRUE, curbp, FALSE);
    return status;
}

/*
 * This function writes the kill register to a file
 * Uses the file management routines in the
 * "fileio.c" package. The number of lines written is
 * displayed.
 */
int
kwrite(char *fn, int msgf)
{
    KILL *kp;			/* pointer into kill register */
    int nline;
    B_COUNT nchar;
    int s;
    int c;
    int i;
    char *sp;			/* pointer into string to insert */

    /* make sure there is something to put */
    if (kbs[ukb].kbufh == NULL) {
	if (msgf)
	    mlforce("Nothing to write");
	return FALSE;		/* not an error, just nothing */
    }
#if	OPT_ENCRYPT
    if ((s = vl_resetkey(curbp, fn)) != TRUE)
	return s;
#endif
    if ((s = ffwopen(fn, FALSE)) != FIOSUC) {	/* Open writes message. */
	return FALSE;
    }
    /* tell us we're writing */
    if (msgf == TRUE)
	mlwrite("[Writing...]");
    nline = 0;			/* Number of lines.     */
    nchar = 0;

    kp = kbs[ukb].kbufh;
    while (kp != NULL) {
	i = KbSize(ukb, kp);
	sp = (char *) kp->d_chunk;
	nchar += i;
	while (i--) {
	    if ((c = *sp++) == '\n')
		nline++;
	    if ((s = ffputc(c)) != FIOSUC)
		break;
	}
	kp = kp->d_next;
    }
    if (s == FIOSUC) {		/* No write error.      */
	s = ffclose();
	if (s == FIOSUC && msgf) {	/* No close error.      */
	    writelinesmsg(fn, nline, nchar);
	}
    } else {			/* Ignore close error   */
	(void) ffclose();	/* if a write error.    */
    }
    if (s != FIOSUC)		/* Some sort of error.      */
	return FALSE;
    return TRUE;
}

/*
 * The command allows the user
 * to modify the file name associated with
 * the current buffer. It is like the "f" command
 * in UNIX "ed". The operation is simple; just zap
 * the name in the BUFFER structure, and mark the windows
 * as needing an update. You can type a blank line at the
 * prompt if you wish.
 */
/* ARGSUSED */
int
vl_filename(int f GCC_UNUSED, int n GCC_UNUSED)
{
    register int s;
    char fname[NFILEN];

    if (end_named_cmd()) {
	return showcpos(FALSE, 1);
    }

    if ((s = mlreply_file("Name: ", (TBUFF **) 0, FILEC_UNKNOWN, fname))
	== ABORT)
	return s;
    if (s == FALSE)
	return s;

#if OPT_LCKFILES
    if (global_g_val(GMDUSEFILELOCK)) {
	if (!b_val(curbp, MDLOCKED) && !b_val(curbp, MDVIEW))
	    release_lock(curbp->b_fname);
	ch_fname(curbp, fname);
	make_global_b_val(curbp, MDLOCKED);
	make_global_b_val(curbp, VAL_LOCKER);
	grab_lck_file(curbp, fname);
    } else
#endif
	ch_fname(curbp, fname);
    curwp->w_flag |= WFMODE;
    updatelistbuffers();
    return TRUE;
}

/*
 * Insert file "fname" into the current
 * buffer, Called by insert file command. Return the final
 * status of the read.
 */
int
ifile(char *fname, int belowthisline, FILE * haveffp)
{
    register LINEPTR prevp;
    register LINEPTR newlp;
    register LINEPTR nextp;
    register BUFFER *bp;
    register int s;
    int nbytes;
    register int nline;

    bp = curbp;			/* Cheap.               */
    b_clr_flags(bp, BFINVS);	/* we are not temporary */
    if (!haveffp) {
	if ((s = ffropen(fname)) == FIOERR)	/* Hard file open.          */
	    goto out;
	if (s == FIOFNF)	/* File not found.  */
	    return no_such_file(fname);
#if	OPT_ENCRYPT
	if ((s = vl_resetkey(curbp, fname)) != TRUE)
	    return s;
#endif
	mlwrite("[Inserting...]");
	CleanToPipe();

    } else {			/* we already have the file pointer */
	ffp = haveffp;
    }
    prevp = DOT.l;
    DOT.o = 0;
    MK = DOT;

    nline = 0;
    nextp = null_ptr;
    while ((s = ffgetline(&nbytes)) <= FIOSUC) {
#if OPT_DOSFILES
	if (b_val(curbp, MDDOS)
	    && (nbytes > 0)
	    && fflinebuf[nbytes - 1] == '\r')
	    nbytes--;
#endif
	if (!belowthisline) {
	    nextp = prevp;
	    prevp = lback(prevp);
	}

	beginDisplay();
	if (add_line_at(curbp, prevp, fflinebuf, nbytes) != TRUE) {
	    s = FIOMEM;
	    break;
	}
	newlp = lforw(prevp);
	tag_for_undo(newlp);
	endofDisplay();

	prevp = belowthisline ? newlp : nextp;
	++nline;
	if (s < FIOSUC)
	    break;
    }
    if (!haveffp) {
	CleanAfterPipe(FALSE);
	(void) ffclose();	/* Ignore errors.       */
	readlinesmsg(nline, s, fname, FALSE);
    }
  out:
    /* advance to the next line and mark the window for changes */
    DOT.l = lforw(DOT.l);

    /* copy window parameters back to the buffer structure */
    copy_traits(&(curbp->b_wtraits), &(curwp->w_traits));

    imply_alt(fname, FALSE, FALSE);
    chg_buff(curbp, WFHARD);

    return (s != FIOERR);
}

/*
 * Insert file "fname" into the kill register
 * Called by insert file command. Return the final
 * status of the read.
 */
static int
kifile(char *fname)
{
    register int i;
    register int s;
    register int nline;
    int nbytes;

    ksetup();
    if ((s = ffropen(fname)) == FIOERR)		/* Hard file open.  */
	goto out;
    if (s == FIOFNF)		/* File not found.  */
	return no_such_file(fname);

    nline = 0;
#if	OPT_ENCRYPT
    if ((s = vl_resetkey(curbp, fname)) == TRUE)
#endif
    {
	mlwrite("[Reading...]");
	CleanToPipe();
	while ((s = ffgetline(&nbytes)) <= FIOSUC) {
	    for (i = 0; i < nbytes; ++i)
		if (!kinsert(fflinebuf[i]))
		    return FIOMEM;
	    if ((s == FIOSUC) && !kinsert('\n'))
		return FIOMEM;
	    ++nline;
	    if (s < FIOSUC)
		break;
	}
	CleanAfterPipe(FALSE);
    }
    kdone();
    (void) ffclose();		/* Ignore errors.       */
    readlinesmsg(nline, s, fname, FALSE);

  out:
    return (s != FIOERR);
}

int create_save_dir(char *dirnam);

/* try really hard to create a private subdirectory in some tmp
 * space to save the modified buffers in.  start with $TMPDIR, and
 * then the rest of the table.  if all that fails, try and create a subdir
 * of the current directory.
 */
int
create_save_dir(char *dirnam)
{
#if HAVE_MKDIR && !SYS_MSDOS && !SYS_OS2
    static char *tbl[] =
    {
	0,			/* reserved for $TMPDIR */
#if SYS_UNIX
	"/var/tmp",
	"/usr/tmp",
	"/tmp",
#endif
	"."
    };

    char *np;
    unsigned n;

    if ((np = getenv("TMPDIR")) != 0 &&
	(int) strlen(np) < 32 &&
	is_directory(np)) {
	tbl[0] = np;
	n = 0;
    } else {
	n = 1;
    }
    for (; n < TABLESIZE(tbl); n++) {
	if (is_directory(tbl[n])) {
	    (void) pathcat(dirnam, tbl[n], "vileDXXXXXX");
	    (void) mktemp(dirnam);
	    /* on failure, keep going */
	    if (vl_mkdir(dirnam, 0700) == 0)
		return TRUE;
	}
    }
#endif /* no mkdir, or dos, or os/2 */
    return FALSE;
}

#if SYS_UNIX
static const char *mailcmds[] =
{
    "/usr/lib/sendmail",
    "/sbin/sendmail",
    "/usr/sbin/sendmail",
    "/bin/mail",
    0
};
#endif

/* called on hangups, interrupts, and quits */
/* This code is definitely not production quality, or probably very
	robust, or probably very secure.  I whipped it up to save
	myself while debugging...		pgf */
/* on the other hand, it has limped along for well over six years now :-/ */

SIGT
imdying(int ACTUAL_SIG_ARGS)
{
    static char dirnam[NSTRING] = "";
    static int wrote = 0;
    static int i_am_dead;
    char filnam[NFILEN];
    BUFFER *bp;
    int bad_karma = FALSE;
#if SYS_UNIX
    char cmd[NFILEN + 250];
#endif
    static int created = FALSE;
    char temp[NFILEN];

#if SYS_APOLLO
    extern char *getlogin(void);
#endif /* SYS_APOLLO */

#if OPT_WORKING && defined(SIGALRM)
    setup_handler(SIGALRM, SIG_IGN);
#endif

    if (i_am_dead++)		/* prevent recursive faults */
	_exit(signo);

#if SYS_APOLLO
    (void) lsprintf(cmd,
		    "(echo signal %d killed vile;/com/tb %d)| /bin/mail %s",
		    signo, getpid(), getlogin());
    (void) system(cmd);
#endif /* SYS_APOLLO */

    /* write all modified buffers to the temp directory */
    set_global_g_val(GMDIMPLYBUFF, FALSE);	/* avoid side-effects! */
    for_each_buffer(bp) {
	if (!b_is_temporary(bp) &&
	    bp->b_active == TRUE &&
	    b_is_changed(bp)) {
	    if (!created) {
		created = create_save_dir(dirnam);
	    }
	    if (created) {
		/* then the path is dir+file */
		(void) pathcat(filnam, dirnam, bp->b_bname);
	    } else {
		/* no dir: the path is V+file */
		(void) strcpy(filnam,
			      strcat(strcpy(temp, "V"), bp->b_bname));
	    }
	    set_b_val(bp, MDVIEW, FALSE);
	    if (writeout(filnam, bp, TRUE, FALSE) != TRUE) {
		bad_karma = TRUE;
		continue;
	    }
	    wrote++;
	}
    }
#if SYS_UNIX			/* try and send mail */
    /* if we wrote, or tried to */
    if (wrote || bad_karma) {
	const char **mailcmdp;
	struct stat sb;
	char *np;
	/* choose the first mail sender we can find.
	   it used to be you could rely on /bin/mail being
	   a simple mailer, but no more.  and sendmail has
	   been moving around too. */
	for (mailcmdp = mailcmds; *mailcmdp != 0; mailcmdp++) {
	    if (stat(*mailcmdp, &sb) == 0)
		break;
	}
	if (*mailcmdp &&
	    ((np = getenv("LOGNAME")) != 0 ||
	     (np = getenv("USER")) != 0)) {
	    char *cp;
	    cp = lsprintf(cmd, "( %s %s; %s; %s; %s %d;",
			  "echo To:", np,
			  "echo Subject: vile died, files saved",
			  "echo",
			  "echo vile died due to signal", signo);

#if HAVE_GETHOSTNAME
	    {
		char hostname[128];
		if (gethostname(hostname, sizeof(hostname)) < 0)
		    (void) strcpy(hostname, "unknown");
		hostname[sizeof(hostname) - 1] = EOS;
		cp = lsprintf(cp, "%s %s;",
			      "echo on host", hostname);
	    }
#endif
	    cp = lsprintf(cp,
			  "echo the following files were saved in directory %s;",
			  dirnam);

	    cp = lsprintf(cp, "%s%s%s;",
#if HAVE_MKDIR
	    /* reverse sort so '.' comes last, in case it
	     * terminates the mail message early */
			  "ls -a ", dirnam, " | sort -r"
#else
			  "ls ", dirnam, "/V*"
#endif
		);

	    if (bad_karma)
		cp = lsprintf(cp, "%s %s",
			      "echo errors:  check in current",
			      "directory too.;");

	    cp = lsprintf(cp, ") | %s %s", *mailcmdp, np);

	    (void) system(cmd);
	}
    }
    if (signo > 2) {
	ttclean(FALSE);
	abort();
    }
#else
    if (wrote) {
	fprintf(stderr, "vile died: Files saved in directory %s\r\n",
		dirnam);
    }
    if (bad_karma) {
	fprintf(stderr, "problems encountered during save.  sorry\r\n");
    }
#endif

    tidy_exit(wrote ? BADEXIT : GOODEXIT);
    /* NOTREACHED */
    SIGRET;
}

void
markWFMODE(BUFFER *bp)
{
    register WINDOW *wp;	/* scan for windows that need updating */
    for_each_visible_window(wp) {
	if (wp->w_bufp == bp)
	    wp->w_flag |= WFMODE;
    }
}
