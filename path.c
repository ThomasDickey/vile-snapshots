/*	PATH.C
 *		The routines in this file handle the conversion of pathname
 *		strings.
 *
 * $Id: path.c,v 1.186 2025/01/26 14:33:43 tom Exp $
 */

#include "estruct.h"
#include "edef.h"

#if SYS_UNIX
#include <sys/types.h>
#if !SYS_MINGW
#include <pwd.h>
#endif
#endif

#if SYS_VMS
#include <starlet.h>
#include <file.h>
#endif

#if SYS_OS2
# define INCL_DOSFILEMGR
# define INCL_ERRORS
# include <os2.h>
#endif

#include "dirstuff.h"

#if SYS_WINNT
# include <direct.h>
# define curdrive() (_getdrive() + ('A' - 1))
# define curr_dir_on_drive(d) _getdcwd(toUpper(d) - ('A' - 1), temp, sizeof(temp))
#endif

#if SYS_OS2_EMX
# define curdrive() _getdrive()
static char *
curr_dir_on_drive(int d)
{
    static char buffer[NFILEN];
    char *s;
    if (_getcwd1(buffer, d) < 0)
	return 0;
    /* EMX 0.9b documents _getcwd1 fixes slashes, but it doesn't work */
    for (s = buffer; *s; s++)
	if (*s == '\\')
	    *s = '/';
    return buffer;
}
#endif

#ifdef GMDRESOLVE_LINKS
#if defined(HAVE_SYS_ITIMER_H) && defined(HAVE_SETITIMER)
#include <sys/itimer.h>
#endif
#endif

static char empty_string[] = "";

/*
 * Fake directory-routines for system where we cannot otherwise figure out how
 * to read the directory-file.
 */
#if USE_LS_FOR_DIRS
DIR *
opendir(char *path)
{
    static const char fmt[] = "/bin/ls %s";
    char lscmd[NFILEN + sizeof(fmt)];

    (void) lsprintf(lscmd, fmt, path);
    return npopen(lscmd, "r");
}

DIRENT *
readdir(DIR * dp)
{
    static DIRENT dummy;

    if ((vl_fgets(dummy.d_name, NFILEN, dp)) != NULL) {
	/* zap the newline */
	dummy.d_name[strlen(dummy.d_name) - 1] = EOS;
	return &dummy;
    }
    return 0;
}

int
closedir(DIR * dp)
{
    (void) npclose(dp);
    return 0;
}
#endif

/*
 * Use this routine to fake compatibility with unix directory routines.
 */
#if OLD_STYLE_DIRS
DIRENT *
readdir(DIR * dp)
{
    static DIRENT dbfr;

    return (fread(&dbfr, sizeof(dbfr), 1, dp)
	    ? &dbfr
	    : (DIRENT *) 0);
}
#endif

#if OPT_MSDOS_PATH
/*
 * If the pathname begins with an MSDOS-drive, return the pointer past it.
 * Otherwise, return null.
 */
char *
is_msdos_drive(char *path)
{
#if OPT_UNC_PATH
    if (is_slashc(path[0]) && is_slashc(path[1]))
	return (path + 1);
#endif
    if (isAlpha(path[0]) && path[1] == ':')
	return (path + 2);
    return 0;
}
#endif

#if OPT_VMS_PATH
#define VMSPATH_END_NODE   1
#define VMSPATH_END_DEV    2
#define VMSPATH_BEGIN_DIR  3
#define VMSPATH_NEXT_DIR   4
#define VMSPATH_END_DIR    5
#define	VMSPATH_BEGIN_FILE 6
#define VMSPATH_BEGIN_TYP  7
#define VMSPATH_BEGIN_VER  8

/*
 * Returns true if the string is delimited in a manner compatible with VMS
 * pathnames.  To be consistent with the use of 'is_pathname()', insist that
 * at least the "[]" characters be given.
 *
 * Complete syntax:
 *	node::device:[dir1.dir2]filename.type;version
 *	    ^1     ^2^3   ^4  ^5^6      ^7   ^8
 *
 * 'option' is true:directory, false:file, -true:don't care
 */
int
is_vms_pathname(const char *path, int option)
{
    const char *base = path;
    int this = 0, next = -1;

    if (isEmpty(path))		/* this can happen with null buffer-name */
	return FALSE;

    while (ispath(*path)) {
	switch (*path) {
	case '[':
	    if (this >= VMSPATH_BEGIN_FILE)
		return FALSE;
	    next = VMSPATH_BEGIN_DIR;
	    break;
	case ']':
	    if (this < VMSPATH_BEGIN_DIR)
		return FALSE;
	    if (path != base	/* rooted logical? */
		&& path[1] == '['
		&& path[-1] == '.')
		path++;
	    else
		next = VMSPATH_END_DIR;
	    break;
	case '.':
	    if (this >= VMSPATH_BEGIN_TYP) {
		if (this >= VMSPATH_BEGIN_VER)
		    return FALSE;
		next = VMSPATH_BEGIN_VER;
		break;
	    }
	    next = (this >= VMSPATH_END_DIR)
		? VMSPATH_BEGIN_TYP
		: (this >= VMSPATH_BEGIN_DIR
		   ? VMSPATH_NEXT_DIR
		   : VMSPATH_BEGIN_TYP);
	    break;
	case ';':
	    next = VMSPATH_BEGIN_VER;
	    break;
	case ':':
	    if (path[1] == ':') {
		path++;		/* eat "::" */
		if (this >= VMSPATH_END_NODE)
		    return FALSE;
		next = VMSPATH_END_NODE;
	    } else
		next = VMSPATH_END_DEV;
	    break;
	case '!':
	case '/':
	case CH_TILDE:
	    return FALSE;	/* a DEC-shell name */
	default:
	    if (!ispath(*path))
		return FALSE;
	    next = (this == VMSPATH_END_DIR)
		? VMSPATH_BEGIN_FILE
		: this;
	    break;
	}
	if (next < this)
	    break;
	this = next;
	path++;
    }

    if ((*path != EOS)
	|| (this < next))
	return FALSE;

    if (this == 0)
	this = VMSPATH_BEGIN_FILE;

    return (option == TRUE && (this == VMSPATH_END_DIR))	/* dir? */
	|| (option == TRUE && (this == VMSPATH_END_DEV))	/* dev? */
	|| (option == FALSE && (this >= VMSPATH_BEGIN_FILE))	/* file? */
	|| (option == -TRUE && (this >= VMSPATH_END_DIR		/* anything? */
				|| this < VMSPATH_BEGIN_DIR));
}
#endif

#if OPT_VMS_PATH
/*
 * Returns a pointer to the argument's last path-leaf (i.e., filename).
 */
char *
vms_pathleaf(char *path)
{
    char *s;
    for (s = skip_string(path);
	 s > path && !strchr(":]", s[-1]);
	 s--) ;
    return s;
}
#endif

/*
 * Returns a pointer to the argument's last path-leaf (i.e., filename).
 */

#if !OPT_VMS_PATH
#define	unix_pathleaf	pathleaf
#endif

char *
unix_pathleaf(char *path)
{
    char *s = last_slash(path);
    if (s == NULL) {
#if OPT_MSDOS_PATH
	if ((s = is_msdos_drive(path)) == 0)
#endif
	    s = path;
    } else
	s++;
    return s;
}

#if OPT_VMS_PATH
char *
pathleaf(char *path)
{
    if (is_vms_pathname(path, -TRUE))
	return vms_pathleaf(path);
    return unix_pathleaf(path);
}
#endif

/*
 * Concatenates a directory and leaf name to form a full pathname.  The result
 * must be no longer than NFILEN.
 */
