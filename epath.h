/*	EPATH:	This file contains certain info needed to locate the
		certain needed files on a system dependent basis.

									*/

/*	possible names and paths of help files under different OSs	*/

/*
 * $Header: /users/source/archives/vile.vcs/RCS/epath.h,v 1.20 1996/02/28 03:51:24 pgf Exp $
 */

/* first two entries are default startup and help files, the rest are
	possible places to look for them */

char *exec_pathname = "."; /* replaced at runtime with path-head of argv[0] */

const char * const pathname[] =


#if	SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT
{
	"vile.rc",
	"vile.hlp",
	"\\sys\\public\\",
	"\\usr\\bin\\",
	"\\bin\\",
	"\\"
};
#endif

#if	SYS_UNIX
{
	".vilerc",
	"vile.hlp",
#ifdef HELP_LOC
#ifndef lint	/* makefile gives inconsistent quoting for lint, compiler */
	HELP_LOC,
#endif	/* lint */
#endif
	"/usr/local/lib/",
	"/usr/local/",
	"/usr/lib/"
};
#endif

#if	SYS_VMS
{
	"vile.rc",
	"vile.hlp",
	"sys$login:",
	"",
	"sys$sysdevice:[vmstools]"
};
#endif

#define	NPNAMES	(sizeof(pathname)/sizeof(char *))
