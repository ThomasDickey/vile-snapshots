/*	FILE.C:   for MicroEMACS
 *
 *	The routines in this file handle the reading, writing
 *	and lookup of disk files.  All of details about the
 *	reading and writing of the disk are in "fileio.c".
 *
 *
 * $Header: /users/source/archives/vile.vcs/RCS/file.c,v 1.223 1998/04/12 15:26:54 tom Exp $
 *
 */

#include	"estruct.h"
#include        "edef.h"

static	int	bp2swbuffer(BUFFER *bp, int ask_rerun, int lockfl);
static	int	kifile(char *fname);
static	int	writereg(REGION *rp, const char *fn, int msgf, BUFFER *bp, int forced);
static	void	readlinesmsg(int n, int s, const char *f, int rdo);

#if OPT_DOSFILES
/* give DOS the benefit of the doubt on ambiguous files */
# if CRLF_LINES
#  define MORETHAN >=
# else
#  define MORETHAN >
# endif
#endif

#if !(SYS_MSDOS || SYS_WIN31)
static	int	quickreadf(BUFFER *bp, int *nlinep);
#endif

/*--------------------------------------------------------------------------*/

/* Returns the modification time iff the path corresponds to an existing file,
 * otherwise it returns zero.
 */
#if	defined(MDCHK_MODTIME) || SYS_VMS || SYS_UNIX
time_t
file_modified(char *path)
{
	struct stat	statbuf;
	time_t		the_time = 0;

	if (stat(SL_TO_BSL(path), &statbuf) >= 0
#if CC_CSETPP
	 && (statbuf.st_mode & S_IFREG) == S_IFREG)
#else
	 && (statbuf.st_mode & S_IFMT) == S_IFREG)
#endif
	{
#if SYS_VMS
		the_time = statbuf.st_ctime; /* e.g., creation-time */
#else
		the_time = statbuf.st_mtime;
#endif
	}
	return the_time;
}
#endif

#ifdef	MDCHK_MODTIME
static int
PromptModtime (
BUFFER	*bp,
char *fname,
const char *question,
int	iswrite)
{
	int status = SORTOFTRUE;
	time_t current;
	char prompt[NLINE];

	if (!isInternalName(bp->b_fname)
	 && b_val(bp, MDCHK_MODTIME)
	 && bp->b_active	/* only buffers that are loaded */
	 && same_fname(fname, bp, FALSE)
	 && get_modtime(bp, &current)) {
		time_t check_against;
		const char *remind, *again;
		if (iswrite) {
			check_against = bp->b_modtime;
			remind = "Reminder: ";
			again = "";
		} else {
			remind = "";
			if (bp->b_modtime_at_warn) {
				check_against = bp->b_modtime_at_warn;
				again = "again ";
			} else {
				check_against = bp->b_modtime;
				again = "";
			}
		}

		if (check_against != current) {
			(void)lsprintf(prompt,
			"%sFile for buffer \"%s\" has changed %son disk.  %s",
				remind, bp->b_bname, again, question);
			if ((status = mlyesno( prompt )) != TRUE)
				mlerase();
			/* avoid reprompts */
			bp->b_modtime_at_warn = current;
		}
	}
	return status;
}

int
ask_shouldchange(BUFFER *bp)
{
	int status;
	status = PromptModtime(bp, bp->b_fname, "Continue", FALSE);
	return (status == TRUE || status == SORTOFTRUE);
}

int
get_modtime (BUFFER *bp, time_t *the_time)
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
	time_t	current;

	if (same_fname(fn, bp, FALSE) && get_modtime(bp, &current)) {
		bp->b_modtime = current;
		bp->b_modtime_at_warn = 0;
	}
}

int
check_modtime(BUFFER *bp, char *fn)
{
	int status = TRUE;

	if (PromptModtime(bp, fn, "Read from disk", FALSE) == TRUE) {
#if OPT_LCKFILES
		/* release own lock before read the file again */
		if ( global_g_val(GMDUSEFILELOCK) ) {
			if (!b_val(curbp,MDLOCKED) && !b_val(curbp,MDVIEW))
				release_lock(fn);
		}
#endif
		status = readin(fn, TRUE, bp, TRUE);
	}
	return status;
}

static int
inquire_modtime(BUFFER *bp, char *fn)
{
	register int status;
	if ((status = PromptModtime(bp, fn, "Continue write", TRUE)) != TRUE
	 && (status != SORTOFTRUE)) {
		mlforce("[Write aborted]");
		return FALSE;
	}
	return TRUE;
}

int
check_visible_modtimes (void)
{
	register WINDOW *wp;

	for_each_window(wp)
		(void)check_modtime(wp->w_bufp, wp->w_bufp->b_fname);
	return TRUE;
}
#endif	/* MDCHK_MODTIME */

#if SYS_UNIX || SYS_MSDOS
#define	CleanToPipe()	if (fileispipe) ttclean(TRUE)

static void
CleanAfterPipe (int Wrote)
{
	if (fileispipe == TRUE) {
		ttunclean();	/* may clear the screen as a side-effect */
	        TTflush();
		if (Wrote) pressreturn();
	        sgarbf = TRUE;
	}
}

#else /* !SYS_UNIX */
#ifdef GMDW32PIPES

static void
CleanToPipe(void)
{
    if (fileispipe)
    {
        if (global_g_val(GMDW32PIPES))
        {
            bottomleft();
            TTputc('\n');
            TTputc(' ');  /* Forces ntconio routines to flush \r \n */
            TTputc('\r'); /* Erase ' ' :-)      */
            TTflush();    /* Actually necessary */
        }
        else
            TTkclose();
    }
}

static void
CleanAfterPipe(int Wrote)
{
    if (fileispipe)
    {
        if (global_g_val(GMDW32PIPES))
        {
            if (Wrote) pressreturn();
            sgarbf = TRUE;
        }
        else
            TTkopen();
    }
}

#else /* !GMDW32PIPES */
#define	CleanToPipe()		TTkclose()
#define	CleanAfterPipe(f)	TTkopen()
#endif
#endif /* SYS_UNIX */

