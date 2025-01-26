/*
 *	glob.c
 *
 * Performs wildcard-expansion for UNIX, VMS and MS-DOS systems.
 * Written by T.E.Dickey for vile (april 1993).
 *
 * (MS-DOS code was originally taken from the winc app example of the
 * zortech compiler - pjr)
 *
 * To do:
 *	make the wildcard expansion know about escaped characters (e.g.,
 *	with backslash a la UNIX.
 *
 *	modify (ifdef-style) 'expand_leaf()' to allow ellipsis.
 *
 * $Id: glob.c,v 1.106 2025/01/26 14:13:25 tom Exp $
 *
 */

#include "estruct.h"		/* global structures and defines */
#include "edef.h"		/* defines 'slash' */

#if SYS_OS2
# define INCL_DOSFILEMGR
# define INCL_ERRORS
# include <os2.h>
#else
# include "dirstuff.h"		/* directory-scanning interface & definitions */
#endif

#define BAKTIK '`'		/* used in UNIX shell for pipe */
#define	isname(c)	(isAlnum(c) || ((c) == '_'))
#define	isdelim(c)	((c) == '(' || ((c) == '{'))

#if SYS_MSDOS || SYS_OS2 || SYS_WINNT
# if UNIX_GLOBBING
#  define DirEntryStr(p)		p->d_name
# else
#  ifdef __ZTC__
#   define DeclareFind(p)		struct FIND *p
#   define DirEntryStr(p)		p->name
#   define DirFindFirst(path,p)		(p = findfirst(path, 0))
#   define DirFindNext(p)		(p = findnext())
#  else
#   define DeclareFind(p)		struct find_t p
#   define DirEntryStr(p)		p.name
#   define DirFindFirst(path,p)		(!_dos_findfirst(path, 0, &p))
#   define DirFindNext(p)		(!_dos_findnext(&p))
#  endif
# endif
# define DirEntryLen(p)			strlen(DirEntryStr(p))
#endif /* SYS_MSDOS */

/*
 * Verify that we don't have both boolean- and string-valued 'glob' modes.
 */
#if defined(GMDGLOB) && defined(GVAL_GLOB)
huh ? ?
#else
# ifdef GMDGLOB			/* boolean */
#  define globbing_active() global_g_val(GMDGLOB)
# endif
# ifdef GVAL_GLOB		/* string */
#  define globbing_active() !is_falsem(global_g_val_ptr(GVAL_GLOB))
# endif
# ifndef globbing_active
#  define globbing_active() TRUE
# endif
#endif

/*
 * Some things simply don't work on VMS: pipes and $variables
 */
#if OPT_VMS_PATH
#
# undef  UNIX_GLOBBING
# define UNIX_GLOBBING 0
#
# undef  OPT_GLOB_ENVIRON
# define OPT_GLOB_ENVIRON 0
#
# undef  OPT_GLOB_PIPE
# define OPT_GLOB_PIPE 0
#
#endif

/*--------------------------------------------------------------------------*/

/* the expanded list is defined outside of the functions because if we
 * handle ellipsis, the generating function must be recursive.
 */
static size_t myMax = 0;	/* length and index of the expanded list */
static size_t myLen = 0;	/* length and index of the expanded list */
static char **myVec = NULL;	/* the expanded list */

/*--------------------------------------------------------------------------*/
#if UNIX_GLOBBING
static void
strip_escapes(char *buffer)
{
    int j, k, ch;

    for (j = k = 0; (ch = buffer[j]) != EOS; ++j) {
	if (ch == BACKSLASH)
	    ++j;
	if ((buffer[k++] = buffer[j]) == EOS)
	    break;
    }
    buffer[k] = EOS;
}
#endif

/*
 * Use this function to decide if we should perform wildcard expansion.
 */