char *
pathcat(char *dst, const char *path, const char *cleaf)
{
    char save_path[NFILEN];
    char save_leaf[NFILEN];
    char *leaf;
    char *s;
    size_t have;

    if (dst == NULL)
	return NULL;

    if (cleaf == NULL)
	cleaf = "";

    leaf = vl_strncpy(save_leaf, cleaf, (size_t) NFILEN);	/* leaf may be in dst */

    if (isEmpty(path)) {
	(void) strcpy(dst, leaf);
    } else {

	path = vl_strncpy(save_path, path, (size_t) NFILEN);	/* path may be in dst */

#if OPT_MSDOS_PATH
	if (is_msdos_drive(save_path) != 0
	    && is_msdos_drive(leaf) == 0) {
	    have = strlen(strcpy(dst, path));
	    if (have + strlen(leaf) + 2 < NFILEN) {
		s = dst + have - 1;
		if (!is_slashc(*s) && !is_slashc(*leaf))
		    *(++s) = SLASHC;
		(void) strcpy(s + 1, leaf);
	    }
	} else
#endif
	if (is_abs_pathname(leaf)) {
	    (void) strcpy(dst, leaf);
	} else if (leaf != NULL) {
	    have = strlen(strcpy(dst, path));
	    if (have + strlen(leaf) + 2 < NFILEN) {
		s = dst + have - 1;

#if OPT_VMS_PATH
		if (!is_vms_pathname(dst, TRUE))	/* could be DecShell */
#endif
		    if (!is_slashc(*s)) {
			*(++s) = SLASHC;
		    }

		(void) strcpy(s + 1, leaf);
	    }
#if OPT_VMS_PATH
	    if (is_vms_pathname(path, -TRUE)
		&& is_vms_pathname(leaf, -TRUE)
		&& !is_vms_pathname(dst, -TRUE))
		(void) strcpy(dst, leaf);
#endif
	}
    }
    return dst;
}

/*
 * Tests to see if the string contains a slash-delimiter.  If so, return the
 * last one (so we can locate the path-leaf).
 */
char *
last_slash(char *fn)
{
    char *s;

    if (*fn != EOS)
	for (s = skip_string(fn); s > fn; s--)
	    if (is_slashc(s[-1]))
		return s - 1;
    return NULL;
}

/*
 * If a pathname begins with "~", lookup the name .
 * On Unix we lookup in the password file, under other systems use the $HOME
 * environment variable only, if it exists.
 * Cache the names that we lookup.
 * Searching the password-file on unix can be slow,
 * and users really don't move that often.
 */

#if SYS_UNIX || !SMALLER
typedef struct _upath {
    struct _upath *next;
    char *name;
    char *path;
} UPATH;

static UPATH *user_paths;

static char *
save_user(const char *name, const char *path)
{
    UPATH *q;

    if (name != NULL
	&& path != NULL
	&& (q = typecalloc(UPATH)) != NULL) {
	TRACE(("save_user(name=%s, path=%s)\n", name, path));
	if ((q->name = strmalloc(name)) != NULL
	    && (q->path = strmalloc(path)) != NULL) {
	    q->next = user_paths;
	    user_paths = q;
	    return q->path;
	} else {
	    FreeIfNeeded(q->name);
	    FreeIfNeeded(q->path);
	    free(q);
	}
    }
    return NULL;
}

static char *
find_user(const char *name)
{
#if SYS_UNIX
    struct passwd *p;
#endif
    UPATH *q;

    if (name != NULL) {
	for (q = user_paths; q != NULL; q = q->next) {
	    if (!strcmp(q->name, name)) {
		return q->path;
	    }
	}
#if SYS_UNIX && !SYS_MINGW
	/* not-found, do a lookup.
	 * First try getpwnam with the specified name,
	 * which will use ~ or whatever was passed
	 */
	if (*name != EOS) {
	    p = getpwnam(name);
	} else {
	    /* *name == EOS
	     * so lookup the current uid, and then lookup
	     * the name based on that.
	     */
	    p = getpwuid((uid_t) getuid());
	}

	/* if either of the above lookups worked
	 * then save the result
	 */
	if (p != NULL) {
	    return save_user(name, p->pw_dir);
	}
#endif
	if (*name == EOS) {
	    char *env = home_dir();
	    if (env != NULL) {
		return save_user(name, env);
	    }
	}
    }
#if SYS_UNIX && VILE_NEEDED
    else {			/* lookup all users (for globbing) */
	(void) setpwent();
	while ((p = getpwent()) != NULL)
	    (void) save_user(p->pw_name, p->pw_dir);
	(void) endpwent();
    }
#endif
    return NULL;
}

/*
 * Returns the home-directory as specified by environment variables.  This is
 * not necessarily what the passwd interface would say.
 */
char *
home_dir(void)
{
    char *result;
#if SYS_VMS
    if ((result = getenv("SYS$LOGIN")) == 0)
	result = getenv("HOME");
#else
    result = getenv("HOME");
#if SYS_WINNT
    if (result == 0) {
	if ((result = getenv("USERPROFILE")) == 0 &&
	    (result = getenv("HOMESHARE")) == 0) {
	    /* M$ copies from VMS, but does not learn from its mistakes... */
	    if ((result = getenv("HOMEPATH")) != 0) {
		static char desktop[NFILEN];
		char *drive = getenv("HOMEDRIVE");
		result = pathcat(desktop, drive, result);
	    }
	}
    }
#endif
#endif
    TRACE(("home_dir ->%s\n", TRACE_NULL(result)));
    return result;
}

/*
 * Expand a leading "~user" or "~/" on a given pathname.
 */
char *
home_path(char *path)
{
    if (*path == CH_TILDE) {
	char temp[NFILEN];
	char *s;
	char *d;

	/* parse out the user-name portion */
	for (s = path + 1, d = temp; (*d = *s) != EOS; d++, s++) {
	    if (is_slashc(*d)) {
		*d = EOS;
		s++;
		break;
	    }
	}

#if OPT_VMS_PATH
	(void) mklower(temp);
#endif
	if ((d = find_user(temp)) != NULL) {
	    (void) pathcat(path, d, s);
#if OPT_VMS_PATH
	    /*
	     * path name is now potentially in this form:
	     *
	     *    disk:[rooted_logical.][dir]subdir/leaf
	     *    disk:[dir]subdir/leaf
	     *            etc.
	     *
	     * which is not legit.  fix it up.
	     */

	    if ((s = strrchr(path, '/')) != NULL) {
		if ((d = strrchr(path, ']')) != NULL)
		    *d = '.';
		*s = ']';
		d = path;
		while ((s = strchr(d, '/')) != NULL) {
		    *s++ = '.';
		    d = s;
		}
	    }
#endif
	}
    }
    return path;
}
#endif

#ifndef HAVE_REALPATH
#ifdef GMDRESOLVE_LINKS
/*
 * Some of this code was "borrowed" from the GNU C library (getcwd.c).  It has
 * been largely rewritten and bears little resemblance to what it started out
 * as.
 *
 * The purpose of this code is to generalize getcwd.  The idea is to pass it as
 * input some path name.  This pathname can be relative, absolute, whatever.
 * It may have elements which reference symbolic links.  The output from this
 * function will be the absolute pathname representing the same file.
 * Actually, it only returns the directory.  If the thing you pass it is a
 * directory, you'll get that directory back (canonicalized).  If you pass it a
 * path to an ordinary file, you'll get back the canonicalized directory which
 * contains that file.
 *
 * The way that this code works is similar to the classic implementation of
 * getcwd (or getwd).  The difference is that once it finds a directory, it
 * will cache it.  If that directory is referenced again, finding it will be
 * very fast.  The caller of this function should not free up the pointer which
 * is returned.  This will be done automatically by the caching code.  The
 * value returned will exist at least up until the next call.  It should not be
 * relied on any longer than this.  Care should be taken not to corrupt the
 * value returned.
 *
 * FIXME: there should be some way to reset the cache in case directories are
 * renamed.
 */

#define CPN_CACHE_SIZE 64
#define CPN_CACHE_MASK (CPN_CACHE_SIZE-1)

#if !defined(HAVE_SETITIMER) || !defined(HAVE_SIGACTION)
#define TimedStat file_stat
#else /* !defined(HAVE_SETITIMER) */

#define TimedStat stat_with_timeout

static jmp_buf stat_jmp_buf;	/* for setjmp/longjmp on timeout */

static SIGT
StatHandler(int ACTUAL_SIG_ARGS)
{
    longjmp(stat_jmp_buf, signo);
    SIGRET;
}

