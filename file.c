/*
 * The routines in this file
 * handle the reading and writing of
 * disk files. All of details about the
 * reading and writing of the disk are
 * in "fileio.c".
 *
 * $Id: file.c,v 1.466 2025/01/26 14:10:09 tom Exp $
 */

#include "estruct.h"
#include "edef.h"
#include "nefsms.h"

#if SYS_WINNT || SYS_MINGW
#include <io.h>			/* for mktemp */
#include <direct.h>		/* for mkdir */
#define vl_mkdir(path,mode) mkdir(path)
#else
#define	vl_mkdir(path,mode) mkdir(path,mode)
#endif

#ifdef HAVE_UMASK
#define vl_umask(n) umask(n)
#else
#define vl_umask(n) n
#endif

#if CC_NEWDOSCC
#include <io.h>
#endif

#if CC_CSETPP
#define isFileMode(mode) (mode & S_IFREG) == S_IFREG
#else
#define isFileMode(mode) (mode & S_IFMT) == S_IFREG
#endif

#define NO_BNAME "NoName"

#if SYS_WINNT && DISP_NTWIN
    /*
     * Create a shadow copy of ffstatus for the purposes of distinguishing
     * a new file from an existing file.  A new file does not yet exist on
     * disk and the user may decide not to write back its contents before
     * exiting the editor.
     *
     * A shadow copy is used here to prevent breaking a _lot_ of existing
     * code that references the value of ffstatus.
     */
static FFType ffshadow;
#endif

static int
FIO2Status(int fio)
{
    int result;

    if (fio < FIOERR)		/* no error - all ok */
	result = TRUE;
    else if (fio == FIOERR)	/* a non-fatal error */
	result = FALSE;
    else if (fio == FIOABRT)	/* user killed it */
	result = SORTOFTRUE;
    else			/* something bad happened */
	result = ABORT;
    return result;
}

/*--------------------------------------------------------------------------*/

/* Returns the modification time iff the path corresponds to an existing file,
 * otherwise it returns zero.
 */
