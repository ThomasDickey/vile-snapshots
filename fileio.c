/*
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here.
 *
 * $Id: fileio.c,v 1.212 2025/01/26 17:03:16 tom Exp $
 */

#include <estruct.h>

#if SYS_VMS
#include <file.h>
#endif

#if SYS_WINNT || (SYS_OS2 && CC_CSETPP) || CC_DJGPP
#include <io.h>
#include <fcntl.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#if CC_NEWDOSCC
#include <io.h>
#endif

#ifndef EISDIR
#define EISDIR EACCES
#endif

#include <edef.h>
#include <nefsms.h>

/*--------------------------------------------------------------------------*/

static void
free_fline(void)
{
    beginDisplay();
    FreeAndNull(fflinebuf);
    fflinelen = 0;
    endofDisplay();
}

#if OPT_FILEBACK
/*
 * Copy file when making a backup
 */
static int
copy_file(const char *src, const char *dst)
{
    FILE *ifp;
    FILE *ofp;
    int chr;
    int ok = FALSE;

    if ((ifp = fopen(SL_TO_BSL(src), FOPEN_READ)) != NULL) {
	if ((ofp = fopen(SL_TO_BSL(dst), FOPEN_WRITE)) != NULL) {
	    ok = TRUE;
	    for_ever {
		chr = vl_getc(ifp);
		if (feof(ifp))
		    break;
		vl_putc(chr, ofp);
		if (ferror(ifp) || ferror(ofp)) {
		    ok = FALSE;
		    break;
		}
	    }
	    (void) fclose(ofp);
	}
	(void) fclose(ifp);
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
# ifndef HAVE_LONG_FILE_NAMES
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

    if (file_stat(orig, &ostat) != 0)
	return FALSE;

    if (file_stat(backup, &bstat) == 0) {	/* the backup file exists */

#if SYS_UNIX
	/* same file, somehow? */
	if (bstat.st_dev == ostat.st_dev &&
	    bstat.st_ino == ostat.st_ino) {
	    return FALSE;
	}
#endif

	/* remove it */
	if (unlink(SL_TO_BSL(backup)) != 0)
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
#ifdef HAVE_UTIMES
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
	s = utimes(SL_TO_BSL(backup), buf);
	if (s != 0) {
	    (void) unlink(SL_TO_BSL(backup));
	    return FALSE;
	}
    }
#else
#ifdef HAVE_UTIME
    {
	struct utimbuf buf;
	buf.actime = ostat.st_atime;
	buf.modtime = ostat.st_mtime;
	s = utime(SL_TO_BSL(backup), &buf);
	if (s != 0) {
	    (void) unlink(SL_TO_BSL(backup));
	    return FALSE;
	}
    }
#endif
#endif

#if SYS_OS2 && CC_CSETPP
    s = chmod(SL_TO_BSL(backup), ostat.st_mode & (S_IREAD | S_IWRITE));
#else
    s = chmod(SL_TO_BSL(backup), ostat.st_mode & 0777);
#endif
    if (s != 0) {
	(void) unlink(SL_TO_BSL(backup));
	return FALSE;
    }

    return TRUE;
}

static int
make_backup(char *fname)
{
    int ok = TRUE;

    if (ffexists(fname)) {	/* if the file exists, attempt a backup */
	char tname[NFILEN];
	char *s = pathleaf(vl_strncpy(tname, fname, sizeof(tname)));
	char *t = strrchr(s, '.');
	char *gvalfileback = b_val_ptr(curbp, VAL_BACKUPSTYLE);

	switch (choice_to_code(&fsm_backup_blist, gvalfileback, strlen(gvalfileback))) {
	case bak_BAK:
	    if (t == NULL	/* i.e. no '.' at all */
#if SYS_UNIX
		|| t == s	/* i.e. leading char is '.' */
#endif
		)
		t = skip_string(s);	/* then just append */
	    (void) strcpy(t, ".bak");
	    break;

#if SYS_UNIX
	case bak_TILDE:
	    t = skip_string(s);
#ifndef HAVE_LONG_FILE_NAMES
	    if (t - s >= MAX_FN_LEN) {
		if (t - s == MAX_FN_LEN &&
		    s[MAX_FN_LEN - 2] == '.')
		    s[MAX_FN_LEN - 2] = s[MAX_FN_LEN - 1];
		t = &s[MAX_FN_LEN - 1];
	    }
#endif
	    (void) strcpy(t, "~");
	    break;

#if VILE_SOMEDAY
	case bak_TILDE_N0:
	    /* numbered backups if one exists, else simple */
	    break;

	case bak_TILDE_N:
	    /* numbered backups of all files */
	    break;
#endif
#endif /* SYS_UNIX */

	default:
	    mlforce("BUG: bad fileback value");
	    return FALSE;
	}

	ok = write_backup_file(fname, tname);

    }
    return ok;
}
#endif /* OPT_FILEBACK */

/*
 * Definitions for file_stat()
 */
typedef struct {
    int rc;
    char fn[NFILEN];
    struct stat sb;
} CACHE_STAT;

#define MAX_CACHE_STAT 2

/*
 * Cache the most recent few stat() calls on the filesystem.  If the 'sb'
 * pointer is null, purge our cache entry for the corresponding file.  We
 * usually purge only when asking for the modification-time of a file, since
 * that may change externally.
 */
int
file_stat(const char *fn, struct stat *sb)
{
    static CACHE_STAT cache[MAX_CACHE_STAT];

    int rc = 0;
    unsigned n;
    int found = FALSE;

    if (fn != NULL) {
	for (n = 0; n < TABLESIZE(cache); ++n) {
	    if (!strcmp(fn, cache[n].fn)) {
		found = TRUE;
		break;
	    }
	}
	if (sb != NULL) {
	    if (!found) {
		for (n = TABLESIZE(cache) - 1; n != 0; --n) {
		    if (strlen(cache[n].fn) == 0)
			break;
		}
		cache[n].rc = stat(SL_TO_BSL(vl_strncpy(cache[n].fn, fn,
							sizeof(cache[n].fn))),
				   &(cache[n].sb));
	    }
	    rc = cache[n].rc;
	    *sb = cache[n].sb;
	} else if (found) {
	    vl_strncpy(cache[n].fn, "", (size_t) NFILEN);
	}
    } else {
	fn = "";
	for (n = 0; n < TABLESIZE(cache); ++n) {
	    vl_strncpy(cache[n].fn, fn, (size_t) NFILEN);
	}
    }
    TRACE(("file_stat(%s) = %d%s\n", fn, rc,
	   (found
	    ? ((sb != NULL)
	       ? " cached"
	       : " purged")
	    : "")));
    return rc;
}

/*
 * Open a file for reading.
 */
int
ffropen(char *fn)
{
    int rc = FIOSUC;

    fileeof = FALSE;
    ffstatus = file_is_closed;

    TRACE(("ffropen(fn=%s)\n", fn));
#if OPT_SHELL
    if (isShellOrPipe(fn)) {
	ffp = NULL;
#if SYS_UNIX || SYS_MSDOS || SYS_OS2 || SYS_WINNT
	ffp = npopen(fn + 1, FOPEN_READ);
#endif
#if SYS_VMS
	ffp = vms_rpipe(fn + 1, 0, (char *) 0);
	/* really a temp-file, but we cannot fstat it to get size */
#endif
	if (ffp == NULL) {
	    mlerror("opening pipe for read");
	    rc = FIOERR;
	} else {
	    ffstatus = file_is_pipe;
	    count_fline = 0;
	}
    } else
#endif
    if (is_directory(fn)) {
#if COMPLETE_FILES || COMPLETE_DIRS
	ffstatus = file_is_internal;	/* will store directory as buffer */
	rc = FIOSUC;
#else
	set_errno(EISDIR);
	mlerror("opening directory");
	rc = FIOERR;
#endif
    } else if (is_nonfile(fn)) {
	mlforce("[Not a file: %s]", fn);
	rc = FIOERR;
    } else if ((ffp = fopen(SL_TO_BSL(fn), FOPEN_READ)) == NULL) {
	if (errno != ENOENT
#if SYS_OS2 && CC_CSETPP
	    && errno != ENOTEXIST
	    && errno != EBADNAME
#endif
	    && errno != EINVAL) {	/* a problem with Linux to DOS-files */
	    mlerror("opening for read");
	    rc = FIOERR;
	} else {
	    rc = FIOFNF;
	}
    } else {
	ffstatus = file_is_external;
    }

    if (rc == FIOSUC) {
	TPRINTF(("** opened %s for read\n", fn));
    }
    return (rc);
}

/*
 * Open a file for writing. Return TRUE if all is well, and FALSE on error
 * (cannot create).
 */
int
ffwopen(char *fn, int forced)
{
#if SYS_UNIX || SYS_MSDOS || SYS_OS2 || SYS_WINNT
    char *name;
    const char *mode = FOPEN_WRITE;

    (void) forced;

    TRACE(("ffwopen(fn=%s, forced=%d)\n", fn, forced));
    ffstatus = file_is_closed;
    if (i_am_dead) {
	if ((ffd = open(SL_TO_BSL(fn), O_WRONLY | O_CREAT, 0600)) < 0) {
	    return (FIOERR);
	}
	ffstatus = file_is_unbuffered;
    } else
#if OPT_SHELL
    if (isShellOrPipe(fn)) {
	if ((ffp = npopen(fn + 1, mode)) == NULL) {
	    mlerror("opening pipe for write");
	    return (FIOERR);
	}
	ffstatus = file_is_pipe;
    } else
#endif
    {
	if ((name = is_appendname(fn)) != NULL) {
	    fn = name;
	    mode = FOPEN_APPEND;
	} else if (is_directory(fn)) {
	    set_errno(EISDIR);
	    mlerror("opening directory");
	    return (FIOERR);
	} else if (is_nonfile(fn)) {
	    mlforce("[Not a file: %s]", fn);
	    return (FIOERR);
	}
#if OPT_FILEBACK
	/* will we be able to write? (before attempting backup) */
	if (ffexists(fn) && ffronly(fn)) {
	    mlerror("accessing for write");
	    return (FIOERR);
	}

	if (*b_val_ptr(curbp, VAL_BACKUPSTYLE) != 'o') {	/* "off" ? */
	    if (!make_backup(fn)) {
		if (!forced) {
		    mlerror("making backup file");
		    return (FIOERR);
		}
	    }
	}
#endif
	if ((ffp = fopen(SL_TO_BSL(fn), mode)) == NULL) {
	    mlerror("opening for write");
	    return (FIOERR);
	}
	ffstatus = file_is_external;
    }
#else
#if SYS_VMS
    char temp[NFILEN];
    int fd;
    strip_version(fn = strcpy(temp, fn));

    if (is_appendname(fn)
	|| is_directory(fn)
	|| (fd = vms_creat(temp)) < 0
	|| (ffp = fdopen(fd, FOPEN_WRITE)) == NULL) {
	mlforce("[Cannot open file for writing]");
	return (FIOERR);
    }
#else
    if ((ffp = fopen(fn, FOPEN_WRITE)) == NULL) {
	mlerror("opening for write");
	return (FIOERR);
    }
#endif
    ffstatus = file_is_external;
#endif
    TPRINTF(("** opened %s for write\n", fn));
    return (FIOSUC);
}

/* wrapper for 'access()' */
int
ffaccess(char *fn, UINT mode)
{
    int status;
#ifdef HAVE_ACCESS
    /* these were chosen to match the SYSV numbers, but we'd rather use
     * the symbols for portability.
     */
#ifndef X_OK
#if defined(_WIN32)
#define X_OK 0
#else
#define X_OK 1
#endif
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif
    int n = 0;
    if (mode & FL_EXECABLE)
	n |= X_OK;
    if (mode & FL_WRITEABLE)
	n |= W_OK;
    if (mode & FL_READABLE)
	n |= R_OK;
    status = (!isInternalName(fn)
	      && access(SL_TO_BSL(fn), n) == 0);
#else
    int fd;
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
	if ((fd = open(SL_TO_BSL(fn), O_WRONLY | O_APPEND)) < 0) {
	    status = FALSE;
	} else {
	    (void) close(fd);
	    status = TRUE;
	}
	break;
    default:
	status = ffexists(fn);
	break;
    }
#endif
    TRACE(("ffaccess(fn=%s, mode=%d) = %d\n", SL_TO_BSL(fn), mode, status));
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
    TRACE(("ffronly(fn=%s) = %d\n", fn, status));
    return status;
}