static int
stat_with_timeout(const char *path, struct stat *statbuf)
{
    struct sigaction newact;
    struct sigaction oldact;
    sigset_t newset;
    sigset_t oldset;
    struct itimerval timeout;
    struct itimerval oldtimerval;
    int retval, stat_errno;

    newact.sa_handler = StatHandler;
    newact.sa_flags = 0;
    if (sigemptyset(&newact.sa_mask) < 0
	|| sigfillset(&newset) < 0
	|| sigdelset(&newset, SIGPROF) < 0
	|| sigprocmask(SIG_BLOCK, &newset, &oldset) < 0)
	return -1;

    if (sigaction(SIGPROF, &newact, &oldact) < 0) {
	sigprocmask(SIG_SETMASK, &oldset, (sigset_t *) 0);
	return -1;
    }

    timeout.it_interval.tv_sec = 0;
    timeout.it_interval.tv_usec = 0;
    timeout.it_value.tv_sec = 0;
    timeout.it_value.tv_usec = 75000;

    (void) setitimer(ITIMER_PROF, &timeout, &oldtimerval);

    /*
     * POSIX says that 'stat()' won't return an error if it's interrupted,
     * so we force an error by making a longjmp from the timeout handler,
     * and forcing the error return status.
     */
    if (setjmp(stat_jmp_buf)) {
	retval = -1;
	stat_errno = EINTR;
    } else {
	retval = file_stat(path, statbuf);
	stat_errno = errno;
    }

    timeout.it_value.tv_usec = 0;
    (void) setitimer(ITIMER_PROF, &timeout, (struct itimerval *) 0);

    (void) sigaction(SIGPROF, &oldact, (struct sigaction *) 0);
    (void) sigprocmask(SIG_SETMASK, &oldset, (sigset_t *) 0);
    (void) setitimer(ITIMER_PROF, &oldtimerval, (struct itimerval *) 0);

    errno = stat_errno;
    return retval;
}
#endif /* !defined(HAVE_SETITIMER) */

static char *
resolve_directory(char *path_name, char **file_namep)
{
    dev_t rootdev, thisdev;
    ino_t rootino, thisino;
    struct stat st;

    static const char rootdir[] =
    {SLASHC, EOS};

    static TBUFF *last_leaf;
    static TBUFF *last_path;
    static TBUFF *last_temp;

    char *temp_name;
    char *tnp;			/* temp name pointer */
    char *temp_path;		/* the path that we've determined */

    size_t len;			/* temporary for length computations */

    static struct cpn_cache {
	dev_t ce_dev;
	ino_t ce_ino;
	TBUFF *ce_dirname;
    } cache_entries[CPN_CACHE_SIZE];

    struct cpn_cache *cachep;

    tb_free(&last_leaf);
    *file_namep = NULL;
    len = strlen(path_name);

    if (!tb_alloc(&last_temp, len + 1))
	return NULL;
    tnp = (temp_name = tb_values(last_temp)) + len;

    if (!tb_alloc(&last_path, len + 1))
	return NULL;
    *(temp_path = tb_values(last_path)) = EOS;

    (void) strcpy(temp_name, path_name);

    /*
     * Test if the given pathname is an actual directory, or not.  If it's
     * a symbolic link, we'll have to determine if it points to a directory
     * before deciding how to split it.
     */
    if (lstat(temp_name, &st) < 0)
	st.st_mode = S_IFREG;	/* assume we're making a file... */

    if (!S_ISDIR(st.st_mode)) {
	int levels = 0;
	char target[NFILEN];

	/* loop until no more links */
	while ((st.st_mode & S_IFMT) == S_IFLNK) {
	    int got = 0;

	    if (levels++ > 4	/* FIXME */
		|| (got = readlink(temp_name,
				   target, sizeof(target) - 1)) < 0) {
		return NULL;
	    }
	    target[got] = EOS;

	    if (tb_alloc(&last_temp,
			 (size_t) (strlen(temp_name) + got + 1)) == 0)
		return NULL;

	    temp_name = tb_values(last_temp);

	    if (!is_slashc(target[0])) {
		tnp = pathleaf(temp_name);
		if (tnp != temp_name && !is_slashc(tnp[-1]))
		    *tnp++ = SLASHC;
		(void) strcpy(tnp, target);
	    } else {
		(void) strcpy(temp_name, target);
	    }
	    if (lstat(temp_name, &st) < 0)
		break;
	}

	/*
	 * If we didn't resolve any symbolic links, we can find the
	 * filename leaf in the original 'path_name' argument.
	 */
	tnp = last_slash(temp_name);
	if (tnp == NULL) {
	    tnp = temp_name;
	    if (tb_scopy(&last_leaf, tnp) == 0)
		return NULL;
	    *tnp++ = '.';
	    *tnp = EOS;
	} else if (tb_scopy(&last_leaf, tnp + 1) == 0) {
	    return NULL;
	}
	if (tnp == temp_name && is_slashc(*tnp))	/* initial slash */
	    tnp++;
	*tnp = EOS;

	/*
	 * If the parent of the given path_name isn't a directory, give
	 * up...
	 */
	if (TimedStat(temp_name, &st) < 0 || !S_ISDIR(st.st_mode))
	    return NULL;
    }

    /*
     * Now, 'temp_name[]' contains a null-terminated directory-path, and
     * 'tnp' points to the null.  If we've set file_namep, we've allocated
     * a pointer since it may be pointing within the temp_name string --
     * which may be overwritten.
     */
    *file_namep = tb_values(last_leaf);

    thisdev = st.st_dev;
    thisino = st.st_ino;

    cachep = &cache_entries[(thisdev ^ thisino) & CPN_CACHE_MASK];
    if (tb_values(cachep->ce_dirname) != 0
	&& cachep->ce_ino == thisino
	&& cachep->ce_dev == thisdev) {
	return tb_values(cachep->ce_dirname);
    } else {
	cachep->ce_ino = thisino;
	cachep->ce_dev = thisdev;
	tb_free(&(cachep->ce_dirname));		/* will reset iff ok */
    }

    if (TimedStat(rootdir, &st) < 0)
	return NULL;

    rootdev = st.st_dev;
    rootino = st.st_ino;

    while ((thisdev != rootdev)
	   || (thisino != rootino)) {
	DIR *dp;
	DIRENT *de;
	dev_t dotdev;
	ino_t dotino;
	char mount_point;
	size_t namelen = 0;

	len = tnp - temp_name;
	if (tb_alloc(&last_temp, 4 + len) == 0)
	    return NULL;

	tnp = (temp_name = tb_values(last_temp)) + len;
	*tnp++ = SLASHC;
	*tnp++ = '.';
	*tnp++ = '.';
	*tnp = EOS;

	/* Figure out if this directory is a mount point.  */
	if (TimedStat(temp_name, &st) < 0)
	    return NULL;

	dotdev = st.st_dev;
	dotino = st.st_ino;
	mount_point = (dotdev != thisdev);

	/* Search for the last directory.  */
	if ((dp = opendir(temp_name)) != 0) {
	    int found = FALSE;

	    while ((de = readdir(dp)) != NULL) {
#if USE_D_NAMLEN
		namelen = de->d_namlen;
#else
		namelen = strlen(de->d_name);
#endif
		/* Ignore "." and ".." */
		if (de->d_name[0] == '.'
		    && (namelen == 1
			|| (namelen == 2 && de->d_name[1] == '.')))
		    continue;

		if (mount_point
		    || ((long) de->d_ino == (long) thisino)) {
		    len = tnp - temp_name;
		    if (tb_alloc(&last_temp, len + namelen + 1) == 0)
			break;

		    temp_name = tb_values(last_temp);
		    tnp = temp_name + len;

		    *tnp = SLASHC;
		    (void) strncpy(tnp + 1, de->d_name, namelen);
		    tnp[namelen + 1] = EOS;

		    if (TimedStat(temp_name, &st) == 0
			&& st.st_dev == thisdev
			&& st.st_ino == thisino) {
			found = TRUE;
			break;
		    }
		}
	    }

	    if (found) {
		/*
		 * Push the latest directory-leaf before the
		 * string already in 'temp_path[]'.
		 */
		len = strlen(temp_path) + 1;
		if (tb_alloc(&last_path, len + namelen + 1) == 0) {
		    (void) closedir(dp);
		    return NULL;
		}
		temp_path = tb_values(last_path);
		while (len-- != 0)
		    temp_path[namelen + 1 + len] = temp_path[len];
		temp_path[0] = SLASHC;
		(void) memcpy(temp_path + 1, de->d_name, namelen);
	    }
	    (void) closedir(dp);
	    if (!found)
		return NULL;
	} else			/* couldn't open directory */
	    return NULL;

	thisdev = dotdev;
	thisino = dotino;
    }

    if (tb_scopy(&(cachep->ce_dirname),
		 *temp_path ? temp_path : rootdir) == 0)
	return NULL;

    return tb_values(cachep->ce_dirname);
}
#endif /* defined(GMDRESOLVE_LINKS) */
#endif /* defined(HAVE_REALPATH) */