static int
string_has_globs(const char *item)
{
#if OPT_VMS_PATH || SYS_UNIX || OPT_MSDOS_PATH
    while (*item != EOS) {
	if (*item == GLOB_SINGLE || *item == GLOB_MULTI)
	    return TRUE;
#if OPT_GLOB_ELLIPSIS || SYS_VMS
	if (vl_glob_opts & vl_GLOB_ELLIPSIS) {
	    if (!strncmp(item, GLOB_ELLIPSIS, sizeof(GLOB_ELLIPSIS) - 1))
		return TRUE;
	}
#endif
#if !OPT_VMS_PATH
#if OPT_GLOB_RANGE
	if (vl_glob_opts & vl_GLOB_RANGE) {
	    if (*item == GLOB_RANGE[0])
		return TRUE;
	}
#endif
#endif
	item++;
    }
#endif
    return FALSE;
}

/*
 * Use this function to decide if we should do wildcard/tilde/variable
 * expansion.
 */
int
string_has_wildcards(const char *item)
{
    const char *base = item;

#if OPT_VMS_PATH || SYS_WINNT	/* either host can support ~/whatever */
    if (*item == CH_TILDE)
	return TRUE;
#endif
#if OPT_VMS_PATH || SYS_UNIX || OPT_MSDOS_PATH
    while (*item != EOS) {
#if UNIX_GLOBBING
	if (iswild(*item))
	    return TRUE;
#endif
#if !OPT_VMS_PATH
#if OPT_GLOB_ENVIRON
	if ((vl_glob_opts & vl_GLOB_ENVIRON)
	    && (*item == GLOB_ENVIRON[0])
	    && (item == base || !isEscaped(item))
	    && (isname(item[1]) || isdelim(item[1]))) {
	    return TRUE;
	}
#endif
#endif
	item++;
    }
#endif
    return string_has_globs(base);
}

/*
 * Record a pattern-match in 'myVec[]', returning false if an error occurs
 */
static int
record_a_match(char *item)
{
    int result = TRUE;
    char **myTmp;

    beginDisplay();
    if (item != NULL && *item != EOS) {
	if ((item = strmalloc(item)) == NULL) {
	    result = no_memory("glob-match");
	} else {
	    if (myLen + 2 >= myMax) {
		myMax = myLen + 2;
		if (myVec == NULL) {
		    myVec = typeallocn(char *, myMax);
		} else {
		    myTmp = typereallocn(char *, myVec, myMax);
		    if (myTmp == NULL) {
			myVec = glob_free(myVec);
		    } else {
			myVec = myTmp;
		    }
		}
	    }
	    if (myVec == NULL) {
		free(item);
		result = no_memory("glob-pointers");
	    } else {
		myVec[myLen++] = item;
		myVec[myLen] = NULL;
	    }
	}
    }
    endofDisplay();
    return result;
}

#if !SMALLER || UNIX_GLOBBING

static int
cs_char(int ch)
{
    return (global_g_val(GMDFILENAME_IC)
	    ? (isUpper(ch)
	       ? toLower(ch)
	       : ch)
	    : CharOf(ch));
}

static int
only_multi(char *pattern)
{
    int result = TRUE;
    while (*pattern != 0) {
	if (*pattern++ != GLOB_MULTI) {
	    result = FALSE;
	    break;
	}
    }
    return result;
}

/*
 * This is the heart of the wildcard matching.  We are given a directory
 * leaf and a pointer to the leaf that contains wildcards that we must
 * match against the leaf.
 */