time_t
file_modified(char *path)
{
    struct stat statbuf;
    time_t the_time = 0;

    (void) file_stat(path, NULL);
    if (file_stat(path, &statbuf) >= 0
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
    time_t curtime = 0;
    int modtimes_mismatch = FALSE;
    int fuids_mismatch = FALSE;
#ifdef CAN_CHECK_INO
    FUID curfuid;
#endif

    remind = again = "";

    if (bp == NULL || bp->b_fname == NULL)
	return FALSE;
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
		fuids_mismatch = !fileuid_compare(check_against, &curfuid);
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
	    if (!b_val(bp, MDLOCKED) && !b_val(bp, MDVIEW))
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
    int status;
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
    WINDOW *wp;

    for_each_visible_window(wp)
	(void) check_file_changed(wp->w_bufp, wp->w_bufp->b_fname);
    return TRUE;
}
#endif /*  MDCHK_MODTIME */

#ifdef MDCHK_MODTIME
int
get_modtime(BUFFER *bp, time_t *the_time)
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

/*
 * Utility functions for preparing-for, and cleanup-from a pipe.  We have to
 * switch back to cooked I/O mode while the pipe is running, in case it is
 * interactive, e.g.,
 *	:w !more
 * as well as remember to repaint the screen, if we wrote a pipe rather than
 * reading it.
 */
static int must_clean_pipe;	/* copy of ffstatus (it is reset in ffclose) */
static int writing_to_pipe;	/* if writing, we disable 'working...' */

static void
CleanToPipe(int writing)
{
    must_clean_pipe = (ffstatus == file_is_pipe);
    writing_to_pipe = writing;

    if (must_clean_pipe) {
	if (writing) {
	    beginDisplay();
	}
#if SYS_UNIX || SYS_MSDOS
	term.clean(TRUE);
#else
#ifdef GMDW32PIPES
	kbd_erase_to_end(0);
	kbd_flush();
#endif
	term.kclose();
#endif
    }
}

static void
CleanAfterPipe(int Wrote)
{
    if (must_clean_pipe) {
#if SYS_UNIX || SYS_MSDOS
	term.unclean();		/* may clear the screen as a side-effect */
	term.open();		/* (re)open */
	term.kopen();
	term.flush();
	if (Wrote)
	    pressreturn();
	sgarbf = TRUE;
#else
	term.kopen();
#ifdef GMDW32PIPES
	if (global_g_val(GMDW32PIPES)) {
	    if (Wrote)
		pressreturn();
	    sgarbf = TRUE;
	}
#endif
#endif
	must_clean_pipe = FALSE;

	if (writing_to_pipe) {
	    writing_to_pipe = FALSE;
	    endofDisplay();
	}
    }
}

/*
 * On faster machines, a pipe-writer will tend to keep the pipe full. This
 * function is used by 'slowreadf()' to test if we've not done an update
 * recently even if this is the case.
 */
#if SYS_UNIX && OPT_SHELL
static int
slowtime(time_t *refp)
{
    int status = FALSE;

    if (ffstatus == file_is_pipe) {
	time_t temp = time((time_t *) 0);

	status = (!ffhasdata() || (temp != *refp));
	if (status)
	    *refp = temp;
    }
    return status;
}
#else
# if SYS_WINNT
#  define slowtime(refp) ((ffstatus == file_is_pipe) && !nowait_pipe_cmd && !ffhasdata())
# else
#  define slowtime(refp) ((ffstatus == file_is_pipe) && !ffhasdata())
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

#if OPT_VMS_PATH
static char *
resolve_filename(BUFFER *bp)
{
    char temp[NFILEN];
#if SYS_VMS
    ch_fname(bp, fgetname(ffp, temp));
#else
    /*
     * The fgetname() should be just a shortcut; we should already have a
     * usable filename in bp->b_fname.
     */
    if (bp->b_fname == 0) {
	TRACE(("resolve_filename NULL\n"));
    } else if (is_vms_pathname(bp->b_fname, FALSE)
	       && strcspn(bp->b_fname, ":[];") != strlen(bp->b_fname)) {
	TRACE(("resolve_filename NONEED: %s\n", bp->b_fname));
    } else {
	char temp2[NFILEN];
	mklower(strcpy(temp2, bp->b_fname));
	if (realpath(temp2, temp)) {
	    (void) unix2vms_path(temp, temp);
	    TRACE(("resolve_filename %s -> %s\n", bp->b_fname, temp));
	    ch_fname(bp, temp);
	} else {
	    TRACE(("resolve_filename FAILED: %s\n", bp->b_fname));
	}
    }
#endif
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

    if (fname == NULL
	|| bp->b_fname == NULL
	|| isInternalName(fname)
	|| isInternalName(bp->b_fname)) {
	return (status);
    }

    TRACE(("same_fname(\n\tfname=%s,\n\tbname=%s,\n\tlengthen=%d)\n",
	   fname, bp->b_bname, lengthen));

    if (lengthen)
	fname = lengthen_path(vl_strncpy(temp, fname, sizeof(temp)));

#if OPT_VMS_PATH
    /* ignore version numbers in this comparison unless both are given */
    if (is_vms_pathname(fname, FALSE)) {
	char *bname = bp->b_fname;
	char *s = version_of(bname);
	char *t = version_of((char *) fname);

	if ((!explicit_version(s)
	     || !explicit_version(t))
	    && ((s - bname) == (t - fname))) {
	    status = !strnicmp(fname, bname, (size_t) (s - bname));
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
fileuid_get(const char *fname GCC_UNUSED, FUID * fuid GCC_UNUSED)
{
#ifdef CAN_CHECK_INO
    struct stat sb;

    TRACE(("fileuid_get(%s) discarding cache\n", fname));
    (void) file_stat(fname, NULL);
    memset(fuid, 0, sizeof(*fuid));
    if (file_stat(fname, &sb) == 0) {
	fuid->ino = sb.st_ino;
	fuid->dev = sb.st_dev;
	fuid->valid = TRUE;
	return TRUE;
    }
#endif
    return FALSE;
}

void
fileuid_set(BUFFER *bp GCC_UNUSED, FUID * fuid GCC_UNUSED)
{
#ifdef CAN_CHECK_INO
    bp->b_fileuid = *fuid;	/* struct copy */
#endif
}

void
fileuid_invalidate(BUFFER *bp GCC_UNUSED)
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
    (void) fuid1;
    (void) fuid2;
    return (FALSE);		/* Doesn't really matter */
#endif
}

int
fileuid_same(BUFFER *bp, FUID * fuid)
{
#ifdef CAN_CHECK_INO
    return fileuid_compare(&bp->b_fileuid, fuid);
#else
    (void) bp;
    (void) fuid;
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
#if OPT_TITLE
    set_editor_title();
#endif
}

static int
cannot_reread(void)
{
    mlforce("[Cannot reread]");
    return FALSE;
}

/*
 * Zap the given buffer without trying to close its window, by renaming it
 * to the unnamed-buffer and resetting its local modes.  Keeping the window
 * sidesteps the issue of whether it was the only buffer, while allowing us
 * to free the associated memory.
 *
 * FIXME: this may also have a majormode associated with it, should remove it.
 */
static void
reset_to_unnamed(BUFFER *bp)
{
    WINDOW *wp;
    int n;

    for (n = 0; n < NUM_B_VALUES; ++n) {
	if (is_local_b_val(bp, n)) {
	    make_global_b_val(bp, n);
	}
    }
    bclear(bp);
    set_bname(bp, UNNAMED_BufName);
    ch_fname(bp, "");

    for_each_window(wp) {
	if (wp->w_bufp == bp) {
	    wp->w_flag |= (WFHARD | WFMODE);
	}
    }
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
    BUFFER *bp = curbp;
    int status;
    char fname[NFILEN];
    W_VALUES *save_wvals;

    TRACE((T_CALLED "fileread(%d,%d)\n", f, n));

    if (more_named_cmd()) {
	if ((status = mlreply_file("Replace with file: ", (TBUFF **) 0,
				   FILEC_REREAD, fname)) != TRUE)
	    returnCode(status);
    } else if (b_is_temporary(bp)) {
	returnCode(cannot_reread());
    } else if (!(global_g_val(GMDWARNREREAD) || bp->b_fname[0] == EOS)
	       || (mlyesno("Reread current buffer") == TRUE)) {
	(void) vl_strncpy(fname, bp->b_fname, sizeof(fname));
	/* Check if we are rereading an unnamed-buffer if it is not
	 * associated with a file.
	 */
	if (is_internalname(bp->b_fname)) {
	    WINDOW *wp;
	    if (eql_bname(bp, STDIN_BufName)
		|| eql_bname(bp, UNNAMED_BufName)) {
		status = bclear(bp);
		if (status == TRUE)
		    mlerase();
		for_each_visible_window(wp) {
		    if (wp->w_bufp == bp)
			wp->w_flag |= (WFMODE | WFHARD);
		}
		returnCode(status);
	    }
	    returnCode(cannot_reread());
	}
    } else {
	returnCode(FALSE);
    }

#if OPT_LCKFILES
    /* release own lock before read the replaced file */
    if (global_g_val(GMDUSEFILELOCK)) {
	if (!b_val(bp, MDLOCKED) && !b_val(bp, MDVIEW))
	    release_lock(bp->b_fname);
    }
#endif

    /* we want no errors or complaints, so mark it unchanged */
    b_clr_changed(bp);

    save_wvals = save_window_modes(bp);
    status = readin(fname, TRUE, bp, TRUE);
    if (status == ABORT) {
	reset_to_unnamed(bp);
    } else {
	set_buffer_name(bp);
#if OPT_FINDERR
	/*
	 * Doing ":e!" on [Output] will reset the buffer name to match the
	 * command.  Even if it was not [Output], it is still useful to make
	 * the error-buffer match the last-read shell/pipe output.
	 */
	if (isShellOrPipe(bp->b_fname))
	    set_febuff(bp->b_bname);
#endif
    }
    restore_window_modes(bp, save_wvals);

    returnCode(status);
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

static int
bp2swbuffer(BUFFER *bp, int ask_rerun, int lockfl)
{
    int s;

    if ((s = (bp != NULL)) != FALSE) {
	if (bp->b_active) {
	    if (ask_rerun) {
		switch (mlyesno("Old command output -- rerun")) {
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
	if (s == TRUE)
	    s = swbuffer_lfl(bp, lockfl, FALSE);
	if (s == TRUE)
	    curwp->w_flag |= WFMODE | WFHARD;
    }

    return s;
}

int
set_files_to_edit(const char *prompt, int appflag)
{
    int status;
    BUFFER *bp, *bp_next;

    char fname[NFILEN];
    char *actual;
    BUFFER *firstbp = NULL;

    TRACE((T_CALLED "set_files_to_edit(%s, %d)\n", NONNULL(prompt), appflag));

    if ((status = mlreply_file(prompt, &lastfileedited,
			       FILEC_READ | FILEC_EXPAND,
			       fname)) == TRUE) {
	while ((actual = filec_expand()) != NULL) {
	    if ((bp = getfile2bp(actual, !clexec, FALSE)) == NULL) {
		status = FALSE;
		break;
	    }
	    bp->b_flag |= BFARGS;	/* treat this as an argument */
	    if (firstbp == NULL) {
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
	if (find_bp(firstbp) != NULL) {
	    status = bp2swbuffer(firstbp, FALSE, TRUE);
	    if (status == ABORT)
		reset_to_unnamed(firstbp);
	}
	if (status == TRUE)
	    set_last_bp(firstbp);
    }
    returnCode(status);
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
    int s;			/* status return */
    char *actual;
    static TBUFF *last;

    if ((s = mlreply_file("View file: ", &last, FILEC_READ | FILEC_EXPAND,
			  fname)) == TRUE) {
	while ((actual = filec_expand()) != NULL) {
	    if ((s = getfile(actual, FALSE)) != TRUE)
		break;
	    /* if we succeed, put it in view mode */
	    set_local_b_val(curwp->w_bufp, MDVIEW, TRUE);
	    markWFMODE(curwp->w_bufp);
	}
    }
    return s;
}

/* utility routine for no. of lines read */
static void
readlinesmsg(int n, int s, const char *f, int rdo)
{
    char fname[NFILEN], *short_f;
    const char *m;

    /* if "f" is a pipe cmd, it can be arbitrarily long */
    vl_strncpy(fname, f, sizeof(fname));
    fname[sizeof(fname) - 1] = '\0';

    short_f = shorten_path(fname, TRUE);
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
	if ((aname = is_appendname(fn)) != NULL) {
	    fn = aname;
	    action = "Appended";
	} else {
	    action = "Wrote";
	}
	sprintf(strlines, "%d", nline);
	sprintf(strchars, "%lu", nchar);
	lines_len = (int) strlen(strlines);
	chars_len = (int) strlen(strchars);
	outlen = (term.cols - 1) -
	    (
		(int) (sizeof(WRITE_FILE_FMT) - 13) +
		(int) strlen(action) +
		lines_len +
		chars_len +
		(int) strlen(PLURAL(nline)) +
		(int) strlen(PLURAL(nchar))
	    );
	mlforce(WRITE_FILE_FMT,
		action,
		strlines,
		PLURAL(nline),
		strchars,
		PLURAL(nchar),
		path_trunc(fn, outlen, tmp, (int) sizeof(tmp)));
    } else {
	mlforce("[%d lines]", nline);
    }
#undef WRITE_FILE_FMT
}

static void
set_rdonly_modes(BUFFER *bp, int always)
{
    /* set view mode for read-only files */
    if ((global_g_val(GMDRONLYVIEW))) {
	set_local_b_val(bp, MDVIEW, TRUE);
    }
    /* set read-only mode for read-only files */
    if (always || global_g_val(GMDRONLYRONLY)) {
	set_local_b_val(bp, MDREADONLY, TRUE);
    }
}

#if COMPLETE_FILES || COMPLETE_DIRS
static void
ff_read_directory(BUFFER *bp, char *fname)
{
    int count = fill_directory_buffer(bp, fname, (size_t) 0);

    mlwrite("[Read %d line%s]", count, PLURAL(count));
    b_set_flags(bp, BFDIRS);
    set_rdonly_modes(bp, TRUE);
}

/*
 * If we are inserting text from a directory list, make a buffer if necessary,
 * and redirect ffgetline() to read from that buffer.
 */
static int
ff_load_directory(char *fname)
{
    if (ffstatus == file_is_internal) {
	char temp[NFILEN];
	BUFFER *bp = find_b_file(lengthen_path(vl_strncpy(temp, fname,
							  sizeof(temp))));

	if (bp == NULL)
	    bp = getfile2bp(temp, FALSE, FALSE);

	if (bp == NULL)
	    return FIOERR;

	ffbuffer = bp;
	ff_read_directory(bp, temp);
	ffcursor = lforw(buf_head(bp));
	bp->b_active = TRUE;
    }
    return FIOSUC;
}
#endif

/*
 * Insert file "fname" into the kill register.  Called by insert file command.
 * Return the final status of the read as true/false/abort.
 */
static int
kifile(char *fname)
{
#if OPT_ENCRYPT
    BUFFER *bp = curbp;
#endif
    int s;
    int nline;
    size_t i;
    size_t nbytes;

    TRACE((T_CALLED "kifile(%s)\n", NONNULL(fname)));

    ksetup();
    if ((s = ffropen(fname)) == FIOERR) {	/* Hard file open */
	goto out;
    } else if (s == FIOFNF) {	/* File not found */
	returnCode(no_such_file(fname));
#if COMPLETE_FILES || COMPLETE_DIRS
    } else if ((s = ff_load_directory(fname)) == FIOERR) {
	goto out;
#endif
    } else {

	nline = 0;
#if OPT_ENCRYPT
	if ((s = vl_resetkey(bp, fname)) == TRUE)
#endif
	{
	    mlwrite("[Reading...]");
	    CleanToPipe(FALSE);
	    while ((s = ffgetline(&nbytes)) <= FIOSUC) {
		for (i = 0; i < nbytes; ++i)
		    if (!kinsert(fflinebuf[i]))
			returnCode(FIOMEM);
		if ((s == FIOSUC) && !kinsert('\n')) {
		    s = FIOMEM;
		    goto out;
		}
		++nline;
		if (s < FIOSUC)
		    break;
	    }
	    CleanAfterPipe(FALSE);
	}
	kdone();
	(void) ffclose();	/* Ignore errors.       */
	readlinesmsg(nline, s, fname, FALSE);
    }

  out:
    returnCode(FIO2Status(s));
}

/*
 * Insert a file into the current buffer.  This is really easy; all you do is
 * find the name of the file, and call the standard "insert a file into the
 * current buffer" code.
 */
/* ARGSUSED */
int
insfile(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int status;
    char fname[NFILEN];
    static TBUFF *last;

    TRACE((T_CALLED "insfile(%d, %d)\n", f, n));

    if (calledbefore) {
	(void) vl_strncpy(fname, tb_values(last), sizeof(fname));
    } else {
	if ((status = mlreply_file("Insert file: ", &last,
				   FILEC_READ | FILEC_PROMPT, fname)) != TRUE)
	    returnCode(status);
    }

    if (ukb == 0)
	status = ifile(fname, TRUE, (FILE *) 0);
    else
	status = kifile(fname);

    if (status == ABORT) {
	TRACE(("undo insert to recover memory\n"));
	undo(FALSE, 1);
    }
    returnCode(status);
}

BUFFER *
getfile2bp(const char *fname,	/* file name to find */
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
	|| (strlen(fname) > (size_t) NBUFN - 1)) {

	/* It's not already here by that buffer name.
	 * Try to find it assuming we're given the file name.
	 */
	(void) lengthen_path(vl_strncpy(nfname, fname, sizeof(nfname)));
	if (is_internalname(nfname)) {
	    mlforce("[Buffer not found]");
	    return NULL;
	}
#ifdef CAN_CHECK_INO
	if (global_g_val(GMDUNIQ_BUFS)) {
	    have_fuid = fileuid_get(nfname, &fuid);
	    if (have_fuid) {
		for_each_buffer(bp) {
		    /* is the same unique file */
		    if (fileuid_same(bp, &fuid)) {
			return bp;
		    }
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
	    if (!b_is_argument(bp) &&
		!b_is_changed(bp) &&
		is_empty_buf(bp) &&
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
	    s = mlreply("Will use buffer name: ", bname, (int) sizeof(bname));
	    if (s == ABORT)
		return NULL;
	    if (s == FALSE || bname[0] == EOS)
		makename(bname, fname);
	}
	/* okay, we've got a unique name -- create it */
	if (bp == NULL && (bp = bfind(bname, 0)) == NULL) {
	    mlwarn("[Cannot create buffer]");
	    return NULL;
	}
	/* switch and read it in. */
	ch_fname(bp, nfname);
    }
    return bp;
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
getfile(char *fname,		/* file name to find */
	int lockfl)		/* check the file for locks? */
{
    BUFFER *bp;

    if (isEmpty(fname))
	return no_file_found();

    /* if there are no path delimiters in the name, then the user
       is likely asking for an existing buffer -- try for that
       first */
    if ((bp = find_b_file(fname)) == NULL
	&& ((strlen(fname) > (size_t) NBUFN - 1)	/* too big to be a bname */
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
static int
check_percent_crlf(BUFFER *bp, int doslines, int unixlines)
{
    double total = (((double) doslines + (double) unixlines)
		    * (double) b_val(bp, VAL_PERCENT_CRLF));
    double value = ((double) doslines * 100.0);
    return (total > 0) && (value >= total);
}

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
	result = check_percent_crlf(bp, doslines, unixlines);
    }
    TRACE2(("apply_dosmode %d dos:%d unix:%d\n", result, doslines, unixlines));

    /*
     * Make 'dos' and 'recordseparator' modes local, since this
     * transformation should apply only to the specified file.
     */
    set_record_sep(bp, result ? RS_CRLF : RS_LF);
}

static void
strip_if_dosmode(BUFFER *bp)
{
    if (b_val(bp, MDDOS)) {	/* if it _is_ a dos file, strip 'em */
	LINE *lp;
	int flag = FALSE;

	TRACE(("stripping CR's for dosmode\n"));
	if (b_val(bp, MDUNDO_DOS_TRIM))
	    mayneedundo();

	for_each_line(lp, bp) {
	    int len = llength(lp);
	    if (len > 0) {
		int chr = lgetc(lp, len - 1);
		if ((chr == '\r')
		    || (len == 1 && chr == '\032')) {
		    if (b_val(bp, MDUNDO_DOS_TRIM)) {
			CopyForUndo(lp);
		    }
		    llength(lp)--;
		    flag = TRUE;
		}
	    }
	}

	/*
	 * Force the screen to repaint if we changed anything.  This
	 * operation is not undoable, otherwise we would call chg_buff().
	 */
	if (flag) {
	    if (b_val(bp, MDUNDO_DOS_TRIM))
		chg_buff(bp, WFHARD);
	    else
		set_winflags(TRUE, WFHARD);
	    b_clr_counted(bp);
	}
    }
}

static void
guess_dosmode(BUFFER *bp)
{
    int doslines = 0, unixlines = 0;
    LINE *lp;

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
    apply_dosmode(bp, doslines, unixlines);
    strip_if_dosmode(bp);
    TRACE(("guess_dosmode %d\n", b_val(bp, MDDOS)));
}

/*
 * Forces the current buffer to be either 'dos' or 'unix' format.
 */
void
explicit_dosmode(BUFFER *bp, RECORD_SEP record_sep)
{
    TRACE(("explicit_dosmode(%s)\n",
	   choice_to_name(&fsm_recordsep_blist, record_sep)));

    set_record_sep(bp, record_sep);
}

/*
 * Recompute the buffer size, redisplay the [Settings] buffer.
 */
static int
modified_record_sep(BUFFER *bp, RECORD_SEP record_sep)
{
    TRACE((T_CALLED "modified_record_sep(%s)\n",
	   choice_to_name(&fsm_recordsep_blist, record_sep)));

    explicit_dosmode(bp, record_sep);
    guess_dosmode(bp);
    explicit_dosmode(bp, record_sep);	/* ignore the guess - only want to strip CR's */

    b_clr_counted(bp);
    bsizes(bp);

    returnCode(TRUE);
}

/*
 * Forces the current buffer to be in DOS-mode, stripping any trailing CR's.
 */
/*ARGSUSED*/
int
set_rs_crlf(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return modified_record_sep(curbp, RS_CRLF);
}

/* as above, but forces unix-mode instead */
/*ARGSUSED*/
int
set_rs_lf(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return modified_record_sep(curbp, RS_LF);
}

/* as above, but forces macintosh-mode instead */
/*ARGSUSED*/
int
set_rs_cr(int f GCC_UNUSED, int n GCC_UNUSED)
{
    return modified_record_sep(curbp, RS_CR);
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
	char *locked_by;

	if (!set_lock(fname, locker, sizeof(locker))) {
	    /* we didn't get it */
	    if ((locked_by = strmalloc(locker)) != 0) {
		set_local_b_val(bp, MDVIEW, TRUE);
		set_local_b_val(bp, MDLOCKED, TRUE);
		make_local_b_val(bp, VAL_LOCKER, locked_by);
		set_b_val_ptr(bp, VAL_LOCKER, locked_by);
		markWFMODE(bp);
	    }
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
 * Analyze the file to determine its format
 */
static RECORD_SEP
guess_recordseparator(BUFFER *bp, UCHAR * buffer, B_COUNT length, L_NUM * lines)
{
    B_COUNT count_lf = 0;
    B_COUNT count_cr = 0;
    B_COUNT count_dos = 0;	/* CRLF's */
    B_COUNT count_unix = 0;	/* LF's w/o preceding CR */
    B_COUNT n;
    RECORD_SEP result;

    *lines = 0;
    for (n = 0; n < length; ++n) {
	switch (buffer[n]) {
	case '\r':
	    ++count_cr;
	    break;
	case '\n':
	    ++count_lf;
	    if (n != 0) {
		if (buffer[n - 1] == '\r')
		    ++count_dos;
		else
		    ++count_unix;
	    }
	    break;
	}
    }

    TRACE(("guess_recordseparator assume %s, rs=%s CR:%ld, LF:%ld, CRLF:%ld\n",
	   global_b_val(MDDOS) ? "dos" : "unix",
	   choice_to_name(&fsm_recordsep_blist, global_b_val(VAL_RECORD_SEP)),
	   (long) (count_cr - count_dos), (long) count_unix, (long) count_dos));

    if (count_lf != 0) {
	result = RS_LF;
	if (global_b_val(VAL_RECORD_SEP) == RS_AUTO
	    && (count_dos == 0 || count_dos == count_lf)) {
	    result = count_dos ? RS_CRLF : RS_LF;
	} else if (global_b_val(MDDOS)) {
	    if ((bp->b_flag & BFEXEC) != 0) {
		result = (count_dos && !count_unix) ? RS_CRLF : RS_LF;
	    } else if (check_percent_crlf(bp, (int) count_dos, (int) count_unix)) {
		result = RS_CRLF;
	    }
	} else if (global_b_val(VAL_RECORD_SEP) == RS_CR
		   && (count_cr || !count_unix)) {
	    result = RS_CR;
	}
	if (buffer[length - 1] != '\n')
	    ++count_lf;
	*lines = (L_NUM) ((result == RS_CR) ? count_cr : count_lf);
    } else if (count_cr != 0) {
	result = RS_CR;
	if (buffer[length - 1] != '\r')
	    ++count_cr;
	*lines = (L_NUM) count_cr;
    } else {
	*lines = 1;		/* we still need a line, to allocate data */
	result = RS_SYS_DEFAULT;
    }

    TRACE(("...line count = %ld, format=%s\n", (long) *lines,
	   choice_to_name(&fsm_recordsep_blist, result)));
    return result;
}

/*
 * Return the index in buffer[] past the end of the record we're pointing to
 * with 'offset'.
 */
static B_COUNT
next_recordseparator(UCHAR * buffer, B_COUNT length, RECORD_SEP rscode,
		     B_COUNT offset)
{
    B_COUNT result = offset;
    B_COUNT n;
    int done = FALSE;

    for (n = offset; !done && (n < length); ++n) {
	switch (buffer[n]) {
	case '\r':
	    if (rscode == RS_CR) {
		result = n + 1;
		done = TRUE;
	    }
	    break;
	case '\n':
	    if (rscode == RS_CRLF || rscode == RS_LF) {
		result = n + 1;
		done = TRUE;
	    }
	    break;
	}
    }
    if (!done)
	result = length;
    return result;
}

/*
 * If reading from an external file, read the whole file into memory at once
 * and split it into lines.  That is potentially much faster than allocating
 * each line separately.
 */
static int
quickreadf(BUFFER *bp, int *nlinep)
{
    B_COUNT length;
    B_COUNT request;
    B_COUNT offset;
    LINE *lp;
    L_NUM lineno = 0;
    L_NUM nlines;
    RECORD_SEP rscode;
    UCHAR *buffer;
    int rc;

    (void) lineno;

    TRACE((T_CALLED "quickreadf(buffer=%s, file=%s)\n", bp->b_bname, bp->b_fname));

    beginDisplay();
    if (ffsize(&request) < 0) {
	mlwarn("[Can't size file]");
	rc = FIOERR;
    } else if (request == 0) {
	/* avoid malloc(0) problems down below; let slowreadf() do the work */
	rc = FIONUL;
    } else if ((buffer = castalloc(UCHAR, request + 1)) == NULL) {
	rc = FIOMEM;
    }
#if OPT_ENCRYPT
    else if ((rc = vl_resetkey(bp, bp->b_fname)) != TRUE) {
	free(buffer);
    }
#endif
    else if (ffread((char *) buffer, request, &length) < 0
#if !SYS_VMS
	/*
	 * For most systems, the advertised size of the file will match the
	 * number of chars that we can read from it.  However, VMS's structured
	 * filetypes such as VFC or VAR/CR will return fewer since we are not
	 * using the structure information.
	 */
	     || (length != request)
#endif
	) {
	free(buffer);
	mlerror("reading");
	rc = FIOERR;
    } else {
#if OPT_ENCRYPT
	if (b_val(bp, MDCRYPT)
	    && bp->b_cryptkey[0]) {	/* decrypt the file */
	    vl_setup_encrypt(bp->b_cryptkey, vl_seed);
	    vl_encrypt_blok((char *) buffer, (UINT) length);
	}
#endif
#if OPT_MULTIBYTE
	decode_bom(bp, buffer, &length);
	deduce_charset(bp, buffer, &length, TRUE);
#endif

	/*
	 * Analyze the file to determine its format:
	 */
	rscode = guess_recordseparator(bp, buffer, length, &nlines);

	/*
	 * Modify readin()'s setting for newline mode if needed:
	 */
	if (length == 0
	    || (buffer[length - 1] != (UCHAR) (rscode == RS_CR ? '\r' : '\n'))) {
	    set_b_val(bp, MDNEWLINE, FALSE);
	}

	/* allocate all of the line structs we'll need */
	if ((bp->b_LINEs = typeallocn(LINE, (unsigned) nlines)) == NULL) {
	    free(buffer);
	    ffrewind();
	    rc = FIOMEM;
	} else {
	    bp->b_ltext = buffer;
	    bp->b_ltext_end = bp->b_ltext + length;
	    bp->b_bytecount = length;
	    bp->b_linecount = nlines;
	    bp->b_LINEs_end = bp->b_LINEs + nlines;

	    /* loop through the buffer again, creating
	       line data structure for each line */
#pragma warning(suppress: 6386)
	    memset(bp->b_LINEs, 0, sizeof(LINE));
	    for (lp = bp->b_LINEs, offset = 0; lp != bp->b_LINEs_end; ++lp) {
		B_COUNT next = next_recordseparator(buffer, length, rscode, offset);
		if (next == offset) {
		    break;
		}
#if !SMALLER
		lp->l_number = ++lineno;
#endif
		llength(lp) = (C_NUM) (next - offset - 1);
		if (!b_val(bp, MDNEWLINE) && next == length)
		    llength(lp) += 1;
		lp->l_size = (size_t) llength(lp) + 1;
		lvalue(lp) = (char *) (buffer + offset);
		set_lforw(lp, lp + 1);
		if (lp != bp->b_LINEs)
		    set_lback(lp, lp - 1);
		lsetclear(lp);
		lp->l_nxtundo = NULL;
#if OPT_LINE_ATTRS
		lp->l_attrs = NULL;
#endif
		decode_charset(bp, lp);
		offset = next;
	    }
	    if (lp != bp->b_LINEs)
		lp--;		/* point at last line again */

	    /* connect the end of the list */
	    set_lforw(lp, buf_head(bp));
	    set_lback(buf_head(bp), lp);

	    /* connect the front of the list */
	    set_lback(bp->b_LINEs, buf_head(bp));
	    set_lforw(buf_head(bp), bp->b_LINEs);

	    init_b_traits(bp);

	    *nlinep = nlines;

	    set_record_sep(bp, rscode);
	    strip_if_dosmode(bp);

	    rc = FIOSUC;
	}
    }
    endofDisplay();
    returnCode(rc);
}

/*
 * Read file "fname" into a buffer, blowing away any text found there.  Returns
 * the final status of the read.
 *
 * fname  - name of file to read
 * lockfl - check for file locks?
 * bp     - read into this buffer
 * mflg   - print messages?
 */
/* ARGSUSED */
int
readin(char *fname, int lockfl, BUFFER *bp, int mflg)
{
    WINDOW *wp;
    int s;
    int nline;
#if OPT_ENCRYPT
    int local_crypt = valid_buffer(bp)
    && is_local_val(bp->b_values.bv, MDCRYPT);
#endif

    TRACE((T_CALLED "readin(fname=%s, lockfl=%d, bp=%p, mflg=%d)\n",
	   fname, lockfl, (void *) bp, mflg));

#if SYS_WINNT && DISP_NTWIN
    ffshadow = file_is_closed;	/* an assumption */
#endif

    if (bp == NULL)		/* doesn't hurt to check */
	returnCode(FALSE);

    if (*fname == EOS) {
	TRACE(("...called with NULL fname\n"));
	returnCode(TRUE);	/* we do not want to do that */
    }

    if ((s = bclear(bp)) != TRUE)	/* Might be old.    */
	returnCode(s);

#if OPT_ENCRYPT
    /* bclear() gets rid of local flags */
    if (local_crypt) {
	set_local_b_val(bp, MDCRYPT, TRUE);
    }
#endif

    b_clr_flags(bp, BFINVS | BFCHG);
    ch_fname(bp, fname);
    fname = bp->b_fname;	/* this may have been b_fname! */
#if OPT_DOSFILES
    /* assume that if our OS wants it, that the buffer will have CRLF
     * lines.  this may change when the file is read, based on actual
     * line counts, below.  otherwise, if there's an error, or the
     * file doesn't exist, we will keep this default.
     */
    set_local_b_val(bp, MDDOS, system_crlf);
#endif
    set_local_b_val(bp, MDNEWLINE, TRUE);

    if ((s = ffropen(fname)) == FIOERR) {	/* Hard file error */
	/* do nothing -- error has been reported,
	   and it will appear as empty buffer */
	/*EMPTY */ ;
    } else if (s == FIOFNF) {	/* File not found */
#if SYS_WINNT && DISP_NTWIN
	ffshadow = file_is_new;
#endif
	if (mflg)
	    mlwrite("[New file]");
#if COMPLETE_FILES || COMPLETE_DIRS
    } else if (ffstatus == file_is_internal) {
	ff_read_directory(bp, fname);
#endif
    } else {

	if (mflg) {
#define READING_FILE_FMT "[Reading %s]"

	    int outlen;
	    char tmp[NFILEN];

	    outlen = ((term.cols - 1) -
		      (int) (sizeof(READING_FILE_FMT) - 3));
	    mlforce(READING_FILE_FMT,
		    path_trunc(fname, outlen, tmp, (int) sizeof(tmp)));
#undef READING_FILE_FMT
	}
#if OPT_VMS_PATH
	if (!isInternalName(bp->b_fname))
	    fname = resolve_filename(bp);
#endif
	/* start reading */
	nline = 0;
#if OPT_WORKING
	max_working = cur_working = old_working = 0;
#endif

	if (ffstatus == file_is_pipe
	    || global_g_val(GVAL_READER_POLICY) == RP_SLOW) {
	    s = slowreadf(bp, &nline);
	} else {
	    s = quickreadf(bp, &nline);
	    if (s == FIONUL
		|| (s == FIOMEM
		    && global_g_val(GVAL_READER_POLICY) == RP_BOTH))
		s = slowreadf(bp, &nline);
	}

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
	    if (ffstatus == file_is_pipe)
		set_febuff(bp->b_bname);
#endif
	    (void) ffclose();	/* Ignore errors.       */
	    if (mflg)
		readlinesmsg(nline, s, fname, ffronly(fname));

	    if (ffronly(fname)) {
		set_rdonly_modes(bp, isShellOrPipe(fname));
	    }

	    bp->b_active = TRUE;
	    bp->b_lines_on_disk = bp->b_linecount;
	}

    }

    /*
     * Set the majormode if the file's suffix matches.
     */
    if (s < FIOERR) {
	infer_majormode(bp);
    }

    for_each_window(wp) {
	if (wp->w_bufp == bp) {
	    init_window(wp, bp);
	}
    }
    if (s < FIOERR)
	imply_alt(fname, FALSE, lockfl);
    updatelistbuffers();

#if OPT_LCKFILES
    if (lockfl && s != FIOERR)
	grab_lck_file(bp, fname);
#endif
#if OPT_MODELINE
    /* do this before the $read-hook, so one can set a majormode in the
     * modeline, causing the file to be colored.
     */
    do_modelines(bp);
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
    returnCode(FIO2Status(s));
}

/*
 * Read the file into a given buffer, setting dot to the first line.
 */
int
bp2readin(BUFFER *bp, int lockfl)
{
    int status = readin(bp->b_fname, lockfl, bp, TRUE);

    if (status == TRUE) {
	bp->b_active = TRUE;

#if SYS_WINNT && DISP_NTWIN
	if (status &&
	    (!(isShellOrPipe(bp->b_fname) ||
	       ffshadow == file_is_new ||
	       b_is_registered(bp)))) {
	    /* save file path to windows registry for later recall */

	    store_recent_file_or_folder(bp->b_fname, TRUE);
	    b_set_registered(bp);
	}
#endif
    }

    return status;
}

/*
 * Read a file slowly, e.g., a line at a time, for instance from a pipe.
 */
int
slowreadf(BUFFER *bp, int *nlinep)
{
    int s;
    size_t len;
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

    TRACE((T_CALLED "slowreadf(buffer=%s, file=%s)\n", bp->b_bname, bp->b_fname));

#if OPT_ENCRYPT
    if ((s = vl_resetkey(bp, bp->b_fname)) != TRUE)
	returnCode(s);
#endif
    b_set_counted(bp);		/* make 'addline()' do the counting */
    b_set_reading(bp);
    make_local_b_val(bp, MDDOS);	/* keep it local, if not */
    bp->b_lines_on_disk = 0;
    while ((s = ffgetline(&len)) <= FIOSUC) {
#if OPT_MULTIBYTE
	/*
	 * ffgetline() does the checks for UTF-16, etc., since it is in the
	 * right place to account for alignment bytes.  Copy the attributes
	 * from its buffer to ours so the decode_charset() call will work.
	 */
	if (bp->b_lines_on_disk == 0) {
	    COPY_B_VAL(bp, btempp, VAL_BYTEORDER_MARK);
	    COPY_B_VAL(bp, btempp, VAL_FILE_ENCODING);
	}
	bp->implied_BOM = btempp->implied_BOM;
#endif
#if OPT_DOSFILES
	/*
	 * Strip CR's if we are reading in DOS-mode.  Otherwise,
	 * keep any CR's that we read.
	 */
	if (global_b_val(MDDOS)) {
	    if (len != 0 && fflinebuf[len - 1] == '\r') {
		doslines++;
	    } else {
		unixlines++;
	    }
	}
#endif
	if (addline(bp, fflinebuf, (int) len) != TRUE) {
	    s = FIOMEM;
	    break;
	}
#if SYS_UNIX || SYS_MSDOS || SYS_OS2 || SYS_WINNT
	else {
	    decode_charset(bp, lback(buf_head(bp)));
	    bp->b_lines_on_disk += 1;
	    /* reading from a pipe, and internal? */
	    if (slowtime(&last_updated)) {
		WINDOW *wp;

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
#if OPT_MULTIBYTE
    /*
     * Look for UTF-8 encoding when we have the entire buffer, since only a
     * small part of it may be distinct from ASCII.
     */
    if (b_is_enc_AUTO(bp)) {
	LINE *lp;
	int check, found = SORTOFTRUE;

	TRACE(("...try looking for UTF-8\n"));
	for_each_line(lp, bp) {
	    if (llength(lp) > 0) {
		check = check_utf8((UCHAR *) lvalue(lp), (B_COUNT) llength(lp));
		if (check == FALSE) {
		    found = FALSE;
		} else if (check == TRUE) {
		    found = TRUE;
		}
	    }
	}
	if (found == TRUE) {
	    found_utf8(bp);
	}
    }
#endif
#if OPT_DOSFILES
    if (global_b_val(MDDOS)) {
	apply_dosmode(bp, doslines, unixlines);
	strip_if_dosmode(bp);
    }
#endif
    init_b_traits(bp);
    b_clr_reading(bp);
    returnCode(s);
}

/*
 * Append to the buffer-name characters up to the allowed length, which will
 * make a well-formed name.  The 'blanks' option is true to allow embedded
 * blanks, e.g., for a shell command.  A trailing null is added after the
 * 'len' bytes have been appended.
 */
static char *
add_to_name(char *target, const char *source, size_t len, int blanks)
{
    while (len-- != 0) {
	UINT ch;
#if OPT_MULTIBYTE
	if (vl_encoding >= enc_UTF8) {
	    int used;
	    int check;
	    int code;

	    used = vl_conv_to_utf32(&ch, source, (B_COUNT) len + 1);
	    if (used <= 0)
		break;
	    source += used;

	    if ((code = vl_ucs_to_8bit(&check, (int) ch)) != 0) {
		if (ch <= 127)
		    ch = (UINT) check;
		else
		    code = 0;
	    }
	    if (!code) {
		if (isPrint(ch)) {
		    ch = 'X';
		} else {
		    ch = '?';
		}
	    }
	} else {
	    ch = CharOf(*source++);
	}
#else
	ch = CharOf(*source++);
#endif
	if (!isPrint(ch))
	    break;
	if (isSpace(ch))
	    ch = (UINT) (blanks ? ' ' : '-');
	*target++ = (char) ch;
    }
    *target = '\0';		/* buffer names are always null-terminated */
    return target;
}

/*
 * Take a (null-terminated) file name, and from it fabricate a buffer name.
 * This routine knows about the syntax of file names on the target system.  I
 * suppose that this information could be put in a better place than a line of
 * code.
 */
void
makename(char *bname, const char *fname)
{
    char *fcp;
    char *bcp;
    char temp[NFILEN];

    TRACE(("makename(%s)\n", fname));

    *bname = EOS;
    fcp = skip_string(vl_strncpy(temp, fname, sizeof(temp)));
#if OPT_VMS_PATH
    if (is_vms_pathname(temp, TRUE)) {
	vms2unix_path(temp, temp);
	(void) strncpy0(bname, pathleaf(temp), NBUFN);
	strip_version(bname);
    } else if (is_vms_pathname(temp, FALSE)) {
	for (; fcp > temp && !strchr(":]", fcp[-1]); fcp--) {
	    ;
	}
	(void) strncpy0(bname, fcp, NBUFN);
	strip_version(bname);
	(void) mklower(bname);
    } else
#endif
    {
	/* trim trailing whitespace */
	while (fcp != temp && (isBlank(fcp[-1])
#if SYS_UNIX || OPT_MSDOS_PATH	/* trim trailing slashes as well */
			       || is_slashc(fcp[-1])
#endif
	       )) {
	    *(--fcp) = EOS;
	}
	fcp = skip_space_tab(temp);

#if SYS_UNIX || SYS_MSDOS || SYS_VMS || SYS_OS2 || SYS_WINNT
	bcp = bname;
	if (isShellOrPipe(fcp)) {
	    /* ...it's a shell command; bname is first word */
	    *bcp++ = SCRTCH_LEFT[0];
	    *bcp++ = SHPIPE_LEFT[0];
	    do {
		++fcp;
	    } while (isSpace(*fcp));

	    bcp = add_to_name(bcp, fcp, (size_t) (NBUFN - 4), TRUE);
	    (void) strcpy(bcp, SCRTCH_RIGHT);

	} else {

	    (void) add_to_name(bcp, pathleaf(fcp), (size_t) (NBUFN - 1), FALSE);

	}

#else /* !(SYS_UNIX||SYS_VMS||SYS_MSDOS) */

	(void) add_to_name(bcp, fcp, NBUFN - 1, FALSE);

#endif
    }
    if (*bname == EOS)
	(void) strcpy(bname, NO_BNAME);
    TRACE((" -> '%s'\n", bname));
}

/* generate a unique name for a buffer */
void
unqname(char *name)
{
    size_t j;
    char newname[NBUFN * 2];
    char suffixbuf[NBUFN];
    int suffixlen;
    int adjust;
    int i = 0;
    size_t k;

    j = strlen(name);
    if (j == 0)
	j = strlen(strcpy(name, NO_BNAME));

    /* check to see if this name is in use */
    add_to_name(newname, name, (size_t) (NBUFN - 1), TRUE);
    adjust = is_scratchname(newname);
    strcpy(suffixbuf, "-");
    while (find_b_name(newname) != NULL) {
	/* from "foo" create "foo-1" or "foo-a1b5" */
	/* from "thisisamuchlongernam" create
	   "thisisamuchlongern-1" or
	   "thisisamuchlong-a1b5" */
	/* that is, put suffix at end if it fits, or else
	   overwrite some of the name to make it fit */
	/* the suffix is in "base 36" */
	suffixlen = format_int(suffixbuf + 1, (UINT)++ i, 36) + 1;
	if ((suffixlen + 2) >= NBUFN)
	    break;

	k = ((size_t) NBUFN - 1 - (size_t) suffixlen);
	if (j < k)
	    k = j;
	if (adjust) {
	    strcpy(&newname[k - 1], suffixbuf);
	    strcat(newname, SCRTCH_RIGHT);
	} else
	    strcpy(&newname[k], suffixbuf);
    }
    strncpy0(name, newname, (size_t) NBUFN);
    TRACE(("unqname ->%s\n", name));
}

/*
 * Ask for a file name, and write the
 * contents of the current buffer to that file.
 */
int
filewrite(int f, int n)
{
    int s;
    char fname[NFILEN];
    int forced = (f && n == SPECIAL_BANG_ARG);
    BUFFER *bp = curbp;

    if (more_named_cmd()) {
	if ((s = mlreply_file("Write to file: ", (TBUFF **) 0,
			      (UINT) (forced
				      ? FILEC_WRITE2
				      : FILEC_WRITE),
			      fname)) != TRUE)
	    return s;
    } else
	(void) vl_strncpy(fname, bp->b_fname, sizeof(fname));

    if ((s = writeout(fname, bp, forced, TRUE)) == TRUE)
	unchg_buff(bp, 0);
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
    int s;
    int forced = (f && n == SPECIAL_BANG_ARG);	/* then it was :w! */
    BUFFER *bp = curbp;

    if (no_file_name(bp->b_fname))
	return FALSE;

    if ((s = writeout(bp->b_fname, bp, forced, TRUE)) == TRUE)
	unchg_buff(bp, 0);
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

#if OPT_MULTIBYTE
static int
write_encoded_text(BUFFER *bp, const char *buf, int nbuf, const char *ending)
{
    int rc;

    if (b_val(bp, VAL_FILE_ENCODING) > enc_UTF8) {
	int len = encode_charset(bp, buf, nbuf, ending);
	/*
	 * UTF-16 and UTF-32 buffers are generally bigger than UTF-8.
	 */
	if (nbuf > (2 * len)) {
	    rc = FIOABRT;
	    mlforce("Error writing buffer as %s (encoding error)!",
		    encoding2s(b_val(bp, VAL_FILE_ENCODING)));
	} else {
	    rc = ffputline(bp->encode_utf_buf, len, NULL);
	}
    } else {
	rc = ffputline(buf, nbuf, ending);
    }
    return rc;
}
#define FFPUTLINE(bp,buf,len,end) write_encoded_text(bp,buf,len,end)
#else
#define FFPUTLINE(bp,buf,len,end) ffputline(buf,len,end)
#endif

int
write_region(BUFFER *bp, REGION * rp, int encoded, int *nline, B_COUNT * nchar)
{
    int s;
    LINE *lp;
    const char *ending = get_record_sep(bp);
    B_COUNT len_rs = (B_COUNT) len_record_sep(bp);
    C_NUM offset = rp->r_orig.o;

    (void) encoded;

    lp = rp->r_orig.l;
    *nline = 0;			/* Number of lines     */
    *nchar = 0;			/* Number of chars     */

#if OPT_MULTIBYTE
    if ((s = write_bom(bp)) != FIOSUC) {
	goto out;
    }
#else
    s = FIOSUC;
#endif

    /* first (maybe partial) line and succeeding whole lines */
    while ((rp->r_size + (B_COUNT) offset) >= (B_COUNT) line_length(lp)) {
	C_NUM len = llength(lp) - offset;
	char *text = lvalue(lp) + offset;

	/* If this is the last line (and no fragment will be written
	 * after the line), allow 'newline' mode to suppress the
	 * trailing newline.
	 */
	if (rp->r_size <= line_length(lp)) {
	    rp->r_size = 0;
	    if (!b_val(bp, MDNEWLINE))
		ending = "";
	} else {
	    rp->r_size -= line_length(lp);
	}
#if OPT_SELECTIONS
	if (encoded) {
	    TBUFF *temp;
	    if ((temp = encode_attributes(lp, bp, rp)) != NULL) {
		text = tb_values(temp);
		len = (int) tb_length(temp);
	    }
	    if ((s = FFPUTLINE(bp, text, len, ending)) != FIOSUC) {
		tb_free(&temp);
		goto out;
	    }
	    tb_free(&temp);
	} else
#endif
	if ((s = FFPUTLINE(bp, text, len, ending)) != FIOSUC)
	    goto out;

	*nline += 1;
	*nchar += line_length(lp);
	offset = 0;
	lp = lforw(lp);
    }

    /* last line (fragment) */
    if (rp->r_size > 0) {
#if OPT_SELECTIONS
	C_NUM len = llength(lp);
	char *text = lvalue(lp);

	if (encoded) {
	    TBUFF *temp;
	    if ((temp = encode_attributes(lp, bp, rp)) != NULL) {
		text = tb_values(temp);
		len = (int) tb_length(temp);
	    }
	    if ((s = FFPUTLINE(bp, text, len, NULL)) != FIOSUC) {
		tb_free(&temp);
		goto out;
	    }
	    tb_free(&temp);
	} else
#endif
	if ((s = FFPUTLINE(bp, lvalue(lp), (int) rp->r_size, NULL)) != FIOSUC)
	    goto out;
	*nchar += rp->r_size;
	*nline += 1;		/* it _looks_ like a line */
    }

  out:
    return s;
}

static int
actually_write(REGION * rp, char *fn, int msgf, BUFFER *bp, int forced, int encoded)
{
    int s;
    int nline;
    B_COUNT nchar;

    /* this is adequate as long as we cannot write parts of lines */
    int whole_file = ((rp->r_orig.l == lforw(buf_head(bp)))
		      && (rp->r_end.l == buf_head(bp)));

    TRACE((T_CALLED "actually_write %s\n",
	   encoded ? "encoded" : "not encoded"));
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
	    (void) getregion(bp, rp);
	}
    }
    /*
     * The write-hook may have set the buffer to view-only, or returned
     * an error.  Do not write the buffer in that case.
     */
    switch (s2status(tb_values(last_macro_result))) {
    case TRUE:
    case SORTOFTRUE:
	break;
    default:
	mlwarn("[write-hook returned %s]", tb_values(last_macro_result));
	returnCode(FALSE);
    }
    if (same_fname(fn, bp, FALSE) && b_val(bp, MDVIEW)) {
	mlwarn("[Buffer view-mode has changed]");
	returnCode(FALSE);
    }
#endif

#if OPT_ENCRYPT
    if ((s = vl_resetkey(bp, fn)) != TRUE)
	returnCode(s);
#endif

    /* open writes error message, if needed */
    if (ffwopen(fn, forced) != FIOSUC)
	returnCode(FALSE);

    /* disable "working..." while we are writing - not reading - a pipe, since
     * it may be a quasi-interactive process that we don't want to modify its
     * display.
     */
    if (isShellOrPipe(fn)) {
	beginDisplay();
    }

    /* tell us we're writing */
    if (msgf == TRUE)
	mlwrite("[Writing...]");

    CleanToPipe(TRUE);

    s = write_region(bp, rp, encoded, &nline, &nchar);
    if (s == FIOSUC) {		/* No write error.      */
#if OPT_VMS_PATH
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

    CleanAfterPipe(TRUE);

    if (isShellOrPipe(fn)) {
	endofDisplay();
    }

    if (s != FIOSUC)		/* Some sort of error.      */
	returnCode(FALSE);

#ifdef MDCHK_MODTIME
    set_modtime(bp, fn);
#endif
    fileuid_set_if_valid(bp, fn);
    /*
     * If we've written the unnamed-buffer, rename it according to the file.
     * FIXME: maybe we should do this to all internal-names?
     */
    if (!i_am_dead
	&& whole_file
	&& eql_bname(bp, UNNAMED_BufName)
	&& !isShellOrPipe(fn)
	&& find_b_file(fn) == NULL) {
	ch_fname(bp, fn);
	set_buffer_name(bp);
    }

    imply_alt(fn, whole_file, FALSE);
    returnCode(TRUE);
}

static int
file_protection(char *fn)
{
    int result = -1;
    struct stat sb;

    if (!isShellOrPipe(fn)) {
	if (file_stat(fn, &sb) == 0) {
	    if (isFileMode(sb.st_mode))
		result = (int) (sb.st_mode & 0777);
	}
    }
    return result;
}

static int
writereg(REGION * rp,
	 const char *given_fn,
	 int msgf,
	 BUFFER *bp,
	 int forced,
	 int encoded)
{
    int status;
    char fname[NFILEN], *fn;
    int protection = -1;

    if (no_file_name(given_fn)) {
	status = FALSE;
    } else if (!forced && b_val(bp, MDREADONLY)
	       && (bp->b_fname == NULL || !strcmp(given_fn, bp->b_fname))) {
	mlwarn("[Buffer mode is \"readonly\"]");
	status = FALSE;
    } else {
	if (isShellOrPipe(given_fn)
	    && bp->b_fname != NULL
	    && !strcmp(given_fn, bp->b_fname)
	    && mlyesno("Are you sure (this was a pipe-read)") != TRUE) {
	    mlwrite("File not written");
	    status = FALSE;
	} else {
	    fn = lengthen_path(vl_strncpy(fname, given_fn, sizeof(fname)));
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
#if SYS_WINNT
		    ULONG prev_access_mask = 0;
		    int write_acl_added = FALSE;
#endif
		    if (forced) {
#if SYS_WINNT
			/*
			 * If running cygwin on an NTFS partition, cygwin's
			 * chmod utility adds ACLs to files to simulate
			 * unix file permissions.  In the case of readonly
			 * files (e.g., $ chmod a=r fn), the win32 posix
			 * interfaces don't work well, if at all.  So make
			 * the current file object a bit more Unix API
			 * friendly, if necessary.
			 */

			write_acl_added = w32_add_write_acl(fn,
							    &prev_access_mask);
#endif
			if (!ffaccess(fn, FL_WRITEABLE)
			    && (protection = file_protection(fn)) >= 0) {
			    (void) chmod(SL_TO_BSL(fn), (mode_t) (protection
								  | 0600));
			}
		    }
		    status = actually_write(rp, fn, msgf, bp, forced, encoded);
		    if (protection > 0)
			(void) chmod(SL_TO_BSL(fn), (mode_t) protection);
#if SYS_WINNT
		    if (write_acl_added)
			(void) w32_remove_write_acl(fn, prev_access_mask);
#endif
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
    int status;

    setup_file_region(bp, &region);

    status = writereg(&region, fn, msgf, bp, forced, FALSE);

#if SYS_WINNT && DISP_NTWIN
    if (status && (!(isShellOrPipe(fn) || b_is_registered(bp)))) {
	/* save file path to windows registry for later recall */

	store_recent_file_or_folder(fn, TRUE);
	b_set_registered(bp);
    }
#endif

    return (status);
}

/*
 * Write the currently-selected region (i.e., the range of lines from DOT to
 * MK, inclusive).
 */
static int
prompt_and_write_region(int encoded)
{
    BUFFER *bp = curbp;
    REGION region;
    int status;
    char fname[NFILEN];

    if (end_named_cmd()) {
	if (mlyesno("Okay to write [possible] partial range") != TRUE) {
	    mlwrite("Range not written");
	    return FALSE;
	}
	(void) vl_strncpy(fname, bp->b_fname, sizeof(fname));
    } else {
	if ((status = mlreply_file("Write region to file: ",
				   (TBUFF **) 0, FILEC_WRITE | FILEC_PROMPT,
				   fname)) != TRUE)
	    return status;
    }
    if ((status = getregion(bp, &region)) == TRUE)
	status = writereg(&region, fname, TRUE, bp, FALSE, encoded);
    return status;
}

int
writeregion(void)
{
    return prompt_and_write_region(FALSE);
}

#if OPT_SELECTIONS
int
write_enc_region(void)
{
    return prompt_and_write_region(TRUE);
}

#endif

/*
 * This function writes the kill register to a file
 * Uses the file management routines in the
 * "fileio.c" package. The number of lines written is
 * displayed.
 */
int
kwrite(char *fn, int msgf)
{
#if OPT_ENCRYPT
    BUFFER *bp = curbp;
#endif
    KILL *kp;			/* pointer into kill register */
    int nline;
    B_COUNT nchar;
    int s;
    int c;
    unsigned i;
    char *sp;			/* pointer into string to insert */

    /* make sure there is something to put */
    if (kbs[ukb].kbufh == NULL) {
	if (msgf)
	    mlforce("Nothing to write");
	return FALSE;		/* not an error, just nothing */
    }
#if OPT_ENCRYPT
    if ((s = vl_resetkey(bp, fn)) != TRUE)
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
    BUFFER *bp = curbp;
    int s;
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
	if (!b_val(bp, MDLOCKED) && !b_val(bp, MDVIEW))
	    release_lock(bp->b_fname);
	ch_fname(bp, fname);
	make_global_b_val(bp, MDLOCKED);
	make_global_b_val(bp, VAL_LOCKER);
	grab_lck_file(bp, fname);
    } else
#endif
	ch_fname(bp, fname);

#if SYS_WINNT && DISP_NTWIN
    /* remember new filename if later written out to disk */
    b_clr_registered(bp);
#endif

    curwp->w_flag |= WFMODE;
    updatelistbuffers();
    return TRUE;
}

/*
 * Insert file "fname" into the current buffer, called by insert file command.
 * Return the final status of the read.
 */
int
ifile(char *fname, int belowthisline, FILE *haveffp)
{
    BUFFER *bp = curbp;
    LINE *prevp;
    LINE *newlp;
    LINE *nextp;
    int status;
    size_t nbytes;
    int nline;

    TRACE((T_CALLED "ifile(fname=%s, belowthisline=%d, haveffp=%p)\n",
	   NONNULL(fname), belowthisline, (void *) haveffp));

    b_clr_invisible(bp);	/* we are not temporary */
    if (haveffp == NULL) {
	if ((status = ffropen(fname)) == FIOERR)	/* Hard file open */
	    goto out;
	else if (status == FIOFNF)	/* File not found */
	    returnCode(no_such_file(fname));
#if COMPLETE_FILES || COMPLETE_DIRS
	else if ((status = ff_load_directory(fname)) == FIOERR)
	    goto out;
	if (b_is_directory(bp)) {
	    /* special case: contents are already added */
	    goto out;
	}
#endif
#if OPT_ENCRYPT
	if ((status = vl_resetkey(bp, fname)) != TRUE)
	    returnCode(status);
#endif
	mlwrite("[Inserting...]");
	CleanToPipe(FALSE);

    } else {			/* we already have the file pointer */
	ffp = haveffp;
    }
    prevp = DOT.l;
    DOT.o = b_left_margin(curbp);
    MK = DOT;

    nline = 0;
    nextp = NULL;
    while ((status = ffgetline(&nbytes)) <= FIOSUC) {
#if OPT_DOSFILES
	if (b_val(bp, MDDOS)
	    && (nbytes != 0)
	    && fflinebuf[nbytes - 1] == '\r')
	    nbytes--;
#endif
	if (!belowthisline) {
	    nextp = prevp;
	    prevp = lback(prevp);
	}

	beginDisplay();
	if (add_line_at(bp, prevp, fflinebuf, (int) nbytes) != TRUE) {
	    status = FIOMEM;
	    newlp = NULL;
	} else {
	    newlp = lforw(prevp);
	    if (OkUndo(bp)) {
		if ((tag_for_undo(newlp)) == ABORT) {
		    status = FIOMEM;
		    /*
		     * De-link the line added for which we cannot undo.
		     * If we don't do this, undoworker() won't find a match
		     * against the buffer, and will be deadlocked.
		     */
		    lremove(bp, prevp);
		}
	    }
	}
	endofDisplay();

	if (status == FIOMEM)
	    break;

	prevp = belowthisline ? newlp : nextp;
	++nline;
	if (status < FIOSUC)
	    break;
    }
    if (!haveffp) {
	CleanAfterPipe(FALSE);
	(void) ffclose();	/* Ignore errors.       */
	readlinesmsg(nline, status, fname, FALSE);
    }

    /* mark the window for changes.  could this be moved up to
     * where we actually insert a line? */
    if (nline)
	chg_buff(bp, WFHARD);

  out:
    /* copy window parameters back to the buffer structure */
    copy_traits(&(bp->b_wtraits), &(curwp->w_traits));

    if (status < FIOERR)
	imply_alt(fname, FALSE, FALSE);

#if COMPLETE_FILES || COMPLETE_DIRS
    if (b_is_directory(bp)) {
	DOT.l = lback(DOT.l);
    } else
#endif
	/* advance to the next line */
	DOT.l = lforw(DOT.l);
    DOT.o = b_left_margin(curbp);

    returnCode(FIO2Status(status));
}

/* try really hard to create a private subdirectory in some tmp
 * space to save the modified buffers in.  start with $TMPDIR, and
 * then the rest of the table.  if all that fails, try and create a subdir
 * of the current directory.
 */
static int
create_save_dir(char *dirnam)
{
    int result = FALSE;
#if defined(HAVE_MKDIR) && !SYS_MSDOS && !SYS_OS2
    static const char *tbl[] =
    {
	NULL,			/* reserved for $TMPDIR */
#if SYS_UNIX
	"/var/tmp",
	"/usr/tmp",
	"/tmp",
#endif
	"."
    };

    char *np;
    unsigned n;

    if ((np = getenv("TMPDIR")) != NULL &&
	(int) strlen(np) < 32 &&
	is_directory(np)) {
	tbl[0] = np;
	n = 0;
    } else {
	n = 1;
    }

    for (; n < TABLESIZE(tbl); n++) {
	if (is_directory(tbl[n])) {
	    mode_t omask = vl_umask(0077);
	    (void) pathcat(dirnam, tbl[n], "vileDXXXXXX");

	    /* on failure, keep going */
#if defined(HAVE_MKSTEMP) && defined(HAVE_MKDTEMP)
	    result = (vl_mkdtemp(dirnam) != NULL);
#else
	    if (mktemp(dirnam) != 0 && *dirnam != '\0') {
		result = (vl_mkdir(dirnam, 0700) == 0);
	    }
#endif
	    (void) vl_umask(omask);
	    if (result)
		break;
	}
    }
#endif /* no mkdir, or dos, or os/2 */
    return result;
}

#if SYS_UNIX
static const char *mailcmds[] =
{
    "/usr/lib/sendmail",
    "/sbin/sendmail",
    "/usr/sbin/sendmail",
    "/bin/mail",
    NULL
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
    char filnam[NFILEN];
    BUFFER *bp;
    int bad_karma = FALSE;
#if SYS_UNIX
    char cmd[(NFILEN * 2) + 250];
#endif
    static int created = FALSE;
    char temp[NFILEN];
    char my_buffer[NSTRING];

#if OPT_WORKING && defined(SIGALRM)
    setup_handler(SIGALRM, SIG_IGN);
#endif

    if (i_am_dead++)		/* prevent recursive faults */
	_exit(signo);

    /*
     * Buffered I/O would do things that we can't rely upon in a signal
     * handler.  Force unbuffered I/O in ffopen/.../ffclose.
     */
    beginDisplay();
    fflinebuf = my_buffer;
    fflinelen = 0;
    ffp = NULL;
    ffd = -1;

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
		(void) vl_strncpy(filnam,
				  strcat(strcpy(temp, "V"), bp->b_bname),
				  sizeof(filnam));
	    }
	    set_b_val(bp, MDVIEW, FALSE);
	    if (writeout(filnam, bp, TRUE, FALSE) != TRUE) {
		bad_karma = TRUE;
		continue;
	    }
	    wrote++;
	}
    }
#if SYS_UNIX			/* try to send mail */
    /*
     * If VILE_ERROR_ABORT is defined, send mail even if there are no pieces
     * to pick up.  It makes it simpler to find the core file, if any was
     * forced, and ensures that we can distinguish an abnormal exit from
     * accidentally typing 'Q'.
     */
#ifndef VILE_ERROR_ABORT
    /* if we wrote, or tried to */
    if (wrote || bad_karma)
#endif
    {
	const char **mailcmdp;
	struct stat sb;
	char *np;
	/* choose the first mail sender we can find.
	   it used to be you could rely on /bin/mail being
	   a simple mailer, but no more.  and sendmail has
	   been moving around too. */
	for (mailcmdp = mailcmds; *mailcmdp != NULL; mailcmdp++) {
	    if (file_stat(*mailcmdp, &sb) == 0)
		break;
	}
	if (*mailcmdp &&
	    ((np = getenv("LOGNAME")) != NULL ||
	     (np = getenv("USER")) != NULL)) {
	    char *cp;
	    cp = lsprintf(cmd, "( %s %s; %s; %s; %s %d;",
			  "echo To:", np,
			  ((wrote || bad_karma)
			   ? "echo Subject: vile died, files saved"
			   : "echo Subject: vile died, no files saved"),
			  "echo",
			  "echo vile died due to signal", signo);

#ifdef HAVE_GETHOSTNAME
	    {
		char hostname[128];
		if (gethostname(hostname, sizeof(hostname)) < 0)
		    (void) strcpy(hostname, "unknown");
		hostname[sizeof(hostname) - 1] = EOS;
		cp = lsprintf(cp, "%s %s;",
			      "echo on host", hostname);
	    }
#endif
	    cp = lsprintf(cp, "echo current directory:;pwd;");
	    if (wrote) {
		cp = lsprintf(cp,
			      "echo the following files were saved in directory %s;",
			      dirnam);

		cp = lsprintf(cp, "%s%s%s;",
#ifdef HAVE_MKDIR
		/* reverse sort so '.' comes last, in case it
		 * terminates the mail message early */
			      "ls -a ", dirnam, " | sort -r"
#else
			      "ls ", dirnam, "/V*"
#endif
		    );
	    }

	    if (bad_karma)
		cp = lsprintf(cp, "%s %s",
			      "echo errors:  check in current",
			      "directory too.;");

	    (void) lsprintf(cp, ") | %s %s", *mailcmdp, np);

	    IGNORE_RC(system(cmd));
	}
    }
    term.cursorvis(TRUE);	/* ( this might work ;-) */
    if (
#ifdef SIGHUP
	   signo != SIGHUP &&
#endif
	   signo != SIGINT) {
	term.clean(FALSE);
#ifdef VILE_ERROR_ABORT
	ExitProgram(signo);
#else
	abort();
#endif
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

    tidy_exit(BADEXIT);
    /* NOTREACHED */
    SIGRET;
}

void
markWFMODE(BUFFER *bp)
{
    WINDOW *wp;			/* scan for windows that need updating */
    for_each_visible_window(wp) {
	if (wp->w_bufp == bp)
	    wp->w_flag |= WFMODE;
    }
}