/*
 * The function case_correct_path is intended to determine the true
 * case of all pathname components of a syntactically canonicalized
 * pathname for operating systems which use caseless filenames.
 */
#undef case_correct_path

#if SYS_WINNT

static void
case_correct_path(char *old_file, char *new_file)
{
    WIN32_FIND_DATA fd;
    HANDLE h;
    int len;
    char *next, *current, *end, *sofar;
    char tmp_file[MAX_PATH];

    /* Handle old_file == new_file safely. */
    (void) vl_strncpy(tmp_file, old_file, sizeof(tmp_file));
    old_file = tmp_file;

    if (is_slashc(old_file[0]) && is_slashc(old_file[1])) {

	/* Handle UNC filenames. */
	current = old_file + 2;
	next = strchr(current, SLASHC);
	if (next)
	    next = strchr(next + 1, SLASHC);

	/* Canonicalize the system name and share name. */
	if (next)
	    len = (int) (next - old_file + 1);
	else
	    len = (int) strlen(old_file);
	(void) memcpy(new_file, old_file, len);
	new_file[len] = EOS;
	(void) mklower(new_file);
	if (!next)
	    return;
	sofar = new_file + len;
	current = next + 1;
    } else {

	/* Canonicalize a leading drive letter, if any. */
	if (old_file[0] && old_file[1] == ':') {
	    new_file[0] = old_file[0];
	    new_file[1] = old_file[1];
	    if (isLower(new_file[0]))
		new_file[0] = (char) toUpper(new_file[0]);
	    current = old_file + 2;
	    sofar = new_file + 2;
	} else {
	    current = old_file;
	    sofar = new_file;
	}

	/* Skip a leading slash, if any. */
	if (is_slashc(*current)) {
	    current++;
	    *sofar++ = SLASHC;
	}
    }

    /* Canonicalize each pathname prefix.  Among other things, this discards
     * characters that cannot appear in a valid pathname, such as '<' and '>'.
     */
    end = skip_string(old_file);
    while (current < end) {
	W32_CHAR *lookup;

	if ((next = strchr(current, SLASHC)) == 0)
	    next = end;
	len = (int) (next - current);
	(void) memcpy(sofar, current, len);
	sofar[len] = EOS;

	lookup = w32_charstring(new_file);
	if (lookup != 0
	    && (h = FindFirstFile(lookup, &fd)) != INVALID_HANDLE_VALUE) {
	    char *actual = asc_charstring(fd.cFileName);

	    FindClose(h);

	    (void) strcpy(sofar, actual);
	    free(actual);
	    free(lookup);

	    sofar += strlen(sofar);
	} else {
	    sofar += len;
	}
	if (next != end)
	    *sofar++ = SLASHC;
	current = next + 1;
    }
    return;
}

#elif SYS_OS2

int
is_case_preserving(const char *name)
{
    int case_preserving = 1;

    /* Determine if the filesystem is case-preserving. */
    if (name[0] && name[1] == ':') {
	char drive_name[3];
	char buffer[sizeof(FSQBUFFER2) + 3 * CCHMAXPATH];
	FSQBUFFER2 *pbuffer = (FSQBUFFER2 *) buffer;
	ULONG len = sizeof(buffer);
	APIRET rc;

	drive_name[0] = name[0];
	drive_name[1] = name[1];
	drive_name[2] = EOS;
	rc = DosQueryFSAttach(drive_name, 0, FSAIL_QUERYNAME,
			      pbuffer, &len);
	if (rc == NO_ERROR) {
	    char *name = pbuffer->szName + pbuffer->cbName + 1;

	    if (strcmp(name, "FAT") == 0)
		case_preserving = 0;
	}
    }
    return case_preserving;
}

static void
case_correct_path(char *old_file, char *new_file)
{
    FILEFINDBUF3 fb;
    ULONG entries;
    HDIR hdir;
    APIRET rc;
    char *next, *current, *end, *sofar;
    char tmp_file[NFILEN + 2];
    ULONG len;
    int case_preserving = is_case_preserving(old_file);

    /* Handle old_file == new_file safely. */
    (void) strcpy(tmp_file, old_file);
    old_file = tmp_file;

    /* If it isn't case-preserving then just down-case it. */
    if (!case_preserving) {
	(void) mklower(strcpy(new_file, old_file));
	return;
    }

    /* Canonicalize a leading drive letter, if any. */
    if (old_file[0] && old_file[1] == ':') {
	new_file[0] = old_file[0];
	new_file[1] = old_file[1];
	if (isLower(new_file[0]))
	    new_file[0] = toUpper(new_file[0]);
	current = old_file + 2;
	sofar = new_file + 2;
    } else {
	current = old_file;
	sofar = new_file;
    }

    /* Skip a leading slash, if any. */
    if (is_slashc(*current)) {
	current++;
	*sofar++ = SLASHC;
    }

    /* Canonicalize each pathname prefix. */
    end = skip_string(old_file);
    while (current < end) {
	next = strchr(current, SLASHC);
	if (!next)
	    next = end;
	len = next - current;
	(void) memcpy(sofar, current, len);
	sofar[len] = EOS;
	hdir = HDIR_CREATE;
	entries = 1;
	rc = DosFindFirst(new_file, &hdir,
			  FILE_DIRECTORY | FILE_READONLY, &fb, sizeof(fb),
			  &entries, FIL_STANDARD);
	if (rc == NO_ERROR) {
	    DosFindClose(hdir);
	    (void) strcpy(sofar, fb.achName);
	    sofar += strlen(sofar);
	} else
	    sofar += len;
	if (next != end)
	    *sofar++ = SLASHC;
	current = next + 1;
    }
    return;
}

#else

#define case_correct_path(old_file, new_file)	/* nothing */

#endif /* !SYS_WINNT */

/* canonicalize a pathname, to eliminate extraneous /./, /../, and ////
	sequences.  only guaranteed to work for absolute pathnames */