int
glob_match_leaf(char *leaf, char *pattern)
{
    while (*leaf != EOS && *pattern != EOS) {
	if (*pattern == GLOB_SINGLE) {
	    leaf++;
	    pattern++;
	} else if (*pattern == GLOB_MULTI) {
	    int multi = FALSE;
	    pattern++;
	    while (*leaf != EOS) {
		if (glob_match_leaf(leaf, pattern)) {
		    multi = TRUE;
		    break;
		}
		leaf++;
	    }
	    if (!multi && *leaf != EOS)
		return FALSE;
#if OPT_GLOB_RANGE
	} else if ((vl_glob_opts & vl_GLOB_RANGE)
		   && (*pattern == GLOB_RANGE[0])) {
	    int found = FALSE;
	    char *first = ++pattern;
	    int negate = 0;

	    if (*first == GLOB_NEGATE[0] ||
		(GLOB_NEGATE[1] && *first == GLOB_NEGATE[1])) {
		negate = 1;
		first = pattern++;
	    }
	    while (*pattern != EOS) {
		if (*pattern == GLOB_RANGE[1]) {
		    pattern++;
		    break;
		}
		if (*pattern == '-' && pattern != first) {
		    int lo = CharOf(pattern[-1]);
		    int hi = CharOf(pattern[1]);
		    if (hi == GLOB_RANGE[1])
			hi = '~';	/* last-printable ASCII */
		    if (((cs_char(lo) <= cs_char(*leaf))
			 && (cs_char(*leaf) <= cs_char(hi))) != negate)
			found = TRUE;
		    pattern++;
		} else if ((cs_char(*pattern++) == cs_char(*leaf)) != negate)
		    found = TRUE;
	    }
	    if (!found)
		return FALSE;
	    leaf++;
#endif
	} else if (cs_char(*pattern++) != cs_char(*leaf++))
	    return FALSE;
    }
    return (*leaf == EOS && only_multi(pattern));
}
#endif /* !SMALLER || UNIX_GLOBBING */

#if UNIX_GLOBBING
/*
 * Point to the leaf following the given string (i.e., skip a slash), returns
 * null if none is found.
 */
static char *
next_leaf(char *path)
{
    if (path != NULL) {
	while (*path != EOS) {
	    if (is_slashc(*path))
		return path + 1;
	    path++;
	}
    }
    return NULL;
}

/*
 * Point to the beginning (after slash, if present) of the first leaf in
 * the given pattern argument that contains a wildcard.
 */
static char *
wild_leaf(char *pattern)
{
    int j, k, ok;
    char c;

    /* skip leading slashes */
    for (j = 0; pattern[j] != EOS && is_slashc(pattern[j]); j++) ;

    /* skip to the leaf with wildcards */
    while (pattern[j] != EOS) {
	int skip = FALSE;
	for (k = j + 1; (c = pattern[k]) != EOS; k++) {
	    if (is_slashc(c)) {
		pattern[k] = EOS;
		ok = string_has_globs(pattern + j);
		pattern[k] = c;
		if (ok)
		    return pattern + j;
		skip = TRUE;	/* skip this leaf */
		break;
	    }
	}
	if (skip) {
	    j = k + 1;		/* point past slash */
	} else {
	    break;
	}
    }
    return string_has_globs(pattern + j) ? pattern + j : NULL;
}

#if !SYS_OS2
/*
 * Recursive procedure that allows any leaf (or all!) leaves in a path to
 * have wildcards.  Except for an ellipsis, each wildcard is completed
 * within a single leaf.
 *
 * Returns false if we ran out of memory (a problem on ms-dos), and true
 * if everything went well (e.g., matches).
 */