#if OPT_ENCRYPT
static int ffcrypting = FALSE;

void
ffdocrypt(int crypting)
{
    ffcrypting = crypting;
}
#endif

#if SYS_WINNT || (SYS_MSDOS && CC_DJGPP)
#define FFSIZE_FTELL 1
#else
#define FFSIZE_FTELL 0
#endif

int
ffsize(B_COUNT * have)
{
    int result = -1;

#if FFSIZE_FTELL
    long prev;

    prev = ftell(ffp);
    *have = 0;
    if (fseek(ffp, 0, SEEK_END) >= 0) {
	result = 0;
	*have = ftell(ffp);
	(void) fseek(ffp, prev, SEEK_SET);
    }
#else
    struct stat statbuf;

    *have = 0;
    if ((result = fstat(fileno(ffp), &statbuf)) == 0) {
	*have = (B_COUNT) statbuf.st_size;
    }
#if SYS_VMS
    if (result == -1) {
	/*
	 * Attempting to read a file size across DECNET via fstat()
	 * doesn't work using (at least) VAXC and its runtime library.
	 * So return 0 and let slowreadf() read the file.
	 */
	result = 0;
    }
#endif /* SYS_VMS */
#endif /* FFSIZE_FTELL */

    TRACE(("ffsize %s %lu\n", (result < 0) ? "failed" : "succeeded", *have));
    return result;
}

