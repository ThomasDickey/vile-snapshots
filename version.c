/*
 * version & usage-messages for vile
 *
 * $Id: version.c,v 1.80 2025/01/26 14:34:21 tom Exp $
 *
 */

#include	"estruct.h"	/* global structures and defines */
#include	"edef.h"	/* global definitions */
#include	"patchlev.h"

static char version_string[NSTRING];

void
print_usage(int code)
{
    static const char *const options[] =
    {
	"-h             display [Help] on startup",
	"-c command     execute the given ex-style command",
#if OPT_EVAL || OPT_DEBUGMACROS
	"-D             trace macros into [Trace] buffer",
#endif
	"-e             edit in \"noview\" mode -- changes permitted",
#if OPT_SELECTIONS&&OPT_FILTER
	"-F             run syntax filter only, write to stdout",
#endif
	"-I             use vileinit.rc to initialize",
#if OPT_ENCRYPT
	"-k cryptkey    for encrypted files",
#endif
	"-R             edit files \"read-only\" -- no writes permitted",
#if OPT_TAGS
# if DISP_X11			/* because -title is predefined */
	"-T tagname     look up a tag",
# else
	"-t tagname     look up a tag",
# endif
#endif
	"-v             edit in \"view\" mode -- no changes permitted",
	"-V             for version info",
	"use @cmdfile to run cmdfile as commands (this suppresses "
	DFT_STARTUP_FILE ")",

#if SYS_VMS
	"",
	"VMS-specific:",
	"-80            80-column mode",
	"-132           132-column mode",
#endif

#if DISP_BORLAND
	"",
	"DOS-specific:",
	"-2             25-line mode",
	"-4             43-line mode",
	"-5             50-line mode",
	"-6             60-line mode",
	"(see help file for more screen resolutions)",
#endif

#if SYS_WINNT
	"",
	"Win32-specific:",
# if DISP_NTWIN
	"-fn fontspec   change font",
	"-geometry CxR  set initial size to R rows and C columns",
	"-i             set current-directory to pathname of file-parameter",
#  if defined(VILE_OLE)
	"-Oa            invoke as an OLE Automation server",
	"-Or            register ole automation interface and exit",
	"-Ou            unregister ole automation interface and exit",
	"-invisible     OLE Automation server does not initially show a window",
	"-multiple      multiple instances of OLE Automation server permitted",
#  endif
# endif				/* DISP_NTWIN */

# if DISP_NTCONS
	"-console       if stdin is not a tty, start editor in a new console",
# endif
#endif				/* SYS_WINNT */

#if DISP_X11
	"",
	"X11-specific:",
	"-class name    change class name for X resources (default XVile)",
	"-name name     change program name for X resources",
	"-title name    set name in title bar",
	"-fg color      change foreground color",
	"-bg color      change background color",
	"-fn fontname   change font",
	"-fork          spawn xvile immediately on startup",
	"+fork          force xvile to not spawn on startup",
	"-display       displayname to change the default display",
	"-rv            for reverse video",
	"-geometry CxR  set initial size to R rows and C columns",
	"-xrm resource  change an xvile resource",
	"-leftbar       put scrollbar(s) on left",
	"-rightbar      put scrollbar(s) on right (default)",
#endif				/* DISP_X11 */

	"",
	"Obsolete:",
	"-g NNN         or simply +NNN to go to line NNN",
	"-s string      or +/string to search for \"string\"",
    };

    term.clean(TRUE);
#if DISP_NTWIN
    gui_usage(prog_arg, options, TABLESIZE(options));
#else
    fflush(stdout);
    (void) fprintf(stderr, "usage: %s [-flags] [@cmdfile] files...\n",
		   prog_arg);
    {
	unsigned j;
	for (j = 0; j < TABLESIZE(options); j++) {
	    char *colon = strrchr(options[j], ':');
	    if (colon != NULL && colon[1] == EOS) {
		(void) fprintf(stderr, "%s\n", options[j]);
	    } else {
		(void) fprintf(stderr, "\t%s\n", options[j]);
	    }
	}
    }
#endif
    ExitProgram(code);
}