static char *
canonpath(char *ss)
{
    char *p, *pp;
    char *s;

    TRACE((T_CALLED "canonpath '%s'\n", str_visible(ss)));
    if ((s = is_appendname(ss)) != NULL) {
	returnString((canonpath(s) != NULL) ? ss : NULL);
    } else {

	s = ss;

	if (!*s)
	    returnString(s);

#if OPT_MSDOS_PATH
	if (!global_g_val(GMDFILENAME_IC))
	    (void) mklower(ss);	/* MS-DOS is case-independent */
	if (is_slashc(*ss))
	    *ss = SLASHC;
	/* pretend the drive designator isn't there */
	if ((s = is_msdos_drive(ss)) == 0)
	    s = ss;
#endif

#if SYS_UNIX && !OPT_VMS_PATH
	(void) home_path(s);
#endif

#if OPT_VMS_PATH
	/*
	 * If the code in 'lengthen_path()', as well as the scattered calls on
	 * 'fgetname()' are correct, the path given to this procedure should
	 * be a fully-resolved VMS pathname.  The logic in filec.c will allow a
	 * unix-style name, so we'll fall-thru if we find one.
	 */
	if (is_vms_pathname(s, -TRUE)) {
	    returnString(mkupper(ss));
	}
#endif

#if SYS_UNIX || OPT_MSDOS_PATH || OPT_VMS_PATH
	if (!is_slashc(*s)) {
	    mlforce("BUG: canonpath '%s'", s);
	    returnString(ss);
	}
	*s = SLASHC;

	/*
	 * If the system supports symbolic links (most UNIX systems do), we
	 * cannot do dead reckoning to resolve the pathname.  We've made this a
	 * user-mode because some systems have problems with NFS timeouts which
	 * can make running vile _slow_.
	 */
#ifdef GMDRESOLVE_LINKS
	if (global_g_val(GMDRESOLVE_LINKS)) {
	    char *leaf;
	    char temp[NFILEN];
#ifdef HAVE_REALPATH
	    char temp2[NFILEN];
	    char temp3[NFILEN];
	    char *real = realpath(s, temp);

	    if (real != NULL) {
		(void) strcpy(s, real);
	    } else if ((leaf = pathleaf(vl_strncpy(temp, s,
						   sizeof(temp)))) != NULL) {
		vl_strncpy(temp2, leaf, sizeof(temp2));
		if (leaf == temp + 1)
		    leaf[0] = EOS;
		else
		    leaf[-1] = EOS;
		if (realpath(temp, temp3) != NULL) {
		    pathcat(s, temp3, temp2);
		}
	    }
#else
	    char *head = resolve_directory(s, &leaf);
	    if (head != 0) {
		if (leaf != 0)
		    (void) strcpy(s, pathcat(temp, head, leaf));
		else
		    (void) strcpy(s, head);
	    }
#endif
	} else
#endif
	{
	    p = pp = s;

	    p++;
	    pp++;		/* leave the leading slash */
#if OPT_UNC_PATH
	    if (is_slashc(*pp)) {
		p++;
		pp++;
	    }
#endif
	    while (*pp) {
		if (is_slashc(*pp)) {
		    pp++;
		    continue;
		}
		if (*pp == '.' && is_slashc(*(pp + 1))) {
		    pp += 2;
		    continue;
		}
		break;
	    }
	    while (*pp) {
		if (is_slashc(*pp)) {
		    while (is_slashc(*(pp + 1)))
			pp++;
		    if (p > s && !is_slashc(*(p - 1)))
			*p++ = SLASHC;
		    if (*(pp + 1) == '.') {
			if (*(pp + 2) == EOS) {
			    /* change "/." at end to "" */
			    *(p - 1) = EOS;	/* and we're done */
			    break;
			}
			if (is_slashc(*(pp + 2))) {
			    pp += 2;
			    continue;
			} else if (*(pp + 2) == '.' && (is_slashc(*(pp + 3))
							|| *(pp + 3) == EOS)) {
			    while (p - 1 > s && is_slashc(*(p - 1)))
				p--;
			    while (p > s && !is_slashc(*(p - 1)))
				p--;
			    if (p == s)
				*p++ = SLASHC;
			    pp += 3;
			    continue;
			}
		    }
		    pp++;
		    continue;
		} else {
		    *p++ = *pp++;
		}
	    }
	    if (p > s && is_slashc(*(p - 1)))
		p--;
	    if (p == s)
		*p++ = SLASHC;
	    *p = EOS;
	}
#endif /* SYS_UNIX || SYS_MSDOS */

#if OPT_VMS_PATH
	if (!is_vms_pathname(ss, -TRUE)) {
	    char *tt = skip_string(ss);

	    /*
	     * If we're not looking at "/" or some other path that ends
	     * with a slash, see if we can match the path to a directory
	     * file.  If so, force a slash on the end so that the unix2vms
	     * conversion will show a directory.
	     */
	    if (tt[-1] != SLASHC) {
		struct stat sb;
#if SYS_VMS
		(void) strcpy(tt, ".DIR");
#else
		(void) mklower(ss);
#endif
		if ((file_stat(ss, &sb) >= 0)
		    && S_ISDIR(sb.st_mode))
		    (void) strcpy(tt, "/");
		else
		    *tt = EOS;
	    }

	    /* FIXME: this is a hack to prevent this function from
	     * returning device-level strings, since (at the moment) I
	     * don't have anything that returns a list of the mounted
	     * devices on a VMS system.
	     */
	    if (!strcmp(ss, "/")) {
		(void) strcpy(ss, current_directory(FALSE));
		if ((tt = strchr(ss, ':')) != 0)
		    (void) strcpy(tt + 1, "[000000]");
		else
		    (void) strcat(ss, ":");
		(void) mkupper(ss);
	    } else {
		unix2vms_path(ss, ss);
	    }
	}
#endif

#ifndef case_correct_path
	if (global_g_val(GMDFILENAME_IC))
	    case_correct_path(ss, ss);
#endif
    }
    returnString(ss);
}

char *
shorten_path(char *path, int keep_cwd)
{
    char temp[NFILEN];
    char *cwd;
    char *ff;
    char *slp;
    char *f;
#if OPT_VMS_PATH
    char *dot;
#else
# if SYS_UNIX || OPT_MSDOS_PATH
    int found;
# endif
#endif

    if (isEmpty(path))
	return path;

    if (isInternalName(path))
	return path;

    if ((f = is_appendname(path)) != NULL)
	return (shorten_path(f, keep_cwd) != NULL) ? path : NULL;

    TRACE(("shorten '%s'\n", path));
#if OPT_VMS_PATH
    /*
     * This assumes that 'path' is in canonical form.
     */
    cwd = current_directory(FALSE);
    ff = path;
    dot = 0;
    TRACE(("current '%s'\n", cwd));

    if ((slp = strchr(cwd, '[')) != 0
	&& (slp == cwd
	    || !strncmp(cwd, path, (size_t) (slp - cwd)))) {	/* same device? */
	ff += (slp - cwd);
	cwd = slp;
	(void) strcpy(temp, "[");	/* hoping for relative-path */
	while (*cwd && *ff) {
	    if (*cwd != *ff) {
		if (*cwd == ']' && *ff == '.') {
		    /* "[.DIRNAME]FILENAME.TYP;1" */
		    ;
		} else if (*cwd == '.' && *ff == ']') {
		    /* "[-]FILENAME.TYP;1" */
		    while (*cwd != EOS) {
			if (*cwd++ == '.')
			    (void) strcat(temp, "-");
		    }
		    (void) strcat(temp, "]");
		    ff++;
		} else if (dot != 0) {
		    int diff = (ff - dot);

		    /* "[-.DIRNAME]FILENAME.TYP;1" */
		    while (*cwd != EOS) {
			if (*cwd++ == '.')
			    (void) strcat(temp, "-");
		    }
		    while (dot != ff) {
			if (*dot++ == '.')
			    (void) strcat(temp, "-");
		    }
		    (void) strcat(temp, ".");
		    ff -= (diff - 1);
		}
		break;
	    } else if (*cwd == ']') {
		(void) strcat(temp, cwd);
		ff++;		/* path-leaf, if any */
		break;
	    }

	    if (*ff == '.')
		dot = ff;
	    cwd++;
	    ff++;
	}
    } else {
	*temp = EOS;		/* different device, cannot relate */
    }

    if (!strcmp(temp, "[]")	/* "[]FILENAME.TYP;1" */
	&&!keep_cwd)
	*temp = EOS;

    (void) strcpy(path, strcat(temp, ff));
#else
# if SYS_UNIX || OPT_MSDOS_PATH
    cwd = current_directory(FALSE);
    slp = ff = path;
    while (*cwd && *ff && *cwd == *ff) {
	if (is_slashc(*ff))
	    slp = ff;
	cwd++;
	ff++;
    }

    /* if we reached the end of cwd, and we're at a path boundary,
       then the file must be under '.' */
    found = FALSE;
    if (*cwd == EOS) {
	if (keep_cwd) {
	    temp[0] = '.';
	    temp[1] = SLASHC;
	    temp[2] = EOS;
	} else
	    *temp = EOS;
	if (is_slashc(*ff)) {
	    found = TRUE;
	    (void) strcpy(path, vl_strncat(temp, ff + 1, sizeof(temp)));
	} else if (slp == ff - 1) {
	    found = TRUE;
	    (void) strcpy(path, vl_strncat(temp, ff, sizeof(temp)));
	}
    }

    if (!found) {
	/* if we mismatched during the first path component, we're done */
	if (slp != path) {
	    /* if we mismatched in the last component of cwd, then the file
	       is under '..' */
	    if (last_slash(cwd) == NULL) {
		(void) strcpy(path,
			      vl_strncat(vl_strncpy(temp, "..", sizeof(temp)),
					 slp,
					 sizeof(temp)));
	    }
	}
    }

    /* we're off by more than just '..', so use absolute path */
# endif	/* SYS_UNIX || SYS_MSDOS */
#endif /* OPT_VMS_PATH */

    TRACE(("     -> '%s' shorten\n", path));
    return path;
}