static int
expand_leaf(char *path,		/* built-up pathname, top-level */
	    char *pattern)
{
    DIR *dp;
    DIRENT *de;
    int result = TRUE;
    char save = 0;		/* warning suppression */
    size_t len;
    char *leaf;
    char *wild = wild_leaf(pattern);
    char *next = next_leaf(wild);
    char *s;

    /* Fill-in 'path[]' with the non-wild leaves that we skipped to get
     * to 'wild'.
     */
    if (wild == pattern) {	/* top-level, first leaf is wild */
	if (*path == EOS)
	    (void) strcpy(path, ".");
	leaf = skip_string(path) + 1;
    } else {
	len = (size_t) (wild - pattern) - 1;
	if (*(s = path) != EOS) {
	    s += strlen(s);
	    *s++ = SLASHC;
	}
	if (len != 0) {
	    (void) strncpy(s, pattern, len);
	    s[len] = EOS;
	}
	if (*path == EOS) {
	    path[0] = SLASHC;
	    path[1] = EOS;
	    leaf = path + 1;
	}
#if OPT_MSDOS_PATH
	/* Force the strncpy from 'pattern' to pick up a slash just
	 * after the ':' in a drive specification.
	 */
	else if ((s = is_msdos_drive(path)) != 0 && s[0] == EOS) {
	    s[0] = SLASHC;
	    s[1] = EOS;
	    leaf = s + 1;
	}
#endif
	else {
	    leaf = skip_string(path) + 1;
	}
    }

    if (next != NULL) {
	save = next[-1];
	next[-1] = EOS;		/* restrict 'wild[]' to one leaf */
    }

    /* Scan the directory, looking for leaves that match the pattern.
     */
    if ((dp = opendir(SL_TO_BSL(path))) != NULL) {
	leaf[-1] = SLASHC;	/* connect the path to the leaf */
	while ((de = readdir(dp)) != NULL) {
#if OPT_MSDOS_PATH
	    (void) strcpy(leaf, de->d_name);
	    if (!global_g_val(GMDFILENAME_IC))
		(void) mklower(leaf);
	    if (strchr(pattern, '.') && !strchr(leaf, '.'))
		(void) strcat(leaf, ".");
#else
#if USE_D_NAMLEN
	    len = de->d_namlen;
	    (void) strncpy(leaf, de->d_name, len);
	    leaf[len] = EOS;
#else
	    (void) strcpy(leaf, de->d_name);
#endif
#endif
	    if (!strcmp(leaf, ".")
		|| !strcmp(leaf, ".."))
		continue;
	    if (!glob_match_leaf(leaf, wild))
		continue;
	    if (next != NULL) {	/* there are more leaves */
		if (!string_has_globs(next)) {
		    s = skip_string(leaf);
		    *s++ = SLASHC;
		    (void) strcpy(s, next);
		    if (ffexists(path)
			&& !record_a_match(path)) {
			result = FALSE;
			break;
		    }
		} else if (is_directory(path)) {
#if SYS_MSDOS
		    s = strrchr(path, '.');
		    if (s[1] == EOS)
			s[0] = EOS;
#endif
		    if (!expand_leaf(path, next)) {
			result = FALSE;
			break;
		    }
		}
	    } else if (!record_a_match(path)) {
		result = FALSE;
		break;
	    }
	}
	(void) closedir(dp);
    } else {
	result = SORTOFTRUE;	/* at least we didn't run out of memory */
    }

    if (next != NULL)
	next[-1] = save;

    return result;
}

#else /* SYS_OS2 */

/*
 * Recursive procedure that allows any leaf (or all!) leaves in a path to
 * have wildcards.  Except for an ellipsis, each wildcard is completed
 * within a single leaf.
 *
 * Returns false if we ran out of memory (a problem on ms-dos), and true
 * if everything went well (e.g., matches).
 */