/*
 * On faster machines, a pipe-writer will tend to keep the pipe full. This
 * function is used by 'slowreadf()' to test if we've not done an update
 * recently even if this is the case.
 */
#if SYS_UNIX && OPT_SHELL
static int
slowtime (time_t *refp)
{
	int	status = FALSE;

	if (fileispipe) {
		time_t	temp = time((time_t *)0);

		status = (!ffhasdata() || (temp != *refp));
		if (status)
			*refp = temp;
	}
	return status;
}
#else
#define	slowtime(refp)	(fileispipe && !ffhasdata())
#endif

int
no_such_file(const char * fname)
{
	mlwarn("[No such file \"%s\"]", fname);
	return FALSE;
}

#if OPT_VMS_PATH
static char *
version_of(char *fname)
{
	register char	*s = strchr(fname, ';');
	if (s == 0)
		s = skip_string(fname);
	return s;
}

static int
explicit_version(char *ver)
{
	if (*ver++ == ';') {
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
	char	temp[NFILEN];
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
same_fname(char *fname, BUFFER *bp, int lengthen)
{
	char	temp[NFILEN];

	if (fname == 0
	 || bp->b_fname == 0
	 || isInternalName(fname)
	 || isInternalName(bp->b_fname))
		return FALSE;

	if (lengthen)
		fname = lengthen_path(strcpy(temp, fname));

#if OPT_VMS_PATH
	/* ignore version numbers in this comparison unless both are given */
	if (is_vms_pathname(fname, FALSE)) {
		char	*bname = bp->b_fname,
			*s = version_of(bname),
			*t = version_of(fname);

		if (!explicit_version(s)
		 || !explicit_version(t))
			if ((s-bname) == (t-fname))
				return !strncmp(fname, bname, (SIZE_T)(s-bname));
	}
#endif

	return !strcmp(fname, bp->b_fname);
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
        register int    s;
	char fname[NFILEN];

	if (more_named_cmd()) {
		if ((s = mlreply_file("Replace with file: ", (TBUFF **)0,
				FILEC_REREAD, fname)) != TRUE)
			return s;
	}
	else if (!global_g_val(GMDWARNREREAD)
		 || ((s = mlyesno("Reread current buffer")) == TRUE))
		(void)strcpy(fname, curbp->b_fname);
	else
		return FALSE;

#if OPT_LCKFILES
	/* release own lock before read the replaced file */
	if ( global_g_val(GMDUSEFILELOCK) ) {
		if (!b_val(curbp,MDLOCKED) && !b_val(curbp,MDVIEW))
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

TBUFF	*lastfileedited;

void
set_last_file_edited(const char *f)
{
	tb_scopy(&lastfileedited, f);
}

/* ARGSUSED */
int
filefind(int f GCC_UNUSED, int n GCC_UNUSED)
{
	register int	s;
	register BUFFER *bp;

	char fname[NFILEN];
	char *actual;
	BUFFER *firstbp = 0;

	if ((s = mlreply_file("Find file: ", &lastfileedited,
					FILEC_READ|FILEC_EXPAND,
			fname)) == TRUE) {
		while ((actual = filec_expand()) != 0) {
			if ((bp = getfile2bp(actual, !clexec,FALSE)) == 0)
				break;
			bp->b_flag |= BFARGS;	/* treat this as an argument */
			if (firstbp == 0)
				firstbp = bp;
		}
		if (firstbp != 0)
			s = bp2swbuffer(firstbp, FALSE, TRUE);
	}
	return s;
}

/* ARGSUSED */
int
viewfile(int f GCC_UNUSED, int n GCC_UNUSED)	/* visit a file in VIEW mode */
{
	char fname[NFILEN];	/* file user wishes to find */
	register int s;		/* status return */
	char	*actual;
	static	TBUFF	*last;

	if ((s = mlreply_file("View file: ", &last, FILEC_READ|FILEC_EXPAND,
			fname)) == TRUE) {
		while ((actual = filec_expand()) != 0) {
			if ((s = getfile(actual, FALSE)) != TRUE)
				break;
			/* if we succeed, put it in view mode */
			make_local_b_val(curwp->w_bufp,MDVIEW);
			set_b_val(curwp->w_bufp,MDVIEW,TRUE);
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
        register int    s;
	char fname[NFILEN];
	static	TBUFF	*last;

	if (!calledbefore) {
	        if ((s= mlreply_file("Insert file: ", &last,
				FILEC_READ|FILEC_PROMPT, fname)) != TRUE)
	                return s;
	}
	if (ukb == 0)
	        return ifile(fname, TRUE, (FILE *)0);
	else
	        return kifile(fname);
}

BUFFER *
getfile2bp(
const char *fname,		/* file name to find */
int ok_to_ask,
int cmdline)
{
	register BUFFER *bp = 0;
        register int    s;
	char bname[NBUFN];	/* buffer name to put file */
	char nfname[NFILEN];	/* canonical form of 'fname' */

	/* user may have renamed buffer to look like filename */
	if (cmdline
	 || (bp = find_b_name(fname)) == NULL
	 || (strlen(fname) > (SIZE_T)NBUFN-1)) {

		/* It's not already here by that buffer name.
		 * Try to find it assuming we're given the file name.
		 */
		(void)lengthen_path(strcpy(nfname, fname));
		if (is_internalname(nfname)) {
			mlforce("[Buffer not found]");
			return 0;
		}
	        for_each_buffer(bp) {
			/* is it here by that filename? */
	                if (same_fname(nfname, bp, FALSE)) {
				return bp;
	                }
	        }
		/* it's not here */
	        makename(bname, nfname);            /* New buffer name.     */
		/* make sure the buffer name doesn't exist */
		while ((bp = find_b_name(bname)) != NULL) {
			if ( !b_is_changed(bp) && is_empty_buf(bp) &&
			    		!ffexists(bp->b_fname)) {
				/* empty and unmodified -- then it's okay
					to re-use this buffer */
				bp->b_active = FALSE;
				ch_fname(bp, nfname);
				return bp;
			}
			/* old buffer name conflict code */
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
		if (bp==NULL && (bp=bfind(bname, 0))==NULL) {
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
 * user readhook couldn't use that info successfully.
 * if this change appears successful, the bp2readin routine (4 lines) can
 * be folded into swbuffer_lfl(), which is the only caller.  --pgf */
		if (!(bp->b_active))
			s = bp2readin(bp, lockfl);
#endif
		if (s == TRUE)
			s = swbuffer_lfl(bp, lockfl);
		if (s == TRUE)
			curwp->w_flag |= WFMODE|WFHARD;
	}

	return s;
}

int
getfile(
char *fname,		/* file name to find */
int lockfl)		/* check the file for locks? */
{
        register BUFFER *bp;

	/* if there are no path delimiters in the name, then the user
		is likely asking for an existing buffer -- try for that
		first */
        if ((strlen(fname) > (SIZE_T)NBUFN-1)  /* too big to be a bname */
	 || maybe_pathname(fname)  /* looks a lot like a filename */
	 || (bp = find_b_name(fname)) == NULL) {
		/* oh well.  canonicalize the name, and try again */
		bp = getfile2bp(fname,!clexec,FALSE);
		if (!bp)
			return FALSE;
	}
	return bp2swbuffer(bp, isShellOrPipe(bp->b_fname), lockfl);
}

/*
 * Scan a buffer to see if it contains more lines terminated by CR-LF than by
 * LF alone.  If so, set the DOS-mode to true, otherwise false.
 */
#if OPT_DOSFILES
/*
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
	set_b_val(bp, MDDOS, result);
}

static void
guess_dosmode(BUFFER *bp)
{
	int	doslines = 0,
		unixlines = 0;
	register LINE *lp;

	make_local_b_val(bp, MDDOS);	/* keep it local, if not */
	/* first count 'em */
	for_each_line(lp,bp) {
		if (llength(lp) > 0 &&
				lgetc(lp, llength(lp)-1) == '\r') {
			doslines++;
		} else {
			unixlines++;
		}
	}
	apply_dosmode(bp, doslines, unixlines);
	if (b_val(bp, MDDOS)) {
		/* then eliminate 'em if necessary */
		for_each_line(lp,bp) {
			if (llength(lp) > 0 &&
					lgetc(lp, llength(lp)-1) == '\r') {
				llength(lp)--;
			}
		}
	}
	bp->b_bytecount -= doslines;
	TRACE(("guess_dosmode %d\n", b_val(bp, MDDOS)))
}

/*
 * Forces the current buffer to be in DOS-mode, stripping any trailing CR's.
 * ( any argument forces non-DOS mode, trailing CR's still stripped )
 */
/*ARGSUSED*/
int
set_dosmode(int f, int n GCC_UNUSED)
{
	make_local_b_val(curbp, MDDOS);

	guess_dosmode(curbp);

	/* force dos mode on the buffer, based on the user argument */
	set_b_val(curbp, MDDOS, !f);

	curwp->w_flag |= WFMODE;
	return TRUE;
}

/* as above, but forces unix-mode instead */
/*ARGSUSED*/
int
set_unixmode(int f, int n)
{
	return set_dosmode(!f, n);
}
#endif

#if OPT_LCKFILES
static void
grab_lck_file(BUFFER *bp, char *fname)
{
	/* Write the lock */
	if (global_g_val(GMDUSEFILELOCK)	&&
		! isShellOrPipe(fname)		&&
		! b_val(bp,MDVIEW) )
	{
		char	locker[100];

		if ( ! set_lock(fname,locker,sizeof(locker)) ) {
			/* we didn't get it */
			make_local_b_val(bp,MDVIEW);
			set_b_val(bp,MDVIEW,TRUE);
			make_local_b_val(bp,MDLOCKED);
			set_b_val(bp,MDLOCKED,TRUE);
			make_local_b_val(bp,VAL_LOCKER);
			set_b_val_ptr(bp,VAL_LOCKER, strmalloc(locker));
			markWFMODE(bp);
		}
	}
}
#endif

/*
 *	Read file "fname" into a buffer, blowing away any text
 *	found there.  Returns the final status of the read.
 */

/* ARGSUSED */
int
readin(
char    *fname,		/* name of file to read */
int	lockfl,		/* check for file locks? */
register BUFFER *bp,	/* read into this buffer */
int	mflg)		/* print messages? */
{
        register WINDOW *wp;
	register int    s;
        int    nline;

	if (bp == 0)				/* doesn't hurt to check */
		return FALSE;

	if (*fname == EOS) {
		mlwrite("BUG: readin called with NULL fname");
		return FALSE;
	}

#if	OPT_ENCRYPT
	if ((s = resetkey(bp, fname)) != TRUE)
		return s;
#endif

        if ((s=bclear(bp)) != TRUE)             /* Might be old.        */
                return s;

#if	OPT_ENCRYPT
	/* bclear() gets rid of local flags */
	if (bp->b_key[0] != EOS) {
		make_local_b_val(bp, MDCRYPT);
		set_b_val(bp, MDCRYPT, TRUE);
	}
#endif

	b_clr_flags(bp, BFINVS|BFCHG);
	ch_fname(bp,fname);
	fname = bp->b_fname;		/* this may have been b_fname! */
#if OPT_DOSFILES
	make_local_b_val(bp, MDDOS);
	/* assume that if our OS wants it, that the buffer will have CRLF
	 * lines.  this may change when the file is read, based on actual
	 * line counts, below.  otherwise, if there's an error, or the
	 * file doesn't exist, we will keep this default.
	 */
	set_b_val(bp, MDDOS, CRLF_LINES);
#endif
	make_local_b_val(bp,MDNEWLINE);
	set_b_val(bp, MDNEWLINE, TRUE);		/* assume we've got it */

        if ((s = ffropen(fname)) == FIOERR) {	/* Hard file error.      */
			/* do nothing -- error has been reported,
				and it will appear as empty buffer */
		/*EMPTY*/;
        } else if (s == FIOFNF) {		/* File not found.      */
                if (mflg)
			mlwrite("[New file]");
        } else {

        	if (mflg)
			mlforce("[Reading %s ]", fname);
#if SYS_VMS
		if (!isInternalName(bp->b_fname))
			fname = resolve_filename(bp);
#endif
		/* read the file in */
        	nline = 0;
#if OPT_WORKING
		max_working = cur_working = old_working = 0;
#endif
#if ! (SYS_MSDOS||SYS_WIN31)
		if (fileispipe || (s = quickreadf(bp, &nline)) == FIOMEM)
#endif
			s = slowreadf(bp, &nline);
#if OPT_WORKING
		cur_working = 0;
#endif
		if (s == FIOERR) {
			/*EMPTY*/;
		} else {

			if (s == FIOFUN)	/* last line is incomplete */
				set_b_val(bp, MDNEWLINE, FALSE);
			b_clr_changed(bp);
#if OPT_FINDERR
			if (fileispipe == TRUE)
				set_febuff(bp->b_bname);
#endif
        		(void)ffclose();	/* Ignore errors.       */
			if (mflg)
				readlinesmsg(nline, s, fname, ffronly(fname));

			/* set view mode for read-only files */
			if ((global_g_val(GMDRONLYVIEW) && ffronly(fname) )) {
				make_local_b_val(bp, MDVIEW);
				set_b_val(bp, MDVIEW, TRUE);
			}
			/* set read-only mode for read-only files */
			if (isShellOrPipe(fname) ||
				(global_g_val(GMDRONLYRONLY) &&
						ffronly(fname) )) {
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
	setm_by_suffix(bp);
	setm_by_preamble(bp);

	for_each_window(wp) {
		if (wp->w_bufp == bp) {
			wp->w_line.l = lforw(buf_head(bp));
			wp->w_dot.l  = lforw(buf_head(bp));
			wp->w_dot.o  = 0;
#if WINMARK
			wp->w_mark = nullmark;
#endif
			wp->w_lastdot = nullmark;
			wp->w_flag |= WFMODE|WFHARD;
		}
	}
	imply_alt(fname, FALSE, lockfl);
	updatelistbuffers();

#if OPT_LCKFILES
	if (lockfl && s != FIOERR)
		grab_lck_file(bp,fname);
#endif
#if OPT_PROCEDURES
	if (s <= FIOEOF) {
	    static int readhooking;
	    if (!readhooking && *readhook && !b_is_temporary(bp)) {
		    readhooking = TRUE;
		    run_procedure(readhook);
		    readhooking = FALSE;
	    }
	}
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
	bp->b_dot.l  = lforw(buf_head(bp));
	bp->b_dot.o  = 0;
	bp->b_active = TRUE;
	return s;
}

#if ! (SYS_MSDOS || SYS_WIN31)
static int
quickreadf(register BUFFER *bp, int *nlinep)
{
        register UCHAR *textp;
        UCHAR *countp;
	L_NUM nlines;
        int incomplete = FALSE;
	B_COUNT len, nbytes;

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
	bp->b_ltext = castalloc(UCHAR, (ALLOC_T)(len + 2));
	if (bp->b_ltext == NULL)
		return FIOMEM;

	if ((len = ffread((char *)&bp->b_ltext[1], len)) < 0) {
		FreeAndNull(bp->b_ltext);
		mlerror("reading");
		return FIOERR;
	}

#if OPT_ENCRYPT
	if (b_val(bp, MDCRYPT)
	 && bp->b_key[0]) {	/* decrypt the file */
	 	char	temp[NPAT];
		(void)strcpy(temp, bp->b_key);
		ue_crypt((char *)0, 0);
		ue_crypt(temp, strlen(temp));
		ue_crypt((char *)&bp->b_ltext[1], (ALLOC_T)len);
	}
#endif

	/* loop through the buffer, replacing all newlines with the
		length of the _following_ line */
	bp->b_ltext_end = bp->b_ltext + len + 1;
	countp = bp->b_ltext;
	textp = countp + 1;
	nbytes = len;
        nlines = 0;

	if (textp[len-1] != '\n') {
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
				len = (B_COUNT)(countp - bp->b_ltext);
				incomplete = TRUE;
				/* we'll re-read the rest later */
				if (len)  {
					ffseek(len);
					np = castrealloc(UCHAR, bp->b_ltext, (ALLOC_T)len);
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
		bp->b_LINEs = typeallocn(LINE,nlines);
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
				lp->l_text = (char *)textp + 1;
				set_lforw(lp, lp + 1);
				if (lp != bp->b_LINEs)
					set_lback(lp, lp - 1);
				lsetclear(lp);
				lp->l_nxtundo = null_ptr;
				lp++;
				textp += *textp + 1;
			}
			/*
			if (textp != bp->b_ltext_end - 1)
				mlwrite("BUG: textp not equal to end %d %d",
					textp,bp->b_ltext_end);
			*/
			lp--;  /* point at last line again */

			/* connect the end of the list */
			set_lforw(lp, buf_head(bp));
			set_lback(buf_head(bp), lp);

			/* connect the front of the list */
			set_lback(bp->b_LINEs, buf_head(bp));
			set_lforw(buf_head(bp), bp->b_LINEs);
		}
	}

	*nlinep = nlines;

	if (incomplete)
		return FIOMEM;
#if OPT_DOSFILES
	if (global_b_val(MDDOS))
		guess_dosmode(bp);
#endif
	return b_val(bp, MDNEWLINE) ? FIOSUC : FIOFUN;
}

#endif /* ! SYS_MSDOS */

int
slowreadf(register BUFFER *bp, int *nlinep)
{
	int s;
	int len;
#if OPT_DOSFILES
	int	doslines = 0,
		unixlines = 0;
#endif
#if SYS_UNIX || SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT	/* i.e., we can read from a pipe */
	USHORT	flag = 0;
	int	done_update = FALSE;
#endif
#if SYS_UNIX && OPT_SHELL
	time_t	last_updated = time((time_t *)0);
#endif
	b_set_counted(bp);	/* make 'addline()' do the counting */
#if OPT_DOSFILES
	/*
	 * There might be some pre-existing lines if quickreadf
	 * read part of the file and then left the rest up to us.
	 */
	make_local_b_val(bp, MDDOS);	/* keep it local, if not */
	if (global_b_val(MDDOS)) {
		register LINE   *lp;
		for_each_line(lp,bp) {
			if (llength(lp) > 0 &&
				  lgetc(lp, llength(lp)-1) == '\r') {
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
			if (len > 0 && fline[len-1] == '\r') {
				doslines++;
			} else {
				unixlines++;
			}
		}
#endif
		if (addline(bp,fline,len) != TRUE) {
                        s = FIOMEM;             /* Keep message on the  */
                        break;                  /* display.             */
                }
#if SYS_UNIX || SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT
		else {
                	/* reading from a pipe, and internal? */
			if (slowtime(&last_updated)) {
				register WINDOW *wp;

				flag |= (WFEDIT|WFFORCE);

				if (!done_update || bp->b_nwnd > 1)
					flag |= WFHARD;
				for_each_window(wp) {
			                if (wp->w_bufp == bp) {
			                        wp->w_line.l=
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
				curwp->w_flag |= WFMODE|WFKILLS;
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
		if (s == FIOFUN) {
			set_b_val(bp, MDNEWLINE, FALSE);
			break;
		}
        }
#if OPT_DOSFILES
	if (global_b_val(MDDOS)) {
		apply_dosmode(bp, doslines, unixlines);
		if (b_val(bp, MDDOS)) {  /* if it _is_ a dos file, strip 'em */
        		register LINE   *lp;
			for_each_line(lp,bp) {
				if (llength(lp) > 0 &&
					  lgetc(lp, llength(lp)-1) == '\r') {
					llength(lp)--;
				}
			}
			bp->b_bytecount -= doslines;
		}
	}
#endif
	return s;
}

/* utility routine for no. of lines read */
static void
readlinesmsg(int n, int s, const char *f, int rdo)
{
	char fname[NFILEN];
	const char *m;
	char *short_f = shorten_path(strcpy(fname,f),TRUE);
	switch(s) {
		case FIOFUN:	m = "INCOMPLETE LINE, ";break;
		case FIOERR:	m = "I/O ERROR, ";	break;
		case FIOMEM:	m = "OUT OF MEMORY, ";	break;
		case FIOABRT:	m = "ABORTED, ";	break;
		default:	m = "";			break;
	}
	if (!global_b_val(MDTERSE))
		mlwrite("[%sRead %d line%s from \"%s\"%s]", m,
			n, PLURAL(n), short_f, rdo ? "  (read-only)":"" );
	else
		mlforce("[%s%d lines]",m,n);
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
	char	temp[NFILEN];

	fcp = skip_string(strcpy(temp, fname));
#if OPT_VMS_PATH
	if (is_vms_pathname(temp, TRUE)) {
		(void)strcpy(bname, "NoName");
		return;
	}
	if (is_vms_pathname(temp, FALSE)) {
		for (;
			fcp > temp && !strchr(":]", fcp[-1]);
				fcp--)
				;
		(void)strncpy0(bname, fcp, NBUFN);
		strip_version (bname);
		(void)mklower(bname);
		return;
	}
#endif
	/* trim trailing whitespace */
	while (fcp != temp && (isBlank(fcp[-1])
#if SYS_UNIX || OPT_MSDOS_PATH	/* trim trailing slashes as well */
					 || is_slashc(fcp[-1])
#endif
							) )
                *(--fcp) = EOS;
	fcp = temp;
	/* trim leading whitespace */
	while (isBlank(*fcp))
		fcp++;

#if     SYS_UNIX || SYS_MSDOS || SYS_WIN31 || SYS_VMS || SYS_OS2 || SYS_WINNT
	bcp = bname;
	if (isShellOrPipe(fcp)) {
		/* ...it's a shell command; bname is first word */
		*bcp++ = SCRTCH_LEFT[0];
		*bcp++ = SHPIPE_LEFT[0];
		do {
			++fcp;
		} while (isSpace(*fcp));
		(void)strncpy0(bcp, fcp, (SIZE_T)(NBUFN - (bcp - bname)));
		for (j = 4; (j < NBUFN) && isPrint(*bcp); j++) {
			bcp++;
		}
		(void) strcpy(bcp, SCRTCH_RIGHT);
		return;
	}

	(void)strncpy0(bcp, pathleaf(fcp), (SIZE_T)(NBUFN - (bcp - bname)));

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

#else	/* !(SYS_UNIX||SYS_VMS||SYS_MSDOS) */

	bcp = skip_string(fcp);
	{
		register char *cp2 = bname;
		strcpy0(bname, bcp, NBUFN);
		cp2 = strchr(bname, ':');
		if (cp2) *cp2 = EOS;
	}
#endif
}


void
unqname(	/* make sure a buffer name is unique */
char *name)	/* name to check on */
{
	register SIZE_T	j;
	char newname[NBUFN * 2];
	char suffixbuf[NBUFN];
	int suffixlen;
	int adjust;
	int i = 0;
	SIZE_T k;

	j = strlen(name);
	if (j == 0)
		j = strlen(strcpy(name, "NoName"));

	/* check to see if it is in the buffer list */
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
		suffixlen = (int)(lsprintf(suffixbuf,"-%r", 36, ++i) - suffixbuf);
		k = NBUFN - 1 - suffixlen;
		if (j < k)
			k = j;
		if (adjust) {
			strcpy(&newname[k-1], suffixbuf);
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
        register int    s;
        char            fname[NFILEN];
	int 		forced = (f && n == SPECIAL_BANG_ARG);

	if (more_named_cmd()) {
	        if ((s= mlreply_file("Write to file: ", (TBUFF **)0,
				FILEC_WRITE, fname)) != TRUE)
	                return s;
        } else
		(void) strcpy(fname, curbp->b_fname);

        if ((s=writeout(fname,curbp,forced,TRUE)) == TRUE)
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
        register int    s;
	int forced = (f && n == SPECIAL_BANG_ARG); /* then it was :w! */

        if (curbp->b_fname[0] == EOS) {		/* Must have a name.    */
                mlwarn("[No file name]");
                return FALSE;
        }

        if ((s=writeout(curbp->b_fname,curbp,forced,TRUE)) == TRUE)
		unchg_buff(curbp, 0);
        return s;
}

static void
setup_file_region(BUFFER *bp, REGION *rp)
{
	(void)bsizes(bp);	/* make sure we have current count */
	/* starting at the beginning of the buffer */
	rp->r_orig.l = lforw(buf_head(bp));
        rp->r_orig.o = 0;
        rp->r_size   = bp->b_bytecount;
        rp->r_end    = bp->b_line;
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
		(void)strcpy(fname, curbp->b_fname);
	} else {
	        if ((status = mlreply_file("Write region to file: ",
			(TBUFF **)0, FILEC_WRITE|FILEC_PROMPT, fname)) != TRUE)
	                return status;
        }
        if ((status=getregion(&region)) == TRUE)
		status = writereg(&region, fname, TRUE, curbp, FALSE);
	return status;
}


static int
writereg(
REGION	*rp,
const char  *given_fn,
int 	msgf,
BUFFER	*bp,
int	forced)
{
        register int    s;
        register LINE   *lp;
        register int    nline;
	register int i;
	char	fname[NFILEN], *fn;
	B_COUNT	nchar;
	const char * ending =
#if OPT_DOSFILES
			b_val(bp, MDDOS) ? "\r\n" : "\n"
#else
			"\n"
#endif	/* OPT_DOSFILES */
		;
	C_NUM	offset = rp->r_orig.o;

	/* this is adequate as long as we cannot write parts of lines */
	int	whole_file = (rp->r_orig.l == lforw(buf_head(bp)))
			  && (rp->r_end.l == buf_head(bp));

	if (is_internalname(given_fn)) {
		mlwarn("[No filename]");
		return FALSE;
	}

	if (!forced && b_val(bp,MDREADONLY)) {
		mlwarn("[Buffer mode is \"readonly\"]");
		return FALSE;
	}

	if (isShellOrPipe(given_fn)
	 && bp->b_fname != 0
	 && !strcmp(given_fn, bp->b_fname)
	 && mlyesno("Are you sure (this was a pipe-read)") != TRUE) {
		mlwrite("File not written");
		return FALSE;
	}

#if OPT_PROCEDURES
	{
	    static int writehooking;

	    if (!writehooking && *writehook) {
		    writehooking = TRUE;
		    run_procedure(writehook);
		    writehooking = FALSE;

		    /*
		     * The write-hook may have modified the buffer.  Assume
		     * the worst, and reconstruct the region.
		     */
		    (void)bsizes(bp);
		    if (whole_file
		     || line_no(bp, rp->r_orig.l) > bp->b_linecount
		     || line_no(bp, rp->r_end.l)  > bp->b_linecount) {
			setup_file_region(bp, rp);
		    } else {
			DOT = rp->r_orig;
			MK  = rp->r_end;
			(void)getregion(rp);
		    }
		    offset = rp->r_orig.o;
	    }
	}
#endif

	fn = lengthen_path(strcpy(fname, given_fn));
	if (same_fname(fn, bp, FALSE) && b_val(bp,MDVIEW)) {
		mlwarn("[Can't write-back from view mode]");
		return FALSE;
	}

#if	OPT_ENCRYPT
	if ((s = resetkey(curbp, fn)) != TRUE)
		return s;
#endif

#ifdef MDCHK_MODTIME
	if ( ! inquire_modtime( bp, fn ) )
		return FALSE;
#endif
        if ((s=ffwopen(fn,forced)) != FIOSUC)       /* Open writes message. */
                return FALSE;

	/* tell us we're writing */
	if (msgf == TRUE)
		mlwrite("[Writing...]");

	if (isShellOrPipe(given_fn)) {
		beginDisplay();
		CleanToPipe();
	}

        lp = rp->r_orig.l;
        nline = 0;                              /* Number of lines     */
        nchar = 0;                              /* Number of chars     */

	/* first (maybe partial) line and succeeding whole lines */
        while ((rp->r_size+offset) >= llength(lp)+1) {
		register C_NUM	len = llength(lp) - offset;
		register char	*text = lp->l_text + offset;

		/* If this is the last line (and no fragment will be written
		 * after the line), allow 'newline' mode to suppress the
		 * trailing newline.
		 */
		if ((rp->r_size -= (len + 1)) <= 0
		 && !b_val(bp,MDNEWLINE))
			ending = "";
                if ((s = ffputline(text, len, ending)) != FIOSUC)
			goto out;

                ++nline;
		nchar += len + 1;
		offset = 0;
                lp = lforw(lp);
        }

	/* last line (fragment) */
	if (rp->r_size > 0) {
		for (i = 0; i < rp->r_size; i++)
		        if ((s = ffputc(lgetc(lp,i))) != FIOSUC)
		                goto out;
		nchar += rp->r_size;
		++nline;	/* it _looks_ like a line */
	}

 out:
        if (s == FIOSUC) {                      /* No write error.      */
#if SYS_VMS
		if (same_fname(fn, bp, FALSE))
			fn = resolve_filename(bp);
#endif
                s = ffclose();
                if (s == FIOSUC && msgf) {      /* No close error.      */
			if (!global_b_val(MDTERSE)) {
				char *aname;
				const char *action;
				if ((aname = is_appendname(fn)) != 0) {
					fn = aname;
					action = "Appended";
				} else {
					action = "Wrote";
				}
				mlforce("[%s %d line%s %ld char%s to \"%s\"]",
					action, nline, PLURAL(nline),
					nchar, PLURAL(nchar), fn);
			} else {
				mlforce("[%d lines]", nline);
			}
                }
        } else {                                /* Ignore close error   */
                (void)ffclose();                /* if a write error.    */
	}
	if (whole_file) {
		bp->b_linecount = 
			bp->b_lines_on_disk = nline;
	}

	if (isShellOrPipe(given_fn)) {
		CleanAfterPipe(TRUE);
		endofDisplay();
	}

        if (s != FIOSUC)                        /* Some sort of error.  */
                return FALSE;

#ifdef MDCHK_MODTIME
	set_modtime(bp, fn);
#endif
	/*
	 * If we've written the unnamed-buffer, rename it according to the file.
	 * FIXME: maybe we should do this to all internal-names?
	 */
	if (whole_file
	 && eql_bname(bp, UNNAMED_BufName)
	 && find_b_file(fname) == 0) {
	  	ch_fname(bp, fname);
		set_buffer_name(bp);
	}

	imply_alt(fn, whole_file, FALSE);
        return TRUE;
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
	register KILL *kp;		/* pointer into kill register */
	register int	nline;
	register int	s;
	register int	c;
	register int	i;
	register char	*sp;	/* pointer into string to insert */

	/* make sure there is something to put */
	if (kbs[ukb].kbufh == NULL) {
		if (msgf) mlforce("Nothing to write");
		return FALSE;		/* not an error, just nothing */
	}

#if	OPT_ENCRYPT
	if ((s = resetkey(curbp, fn)) != TRUE)
		return s;
#endif
	if ((s=ffwopen(fn,FALSE)) != FIOSUC) {	/* Open writes message. */
		return FALSE;
	}
	/* tell us we're writing */
	if (msgf == TRUE)
		mlwrite("[Writing...]");
	nline = 0;				/* Number of lines.	*/

	kp = kbs[ukb].kbufh;
	while (kp != NULL) {
		i = KbSize(ukb,kp);
		sp = (char *)kp->d_chunk;
		while (i--) {
			if ((c = *sp++) == '\n')
				nline++;
			if ((s = ffputc(c)) != FIOSUC)
				break;
		}
		kp = kp->d_next;
	}
	if (s == FIOSUC) {			/* No write error.	*/
		s = ffclose();
		if (s == FIOSUC && msgf) {	/* No close error.	*/
			if (!global_b_val(MDTERSE))
				mlwrite("[Wrote %d line%s to %s ]",
					nline, PLURAL(nline), fn);
			else
				mlforce("[%d lines]", nline);
		}
	} else	{				/* Ignore close error	*/
		(void)ffclose();		/* if a write error.	*/
	}
	if (s != FIOSUC)			/* Some sort of error.	*/
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
filename(int f GCC_UNUSED, int n GCC_UNUSED)
{
        register int    s;
        char            fname[NFILEN];

	if (end_named_cmd()) {
		return showcpos(FALSE,1);
	}

        if ((s = mlreply_file("Name: ", (TBUFF **)0, FILEC_UNKNOWN, fname))
						== ABORT)
                return s;
        if (s == FALSE)
                return s;
	make_global_b_val(curbp,MDVIEW); /* no longer read only mode */
#if OPT_LCKFILES
	if ( global_g_val(GMDUSEFILELOCK) ) {
		if (!b_val(curbp,MDLOCKED) && !b_val(curbp,MDVIEW))
			release_lock(curbp->b_fname);
		ch_fname(curbp, fname);
		make_global_b_val(curbp,MDLOCKED);
		make_global_b_val(curbp,VAL_LOCKER);
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
ifile(char *fname, int belowthisline, FILE *haveffp)
{
	register LINEPTR prevp;
	register LINEPTR newlp;
	register LINEPTR nextp;
        register BUFFER *bp;
        register int    s;
        int    nbytes;
        register int    nline;

        bp = curbp;                             /* Cheap.               */
	b_clr_flags(bp, BFINVS);		/* we are not temporary*/
	if (!haveffp) {
	        if ((s=ffropen(fname)) == FIOERR) /* Hard file open.      */
	                goto out;
	        if (s == FIOFNF)		/* File not found.      */
			return no_such_file(fname);
#if	OPT_ENCRYPT
		if ((s = resetkey(curbp, fname)) != TRUE)
			return s;
#endif
	        mlwrite("[Inserting...]");
		CleanToPipe();

	} else { /* we already have the file pointer */
		ffp = haveffp;
	}
	prevp = DOT.l;
	DOT.o = 0;
	MK = DOT;

	nline = 0;
	nextp = null_ptr;
	while ((s=ffgetline(&nbytes)) <= FIOSUC) {
#if OPT_DOSFILES
		if (b_val(curbp,MDDOS)
		 && (nbytes > 0)
		 && fline[nbytes-1] == '\r')
			nbytes--;
#endif
		if (!belowthisline) {
			nextp = prevp;
			prevp = lback(prevp);
		}

		if (add_line_at(curbp, prevp, fline, nbytes) != TRUE) {
			s = FIOMEM;		/* Keep message on the	*/
			break;			/* display.		*/
		}
		newlp = lforw(prevp);
		tag_for_undo(newlp);
		prevp = belowthisline ? newlp : nextp;
		++nline;
		if (s < FIOSUC)
			break;
	}
	if (!haveffp) {
		CleanAfterPipe(FALSE);
		(void)ffclose();		/* Ignore errors.	*/
		readlinesmsg(nline,s,fname,FALSE);
	}
out:
	/* advance to the next line and mark the window for changes */
	DOT.l = lforw(DOT.l);

	/* copy window parameters back to the buffer structure */
	copy_traits(&(curbp->b_wtraits), &(curwp->w_traits));

	imply_alt(fname, FALSE, FALSE);
	chg_buff (curbp, WFHARD);

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
        register int    i;
        register int    s;
        register int    nline;
        int    nbytes;

	ksetup();
        if ((s=ffropen(fname)) == FIOERR)       /* Hard file open.      */
                goto out;
        if (s == FIOFNF)			/* File not found.      */
		return no_such_file(fname);

        nline = 0;
#if	OPT_ENCRYPT
	if ((s = resetkey(curbp, fname)) == TRUE)
#endif
	{
        	mlwrite("[Reading...]");
		CleanToPipe();
		while ((s=ffgetline(&nbytes)) <= FIOSUC) {
			for (i=0; i<nbytes; ++i)
				if (!kinsert(fline[i]))
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
        (void)ffclose();                        /* Ignore errors.       */
	readlinesmsg(nline,s,fname,FALSE);

out:
	return (s != FIOERR);
}

#if SYS_UNIX
static const char *mailcmds[] = {
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
#if SYS_UNIX
#if HAVE_MKDIR
	static char dirnam[NSTRING] = "/tmp/vileDXXXXXX";
#else
	static char dirnam[NSTRING] = "/tmp";
	char temp[NFILEN];
#endif
#else
#if HAVE_MKDIR && !SYS_MSDOS && !SYS_OS2
	static char dirnam[NSTRING] = "vileDXXXXXX";
#else
	static char dirnam[NSTRING] = "";
	char temp[NFILEN];
#endif
#endif
	char filnam[NFILEN];
	BUFFER *bp;
#if SYS_UNIX
	char cmd[200];
	char *np;
#endif
	static int wrote = 0;
#if HAVE_MKDIR && !SYS_MSDOS && !SYS_OS2
	static int created = FALSE;
#endif

#if SYS_APOLLO
	extern	char	*getlogin(void);
	static	int	i_am_dead;
#endif	/* SYS_APOLLO */

#if OPT_WORKING && defined(SIGALRM)
	setup_handler(SIGALRM, SIG_IGN);
#endif

#if SYS_APOLLO
	if (i_am_dead++)	/* prevent recursive faults */
		_exit(signo);
	(void)lsprintf(cmd,
		"(echo signal %d killed vile;/com/tb %d)| /bin/mail %s",
		signo, getpid(), getlogin());
	(void)system(cmd);
#endif	/* SYS_APOLLO */

	/* write all modified buffers to the temp directory */
	set_global_g_val(GMDIMPLYBUFF,FALSE);	/* avoid side-effects! */
	for_each_buffer(bp) {
		if (!b_is_temporary(bp) &&
			bp->b_active == TRUE &&
			b_is_changed(bp)) {
#if HAVE_MKDIR && !SYS_MSDOS && !SYS_OS2
			if (!created) {
				(void)mktemp(dirnam);
				if(mkdir(dirnam,0700) != 0) {
					tidy_exit(BADEXIT);
				}
				created = TRUE;
			}
			(void)pathcat(filnam, dirnam, bp->b_bname);
#else
			(void)pathcat(filnam, dirnam,
				strcat(strcpy(temp, "V"), bp->b_bname));
#endif
			set_b_val(bp,MDVIEW,FALSE);
			if (writeout(filnam,bp,TRUE,FALSE) != TRUE) {
				tidy_exit(BADEXIT);
			}
			wrote++;
		}
	}
#if SYS_UNIX
	if (wrote) {
		const char **mailcmdp;
		struct stat sb;
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
#if HAVE_GETHOSTNAME
			char hostname[128];
			if (gethostname(hostname, sizeof(hostname)) < 0)
				(void)strcpy(hostname, "unknown");
			hostname[sizeof(hostname)-1] = EOS;
#endif
			(void)lsprintf(cmd,
			 "( %s%s; %s; %s; %s%d; %s%s; %s%s; %s%s%s ) | %s %s",
			"echo To: ", np,
			"echo Subject: vile died, files saved",
			"echo ",
			"echo vile died due to signal ", signo,
#if HAVE_GETHOSTNAME
			"echo on host ", hostname,
#else
			"echo ", "",
#endif
			"echo the following files were saved in directory ",
			dirnam,
#if HAVE_MKDIR
			/* reverse sort so '.' comes last, in case it
			 * terminates the mail message early */
			"ls -a ", dirnam, " | sort -r",
#else
			"ls ", dirnam, "/V*",
#endif
			*mailcmdp, np);
			(void)system(cmd);
		}
	}
	if (signo > 2) {
		ttclean(FALSE);
		abort();
	}
#else
	if (wrote) {
		fprintf(stderr, "vile died: Files saved in directory %s\n",
                	dirnam);
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
        for_each_window(wp) {
                if (wp->w_bufp == bp)
                        wp->w_flag |= WFMODE;
        }
}

#if	OPT_ENCRYPT
int
resetkey(		/* reset the encryption key if needed */
BUFFER	*bp,
const char *fname)
{
	register int s;	/* return status */

	/* turn off the encryption flag */
	cryptflag = FALSE;

	/* if we are in crypt mode */
	if (b_val(bp, MDCRYPT)) {
		char	temp[NFILEN];

		/* don't automatically inherit key from other buffers */
		if (bp->b_key[0] != EOS
		 && strcmp(lengthen_path(strcpy(temp, fname)), bp->b_fname)) {
			char	prompt[80];
			(void)lsprintf(prompt,
				"Use crypt-key from %s", bp->b_bname);
			s = mlyesno(prompt);
			if (s != TRUE)
				return (s == FALSE);
		}

		/* make a key if we don't have one */
		if (bp->b_key[0] == EOS) {
			s = ue_makekey(bp->b_key, sizeof(bp->b_key));
			if (s != TRUE)
				return (s == FALSE);
		}

		/* let others know... */
		cryptflag = TRUE;

		/* and set up the key to be used! */
		/* de-encrypt it */
		ue_crypt((char *)0, 0);
		ue_crypt(bp->b_key, strlen(bp->b_key));

		/* re-encrypt it...seeding it to start */
		ue_crypt((char *)0, 0);
		ue_crypt(bp->b_key, strlen(bp->b_key));
	}

	return TRUE;
}
#endif