#if OPT_VMS_PATH
static int
mixed_case(const char *path)
{
    int c;
    int had_upper = FALSE;
    int had_lower = FALSE;
    while ((c = *path++) != EOS) {
	if (isLower(c))
	    had_lower = TRUE;
	if (isUpper(c))
	    had_upper = TRUE;
    }
    return (had_upper && had_lower);
}
#endif

/*
 * Undo nominal effect of 'shorten_path()'
 */
char *
lengthen_path(char *path)
{
#if SYS_VMS
    struct FAB my_fab;
    struct NAM my_nam;
    char my_esa[NAM$C_MAXRSS + 1];	/* expanded: sys$parse */
    char my_rsa[NAM$C_MAXRSS + 1];	/* result: sys$search */
#endif
    int len;
    const char *cwd = NULL;
    char *f;
    char temp[NFILEN];
#if OPT_MSDOS_PATH
    int free_cwd = 0;
    char drive;
#endif

    if ((f = is_appendname(path)) != NULL)
	return (lengthen_path(f) != NULL) ? path : NULL;

    if ((f = path) == NULL
	|| isErrorVal(f))
	return path;

    if (*path != EOS && isInternalName(path)) {
#if OPT_VMS_PATH
	/*
	 * The conflict between VMS pathnames (e.g., "[-]") and Vile's
	 * scratch-buffer names is a little ambiguous.  On VMS, though,
	 * we'll have to give VMS pathnames the edge.  We cheat a little,
	 * by exploiting the fact (?) that the system calls return paths
	 * in uppercase only.
	 */
	if (!is_vms_pathname(path, TRUE) && !mixed_case(path))
#endif
	    return path;
    }
#if SYS_UNIX && !OPT_VMS_PATH
    (void) home_path(f);
#endif

#if SYS_VMS
    /*
     * If the file exists, we can ask VMS to tell the full pathname.
     */
    if ((*path != EOS) && maybe_pathname(path) && is_vms_pathname(path, -TRUE)) {
	int fd;
	long status;
	char temp[NFILEN], leaf[NFILEN];
	char *s;

	if (!strchr(path, '*') && !strchr(path, '?')) {
	    if ((fd = open(SL_TO_BSL(path), O_RDONLY, 0)) >= 0) {
		getname(fd, temp);
		(void) close(fd);
		return strcpy(path, temp);
	    }
	}

	/*
	 * Path either contains a wildcard, or the file does
	 * not already exist.  Use the system parser to expand
	 * the pathname components.
	 */
	my_fab = cc$rms_fab;
	my_fab.fab$l_fop = FAB$M_NAM;
	my_fab.fab$l_nam = &my_nam;	/* FAB => NAM block     */
	my_fab.fab$l_dna = "";	/* Default-selection    */
	my_fab.fab$b_dns = strlen(my_fab.fab$l_dna);

	my_fab.fab$l_fna = path;
	my_fab.fab$b_fns = strlen(path);

	my_nam = cc$rms_nam;
	my_nam.nam$b_ess = NAM$C_MAXRSS;
	my_nam.nam$l_esa = my_esa;
	my_nam.nam$b_rss = NAM$C_MAXRSS;
	my_nam.nam$l_rsa = my_rsa;

	if ((status = sys$parse(&my_fab)) == RMS$_NORMAL) {
	    char *s = my_esa;
	    int len = my_nam.nam$b_esl;
	    s[len] = EOS;
	    if (len > 2) {
		s = pathleaf(s);
		if (!strcmp(s, ".;"))
		    *s = EOS;
	    }
	    return strcpy(path, my_esa);
	} else {
	    /* FIXME: try to expand partial directory specs, etc. */
	}
    }
#else
# if OPT_VMS_PATH
    if ((*path != EOS) && maybe_pathname(path)) {
	/*
	 * getname() returns a full pathname, like realpath().
	 */
	if (realpath(path, temp)) {
	    strcpy(path, temp);
	}
	/* this is only for testing! */
	if (fakevms_filename(path)) {
	    return path;
	}
    }
# endif
#endif

#if SYS_UNIX || OPT_MSDOS_PATH || OPT_VMS_PATH
#if OPT_MSDOS_PATH
    if ((f = is_msdos_drive(path)) != 0) {
	drive = *path;
    } else {
	drive = EOS;
	f = path;
    }
#endif
    if (!is_slashc(f[0])) {
#if OPT_MSDOS_PATH

#if OPT_UNC_PATH
	if (drive == EOS) {
	    W32_CHAR uncdir[NFILEN];
	    GetCurrentDirectory(sizeof(uncdir) / sizeof(uncdir[0]), uncdir);
	    cwd = bsl_to_sl_inplace(asc_charstring(uncdir));
	    free_cwd = TRUE;
	} else
#endif
	    cwd = curr_dir_on_drive(drive != EOS
				    ? drive
				    : curdrive());

	if (!cwd) {
	    /* Drive will be unspecified with UNC Paths */
	    if ((temp[0] = drive) != EOS) {
		(void) strcpy(temp + 1, ":/");
	    }
	    cwd = temp;
	}
#else
	cwd = current_directory(FALSE);
	if (!is_slashc(*cwd))
	    return path;
#endif
#if OPT_VMS_PATH
	vms2unix_path(temp, cwd);
#else
	(void) vl_strncpy(temp, cwd, sizeof(temp));
#endif
	len = (int) strlen(temp);
	temp[len++] = SLASHC;
	(void) vl_strncpy(temp + len, f, sizeof(temp) - (size_t) len);
	(void) vl_strncpy(path, temp, NFILEN);
    }
#if OPT_MSDOS_PATH
    if (is_msdos_drive(path) == 0) {	/* ensure that we have drive too */
	/* UNC paths have no drive */
	if (curdrive() != 0) {
	    temp[0] = (char) curdrive();
	    temp[1] = ':';
	    (void) vl_strncpy(temp + 2, path, sizeof(temp) - 2);
	    (void) vl_strncpy(path, temp, NFILEN);
	}
    }
    if (free_cwd)
	free((char *) cwd);
#endif
#endif /* SYS_UNIX || SYS_MSDOS */

    return canonpath(path);
}

/*
 * Returns true if the argument looks like an absolute pathname (e.g., on
 * unix, begins with a '/').
 */
int
is_abs_pathname(char *path)
{
    int result = FALSE;
    char *f;

    if (path == NULL)
	result = FALSE;
    else if ((f = is_appendname(path)) != NULL)
	result = is_pathname(f);

#if OPT_VMS_PATH
    else if (is_vms_pathname(path, -TRUE)
	     && (strchr(path, L_BLOCK) != 0
		 || strchr(path, ':') != 0))
	result = TRUE;
#endif

#if OPT_MSDOS_PATH
    else if ((f = is_msdos_drive(path)) != 0)
	result = is_abs_pathname(f);
#endif

#if SYS_UNIX || OPT_MSDOS_PATH || SYS_VMS
#if SYS_UNIX
    else if (path[0] == CH_TILDE)
	result = TRUE;
#endif
    else if (is_slashc(path[0]))
	result = TRUE;
#endif /* SYS_UNIX || OPT_MSDOS_PATH || SYS_VMS */

    return result;
}

/*
 * Returns true if the argument looks like a relative pathname (e.g., on
 * unix, begins with "./" or "../")
 */
static int
is_relative_pathname(const char *path)
{
    int n;
#if OPT_VMS_PATH
    if (is_vms_pathname(path, -TRUE)
	&& !strncmp(path, "[-", 2))
	return TRUE;
#endif
#if SYS_UNIX || OPT_MSDOS_PATH || SYS_VMS
    n = 0;
    if (path[n++] == '.') {
	if (path[n] == '.')
	    n++;
	if (is_slashc(path[n]))
	    return TRUE;
    }
#endif /* SYS_UNIX || OPT_MSDOS_PATH || SYS_VMS */

    return FALSE;
}