static int
expand_leaf(char *path,		/* built-up pathname, top-level */
	    char *pattern)
{
    FILEFINDBUF3 fb;
    ULONG entries;
    HDIR hdir;
    APIRET rc;

    int result = TRUE;
    char save = 0;		/* warning suppression */
    size_t len;
    char *leaf;
    char *wild = wild_leaf(pattern);
    char *next = next_leaf(wild);
    char *s;

    /* Fill-in 'path[]' with the non-wild leaves that we skipped to get
     * to 'wild'.
     */
    if (wild == pattern) {	/* top-level, first leaf is wild */
	if (*path == EOS)
	    (void) strcpy(path, ".");
    } else {
	len = wild - pattern - 1;
	if (*(s = path) != EOS) {
	    s += strlen(s);
	    *s++ = SLASHC;
	}
	if (len != 0)
	    strncpy(s, pattern, len)[len] = EOS;
    }
    leaf = skip_string(path) + 1;

    if (next != 0) {
	save = next[-1];
	next[-1] = EOS;		/* restrict 'wild[]' to one leaf */
    }

    /* Scan the directory, looking for leaves that match the pattern.
     */
    len = strlen(path);
    if (!is_slashc(path[len - 1]))
	(void) strcat(path, "/*.*");
    else
	(void) strcat(path, "*.*");

    hdir = HDIR_CREATE;
    entries = 1;
    rc = DosFindFirst(SL_TO_BSL(path), &hdir,
		      FILE_DIRECTORY | FILE_READONLY,
		      &fb, sizeof(fb), &entries, FIL_STANDARD);
    if (rc == NO_ERROR) {
	leaf[-1] = SLASHC;

	do {
	    (void) mklower(strcpy(leaf, fb.achName));

	    if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0)
		continue;
	    if (!glob_match_leaf(leaf, wild))
		continue;

	    if (next != 0) {	/* there are more leaves */
		if (!string_has_globs(next)) {
		    s = skip_string(leaf);
		    *s++ = SLASHC;
		    (void) strcpy(s, next);
		    if (!record_a_match(path)) {
			result = FALSE;
			break;
		    }
		} else if (is_directory(path)) {
		    s = strrchr(path, '.');
		    if (s[1] == EOS)
			s[0] = EOS;
		    if (!expand_leaf(path, next)) {
			result = FALSE;
			break;
		    }
		}
	    } else if (!record_a_match(path)) {
		result = FALSE;
		break;
	    }

	} while (entries = 1,
		 DosFindNext(hdir, &fb, sizeof(fb), &entries) == NO_ERROR
		 && entries == 1);

	DosFindClose(hdir);
    } else {
	result = SORTOFTRUE;	/* at least we didn't run out of memory */
    }

    if (next != 0)
	next[-1] = save;

    return result;
}
#endif /* SYS_OS2 */

static int
compar(const void *a, const void *b)
{
    return cs_strcmp(global_g_val(GMDFILENAME_IC),
		     *(const char *const *) a,
		     *(const char *const *) b);
}
#endif /* UNIX_GLOBBING */

#if OPT_GLOB_PIPE
static int
glob_from_pipe(const char *pattern)
{
    static char only_echo[] = "!echo %s";
#ifdef GVAL_GLOB
    char *cmd = global_g_val_ptr(GVAL_GLOB);
    int single;
#else
    char *cmd = only_echo;
    static int single = TRUE;
#endif
    FILE *cf;
    char tmp[NFILEN];
    int result = FALSE;
    size_t len;
    char *s, *d;

#ifdef GVAL_GLOB

    /*
     * For now, assume that only 'echo' will supply the result all on one
     * line.  Other programs (e.g., 'ls' and 'find' do the sensible thing
     * and break up the output with newlines.
     */
    if (!isShellOrPipe(cmd)) {
	cmd = only_echo;
	single = TRUE;
	d = cmd + 1;
    } else {
	char save = EOS;
	for (d = cmd + 1; *d != EOS && isSpace(*d); d++) ;
	for (s = d; *s != EOS; s++) {
	    if (isSpace(*s)) {
		save = *s;
		*s = EOS;
		break;
	    }
	}
	single = !strcmp(pathleaf(d), "echo");
	if (save != EOS)
	    *s = save;
    }
#else
    d = cmd + 1;
#endif

    (void) lsprintf(tmp, d, pattern);
    if ((cf = npopen(tmp, "r")) != NULL) {
	char old[NFILEN + 1];

	*(d = old) = EOS;

	while ((len = fread(tmp, sizeof(*tmp), sizeof(tmp), cf)) > 0) {
	    /*
	     * Split the buffer up.  If 'single', split on all
	     * whitespace, otherwise only on newlines.
	     */
	    for (s = tmp; (size_t) (s - tmp) < len; s++) {
		if ((single && isSpace(*s))
		    || (!single && (*s == '\n' || *s == EOS))) {
		    *d = EOS;
		    result = record_a_match(d = old);
		    *d = EOS;
		    if (!result)
			break;
		} else {
		    *d++ = *s;
		}
	    }
	}
	if (*old != EOS) {
	    result = record_a_match(old);
	}
	npclose(cf);
    } else
	result = FALSE;

    return result;
}
#endif

