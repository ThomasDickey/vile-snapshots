/*
 *	dirstuff.h
 *
 *	Definitions to interface to unix-like DIRECTORY(3) procedures.
 *	Include this after "estruct.h"
 *
 * $Header: /users/source/archives/vile.vcs/RCS/dirstuff.h,v 1.26 2004/10/30 14:54:56 tom Exp $
 *
 */

#ifndef DIRSTUFF_H
#define DIRSTUFF_H

#if ! SYS_VMS

#define USE_LS_FOR_DIRS 0
#define OLD_STYLE_DIRS 0	/* e.g., pre-SysV.2 14-char names */

#ifdef DIRNAMES_NOT_NULL_TERMINATED
/* rumor has it that some early readdir implementations didn't null-terminate
   the d_name array, and on those you _have_ to use d_namlen to get
   the length.  most modern dirent structs are null-terminated, however. */
#define USE_D_NAMLEN 1
#endif

#if (defined(_POSIX_VERSION) && _POSIX_VERSION) || defined(HAVE_DIRENT_H) || CC_TURBO || CC_WATCOM || CC_DJGPP || SYS_OS2
# if CC_WATCOM || CC_CSETPP
#   include <direct.h>
# else
#   include <dirent.h>
# endif
# define	DIRENT	struct dirent
#else	/* apollo & other old bsd's */
# define	DIRENT	struct direct
# define USE_D_NAMLEN 1
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# else
#  ifdef HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  else
#   ifdef HAVE_NDIR_H
#    include <ndir.h>
#   else
#    if SYS_WINNT
#     if CC_MSVC
#      include <direct.h>
#     else
#      include <dirent.h>
#     endif
#     undef USE_D_NAMLEN
#     define USE_D_NAMELEN 0
      struct direct {
	char *d_name;
      };
      struct _dirdesc {
	HANDLE hFindFile;
	WIN32_FIND_DATA ffd;
	int first;
	struct direct de;
      };

      typedef struct _dirdesc DIR;

#    else /* SYS_WINNT */
#     undef USE_LS_FOR_DIRS
#     define USE_LS_FOR_DIRS 1
#     undef USE_D_NAMLEN
#     define USE_D_NAMELEN 1
#    endif /* SYS_WINNT */
#   endif
#  endif
# endif
#endif

#else /* SYS_VMS */

#include	<rms.h>
#include	<descrip.h>

#define USE_D_NAMLEN 1

typedef struct	{
	ULONG	d_ino;
	short	d_reclen;
	short	d_namlen;
	char	d_name[NAM$C_MAXRSS + 1];		/* result: SYS$SEARCH */
	} DIRENT;

typedef	struct	{
	DIRENT		dd_ret;
	struct	FAB	dd_fab;
	struct	NAM	dd_nam;
	char		dd_esa[NAM$C_MAXRSS + 1];	/* result: SYS$PARSE */
	} DIR;

#endif	/* SYS_VMS */

#if USE_LS_FOR_DIRS
#define	DIR	FILE
typedef	struct	{
	char	d_name[NFILEN];
	} DIRENT;
#endif

#if SYS_WINNT && !CC_TURBO || SYS_VMS || USE_LS_FOR_DIRS
	/* rename these, just in case there's a shared-library around */
#define opendir  vile_opendir
#define readdir  vile_readdir
#define closedir vile_closedir

extern	DIR *	opendir ( char *path );
extern	DIRENT *readdir ( DIR *dp );
extern	int	closedir ( DIR *dp );
#endif

#if OLD_STYLE_DIRS
	this ifdef is untested
#define USE_D_NAMLEN 1
#define	DIR	FILE
#define	DIRENT	struct direct
#define	opendir(n)	fopen(n,"r")
extern	DIRENT *readdir ( DIR *dp );
#define	closedir(dp)	fclose(dp)
#endif

#ifndef USE_D_NAMLEN
#define USE_D_NAMLEN 0
#endif

#endif /* DIRSTUFF_H */