/*
 * Returns true if the argument looks more like a pathname than anything else.
 *
 * Notes:
 *	This makes a syntax-only test (e.g., at the beginning of the string).
 *	VMS can accept UNIX-style /-delimited pathnames.
 */
int
is_pathname(char *path)
{
    return is_relative_pathname(path)
	|| is_abs_pathname(path);
}

/*
 * A bit weaker than 'is_pathname()', checks to see if the string contains
 * path delimiters.
 */
int
maybe_pathname(char *fn)
{
    if (is_pathname(fn))	/* test the obvious stuff */
	return TRUE;
#if OPT_MSDOS_PATH
    if (is_msdos_drive(fn))
	return TRUE;
#endif
    if (last_slash(fn) != NULL)
	return TRUE;
#if OPT_VMS_PATH
    while (*fn != EOS) {
	if (ispath(*fn) && !isident(*fn))
	    return TRUE;
	fn++;
    }
#endif
    return FALSE;
}

/*
 * Returns the filename portion if the argument is an append-name (and not an
 * internal name!), otherwise null.
 */
char *
is_appendname(char *fn)
{
    if (fn != NULL) {
	if (isAppendToName(fn)) {
	    fn += 2;		/* skip the ">>" prefix */
	    fn = skip_blanks(fn);
	    if (!isInternalName(fn))
		return fn;
	}
    }
    return NULL;
}

/*
 * Returns true if the filename is either a scratch-name, or is the string that
 * we generate for the filename-field of [Help] and [Buffer List].  Use this
 * function rather than simple tests of '[' to make tests for VMS filenames
 * unambiguous.
 */
int
is_internalname(const char *fn)
{
#if OPT_VMS_PATH
    if (is_vms_pathname(fn, FALSE))
	return FALSE;
#endif
    if (!strcmp(fn, non_filename()))
	return TRUE;
    return (*fn == EOS) || is_scratchname(fn);
}

/*
 * Make the simple test only for bracketed name.  We only use this when we're
 * certain it's a buffer name.
 */
int
is_scratchname(const char *fn)
{
    return ((*fn == SCRTCH_LEFT[0]) && (fn[strlen(fn) - 1] == SCRTCH_RIGHT[0]));
}

/*
 * Test if the given path is a directory
 */
int
is_directory(const char *path)
{
#if OPT_VMS_PATH
    char *s;
#endif
    struct stat sb;

    if (isEmpty(path))
	return FALSE;

#if OPT_VMS_PATH
    if (is_vms_pathname(path, TRUE)) {
	return TRUE;
    }

    /* If the name doesn't look like a directory, there's no point in
     * wasting time doing a 'stat()' call.
     */
    s = vms_pathleaf((char *) path);
    if ((s = strchr(s, '.')) != 0) {
	char ftype[NFILEN];
	(void) mkupper(strcpy(ftype, s));
	if (strcmp(ftype, ".DIR")
	    && strcmp(ftype, ".DIR;1"))
	    return FALSE;
    }
#endif
#if OPT_UNC_PATH
    /*
     * WARNING: kludge alert!
     *
     * The problem here is that \\system\share, if it exists,
     * must be a directory.  However, due to a bug in the win32
     * stat function, it may be reported to exist (stat succeeds)
     * but that it is a file, not a directory.  So we special case
     * a stand-alone \\system\share name and force it to be reported
     * as a dir.
     */
    if (is_slashc(path[0]) && is_slashc(path[1])) {
	const char *end = skip_cstring(path);
	int slashes = 0;
	if (end > path && is_slashc(end[-1]))
	    end--;
	while (--end >= path) {
	    if (is_slashc(*end))
		slashes++;
	}
	if (slashes == 3)
	    return 1;
    }
#endif
    return ((file_stat(path, &sb) >= 0) && S_ISDIR(sb.st_mode));
}

static int
is_filemode(struct stat *sb)
{
#pragma warning(suppress: 6287)
    return (S_ISREG(sb->st_mode) || S_ISFIFO(sb->st_mode));
}

/*
 * Test if the given path is a regular file, i.e., one that we might open.
 */
int
is_file(char *path)
{
    struct stat sb;

    return ((file_stat(path, &sb) >= 0) && is_filemode(&sb));
}

/*
 * Test if the given path is not a regular file, i.e., one that we won't open.
 */
int
is_nonfile(char *path)
{
    struct stat sb;

    return ((file_stat(path, &sb) >= 0) && !is_filemode(&sb));
}

/*
 * Parse the next entry in a list of pathnames, returning null only when no
 * more entries can be parsed.
 */
const char *
parse_pathlist(const char *list, char *result, int *first)
{
    TRACE(("parse_pathlist(%s)\n", TRACE_NULL(list)));
    if (list != NULL && *list != EOS) {
	int len = 0;

	if (first && *first && *list == vl_pathchr) {
	    result[len++] = '.';
	} else {
	    if (*list == vl_pathchr)
		++list;
	    while (*list && (*list != vl_pathchr)) {
		if (len < NFILEN - 1)
		    result[len++] = *list;
		list++;
	    }
	    if (len == 0)	/* avoid returning an empty-string */
		result[len++] = '.';
	}
	result[len] = EOS;
    } else {
	list = NULL;
	result[0] = EOS;
    }
    TRACE(("...parse_pathlist(%s) ->'%s'\n", TRACE_NULL(list), result));
    if (first)
	*first = 0;
    return list;
}

#if SYS_WINNT && !CC_TURBO && !CC_MINGW
/********                                               \\  opendir  //
 *                                                        ===========
 * opendir
 *
 * Description:
 *      Prepares to scan the file name entries in a directory.
 *
 * Arguments:   filename in NT format
 *
 * Returns:     pointer to a (malloc-ed) DIR structure.
 *
 * Joseph E. Greer      July 22 1992
 *
 ********/

DIR *
opendir(char *fname)
{
    DIR *od = 0;
    size_t len = strlen(fname);
    char *buf = typeallocn(char, len + 10);

    if (buf != 0) {
	(void) strcpy(buf, fname);

	if (!strcmp(fname, ".")) {
	    /* if it's just a '.', replace with '*.*' */
	    (void) strcpy(fname, "*.*");
	} else {
	    /* If the name ends with a slash, append '*.*' otherwise '\*.*' */
	    if (is_slashc(fname[len - 1]))
		(void) strcat(buf, "*.*");
	    else
		(void) strcat(buf, "\\*.*");
	}

	/* allocate the structure to maintain currency */
	if ((od = typecalloc(DIR)) != NULL) {
	    W32_CHAR *buf2 = w32_charstring(buf);

	    /* Let's try to find a file matching the given name */
	    if ((od->hFindFile = FindFirstFile(buf2, &od->ffd))
		== INVALID_HANDLE_VALUE) {
		FreeAndNull(od);
	    } else {
		od->first = 1;
	    }
	    free(buf2);
	}
    }
    return od;
}

/********                                               \\  readdir  //
 *                                                        ===========
 * readdir
 *
 * Description:
 *      Read a directory entry.
 *
 * Arguments:   a DIR pointer
 *
 * Returns:     A struct direct
 *
 * Joseph E. Greer      July 22 1992
 *
 ********/
DIRENT *
readdir(DIR * dirp)
{
    if (dirp->first)
	dirp->first = 0;
    else if (!FindNextFile(dirp->hFindFile, &dirp->ffd))
	return NULL;
    FreeIfNeeded(dirp->de.d_name);
    dirp->de.d_name = asc_charstring(dirp->ffd.cFileName);
    return &dirp->de;
}

/********                                               \\  closedir  //
 *                                                        ===========
 * closedir
 *
 * Description:
 *      Close a directory entry.
 *
 * Arguments:   a DIR pointer
 *
 * Returns:     A struct direct
 *
 * Joseph E. Greer      July 22 1992
 *
 ********/
int
closedir(DIR * dirp)
{
    FindClose(dirp->hFindFile);
    free(dirp);
    return 0;
}

#endif /* SYS_WINNT */

#if OPT_MSDOS_PATH && !SYS_OS2_EMX
static char *
slconv(const char *f, char *t, char oc, char nc)
{
    char *retp = t;

    if (t != 0) {
	while (*f) {
	    if (*f == oc)
		*t = nc;
	    else
		*t = *f;
	    f++;
	    t++;
	}
	*t = EOS;
    }

    return retp;
}