int
ffexists(char *p)
{
    int status = FALSE;

#if SYS_VMS
    if (!isInternalName(p))
	status = vms_ffexists(p);
#endif

#if SYS_UNIX || SYS_OS2 || SYS_WINNT

    struct stat statbuf;
    if (!isInternalName(p)
	&& file_stat(p, NULL) == 0
	&& file_stat(p, &statbuf) == 0) {
	status = TRUE;
    }
#endif

#if SYS_MSDOS

    if (!isInternalName(p)
	&& ffropen(SL_TO_BSL(p)) == FIOSUC) {
	ffclose();
	status = TRUE;
    }
#endif

    TRACE(("ffexists(fn=%s) = %d\n", p, status));
    return (status);
}

/*
 * Not every system has read().  Use fread() for the others.
 * Also, do not use read() when we use ftell(), since the latter is
 * buffered and the former is not.
 */
#if CC_CSETPP
#define FFREAD_FREAD 1
#else
#define FFREAD_FREAD FFSIZE_FTELL
#endif

/*
 * fread() and read() return ssize_t, which may be a signed long.
 * But ssize_t is less common in header files, so simply use long.
 * Ditto for the limit - use LONG_MAX, which should appear in limits.h
 * but older headers may not).
 */
#ifndef LONG_MAX
#define LONG_MAX (((unsigned long)(~0)) >> 1)
#endif