#if OPT_GLOB_ENVIRON && UNIX_GLOBBING
/*
 * Expand environment variables in 'pattern[]'
 * It allows names of the form
 *
 *	$NAME
 *	$(NAME)
 *	${NAME}
 *
 * but ignores
 *	NAME$
 *	\$NAME
 */
static void
expand_environ(char *pattern)
{
    int j, k;
    int delim, left, right;
    const char *s;
    char save[NFILEN];

    if (vl_glob_opts & vl_GLOB_ENVIRON) {
	for (j = 0; pattern[j] != EOS; j++) {
	    if (j != 0 && isEscaped(pattern + j))
		continue;

	    k = j + 1;
	    if (pattern[j] == GLOB_ENVIRON[0] && pattern[k] != EOS) {
		if (pattern[k] == '(')
		    delim = ')';
		else if (pattern[k] == '{')
		    delim = '}';
		else if (isAlnum(pattern[k]))
		    delim = EOS;
		else
		    continue;

		if (delim != EOS)
		    k++;
		left =
		    right = k;

		while (pattern[k] != EOS) {
		    right = k;
		    if (delim != EOS) {
			if (pattern[k++] == delim)
			    break;
		    } else if (isname(pattern[k])) {
			k++;
		    } else {
			break;
		    }
		}
		if (delim == EOS)
		    right = k;

		(void) vl_strncpy(save, pattern + k, sizeof(save));
		if (right != left) {
		    pattern[right] = EOS;
#if SYS_MSDOS || SYS_OS2
		    mkupper(pattern + left);
#endif
		    if ((s = getenv(pattern + left)) == NULL)
			s = "";
		} else {
		    s = "";
		}

		if ((int) (strlen(pattern) + strlen(s) + 2) < NFILEN) {
		    (void) strcpy(pattern + j, s);
		    (void) strcat(pattern, save);
		    j += (int) strlen(s) - 1;
		} else {
		    /* give up - substitution does not fit */
		    break;
		}
	    }
	}
    }
}

#else
#define	expand_environ(pattern)
#endif

/*
 * Notes:
 *	VMS's sys$search function (embedded in our fake 'readdir()') handles
 *	all of the VMS wildcards.
 *
 *	MS-DOS has non-UNIX functions that scan a directory and recognize DOS-style
 *	wildcards.  Use these to get the smallest executable.  However, DOS-
 *	style wildcards are crude and clumsy compared to UNIX, so we provide them as
 *	an option.  (For example, the DOS-wildcards won't match "..\*\*.bak").
 */
