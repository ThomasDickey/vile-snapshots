/*
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/fileio.c,v 1.137 1999/04/04 21:11:02 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

#if SYS_VMS
#include	<file.h>
#endif

#if HAVE_SYS_IOCTL_H
#include	<sys/ioctl.h>
#endif

#if CC_NEWDOSCC
#include	<io.h>
#endif


#ifndef EISDIR
#define EISDIR EACCES
#endif

/*--------------------------------------------------------------------------*/

static void
free_fline(void)
{
	FreeAndNull(fflinebuf);
	fflinelen = 0;
}

#if OPT_FILEBACK
/*
 * Copy file when making a backup
 */
static int
copy_file (const char *src, const char *dst)
{
	FILE	*ifp;
	FILE	*ofp;
	int	chr;
	int	ok = FALSE;

	if ((ifp = fopen(src, FOPEN_READ)) != 0) {
		if ((ofp = fopen(dst, FOPEN_WRITE)) != 0) {
			ok = TRUE;
			for_ever {
				chr = fgetc(ifp);
				if (feof(ifp))
					break;
				fputc(chr, ofp);
				if (ferror(ifp) || ferror(ofp)) {
					ok = FALSE;
					break;
				}
			}
			(void)fclose(ofp);
		}
		(void)fclose(ifp);
	}
	return ok;
}

/*
 * Before overwriting a file, rename any existing version as a backup.
 * Copy-back to retain the modification-date of the original file.
 *
 * Note: for UNIX, the direction of file-copy should be reversed, if the
 *       original file happens to be a symbolic link.
 */

#if SYS_UNIX
# if ! HAVE_LONG_FILE_NAMES
#  define MAX_FN_LEN 14
# else
#  define MAX_FN_LEN 255
# endif
#endif

static int
write_backup_file(const char *orig, char *backup)
{
	int s;

	struct stat ostat, bstat;

	if (stat(SL_TO_BSL(orig), &ostat) != 0)
		return FALSE;

	if (stat(SL_TO_BSL(backup), &bstat) == 0) {  /* the backup file exists */

#if SYS_UNIX
		/* same file, somehow? */
		if (bstat.st_dev == ostat.st_dev &&
		    bstat.st_ino == ostat.st_ino) {
			return FALSE;
		}
#endif

		/* remove it */
		if (unlink(backup) != 0)
			return FALSE;
	}

	/* there are many reasons for copying, rather than renaming
	   and writing a new file -- the file may have links, which
	   will follow the rename, rather than stay with the real-name.
	   additionally, if the write fails, we need to re-rename back to
	   the original name, otherwise two successive failed writes will
	   destroy the file.
	*/

	s = copy_file(orig, backup);
	if (s != TRUE)
		return s;

	/* change date and permissions to match original */
#if HAVE_UTIMES
	/* we favor utimes over utime, since not all implementations (i.e.
		older ones) declare the utimbuf argument.  NeXT, for example,
		declares it as an array of two timevals instead.  we think
		utimes will be more standard, where it exists.  what we
		really think is that it's probably BSD systems that got
		utime wrong, and those will have utimes to cover for it.  :-) */
	{
	    struct timeval buf[2];
	    buf[0].tv_sec = ostat.st_atime;
	    buf[0].tv_usec = 0;
	    buf[1].tv_sec = ostat.st_mtime;
	    buf[1].tv_usec = 0;
	    s = utimes(backup, buf);
	    if (s != 0) {
		    (void)unlink(backup);
		    return FALSE;
	    }
	}
#else
# if HAVE_UTIME
	{
	    struct utimbuf buf;
	    buf.actime = ostat.st_atime;
	    buf.modtime = ostat.st_mtime;
	    s = utime(backup, &buf);
	    if (s != 0) {
		    (void)unlink(backup);
		    return FALSE;
	    }
	}
#endif
#endif

#if SYS_OS2 && CC_CSETPP
	s = chmod(backup, ostat.st_mode & (S_IREAD | S_IWRITE));
#else
	s = chmod(backup, ostat.st_mode & 0777);
#endif
	if (s != 0) {
		(void)unlink(backup);
		return FALSE;
	}

	return TRUE;
}