int
ffread(char *buf, B_COUNT want, B_COUNT * have)
{
    int result = 0;
    long got;

    *have = 0;
    if (want != 0) {
	/*
	 * Depending on the system, a single read will not necessarily return
	 * all of the data in one call.  Also, since B_COUNT is unsigned, and
	 * read()'s return-value is not, we have to limit the requests to
	 * avoid overflow.
	 */
	while (want != 0) {
	    size_t ask = (((want - *have) > LONG_MAX)
			  ? LONG_MAX
			  : (want - *have));

#if FFREAD_FREAD
	    /* size_t may not fit in long, making a sign-extension */
	    got = (long) fread(buf + *have, 1, ask, ffp);
#else
	    got = read(fileno(ffp), buf + *have, ask);
#endif
	    if (got <= 0)
		break;
	    *have += (B_COUNT) got;
	}
	if (*have == 0)
	    result = -1;
    }
    TRACE(("ffread %s %lu\n", (result < 0) ? "failed" : "succeeded", *have));
    return result;
}

void
ffseek(B_COUNT n)
{
#if SYS_VMS
    ffrewind();			/* see below */
#endif
    (void) fseek(ffp, (long) n, SEEK_SET);
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
    char temp[NFILEN];
    fgetname(ffp, temp);
    (void) fclose(ffp);
    ffp = fopen(temp, FOPEN_READ);
#else
    (void) fseek(ffp, 0L, SEEK_SET);
#endif
}