/*
 * Use this function to filter our internal '/' format pathnames to '\'
 * when invoking system calls (e.g., opendir, chdir).
 */
char *
sl_to_bsl(const char *p)
{
    if (p != 0) {
	static TBUFF *cnv;
	size_t len = strlen(p);
	char *s, *t;

	if (tb_alloc(&cnv, (len | 127) + 1) == 0)
	    return (char *) p;	/* not quite giving up, but close */

	t = tb_values(cnv);
	s = slconv(p, t, '/', '\\');
	if ((s = is_msdos_drive(s)) == 0)
	    s = t;
	/* Trim trailing slash if it's not the first */
	if ((len = strlen(s)) > 1
	    && is_slashc(s[len - 1]))
	    s[--len] = EOS;
	return t;
    }
    return 0;
}

/*
 * Use this function to tidy up and put the path-slashes into internal form.
 */
#ifndef bsl_to_sl_inplace
char *
bsl_to_sl_inplace(char *p)
{
    return slconv(p, p, '\\', '/');
}
#endif

#endif /* OPT_MSDOS_PATH && !SYS_OS2_EMX */

#if OPT_VMS_PATH
/*
 * Locate the version in a VMS filename.  Usually it is the ';', but a '.' also
 * is accepted.  However, a '.' may appear in the directory part, or as the
 * suffix delimiter.
 */
char *
version_of(char *fname)
{
    char *s = 0;
    if (fname != 0) {
	s = strchr(fname, ';');
	if (s == 0) {
	    fname = pathleaf(fname);
	    if ((s = strchr(fname, '.')) == 0
		|| (*++s == EOS)
		|| (s = strchr(fname, '.')) == 0)
		s = skip_string(fname);
	    if (strcmp(s, "*")) {	/* either "*" or a number */
		char *d;
		(void) strtol(s, &d, 10);
		if (*d != EOS)
		    s = skip_string(s);
	    }
	}
    }
    return s;
}

/*
 * Strip the VMS version number, so the resulting path implicitly applies to
 * the current version.
 */
char *
strip_version(char *path)
{
    char *verp = version_of(path);
    if (verp != 0)
	*verp = EOS;
    return path;
}
#endif

/*
 * Check if the given path appears in the path-list
 */
int
find_in_path_list(const char *path_list, char *path)
{
    char temp[NFILEN];
    int found = FALSE;
    char find[NFILEN];
    int first = TRUE;

    vl_strncpy(find, SL_TO_BSL(path), sizeof(find));
    TRACE(("find_in_path_list\n\t%s\n\t%s\n", TRACE_NULL(path_list), find));
    while ((path_list = parse_pathlist(path_list, temp, &first)) != NULL) {
	if (!cs_strcmp(global_g_val(GMDFILENAME_IC), temp, find)) {
	    found = TRUE;
	    break;
	}
    }
    TRACE(("\t-> %d\n", found));
    return found;
}

#if 0
/*
 * Prepend the given path to a path-list
 */
void
prepend_to_path_list(char **path_list, char *path)
{
    char *s, *t;
    size_t need;
    char find[NFILEN];
    struct stat sb;

    vl_strncpy(find, SL_TO_BSL(path), sizeof(find));
    if (*path_list == 0 || **path_list == 0) {
	append_to_path_list(path_list, find);
    } else if (!find_in_path_list(*path_list, find)
#if SYS_UNIX
	       && file_stat(find, &sb) == 0
#endif
	) {
	need = strlen(find) + 2;
	if ((t = *path_list) != 0) {
	    need += strlen(t);
	} else {
	    t = empty_string;
	}
	if ((s = typeallocn(char, need)) != 0) {
	    lsprintf(s, "%s%c%s", find, vl_pathchr, t);
	    if (*path_list != 0)
		free(*path_list);
	    *path_list = s;
	}
    }
    TRACE(("prepend_to_path_list\n\t%s\n\t%s\n", *path_list, find));
}
#endif

/*
 * Append the given path(s) to a path-list
 */
void
append_to_path_list(char **path_list, const char *path)
{
    char *s, *t;
    size_t need;
    char working[NFILEN];
    char find[NFILEN];
    const char *in_work = working;
#if SYS_UNIX
    struct stat sb;
#endif
    int first = TRUE;

    vl_strncpy(working, SL_TO_BSL(path), sizeof(working));
    while ((in_work = parse_pathlist(in_work, find, &first)) != NULL) {
	if (!find_in_path_list(*path_list, find)
#if SYS_UNIX
	    && file_stat(find, &sb) == 0
#endif
	    ) {
	    need = strlen(find) + 2;
	    if ((t = *path_list) != NULL) {
		need += strlen(t);
	    } else {
		t = empty_string;
	    }
	    if ((s = typeallocn(char, need)) != NULL) {
		if (*t != EOS)
		    lsprintf(s, "%s%c%s", t, vl_pathchr, find);
		else
		    strcpy(s, find);
		if (*path_list != NULL)
		    free(*path_list);
		*path_list = s;
	    }
	}
    }
    TRACE(("...append_to_path_list\n\t%s\n", TRACE_NULL(*path_list)));
}

/*
 * FUNCTION
 *   path_trunc(char *path,
 *              int  max_path_len,
 *              char *trunc_buf,
 *              int  trunc_buf_len)
 *
 *   path          - an arbitrarily long file path
 *
 *   max_path_len  - maximum acceptable output path length.  Must be >= 5.
 *
 *   trunc_buf     - format the possibly truncated path in this buffer.
 *
 *   trunc_buf_len - sizeof(trunc_buf).  Must be >= 5.
 *
 * DESCRIPTION
 *   Shorten a path from _the left_ to fit within the bounds of
 *   both max_output_len and trunc_buf_len.  This function is useful when
 *   displaying path names in the message line.
 *
 * RETURNS
 *   trunc_buf
 */

char *
path_trunc(char *path, int max_path_len, char *trunc_buf, int trunc_buf_len)
{
    int max_len, path_len;

    if (trunc_buf_len < 5 || max_path_len < 5)
	return (path);		/* nuh-uh */

    max_len = max_path_len;
    if (trunc_buf_len - 1 < max_len)	/* "- 1" -> terminating NUL */
	max_len = trunc_buf_len - 1;
    path_len = (int) strlen(path);
    if (path_len <= max_len)
	strcpy(trunc_buf, path);	/* fits -- no issues */
    else {
#if SYS_VMS
	strcpy(trunc_buf, ">>>");
	max_len -= sizeof(">>>") - 1;
#else
	strcpy(trunc_buf, "...");
	max_len -= (int) sizeof("...") - 1;
#endif
	strcat(trunc_buf, path + path_len - max_len);
    }
    return (trunc_buf);
}

#ifdef HAVE_PUTENV
/*
 * Put the libdir in our path so we do not have to install the filters in the
 * regular $PATH.  If we can do this right after forking, it will not affect
 * the path for subshells invoked via ":sh".
 */
void
append_libdir_to_path(void)
{
    char *env, *tmp;
    const char *cp;
    char buf[NFILEN];

    if (libdir_path != NULL
	&& (tmp = getenv("PATH")) != NULL
	&& (env = strmalloc(tmp)) != NULL) {
	int first = TRUE;

	cp = libdir_path;
	while ((cp = parse_pathlist(cp, buf, &first)) != NULL) {
	    append_to_path_list(&env, buf);
	}
	if (strcmp(tmp, env)) {
	    if ((tmp = typeallocn(char, 6 + strlen(env))) != NULL) {
		lsprintf(tmp, "PATH=%s", env);
		putenv(tmp);
		TRACE(("putenv %s\n", tmp));
	    }
	}
	free(env);
    }
}

#else
void
append_libdir_to_path(void)
{
}
#endif

#if NO_LEAKS
void
path_leaks(void)
{
#if SYS_UNIX
    while (user_paths != NULL) {
	UPATH *paths = user_paths;
	user_paths = paths->next;
	free(paths->name);
	free(paths->path);
	free(paths);
    }
#endif
}
#endif /* NO_LEAKS */