const char *
getversion(void)
{
    if (*version_string)
	return version_string;
#if SYS_UNIX || SYS_VMS
    /*
     * We really would like to have the date at which this program was
     * linked, but a.out doesn't have that in general.  COFF files do.
     * Getting the executable's modification-time is a reasonable
     * compromise.
     */
    (void) lsprintf(version_string, "%s %s%s for %s",
		    prognam, version, VILE_PATCHLEVEL, opersys);
    {
	char *s;
	if ((s = cfg_locate(prog_arg,
			    (FL_EXECDIR | FL_PATH) | FL_EXECABLE)) != NULL) {
	    time_t mtime = file_modified(s);
	    if (mtime != 0) {
		(void) vl_strncat(version_string, ", installed ", NSTRING);
		(void) vl_strncat(version_string, ctime(&mtime), NSTRING);
		/* trim the newline */
		version_string[strlen(version_string) - 1] = EOS;
	    }
	}
    }
#else
# if SYS_MSDOS || SYS_OS2 || SYS_WINNT
#  if defined(__DATE__) && !SMALLER
    (void) lsprintf(version_string, "%s %s%s for %s, built %s %s with %s",
		    prognam, version, VILE_PATCHLEVEL, opersys, __DATE__, __TIME__,
#   if CC_WATCOM
		    "Watcom C/386"
#   endif
#   if CC_DJGPP
#    if __DJGPP__ >= 2
		    "DJGPP v2"
#    else
		    "DJGPP"
#    endif
#   endif
#   if CC_TURBO
#    ifdef __BORLANDC__
		    "Borland C++"
#    else
		    "Turbo C"
#    endif
#   endif
#   if CC_CSETPP
#    if __IBMC__ >= 300
		    "VisualAge C++"
#    else
		    "IBM C Set ++"
#    endif
#   endif
#   if CC_LCC_WIN32
		    "Lcc-win32"
#   endif
#   if CC_MSVC
		    "Visual C++"
#   endif
#   if __GNUC__
#    if defined(__CYGWIN32__)
		    "Cygwin gcc"
#    elif SYS_MINGW
		    "MinGW gcc"
#    else
		    "gcc"
#    endif
#   endif
	);
#  endif
# endif	/* SYS_MSDOS || SYS_OS2 || SYS_WINNT */
#endif /* not SYS_UNIX or SYS_VMS */
    return version_string;
}

/* i'm not even going to try to justify this.  -pgf */
static void
personals(int n)
{
#if !SMALLER
    const char **cmdp = NULL;

    static const char *pgfcmds[] =
    {
	"bind-key split-current-window ^T",
	"bind-key next-window ^N",
	"bind-key previous-window ^P",
	"set ai atp nobl ul=0 sw=4 csw=4 timeoutlen=50 check-modtime visual-matches=underline",
	NULL
    };

    if (n == 11)
	cmdp = pgfcmds;

    if (n == -11)
	*(int *) (1) = 42;	/* test core dumps */

    if (!cmdp)
	return;

    while (*cmdp) {
	char *line = strmalloc(*cmdp);
	(void) docmd(line, TRUE, FALSE, 1);
	free(line);
	cmdp++;
    }
#else
    (void) n;
#endif

}

/* ARGSUSED */
int
showversion(int f GCC_UNUSED, int n)
{
    personals(n);
    mlforce("%s", getversion());

    return TRUE;
}

/*
 * Returns the special string consisting of program name + version, used to
 * fill in the filename-field for scratch buffers that are not associated with
 * an external file.
 */
const char *
non_filename(void)
{
    static char buf[80];
    if (buf[0] == EOS)
	(void) lsprintf(buf, "       %s   %s%s",
			prognam, version, VILE_PATCHLEVEL);
    return buf;
}