/*
 * Close a file. Should look at the status in all systems.
 */
int
ffclose(void)
{
    int s = 0;

    if (ffstatus == file_is_unbuffered) {
	if (fflinelen) {
	    IGNORE_RC(write(ffd, fflinebuf, (unsigned) fflinelen));
	    fflinelen = 0;
	}
	close(ffd);
    } else {
	free_fline();

	if (ffp != NULL) {
#if SYS_UNIX || SYS_MSDOS || SYS_OS2 || SYS_WINNT
#if OPT_SHELL
	    if (ffstatus == file_is_pipe) {
		npclose(ffp);
		mlwrite("[Read %d lines%s]",
			count_fline,
			interrupted()? "- Interrupted" : "");
#ifdef MDCHK_MODTIME
		(void) check_visible_files_changed();
#endif
	    } else
#endif
	    {
		s = fclose(ffp);
	    }
	    if (s != 0) {
		mlerror("closing");
		return (FIOERR);
	    }
#else
	    (void) fclose(ffp);
#endif
	}
	ffp = NULL;
	ffbuffer = NULL;
    }
    ffstatus = file_is_closed;
    return (FIOSUC);
}

/*
 * Write a line to the already opened file. The "buf" points to the buffer,
 * and the "nbuf" is its length, less the free newline. Return the status.
 */
