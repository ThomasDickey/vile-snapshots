/*
 * lckfiles.c   set_lock() and release_lock(), for maintaining
 *		little file.lck style lockfiles.  this isn't what
 *		we would call a standard mechanism.  but it's simple.
 *
 *		it's also not completely integrated with the editor --
 *		for instance, one can probably bypass the locking mechanism
 *		in various ways.  it is intended as an _aid_ to multiple
 *		edits.  _not_ a cure-all.  it works well for the people
 *		at Baan who contributed the code.  i make no other claims
 *		for it.
 *
 *		operation:  if the global "usefilelock" is on, then when
 *		"file" is edited, a "file.lck" is created, containing the
 *		username of the user doing the editing.  when a file is
 *		edited for which there already exists a .lck file, the buffer
 *		mode "locked" is set to true and "locker" is set to the name
 *		of the user that created the .lck.  this information will
 *		appear on the status line, as "locked by pgf", and the
 *		buffer will be marked readonly.  the .lck file will be
 *		deleted at most of the appropriate times.
 *
 * $Id: lckfiles.c,v 1.13 2020/01/17 22:36:39 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

#if OPT_LCKFILES

#ifndef HAVE_LONG_FILE_NAMES
You probably would not want this code:
  there are no checks on filename length when adding.lck to the end.
#endif

static void
get_lock_owner(char *lockfile, char *who, int n)
{
    FILE *fp;
    int l;
    if ((fp = fopen(lockfile, FOPEN_READ)) != (FILE *) 0) {
	l = read(fileno(fp), who, (size_t) (n - 1));
	if (l < 0) {
	    (void) strcpy(who, "'Can't read .lck'");
	} else {
	    who[l - 1] = EOS;	/* Strip \n */
	}
	fclose(fp);
    } else {
	(void) strcpy(who, "'Can't open .lck'");
    }
}

static char *
ourname(void)
{
    char *np;
    np = getenv("LOGNAME");
    if (!np)
	np = getenv("USER");
    if (!np)
	np = "unknown";
    return np;
}

int
set_lock(const char *fname, char *who, int n)
{
    char lockfile[NFILEN];
    FILE *fp;

    sprintf(lockfile, "%s.lck", fname);

    if (ffexists(lockfile)) {
	/* Lockfile exists */
	get_lock_owner(lockfile, who, n);
	mlwrite("[%s]", who);
	return FALSE;		/* Can't set lock */
    } else {
	if ((fp = fopen(lockfile, FOPEN_WRITE)) != (FILE *) 0) {
	    (void) lsprintf(who, "%s\n", ourname());
	    write(fileno(fp), who, strlen(who));
	    fclose(fp);
	} else {
	    (void) strcpy(who, "'Can't write .lck'");
	    mlwrite("[%s]", who);
	    return (FALSE);	/* Can't set lock */
	}
    }
    return TRUE;		/* Lock ok */
}

void
release_lock(const char *fname)
{
    char lockfile[NFILEN];
    char who[100];

    if (fname && *fname) {
	(void) lsprintf(lockfile, "%s.lck", fname);
	get_lock_owner(lockfile, who, sizeof(who));
	/* is it ours? */
	if (strcmp(who, ourname()) == 0)
	    unlink(lockfile);
    }
}
#endif