static int
make_backup (char *fname)
{
	int	ok	= TRUE;

	if (ffexists(fname)) { /* if the file exists, attempt a backup */
		char	tname[NFILEN];
		char	*s = pathleaf(strcpy(tname, fname)),
			*t = strrchr(s, '.');
		char *gvalfileback = global_g_val_ptr(GVAL_BACKUPSTYLE);

		if (strcmp(gvalfileback,".bak") == 0) {
			if (t == 0	/* i.e. no '.' at all */
#if SYS_UNIX
				|| t == s	/* i.e. leading char is '.' */
#endif
			)
				t = skip_string(s); /* then just append */
			(void)strcpy(t, ".bak");
#if SYS_UNIX
		} else if (strcmp(gvalfileback, "tilde") == 0) {
			t = skip_string(s);
#if ! HAVE_LONG_FILENAMES
			if (t - s >= MAX_FN_LEN) {
				if (t - s == MAX_FN_LEN &&
					s[MAX_FN_LEN-2] == '.')
					s[MAX_FN_LEN-2] = s[MAX_FN_LEN-1];
				t = &s[MAX_FN_LEN-1];
			}
#endif
			(void)strcpy(t, "~");
#if SOMEDAY
		} else if (strcmp(gvalfileback, "tilde_N_existing") {
			/* numbered backups if one exists, else simple */
		} else if (strcmp(gvalfileback, "tilde_N") {
			/* numbered backups of all files*/
#endif
#endif /* SYS_UNIX */
		} else {
			mlforce("BUG: bad fileback value");
			return FALSE;
		}

		ok = write_backup_file(fname, tname);

	}
	return ok;
}
#endif /* OPT_FILEBACK */

/*
 * Open a file for reading.
 */
int
ffropen(char *fn)
{
	fileispipe = FALSE;
	fileeof = FALSE;

#if OPT_SHELL
	if (isShellOrPipe(fn)) {
		ffp = 0;
#if SYS_UNIX || SYS_MSDOS || SYS_OS2 || SYS_WINNT
		ffp = npopen(fn+1, FOPEN_READ);
#endif
#if SYS_VMS
		ffp = vms_rpipe(fn+1, 0, (char *)0);
		/* really a temp-file, but we cannot fstat it to get size */
#endif
		if (ffp == 0) {
			mlerror("opening pipe for read");
			return (FIOERR);
		}

		fileispipe = TRUE;
		count_fline = 0;

	} else
#endif
	if (is_directory(fn)) {
		set_errno(EISDIR);
		mlerror("opening directory");
		return (FIOERR);

	} else if ((ffp=fopen(fn, FOPEN_READ)) == NULL) {
		if (errno != ENOENT
#if SYS_OS2 && CC_CSETPP
		 && errno != ENOTEXIST
		 && errno != EBADNAME
#endif
		 && errno != EINVAL) {	/* a problem with Linux to DOS-files */
			mlerror("opening for read");
			return (FIOERR);
		}
		return (FIOFNF);
	}

	return (FIOSUC);
}

/*
 * Open a file for writing. Return TRUE if all is well, and FALSE on error
 * (cannot create).
 */
int
ffwopen(char *fn, int forced)
{
#if SYS_UNIX || SYS_MSDOS || SYS_OS2 || SYS_WINNT
	char	*name;
	char	*mode = FOPEN_WRITE;

#if OPT_SHELL
	if (isShellOrPipe(fn)) {
		if ((ffp=npopen(fn+1, mode)) == NULL) {
			mlerror("opening pipe for write");
			return (FIOERR);
		}
		fileispipe = TRUE;
	} else
#endif
	{
		if ((name = is_appendname(fn)) != NULL) {
			fn = name;
			mode = FOPEN_APPEND;
		}
		if (is_directory(fn)) {
			set_errno(EISDIR);
			mlerror("opening directory");
			return (FIOERR);
		}

#if OPT_FILEBACK
		/* will we be able to write? (before attempting backup) */
		if (ffexists(fn) && ffronly(fn)) {
			mlerror("accessing for write");
			return (FIOERR);
		}

		if (*global_g_val_ptr(GVAL_BACKUPSTYLE) != 'o') { /* "off" ? */
			if (!make_backup(fn)) {
				if (!forced) {
					mlerror("making backup file");
					return (FIOERR);
				}
			}
		}
#endif
		if ((ffp = fopen(fn, mode)) == NULL) {
			mlerror("opening for write");
			return (FIOERR);
		}
		fileispipe = FALSE;
	}
#else
#if     SYS_VMS
	char	temp[NFILEN];
	register int	fd;
	strip_version(fn = strcpy(temp, fn));

	if (is_appendname(fn)
	||  is_directory(fn)
	|| (fd=vms_creat(temp)) < 0
	|| (ffp=fdopen(fd, FOPEN_WRITE)) == NULL) {
		mlforce("[Cannot open file for writing]");
		return (FIOERR);
	}
#else
	if ((ffp=fopen(fn, FOPEN_WRITE)) == NULL) {
		mlerror("opening for write");
		return (FIOERR);
	}
#endif
#endif
	return (FIOSUC);
}

/* wrapper for 'access()' */
int
ffaccess(char *fn, int mode)
{
	int status = FALSE;
#if HAVE_ACCESS
	/* these were chosen to match the SYSV numbers, but we'd rather use
	 * the symbols for portability.
	 */
#ifndef X_OK
#define X_OK 1
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif
	int n = 0;
	if (mode & FL_EXECABLE)  n |= X_OK;
	if (mode & FL_WRITEABLE) n |= W_OK;
	if (mode & FL_READABLE)  n |= R_OK;
	status = (!isInternalName(fn)
	    &&   access(SL_TO_BSL(fn), n) == 0);
#else
	int	fd;
	switch (mode) {
	case FL_EXECABLE:
		/* FALL-THRU */
	case FL_READABLE:
		if (ffropen(fn) == FIOSUC) {
			ffclose();
			status = TRUE;
		} else {
			status = FALSE;
		}
		break;
	case FL_WRITEABLE:
		if ((fd=open(SL_TO_BSL(fn), O_WRONLY|O_APPEND)) < 0) {
			status = FALSE;
		} else {
			(void)close(fd);
			status = TRUE;
		}
		break;
	default:
		status = ffexists(fn);
		break;
	}
#endif
	TRACE(("ffaccess(fn=%s, mode=%d) = %d\n", fn, mode, status))
	return (status);
}

/* is the file read-only?  true or false */
int
ffronly(char *fn)
{
	int status;
	if (isShellOrPipe(fn)) {
		status = TRUE;
	} else {
		status = !ffaccess(fn, FL_WRITEABLE);
	}
	TRACE(("ffronly(fn=%s) = %d\n", fn, status))
	return status;
}

#if	OPT_ENCRYPT
static int ffcrypting = FALSE;

void
ffdocrypt(int crypting)
{
	ffcrypting = crypting;
}
#endif

B_COUNT
ffsize(void)
{
	long result = -1L;

#if SYS_WINNT

	long prev;
	prev = ftell(ffp);
	if (fseek(ffp, 0, 2) >= 0) {
		result = ftell(ffp);
		fseek(ffp, prev, 0);
	}

#else
#if SYS_UNIX || SYS_VMS || SYS_OS2

	struct stat statbuf;
	if (fstat(fileno(ffp), &statbuf) == 0) {
		result = (B_COUNT)statbuf.st_size;
	}
#if SYS_VMS
	if (result == -1)
	{
	    /*
	     * Attempting to read a file size across DECNET via fstat()
	     * doesn't work using (at least) VAXC and its runtime library.
	     * So return 0 and let slowreadf() read the file.
	     */
	    result = 0;
	}
#endif

#else
#if SYS_MSDOS
#if CC_DJGPP

	long prev;
	prev = ftell(ffp);
	if (fseek(ffp, 0, 2) >= 0) {
		result = ftell(ffp);
		fseek(ffp, prev, 0);
	}

#else

	int fd = fileno(ffp);
	if (fd >= 0)
	result = filelength(fd);

#endif /* CC_DJGPP */
#endif /* SYS_MSDOS */
#endif /* SYS_UNIX || SYS_VMS || SYS_OS2 */
#endif /* SYS_WINNT */

	TRACE(("ffsize = %ld\n", result))
	return result;
}

int
ffexists(char *p)
{
	int status = FALSE;

#if SYS_UNIX || SYS_VMS || SYS_OS2 || SYS_WINNT

	struct stat statbuf;
	if (!isInternalName(p)
	 && stat(SL_TO_BSL(p), &statbuf) == 0) {
		status = TRUE;
	}

#else

#if SYS_MSDOS || SYS_WIN31

	if (!isInternalName(p)
	 && ffropen(SL_TO_BSL(p)) == FIOSUC) {
		ffclose();
		status = TRUE;
	}

#endif
#endif

	TRACE(("ffexists(fn=%s) = %d\n", p, status))
	return (status);
}

#if !SYS_MSDOS
int
ffread(char *buf, long len)
{
#if SYS_VMS
	/*
	 * If the input file is record-formatted (as opposed to stream-lf, a
	 * single read won't get the whole file.
	 */
	int	total = 0;

	while (len > 0) {
		int	this = read(fileno(ffp), buf+total, len-total);
		if (this <= 0)
			break;
		total += this;
	}
	fseek (ffp, len, 1);	/* resynchronize stdio */
	return total;
#else
# if CC_CSETPP
	int got = fread(buf, len, 1, ffp);
	return got == 1 ? len : -1;
# else
	int got = read(fileno(ffp), buf, (SIZE_T)len);
	if (got >= 0)
	    fseek (ffp, len, 1);	/* resynchronize stdio */
	return got;
# endif
#endif
}

void
ffseek(long n)
{
#if SYS_VMS
	ffrewind();	/* see below */
#endif
	fseek (ffp,n,0);
}

void
ffrewind(void)
{
#if SYS_VMS
	/* VAX/VMS V5.4-2, VAX-C 3.2 'rewind()' does not work properly, because
	 * no end-of-file condition is returned after rewinding.  Reopening the
	 * file seems to work.  We can get away with this because we only
	 * reposition in "permanent" files that we are reading.
	 */
	char	temp[NFILEN];
	fgetname(ffp, temp);
	(void)fclose(ffp);
	ffp = fopen(temp, FOPEN_READ);
#else
	fseek (ffp,0L,0);
#endif
}
#endif

/*
 * Close a file. Should look at the status in all systems.
 */
int
ffclose(void)
{
	int s = 0;

	free_fline();

#if SYS_UNIX || SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT
#if OPT_SHELL
	if (fileispipe) {
		npclose(ffp);
		mlforce("[Read %d lines%s]",
			count_fline,
			interrupted() ? "- Interrupted" : "");
#ifdef	MDCHK_MODTIME
		(void)check_visible_modtimes();
#endif
	} else
#endif
	{
		s = fclose(ffp);
	}
	if (s != 0) {
		mlerror("closing");
		return(FIOERR);
	}
#else
	(void)fclose(ffp);
#endif
	return (FIOSUC);
}

/*
 * Write a line to the already opened file. The "buf" points to the buffer,
 * and the "nbuf" is its length, less the free newline. Return the status.
 */
int
ffputline(const char *buf, int nbuf, const char *ending)
{
	register int	i;
	for (i = 0; i < nbuf; ++i)
		if (ffputc(char2int(buf[i])) == FIOERR)
			return FIOERR;

	while (*ending != EOS) {
		if (*ending != '\r' || i == 0 || buf[i-1] != '\r')
			ffputc(*ending);
		ending++;
	}

	if (ferror(ffp)) {
		mlerror("writing");
		return (FIOERR);
	}

	return (FIOSUC);
}

/*
 * Write a char to the already opened file.
 * Return the status.
 */
int
ffputc(int c)
{
	char	d = (char)c;

#if	OPT_ENCRYPT
	if (ffcrypting)
		d = vl_encrypt_char(d);
#endif
	fputc(d, ffp);

	if (ferror(ffp)) {
		mlerror("writing");
		return (FIOERR);
	}

	return (FIOSUC);
}

/*
 * Read a line from a file, and store the bytes in an allocated buffer.
 * "fflinelen" is the length of the buffer. Reallocate and copy as necessary.
 * Check for I/O errors. Return status.
 */

int
ffgetline(
int *lenp)	/* to return the final length */
{
	register int c;
	register ALLOC_T i;	/* current index into fflinebuf */

	if (fileeof)
		return(FIOEOF);

	/* be sure there's a buffer */
	if (!fflinebuf)
		fflinebuf = castalloc(char,fflinelen = NSTRING);
	if (!fflinebuf)
		return(FIOMEM);

	/* accumulate to a newline */
	i = 0;
	for_ever {
		c = fgetc(ffp);
		if (feof(ffp) || ferror(ffp))
			break;
#if	OPT_ENCRYPT
		if (ffcrypting)
			c = vl_encrypt_char(c);
#endif
		if (c == '\n')
			break;
		if (interrupted()) {
			free_fline();
			*lenp = 0;
			return FIOABRT;
		}
		fflinebuf[i++] = (char)c;
		/* grow our buffer -- be sure it grows fast enough */
		if (i >= fflinelen) {
			/* "Small" exponential growth - EJK */
			ALLOC_T growth = (fflinelen >> 3) + NSTRING;
			fflinelen += growth;
			fflinebuf = castrealloc(char,fflinebuf,fflinelen);
			if (!fflinebuf) {
				fflinelen = 0;
				return(FIOMEM);
			}
		}
#if OPT_WORKING
		cur_working++;
#endif
	}


	*lenp = i;	/* return the length, not including final null */
	fflinebuf[i] = EOS;

	if (c == EOF) {  /* problems? */
		if (!feof(ffp) && ferror(ffp)) {
			mlerror("reading");
			return(FIOERR);
		}

		if (i != 0) /* got something */
			fileeof = TRUE;
		else
			return(FIOEOF);
	}

	count_fline++;
	return (fileeof ? FIOBAD : FIOSUC);
}

/*
 * isready_c()
 *
 * This fairly non-portable addition to the stdio set of macros is used to
 * see if stdio has data for us, without actually reading it and possibly
 * blocking.  If you have trouble building this, just define no_isready_c
 * below, so that ffhasdata() always returns FALSE.  If you want to make it
 * work, figure out how your getc in stdio.h knows whether or not to call
 * _filbuf() (or the equivalent), and write isready_c so that it returns
 * true if the buffer has chars available now.  The big win in getting it
 * to work is that reading the output of a pipe (e.g.  ":e !co -p file.c")
 * is _much_much_ faster, and I don't have to futz with non-blocking
 * reads...
 */

#ifndef isready_c
#  if CC_TURBO
#    define	isready_c(p)	( (p)->bsize > ((p)->curp - (p)->buffer) )
#  endif
#  if SYS_OS2_EMX
#    define	isready_c(p)	( (p)->_rcount > 0)
#  endif
#  if SYS_VMS
#    define	isready_c(p)	( (*p)->_cnt > 0)
#  endif
#  if SYS_WINNT && !defined( __BORLANDC__ )
#    define	isready_c(p)	( (p)->_cnt > 0)
#  endif
#endif

int
ffhasdata(void)
{
#ifdef isready_c
	if (isready_c(ffp))
		return TRUE;
#endif
#if defined(FIONREAD) && !SYS_WINNT
	{
	long x;
	return(((ioctl(fileno(ffp),FIONREAD,(caddr_t)&x) < 0) || x == 0) ? FALSE : TRUE);
	}
#else
	return FALSE;
#endif
}

#if NO_LEAKS
void fileio_leaks(void)
{
	free_fline();
}
#endif