int
ffputline(const char *buf, int nbuf, const char *ending)
{
    int i = 0;

    if (buf != NULL) {
	for (i = 0; i < nbuf; ++i)
	    if (ffputc(CharOf(buf[i])) == FIOERR)
		return FIOERR;
    }

    if (ending != NULL) {
	while (*ending != EOS) {
	    if (*ending != '\r' || i == 0 || buf[i - 1] != '\r')
		ffputc(*ending);
	    ending++;
	}
    }

    if (ffp != NULL && ferror(ffp)) {
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
    char d = (char) c;

#if OPT_ENCRYPT
    if (ffcrypting && (ffstatus != file_is_pipe))
	d = (char) vl_encrypt_char(d);
#endif
    if (i_am_dead) {
	fflinebuf[fflinelen++] = d;
	if (fflinelen >= NSTRING) {
	    IGNORE_RC(write(ffd, fflinebuf, (unsigned) fflinelen));
	    fflinelen = 0;
	}
    } else {
	vl_putc(d, ffp);

	if (ferror(ffp)) {
	    mlerror("writing");
	    return (FIOERR);
	}
    }

    return (FIOSUC);
}

/* "Small" exponential growth - EJK */
static int
alloc_linebuf(size_t needed)
{
    beginDisplay();
    needed += 2;
    if (needed < NSTRING)
	needed = NSTRING;

    if (fflinebuf == NULL) {
	fflinelen = needed;
	fflinebuf = castalloc(char, fflinelen);
    } else if (needed >= fflinelen) {
	fflinelen = needed + (needed >> 3);
	safe_castrealloc(char, fflinebuf, fflinelen);
    }

    if (fflinebuf == NULL)
	fflinelen = 0;
    endofDisplay();
    return (fflinebuf != NULL);
}

#define ALLOC_LINEBUF(i) \
	    if ((i) >= fflinelen && !alloc_linebuf((size_t) i)) \
		return (FIOMEM)

/*
 * Read a line from a file, and store the bytes in an allocated buffer.
 * "fflinelen" is the length of the buffer. Reallocate and copy as necessary.
 * Check for I/O errors. Return status.  The final length is returned via
 * the 'lenp' parameter.
 */
int
ffgetline(size_t *lenp)
{
    int c;
    size_t i;			/* current index into fflinebuf */
    int end_of_line = (global_b_val(VAL_RECORD_SEP) == RS_CR) ? '\r' : '\n';

    if (fileeof)
	return (FIOEOF);

    ALLOC_LINEBUF(NSTRING);

    i = 0;
#if COMPLETE_FILES || COMPLETE_DIRS
    if (ffstatus == file_is_internal) {
	if (ffcursor == buf_head(ffbuffer)) {
	    ffcursor = NULL;
	    fileeof = TRUE;
	    return (FIOEOF);
	}
	if (llength(ffcursor) > 0) {
	    i = (size_t) llength(ffcursor);
	    ALLOC_LINEBUF(i);
	    memcpy(fflinebuf, lvalue(ffcursor), i);
	}
	ffcursor = lforw(ffcursor);

	ALLOC_LINEBUF(i);
	fflinebuf[i] = EOS;
	*lenp = i;		/* return the length, not including final null */
    } else
#endif
    {
	/* accumulate to a newline */
	for_ever {
	    c = vl_getc(ffp);
	    if (feof(ffp) || ferror(ffp))
		break;
#if OPT_ENCRYPT
	    if (ffcrypting && (ffstatus != file_is_pipe))
		c = vl_encrypt_char(c);
#endif
	    if (c == end_of_line) {
#if OPT_MULTIBYTE
		/*
		 * If we are reading a UTF-16 file, we may have to read
		 * more bytes to align properly.
		 */
		if (count_fline == 0) {
		    UCHAR *buffer = (UCHAR *) fflinebuf;
		    B_COUNT length = (B_COUNT) i;
		    make_global_b_val(btempp, VAL_BYTEORDER_MARK);
		    make_global_b_val(btempp, VAL_FILE_ENCODING);
		    if (decode_bom(btempp, buffer, &length))
			i = length;
		    deduce_charset(btempp, buffer, &length, FALSE);
		}

		if (b_val(btempp, VAL_FILE_ENCODING) > enc_UTF8) {
		    UCHAR *buffer = (UCHAR *) fflinebuf;
		    B_COUNT length = (B_COUNT) (i + 1);

		    if (!aligned_charset(btempp, buffer, &length)) {
			do {
			    ALLOC_LINEBUF(i + 2);
			    fflinebuf[i++] = (char) c;
			    c = vl_getc(ffp);	/* expecting a null... */
			    length = (B_COUNT) (i + 1);
			    buffer = (UCHAR *) fflinebuf;
			} while (!aligned_charset(btempp, buffer, &length));
			fflinebuf[i] = (char) c;
		    }

		    cleanup_charset(btempp, buffer, &length);
		    i = length - 1;	/* discount the newline */
		}
#endif
		break;
	    }
	    if (interrupted()) {
		free_fline();
		*lenp = 0;
		return FIOABRT;
	    }
	    ALLOC_LINEBUF(i);
	    fflinebuf[i++] = (char) c;
#if OPT_WORKING
	    cur_working++;
#endif
	}

	ALLOC_LINEBUF(i);
	*lenp = i;		/* return the length, not including final null */
	fflinebuf[i] = EOS;

	if (c == EOF) {		/* problems? */
	    if (!feof(ffp) && ferror(ffp)
#ifdef EPIPE
	    /* fix for Borland - no point in warnings if we have EOF */
		&& (errno != EPIPE)
#endif
		) {
		mlerror("reading");
		return (FIOERR);
	    }

	    if (i != 0)		/* got something */
		fileeof = TRUE;
	    else
		return (FIOEOF);
	}
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
#  if SYS_WINNT && defined( _MSC_VER ) && ( _MSC_VER < 1700 )
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
	long x = 0;
	if ((ioctl(fileno(ffp), (long) FIONREAD, (void *) &x) >= 0) && x != 0)
	    return TRUE;
    }
#endif
    return FALSE;
}

#if NO_LEAKS
void
fileio_leaks(void)
{
    free_fline();
}
#endif