static int
expand_pattern(char *item)
{
    int result = FALSE;
#if OPT_VMS_PATH
    DIR *dp;
    DIRENT *de;
    char actual[NFILEN];
    char *leaf;

    strcpy(actual, item);
    /*
     * If we're given a Unix-style pathname, split off the leaf (which may
     * contain wildcards), translate the directory to VMS form, and put the
     * leaf back.  The unix2vms_path function would otherwise mangle the
     * wildcards in the leaf.  We only handle wildcards in the leaf
     * filename, since native VMS syntax for wildcards in the directory is
     * too cumbersome to use.
     */
    if (!is_vms_pathname(actual, -TRUE)) {
	if ((leaf = pathleaf(actual)) != actual)
	    *leaf = EOS;
	unix2vms_path(actual, actual);
	if (leaf != actual)
	    pathcat(actual, actual, pathleaf(item));
    }

    if ((dp = opendir(actual)) != 0) {
	result = TRUE;
	while ((de = readdir(dp)) != 0) {
	    char temp[NFILEN];
#if USE_D_NAMLEN
	    size_t len = de->d_namlen;
	    (void) strncpy(temp, de->d_name, len);
	    temp[len] = EOS;
#else
	    (void) strcpy(temp, de->d_name);
#endif
	    if (!record_a_match(temp)) {
		result = FALSE;
		break;
	    }
	}
	(void) closedir(dp);
    }
#else /* UNIX or MSDOS, etc. */

    (void) item;

#if OPT_GLOB_PIPE
# ifdef GVAL_GLOB
    /*
     * The 'glob' mode value can be on/off or set to a pipe expression,
     * e.g., "!echo %s".  This allows using the shell to expand the
     * pattern, which is slower than vile's internal code, but may allow
     * using specific features to which the user is accustomed.
     *
     * As a special case, we read from a pipe if the expression begins with
     * a back-tick (e.g., `which script`).
     */
    if (isShellOrPipe(global_g_val_ptr(GVAL_GLOB))
	|| *item == BAKTIK) {
	result = glob_from_pipe(item);
    } else
# else
    result = glob_from_pipe(item);
#  if UNIX_GLOBBING
    huh ? ?			/* thought I turned that off ... */
#  endif
# endif
#endif
#if UNIX_GLOBBING
    {
	char builtup[NFILEN];
	char pattern[NFILEN];
	size_t first = myLen;

	(void) vl_strncpy(pattern, item, sizeof(pattern));
	*builtup = EOS;
#if OPT_MSDOS_PATH
	if (!global_g_val(GMDFILENAME_IC))
	    (void) mklower(pattern);
#endif
	expand_environ(pattern);
	if (string_has_globs(pattern)) {
	    /*
	     * FIXME - stripping escapes is done too early, in case we have
	     * mixed [pattern] and \[xxx\] token.
	     */
	    strip_escapes(pattern);
	    if ((result = expand_leaf(builtup, pattern)) != FALSE
		&& (myLen - first > 1)) {
		qsort((char *) &myVec[first], myLen - first,
		      sizeof(*myVec), compar);
	    }
	} else {
	    strip_escapes(pattern);
	    result = record_a_match(pattern);
	}
    }
#endif /* UNIX-style globbing */
#if (SYS_MSDOS || SYS_OS2 || SYS_WINNT) && !UNIX_GLOBBING
    /* native DOS-wildcards */
    DeclareFind(p);
    char temp[FILENAME_MAX + 1];
    char path[FILENAME_MAX + 1];
    char *cp = pathleaf(strcpy(temp, strcpy(path, item)));

    if (DirFindFirst(path, p)) {
	result = TRUE;
	do {
	    (void) strcpy(cp, DirEntryStr(p));
	    if (!record_a_match(temp)) {
		result = FALSE;
		break;
	    }
	} while (DirFindNext(p));
    }
#endif /* native MS-DOS globbing */
#endif /* OPT_VMS_PATH */
    return result;		/* true iff no errors noticed */
}

/*--------------------------------------------------------------------------*/

/*
 * Tests a list of items to see if at least one of them needs to be globbed.
 */
#if !SYS_UNIX
int
glob_needed(char **list_of_items)
{
    int n;

    for (n = 0; list_of_items[n] != 0; n++)
	if (string_has_wildcards(list_of_items[n]))
	    return TRUE;
    return FALSE;
}
#endif

/*
 * Expands the items in a list, returning an entirely new list (so it can be
 * freed without knowing where it came from originally).  This should only
 * return 0 if we run out of memory.
 */
static char **
glob_expand(char **list_of_items)
{
    int len = glob_length(list_of_items);
    int i;

    myMax = 0;
    myLen = 0;
    myVec = NULL;

    for (i = 0; i < len; ++i) {
	char *item = list_of_items[i];
	/*
	 * Expand '~' expressions in case we've got a pattern like
	 * "~/test*.log".
	 */
#if !SMALLER
	char temp[NFILEN];
	item = home_path(vl_strncpy(temp, item, sizeof(temp)));
#endif
	if (!isInternalName(item)
	    && globbing_active()
	    && string_has_wildcards(item)) {
	    if (!expand_pattern(item))
		return NULL;
	} else if (!record_a_match(item)) {
	    return NULL;
	}
    }
    return myVec;
}

/*
 * A special case of 'glob_expand()', expands a single string into a list.
 */
char **
glob_string(char *item)
{
    char *vec[2];

    vec[0] = item;
    vec[1] = NULL;

    return glob_expand(vec);
}

/*
 * Counts the number of items in a list of strings.  This is simpler (and
 * more useful) than returning the length and the list as arguments from
 * a procedure.  Note that since the standard argc/argv convention puts a
 * null pointer on the end, this function is applicable to the 'argv[]'
 * parameter of the main program as well.
 */
int
glob_length(char **list_of_items)
{
    int len;
    if (list_of_items != NULL) {
	for (len = 0; list_of_items[len] != NULL; len++) ;
    } else
	len = 0;
    return len;
}

/*
 * Frees the strings in a list, and the list itself.  Note that this should
 * not be used for the main program's original argv, because on some systems
 * it is a part of a larger data area, as are the command strings.
 */
char **
glob_free(char **list_of_items)
{
    int len;
    beginDisplay();
    if (list_of_items != NULL) {
	for (len = 0; list_of_items[len] != NULL; len++)
	    free(list_of_items[len]);
	free(list_of_items);
    }
    endofDisplay();
    return NULL;
}

#if !SYS_UNIX
/*
 * Expand wildcards for the main program a la UNIX shell.
 */
void
expand_wild_args(int *argcp, char ***argvp)
{
#if SYS_VMS
    int j, k;
    int comma = 0;
    int option = 0;
    /*
     * VMS command-line arguments may be a comma-separated list of
     * filenames, with an optional "/read_only" flag appended.
     */
    for (j = 1; j < *argcp; j++) {
	char *s = (*argvp)[j];
	int c;
	while ((c = *s++) != EOS) {
	    if (c == ',')
		comma++;
	    if (c == '/')
		option++;
	}
    }
    if (comma || option) {
	char **newvec = typeallocn(char *, comma + *argcp + 1);

	if (newvec == 0) {
	    no_memory("expand_wild_args");
	    return;
	}

	for (j = k = 0; j < *argcp; j++) {
	    char *the_arg = strmalloc((*argvp)[j]);
	    char *item, *tmp;

	    if (the_arg == 0)
		break;

	    /*
	     * strip off supported VMS options, else don't muck
	     * with pseudo-unix path delimiters so that the
	     * glob routines will correctly handle filenames
	     * like these:
	     *
	     *     ../readme.txt   ~/vile.rc
	     *
	     * however don't mess with command line arguments
	     * that look like this:
	     *
	     *    +/<string>
	     */
	    if (!(the_arg[0] == '+' && the_arg[1] == '/')) {
		tmp = the_arg;
		while ((item = strrchr(tmp, '/')) != 0) {
		    mkupper(item);
		    if (!strncmp(item,
				 "/READ_ONLY",
				 strlen(item))) {
			set_global_b_val(MDVIEW, TRUE);
			*item = EOS;
		    } else
			tmp = item + 1;
		}
	    }
	    while (*the_arg != EOS) {
		item = strchr(the_arg, ',');
		if (item == 0)
		    item = skip_string(the_arg);
		else
		    *item++ = EOS;
		newvec[k++] = the_arg;
		the_arg = item;
	    }
	}
	newvec[k] = 0;
	*argcp = k;
	*argvp = newvec;
    }
#endif
    if (glob_needed(*argvp)) {
	char **newargs;
	int n;

	for (n = 0; n < *argcp; ++n) {
	    char *test = add_backslashes2((*argvp)[n], "\\$");
	    if (test != (*argvp)[n]) {
		(*argvp)[n] = strmalloc(test);
	    }
	}
	newargs = glob_expand(*argvp);
	if (newargs != 0) {
	    *argvp = newargs;
	    *argcp = glob_length(newargs);
	}
    }
}
#endif

/*
 * Expand a string, permitting only one match.
 */
int
doglob(char *path)
{
    char **expand = glob_string(path);
    int len = glob_length(expand);

    if (len > 1) {
	if (mlyesno("Too many filenames.  Use first") != TRUE) {
	    (void) glob_free(expand);
	    return FALSE;
	}
    }
    if (len > 0) {
	(void) strcpy(path, expand[0]);
	(void) glob_free(expand);
    }
    return TRUE;
}
