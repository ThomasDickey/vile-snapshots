/*
 *	This used to be MicroEMACS 3.9
 *			written by Dave G. Conroy.
 *			substantially modified by Daniel M. Lawrence
 *
 *	Turned into "VI Like Emacs", a.k.a. vile, by Paul Fox
 *
 *	(C)opyright 1987 by Daniel M. Lawrence
 *	MicroEMACS 3.9 can be copied and distributed freely for any
 *	non-commercial purposes. MicroEMACS 3.9 can only be incorporated
 *	into commercial software with the permission of the current author.
 *
 *	The same goes for vile.  -pgf, 1990-1995
 *
 *
 * $Header: /users/source/archives/vile.vcs/RCS/main.c,v 1.302 1997/10/07 00:03:17 tom Exp $
 *
 */

/* Make global definitions not external */
#define realdef
#include	"estruct.h"	/* global structures and defines */
#include	"edef.h"	/* global definitions */
#include	"nevars.h"
#include	"nefunc.h"

#if CC_NEWDOSCC
#include <io.h>
#if CC_DJGPP
#include <dpmi.h>
#include <go32.h>
#endif
#endif

#ifdef VMS
#include <processes.h>
#endif

extern char *exec_pathname;
extern const char *const pathname[];	/* startup file path/name array */

/* for MSDOS, increase the default stack space */
#if	SYS_MSDOS && CC_TURBO
unsigned _stklen = 32768U;
#endif

static	void	get_executable_dir (void);
static	void	global_val_init (void);
static	void	loop (void);
static	void	siginit (void);
static	void	start_debug_log(int ac, char **av);
static	int	cmd_mouse_motion(const CMDFUNC *cfp);

/*--------------------------------------------------------------------------*/
#define	GetArgVal(param)	if (!*(++param))\
					param = argv[++carg];\
				if (param == 0)\
					goto usage

int
main(int argc, char *argv[])
{
	int tt_opened;
	register BUFFER *bp;		/* temp buffer pointer */
	register int	carg;		/* current arg to scan */
	char *vileinit = NULL;		/* the startup file or VILEINIT var */
	int startstat = TRUE;		/* result of running startup */
	BUFFER *firstbp = NULL; 	/* ptr to first buffer in cmd line */
	char *firstname = NULL;		/* name of first buffer in cmd line */
	int gotoflag = FALSE;		/* do we need to goto line at start? */
	int gline = FALSE;		/* if so, what line? */
	int helpflag = FALSE;		/* do we need help at start? */
	REGEXVAL *search_exp = 0;	/* initial search-pattern */
	const char *msg;
#if DISP_X11 && !XTOOLKIT
	int do_newgroup = FALSE;	/* do we spawn at start? */
#endif
#if OPT_TAGS
	int didtag = FALSE;		/* look up a tag to start? */
	char *tname = NULL;
#endif
#if	OPT_ENCRYPT
	char ekey[NPAT];		/* startup encryption key */
	*ekey = EOS;
#endif

	global_val_init();	/* global buffer values */
	charinit();	/* character types -- we need these pretty early  */
	winit(FALSE);		/* command-line */
#if !SYS_UNIX
	expand_wild_args(&argc, &argv);
#endif
	prog_arg = argv[0];	/* this contains our only clue to exec-path */

	start_debug_log(argc,argv);

	get_executable_dir();

	if (strcmp(pathleaf(prog_arg), "view") == 0)
		set_global_b_val(MDREADONLY,TRUE);

#if DISP_X11
	if (argc != 2 || strcmp(argv[1], "-V") != 0)
		x_preparse_args(&argc, &argv);
#endif
	/*
	 * Allow for I/O to the command-line before we initialize the screen
	 * driver.
	 *
	 * FIXME: we only know how to do this for displays that open the
	 * terminal in the same way for command-line and screen.
	 */
	siginit();
#if OPT_DUMBTERM
	if (isatty(fileno(stdin))
	 && isatty(fileno(stdout))) {
		tt_opened = open_terminal(&dumb_term);
	} else
#endif
	 tt_opened = open_terminal(&null_term);

	/* Parse the command line */
	for (carg = 1; carg < argc; ++carg) {
		register char *param = argv[carg];
#if DISP_X11 && !XTOOLKIT
		if (*param == '=') {
			x_set_geometry(param);
			continue;
		}
#endif

		/* Process Switches */
		if (*param == '-') {
			++param;
#if DISP_IBMPC || DISP_BORLAND
		    	/* if it's a digit, it's probably a screen
				resolution */
			if (isDigit(*param)) {
				current_res_name = param;
				continue;
			} else
#endif	/* DISP_IBMPC */
			switch (*param) {
#if DISP_X11 && !XTOOLKIT
			case 'd':
				if ((param = argv[++carg]) != 0)
					x_set_dpy(param);
				else
					goto usage;
				break;
			case 'r':
				x_set_rv();
				break;
			case 'f':
				if (argv[++carg] != 0) {
					if (strcmp(param, "foreground") == 0
					 || strcmp(param, "fg") == 0)
						x_setforeground(argv[carg]);
					else if (!strcmp(param, "fork"))
						do_newgroup = TRUE;
					else
						x_setfont(argv[carg]);
				} else
					goto usage;
				break;
			case 'b':
				if (argv[++carg] != 0) {
					if (strcmp(param, "background") == 0
					 || strcmp(param, "bg") == 0)
						x_setbackground(argv[carg]);
				} else
					goto usage;
				break;
			case 'n':
				if (strcmp(param, "name") == 0
				 && argv[++carg] != 0)
					x_setname(argv[carg]);
				else
					goto usage;
				break;
			case 'w':
				if (strcmp(param, "wm") == 0
				 && argv[++carg] != 0)
					x_set_wm_title(argv[carg]);
				else
					goto usage;
				break;
#endif /* DISP_X11 */
			case 'e':	/* -e for Edit file */
			case 'E':
				set_global_b_val(MDVIEW,FALSE);
				break;
			case 'g':	/* -g for initial goto */
			case 'G':
				gotoflag = TRUE;
				GetArgVal(param);
				gline = atoi(param);
				break;
			case 'h':	/* -h for initial help */
			case 'H':
				helpflag = TRUE;
				break;
#if	OPT_ENCRYPT
			case 'k':	/* -k<key> for code key */
			case 'K':
				GetArgVal(param);
				(void)strcpy(ekey, param);
				(void)memset(param, '.', strlen(param));
				ue_crypt((char *)0, 0);
				ue_crypt(ekey, strlen(ekey));
				break;
#endif
			case 's':  /* -s for initial search string */
			case 'S':
		dosearch:
				GetArgVal(param);
				search_exp = new_regexval(param, global_b_val(MDMAGIC));
				break;
#if OPT_TAGS
			case 't':  /* -t for initial tag lookup */
			case 'T':
				GetArgVal(param);
				tname = param;
				break;
#endif
			case 'v':	/* -v is view mode */
				set_global_b_val(MDVIEW,TRUE);
				break;

			case 'R':	/* -R is readonly mode (like "view") */
				set_global_b_val(MDREADONLY,TRUE);
				break;

			case 'V':
				(void)printf("%s\n", getversion());
				tidy_exit(GOODEXIT);

				/* FALLTHROUGH */

			case '?':
			default:	/* unknown switch */
			usage:
				print_usage();
			}

		} else if (*param == '+') { /* alternate form of -g */
			if (*(++param) == '/') {
				int len = strlen(param);
				if (len > 0 && param[len-1] == '/')
					param[--len] = EOS;
				if (len == 0)
					print_usage();  
				goto dosearch;
			}
			gotoflag = TRUE;
			gline = atoi(param);
		} else if (*param == '@') {
			vileinit = ++param;
		} else if (*param != EOS) {

			/* Process an input file */
#if OPT_ENCRYPT
			cryptkey = (*ekey != EOS) ? ekey : 0;
#endif
			/* set up a buffer for this file */
			bp = getfile2bp(param,FALSE,TRUE);
			if (bp) {
				bp->b_flag |= BFARGS;	/* treat this as an argument */
				make_current(bp); /* pull it to the front */
				if (firstbp == 0) {
					firstbp = bp;
					firstname = param;
				}
			}
#if OPT_ENCRYPT
			cryptkey = 0;
#endif
		}
	}


	/* if stdin isn't a terminal, assume the user is trying to pipe a
	 * file into a buffer.
	 */
#if SYS_UNIX || SYS_VMS || SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT
	if (!isatty(fileno(stdin))) {
#if !DISP_X11
#if SYS_UNIX
# if HAS_TTYNAME
		char	*tty = ttyname(fileno(stderr));
# else
		char	*tty = "/dev/tty";
# endif
#else
  		FILE	*in;
  		int	fd;
#endif /* SYS_UNIX */
#endif /* DISP_X11 */
		BUFFER	*lastbp = firstbp;
		int	nline = 0;

		bp = bfind(STDIN_BufName, BFARGS);
		make_current(bp); /* pull it to the front */
		if (firstbp == 0)
			firstbp = bp;
		ffp = fdopen(dup(fileno(stdin)), "r");
#if !DISP_X11
# if SYS_UNIX
		/*
		 * Note: On Linux, the low-level close/dup operation
		 * doesn't work, since something hangs, apparently
		 * because substituting the file descriptor doesn't communicate
		 * properly up to the stdio routines.
		 */
		if ((freopen(tty, "r", stdin)) == 0
		 || !isatty(fileno(stdin))) {
			fputs("cannot open a terminal\n", stderr);
			tidy_exit(BADEXIT);
		}
#else
# if SYS_WINNT
		/*
		 * Win32 needs to reopen the console, not fd 0.  If the console
		 * is not reopened, the nt console I/O routines die immediately
		 * when attempting to fetch a STDIN handle.
		 */
		freopen("con", "r", stdin);
# else
#  if SYS_VMS
  		fd = open("tt:", O_RDONLY, S_IREAD); /* or sys$command */
#  else					/* e.g., DOS-based systems */
  		fd = fileno(stderr);	/* this normally cannot be redirected */
#  endif
  		if ((fd >= 0)
  		 && (close(0) >= 0)
  		 && (fd = dup(fd)) == 0
  		 && (in = fdopen(fd, "r")) != 0)
  			*stdin = *in;
#  endif /* SYS_WINNT */
# endif /* SYS_UNIX */
#endif /* DISP_X11 */

  		(void)slowreadf(bp, &nline);
		set_rdonly(bp, bp->b_fname, MDREADONLY);
		(void)ffclose();

		if (is_empty_buf(bp)) {
			(void)zotbuf(bp);
			curbp = firstbp = lastbp;
		}
#if OPT_FINDERR
		  else {
			set_febuff(bp->b_bname);
		}
#endif
	}
#endif

#if DISP_X11 && !XTOOLKIT
	if (do_newgroup)
		(void) newprocessgroup(TRUE,1);
#endif
	/* initialize the editor */

	if (!tt_opened)
		siginit();
	(void)open_terminal((TERM *)0);
	TTkopen();		/* open the keyboard */
	TTrev(FALSE);

	if (vtinit() != TRUE)	/* allocate display memory */
		tidy_exit(BADEXIT);

	winit(TRUE);		/* windows */

	/* this comes out to 70 on an 80 (or greater) column display */
	{	register int fill;
		fill = (7 * term.t_ncol) / 8;  /* must be done after vtinit() */
		if (fill > 70) fill = 70;
		set_global_b_val(VAL_FILL, fill);
	}

	/* Create an unnamed buffer, so that the initialization-file will have
	 * something to work on.  We don't pull in any of the command-line
	 * filenames yet, because some of the initialization stuff has to be
	 * triggered by switching buffers after reading the .vilerc file.
	 *
	 * If nothing modifies it, this buffer will be automatically removed
	 * when we switch to the first file (e.g., firstbp), because it is
	 * empty (and presumably isn't named the same as an actual file).
	 */
	bp = bfind(UNNAMED_BufName, 0);
	bp->b_active = TRUE;
#if OPT_DOSFILES
	/* an empty non-existent buffer defaults to line-style
		favored by the OS */
	make_local_b_val(bp, MDDOS);
	set_b_val(bp, MDDOS, CRLF_LINES);
#endif
	fix_cmode(bp, FALSE);
	swbuffer(bp);

	/* run the specified, or the system startup file here.
	   if vileinit is set, it's the name of the user's
	   command-line startup file, i.e. 'vile @mycmds'
	 */
	if (vileinit && *vileinit) {
		if ((startstat = startup(vileinit)) != TRUE)
			goto begin;
		free(startup_file);
		startup_file = strmalloc(vileinit);
	} else {

		/* now vileinit is the contents of their VILEINIT variable */
		vileinit = getenv("VILEINIT");
		if (vileinit != NULL) { /* set... */
			int odiscmd;
			BUFFER *vbp, *obp;
			int oflags = 0;
			if (*vileinit) { /* ...and not null */
				/* mark as modified, to prevent
				 * undispbuff() from clobbering */
				obp = curbp;
				if (obp) {
					oflags = obp->b_flag;
					b_set_changed(obp);
				}

				if ((vbp=bfind(VILEINIT_BufName, 0))==NULL)
					tidy_exit(BADEXIT);

				/* don't want swbuffer to try to read it */
				vbp->b_active = TRUE;
				swbuffer(vbp);
				b_set_scratch(vbp);
				bprintf("%s", vileinit);
				/* if we leave it scratch, swbuffer(obp) 
					may zot it, and we may zot it again */
				b_clr_scratch(vbp);
				set_rdonly(vbp, vbp->b_fname, MDVIEW);

				/* go execute it! */
				odiscmd = discmd;
				discmd = FALSE;
				startstat = dobuf(vbp);
				discmd = odiscmd;
				if (startstat != TRUE)
					goto begin;
				if (obp) {
					swbuffer(obp);
					obp->b_flag = oflags;
				}
				/* remove the now unneeded buffer */
				b_set_scratch(vbp);  /* make sure it will go */
				(void)zotbuf(vbp);
			}
		} else {  /* find and run .vilerc */
			char *fname;
			/* if .vilerc is one of the input files....
					don't clobber it */
#if SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT
			/* search PATH for vilerc under dos */
			fname = flook(startup_file, FL_ANYWHERE|FL_READABLE);
			if (!fname)
				fname = startup_file;
#else
			fname = startup_file;
#endif
			if (firstbp != 0
			 && eql_bname(firstbp, startup_file)) {
				char c;
				c = firstbp->b_bname[0];
				firstbp->b_bname[0] = SCRTCH_LEFT[0];
				startstat = startup(fname);
				firstbp->b_bname[0] = c;
			} else {
				if (fname)
					startstat = startup(fname);
				else
					startstat = TRUE;
			}
			if (startstat != TRUE)
				goto begin;
		}
	}

	/* If there are any files to read, read the first one!  Double-check,
	 * however, since a startup-script may have removed the first buffer.
	 */
	if (firstbp != 0
	 && find_bp(firstbp)) {
		if (find_bp(bp) && is_empty_buf(bp) && !b_is_changed(bp))
			b_set_scratch(bp);	/* remove the unnamed-buffer */
		startstat = swbuffer(firstbp);
		if (firstname)
			set_last_file_edited(firstname);
		if (bp2any_wp(bp) && bp2any_wp(firstbp))
			zotwp(bp);
	}
#if OPT_TAGS
	else if (tname) {
		cmdlinetag(tname);
		didtag = TRUE;
	}
#endif
	msg = s_NULL;
	if (helpflag) {
		if (help(TRUE,1) != TRUE) {
			msg =
	"[Problem with help information. Type \":quit\" to exit if you wish]";
		}
	} else {
		msg = "[Use ^A-h, ^X-h, or :help to get help]";
	}

	/* Deal with startup gotos and searches */
	if (gotoflag + (search_exp != 0)
#if OPT_TAGS
		 + (tname?1:0)
#endif
		> 1) {
#if OPT_TAGS
		msg = "[Search, goto and tag are used one at a time]";
#else
		msg = "[Cannot search and goto at the same time]";
#endif
	} else if (gotoflag) {
		if (gotoline(gline != 0, gline) == FALSE) {
			msg = "[Not that many lines in buffer]";
			(void)gotoeob(FALSE,1);
		}
	} else if (search_exp) {
		FreeIfNeeded(gregexp);
		(void)strncpy0(pat, search_exp->pat, NPAT);
		gregexp = search_exp->reg;
		(void)forwhunt(FALSE, 0);
#if OPT_TAGS
	} else if (tname && !didtag) {
		cmdlinetag(tname);
#endif
	}

#if OPT_POPUP_MSGS
	purge_msgs();
#endif
	if (startstat == TRUE)  /* else there's probably an error message */
		mlforce(msg);

 begin:
	(void)update(FALSE);

#if OPT_POPUP_MSGS
	if (global_g_val(GMDPOPUP_MSGS) == -TRUE)
		set_global_g_val(GMDPOPUP_MSGS, FALSE);
#endif

	/* process commands */
	loop();

	/* NOTREACHED */
	return BADEXIT;
}

/* this is nothing but the main command loop */
static void
loop(void)
{
	const CMDFUNC
		*cfp = NULL, 
		*last_cfp = NULL, 
		*last_failed_motion_cfp = NULL;
	int s,c,f,n;


	for_ever {

		/* vi doesn't let the cursor rest on the newline itself.  This
			takes care of that. */
		/* if we're inserting, or will be inserting again, then
			suppress.  this happens if we're using arrow keys
			during insert */
		if (is_at_end_of_line(DOT) && (DOT.o > w_left_margin(curwp)) &&
				!insertmode && !cmd_mouse_motion(cfp))
			backchar(TRUE,1);

		/* same goes for end-of-file -- I'm actually not sure if
			this can ever happen, but I _am_ sure that it's
			a lot safer not to let it... */
		if (is_header_line(DOT,curbp) && !is_empty_buf(curbp))
			(void)backline(TRUE,1);

		/* start recording for '.' command */
		dotcmdbegin();

		/* Fix up the screen	*/
		s = update(FALSE);

		/* get the next command from the keyboard */
		c = kbd_seq();

		/* if there is something on the command line, clear it */
		if (mpresf != 0) {
			mlerase();
			if (s != SORTOFTRUE) /* did nothing due to typeahead */
				(void)update(FALSE);
		}

		f = FALSE;
		n = 1;

#if LATERMAYBE
/* insertion is too complicated to pop in
	and out of so glibly...   -pgf */
#ifdef insertmode
		/* FIXME: Paul and Tom should check this over. */
		if (insertmode != FALSE) {
			if (!kbd_replaying(FALSE))
			    mayneedundo();
			unkeystroke(c);
			insert(f,n);
			dotcmdfinish();
			continue;
		}
#endif /* insertmode */
#endif /* LATERMAYBE */

		do_repeats(&c,&f,&n);

		kregflag = 0;

		/* flag the first time through for some commands -- e.g. subst
			must know to not prompt for strings again, and pregion
			must only restart the p-lines buffer once for each
			command. */
		calledbefore = FALSE;

		/* and execute the command */
		cfp = kcod2fnc(c);

		if (cfp == &f_dotcmdplay && 
			(last_cfp == &f_undo || 
			 last_cfp == &f_forwredo || 
			 last_cfp == &f_backundo || 
			 last_cfp == &f_inf_undo))
			cfp = &f_inf_undo;

		s = execute(cfp, f, n);

		last_cfp = cfp;

		/* stop recording for '.' command */
		dotcmdfinish();

		/* If this was a motion that failed, sound the alarm (like vi),
		 * but limit it to once, in case the user is holding down the
		 * autorepeat-key.
		 */
		if ( (cfp != NULL)
		 && ((cfp->c_flags & MOTION) != 0)
		 && (s == FALSE) ) {
			if (cfp != last_failed_motion_cfp || 
					global_g_val(GMDMULTIBEEP)) {
				last_failed_motion_cfp = cfp;
				kbd_alarm();
			}
		} else {
			last_failed_motion_cfp = NULL; /* avoid noise! */
		}

		attrib_matches();

	}
}

/* attempt to locate the executable that contains our code.
* leave its directory name in exec_pathname and shorten prog_arg
* to the simple filename (no path).
*/
static void 
get_executable_dir(void)
{
#if SYS_UNIX || SYS_VMS
	char	temp[NFILEN];
	char	*s, *t;

	/* if there are no slashes, we have no idea where we came from */
	if (last_slash(prog_arg) == NULL)
		return;

	/* if there _are_ slashes, then argv[0] was either 
		absolute or relative. lengthen_path figures it out. */
	s = strmalloc(lengthen_path(strcpy(temp, prog_arg)));
	t = pathleaf(s);
	if (t != s) {
# if SYS_UNIX	/* 't' points past slash */
		t[-1] = EOS;
		prog_arg = t;
# else		/* 't' points to ']' */
		*t = EOS;
		prog_arg = t+1;
# endif
		exec_pathname = s;
	} else
		free(s);
#endif
}

void
tidy_exit(int code)
{
	ttclean (TRUE);
#if SYS_UNIX
	setup_handler(SIGHUP,SIG_IGN);
#endif
	ExitProgram(code);
}

#ifndef strmalloc
char *
strmalloc(const char *s)
{
	register char *ns = castalloc(char,strlen(s)+1);
	if (ns != 0)
		(void)strcpy(ns,s);
	return ns;
}
#endif

int
no_memory(const char *s)
{
	mlforce("[%s] %s", out_of_mem, s);
	return FALSE;
}

static void
global_val_init(void)
{
	static const char expand_chars[] =
		{ EXPC_THIS, EXPC_THAT, EXPC_SHELL, EXPC_TOKEN, EXPC_RPAT, 0 };
	register int i;
	char *s;

	/* set up so the global value pointers point at the global
		values.  we never actually use the global pointers
		directly, but when buffers get a copy of the
		global_b_values structure, the pointers will still point
		back at the global values, which is what we want */
	for (i = 0; i <= NUM_G_VALUES; i++)
		make_local_val(global_g_values.gv, i);

	for (i = 0; i <= NUM_B_VALUES; i++)
		make_local_val(global_b_values.bv, i);

	for (i = 0; i <= NUM_W_VALUES; i++)
		make_local_val(global_w_values.wv, i);

#if OPT_MAJORMODE
	/*
	 * Built-in majormodes
	 */
	alloc_mode("c", TRUE);
#endif

	/*
	 * Universal-mode defaults
	 */
	set_global_g_val(GMDABUFF,	TRUE); 	/* auto-buffer */
	set_global_g_val(GMDALTTABPOS,	FALSE); /* emacs-style tab
							positioning */
#ifdef GMDDIRC
	set_global_g_val(GMDDIRC,	FALSE); /* directory-completion */
#endif
	set_global_g_val(GMDERRORBELLS, TRUE);	/* alarms are noticeable */
#if OPT_FLASH
	set_global_g_val(GMDFLASH,  	FALSE);	/* beeps beep by default */
#endif
#ifdef GMDHISTORY
	set_global_g_val(GMDHISTORY,	TRUE);
#endif
	set_global_g_val(GMDMULTIBEEP,	TRUE); /* multiple beeps for multiple
						motion failures */
#if OPT_WORKING
	set_global_g_val(GMDWORKING,  	TRUE);	/* we put up "working..." */
#endif
	/* which 8 bit chars are printable? */
	set_global_g_val(GVAL_PRINT_LOW, 0);
	set_global_g_val(GVAL_PRINT_HIGH, 0);


	/* catnap times: */
	/* how long to wait for ESC seq */
	set_global_g_val(GVAL_TIMEOUTVAL, 500);
	/* how long to wait for user seq */
#if SYS_MSDOS	/* actually, 16-bit ints */
	set_global_g_val(GVAL_TIMEOUTUSERVAL, 30000);
#else
	set_global_g_val(GVAL_TIMEOUTUSERVAL, 60000);
#endif

	/* allow remapping by default */
	set_global_g_val(GMDREMAP, TRUE);

	set_global_g_val(GVAL_MAPLENGTH, 1200);

	/* set noresolve-links by default in case we've got NFS problems */
#ifdef GMDRESOLVE_LINKS
	set_global_g_val(GMDRESOLVE_LINKS, FALSE);
#endif

	set_global_g_val_ptr(GVAL_EXPAND_CHARS, strmalloc(expand_chars));
	set_global_g_val(GMDEXPAND_PATH,FALSE);
#ifdef GMDGLOB
	set_global_g_val(GMDGLOB, TRUE);
#endif
#ifdef GVAL_GLOB
	set_global_g_val_ptr(GVAL_GLOB, strmalloc("!echo %s"));
#endif

	set_global_g_val(GMDIMPLYBUFF,	FALSE); /* imply-buffer */
#if	OPT_POPUPCHOICE
# if	OPT_ENUM_MODES
	set_global_g_val(GVAL_POPUP_CHOICES, POPUP_CHOICES_DELAYED);
# else
	set_global_g_val(GMDPOPUP_CHOICES,TRUE);
# endif
#endif
#if	OPT_FILEBACK
# if	OPT_MSDOS_PATH
	set_global_g_val_ptr(GVAL_BACKUPSTYLE, strmalloc(".bak"));
# else
	set_global_g_val_ptr(GVAL_BACKUPSTYLE, strmalloc("off"));
# endif
#endif
#if	OPT_POPUP_MSGS
	set_global_g_val(GMDPOPUP_MSGS,-TRUE);	/* popup-msgs */
#endif
#ifdef GMDRAMSIZE
	set_global_g_val(GMDRAMSIZE,	TRUE);	/* show ram-usage */
#endif
	set_global_g_val(GVAL_REPORT,	5);	/* report changes */
#if	OPT_XTERM
	set_global_g_val(GMDXTERM_MOUSE,FALSE);	/* mouse-clicking */
#endif
	set_global_g_val(GMDWARNUNREAD,TRUE);	/* warn if quitting without
						looking at all buffers */
	set_global_g_val(GMDWARNREREAD,TRUE);	/* warn before rereading
						a buffer */
	set_global_g_val(GMDWARNRENAME,TRUE);	/* warn before renaming
						a buffer */
	set_global_g_val(GMDSMOOTH_SCROLL, FALSE);
	set_global_g_val(GMDSPACESENT,  TRUE); /* add two spaces after each
						sentence */
#if OPT_COLOR
	set_global_g_val(GVAL_FCOLOR,	C_WHITE); /* foreground color */
	set_global_g_val(GVAL_BCOLOR,	C_BLACK); /* background color */
#endif

	/*
	 * Buffer-mode defaults
	 */
	set_global_b_val(MDAIND,	FALSE); /* auto-indent */
	set_global_b_val(MDASAVE,	FALSE);	/* auto-save */
	set_global_b_val(MDBACKLIMIT,	TRUE); 	/* limit backspacing to
							insert point */
#ifdef	MDCHK_MODTIME
	set_global_b_val(MDCHK_MODTIME,	FALSE); /* modtime-check */
#endif
#if	!OPT_MAJORMODE
	set_global_b_val(MDCMOD,	FALSE); /* C mode */
#endif
#ifdef MDCRYPT
	set_global_b_val(MDCRYPT,	FALSE);	/* crypt */
#endif
	set_global_b_val(MDIGNCASE,	FALSE); /* exact matches */
	set_global_b_val(MDDOS, CRLF_LINES); /* on by default on DOS, off others */
	set_global_b_val(MDMAGIC,	TRUE); 	/* magic searches */
	set_global_b_val( MDMETAINSBIND, TRUE); /* honor meta-bindings when
							in insert mode */
	set_global_b_val(MDNEWLINE,	TRUE); 	/* trailing-newline */
	set_global_b_val(MDREADONLY,	FALSE); /* readonly */
	set_global_b_val(MDSHOWMAT,	FALSE);	/* show-match */
	set_global_b_val(MDSHOWMODE,	TRUE);	/* show-mode */
	set_global_b_val(MDSWRAP,	TRUE); 	/* scan wrap */
	set_global_b_val(MDTABINSERT,	TRUE);	/* allow tab insertion */
	set_global_b_val(MDTAGSRELTIV,	FALSE);	/* path relative tag lookups */
	set_global_b_val(MDTERSE,	FALSE);	/* terse messaging */
#if	OPT_HILITEMATCH
	set_global_b_val(VAL_HILITEMATCH, 0);	/* no hilite */
#endif
#if	OPT_UPBUFF
	set_global_b_val(MDUPBUFF,	TRUE);	/* animated */
#endif
	set_global_b_val(MDVIEW,	FALSE); /* view-only */
	set_global_b_val(MDWRAP,	FALSE); /* wrap */
#if OPT_LCKFILES
	/* locking defaults */
	set_global_g_val(GMDUSEFILELOCK,FALSE);	/* Use filelocks */
	set_global_b_val(MDLOCKED,	FALSE);	/* LOCKED */
	set_global_b_val_ptr(VAL_LOCKER, strmalloc("")); /* Name locker */
#endif
	set_global_g_val(GMDRONLYVIEW,	FALSE);	/* Set view-on-readonly */
	set_global_g_val(GMDRONLYRONLY,	FALSE);	/* Set readonly-on-readonly */

	set_global_b_val(VAL_ASAVECNT,	256);	/* autosave count */
#if OPT_MAJORMODE
	set_submode_val("c", VAL_SWIDTH, 8); 	/* C file shiftwidth */
	set_submode_val("c", VAL_TAB,	8); 	/* C file tab stop */
#else
	set_global_b_val(VAL_C_SWIDTH,	8); 	/* C file shiftwidth */
	set_global_b_val(VAL_C_TAB,	8); 	/* C file tab stop */
#endif
	set_global_b_val(VAL_SWIDTH,	8); 	/* shiftwidth */
	set_global_b_val(VAL_TAB,	8);	/* tab stop */
	set_global_b_val(VAL_TAGLEN,	0);	/* significant tag length */
	set_global_b_val(VAL_UNDOLIM,	10);	/* undo limit */

	set_global_b_val_ptr(VAL_TAGS, strmalloc("tags")); /* tags filename */
	set_global_b_val_ptr(VAL_FENCES, strmalloc("{}()[]")); /* fences */

#if SYS_VMS
#define	DEFAULT_CSUFFIX	"\\.\\(\\([CHIS]\\)\\|CC\\|CXX\\|HXX\\)\\(;[0-9]*\\)\\?$"
#endif
#if SYS_MSDOS || SYS_WIN31
#define	DEFAULT_CSUFFIX	"\\.\\(\\([chis]\\)\\|cc\\|cpp\\|cxx\\|hxx\\)$"
#endif
#ifndef DEFAULT_CSUFFIX	/* UNIX or OS2/HPFS (mixed-case names) */
#define	DEFAULT_CSUFFIX	"\\.\\(\\([Cchis]\\)\\|CC\\|cc\\|cpp\\|cxx\\|hxx\\|scm\\)$"
#endif

#if OPT_MAJORMODE
	set_majormode_rexp("c", MVAL_SUFFIXES, DEFAULT_CSUFFIX);
#else
	/* suffixes for C mode */
	set_global_g_val_rexp(GVAL_CSUFFIXES,
		new_regexval(
			DEFAULT_CSUFFIX,
			TRUE));
#endif

	/* where do paragraphs start? */
	set_global_b_val_rexp(VAL_PARAGRAPHS,
		new_regexval(
			"^\\.[ILPQ]P\\>\\|^\\.P\\>\\|^\\.LI\\>\\|\
^\\.[plinb]p\\>\\|^\\.\\?\\s*$",
			TRUE));

	/* where do comments start and end, for formatting them */
	set_global_b_val_rexp(VAL_COMMENTS,
		new_regexval(
			"^\\s*/\\?\\(\\s*[#*>]\\)\\+/\\?\\s*$",
			TRUE));

	set_global_b_val_rexp(VAL_CMT_PREFIX,
		new_regexval(
			"^\\s*\\(\\s*[#*>]\\)\\+",
			TRUE));

	/* where do sections start? */
	set_global_b_val_rexp(VAL_SECTIONS,
		new_regexval(
			"^[{\014]\\|^\\.[NS]H\\>\\|^\\.HU\\?\\>\\|\
^\\.[us]h\\>\\|^+c\\>",	/* }vi */
			TRUE));

	/* where do sentences start? */
	set_global_b_val_rexp(VAL_SENTENCES,
		new_regexval(
	"[.!?][])\"']* \\?$\\|[.!?][])\"']*  \\|^\\.[ILPQ]P\\>\\|\
^\\.P\\>\\|^\\.LI\\>\\|^\\.[plinb]p\\>\\|^\\.\\?\\s*$",
			TRUE));

	/*
	 * Window-mode defaults
	 */
#ifdef WMDLINEWRAP
	set_global_w_val(WMDLINEWRAP,	FALSE); /* line-wrap */
#endif
	set_global_w_val(WMDLIST,	FALSE); /* list-mode */
	set_global_w_val(WMDNUMBER,	FALSE);	/* number */
	set_global_w_val(WMDHORSCROLL,	TRUE);	/* horizontal scrolling */
#ifdef WMDTERSELECT
	set_global_w_val(WMDTERSELECT,	TRUE);	/* terse selections */
#endif

	set_global_w_val(WVAL_SIDEWAYS,	0);	/* list-mode */

	if ((s = getenv("VILE_HELP_FILE")) == 0)
		s = "vile.hlp";
	helpfile = strmalloc(s);

	if ((s = getenv("VILE_STARTUP_FILE")) == 0) {
#if	SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT || SYS_VMS
		s = "vile.rc";
#else	/* SYS_UNIX */
		s = ".vilerc";
#endif
	}
	startup_file = strmalloc(s);

	if ((s = getenv("VILE_STARTUP_PATH")) == 0) {
#if	SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT
		s = "\\sys\\public\\;\\usr\\bin\\;\\bin\\;\\";
#else
#if	SYS_VMS
		s = "sys$login;sys$sysdevice:[vmstools];sys$library";
#else	/* SYS_UNIX */
		s = "/usr/local/lib/:/usr/local/:/usr/lib/";
#endif
#endif
	}
	startup_path = strmalloc(s);

#ifdef	HELP_LOC
	/*
	 * *NIX install will define this
	 */
	{
		char temp[NFILEN];
		int found = FALSE;
		const char *t = startup_path;

		while ((t = parse_pathlist(t, temp)) != 0) {
			if (!strcmp(temp, HELP_LOC)) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			s = typeallocn(char, strlen(HELP_LOC) + 2 + strlen(startup_path));
			lsprintf(s, "%s%c%s", HELP_LOC, PATHCHR, startup_path);
			startup_path = s;
		}
	}
#endif
}

#if SYS_UNIX || SYS_MSDOS || SYS_WIN31 || SYS_OS2 || SYS_WINNT || SYS_VMS

/* ARGSUSED */
SIGT
catchintr (int ACTUAL_SIG_ARGS)
{
	am_interrupted = TRUE;
#if SYS_MSDOS || SYS_OS2 || SYS_WINNT
	sgarbf = TRUE;	/* there's probably a ^C on the screen. */
#endif
	setup_handler(SIGINT,catchintr);
	if (im_waiting(-1))
		longjmp(read_jmp_buf, signo);
	SIGRET;
}
#endif

#ifndef interrupted  /* i.e. unless it's a macro */
int
interrupted(void)
{
#if SYS_MSDOS && CC_DJGPP

	if (_go32_was_ctrl_break_hit() != 0) {
		while(keystroke_avail())
			(void)keystroke();
		return TRUE;
	}
	if (was_ctrl_c_hit() != 0) {
		while(keystroke_avail())
			(void)keystroke();
		return TRUE;
	}

	if (am_interrupted)
		return TRUE;
	return FALSE;
#endif
}
#endif

void
not_interrupted(void)
{
    am_interrupted = FALSE;
#if SYS_MSDOS
# if CC_DJGPP
    (void)_go32_was_ctrl_break_hit();  /* flush any pending kbd ctrl-breaks */
    (void)was_ctrl_c_hit();  /* flush any pending kbd ctrl-breaks */
# endif
#endif
}

#if SYS_MSDOS
# if CC_WATCOM
    int  dos_crit_handler(unsigned deverror, unsigned errcode, unsigned *devhdr)
# else
    void dos_crit_handler(void)
# endif
{
# if CC_WATCOM
	_hardresume((int)_HARDERR_FAIL);
	return (int)_HARDERR_FAIL;
# else
#  if ! CC_DJGPP
	_hardresume(_HARDERR_FAIL);
#  endif
# endif
}
#endif


static void
siginit(void)
{
#if SYS_UNIX
	setup_handler(SIGINT,catchintr);
	setup_handler(SIGHUP,imdying);
#ifdef SIGBUS
	setup_handler(SIGBUS,imdying);
#endif
#ifdef SIGSYS
	setup_handler(SIGSYS,imdying);
#endif
	setup_handler(SIGSEGV,imdying);
	setup_handler(SIGTERM,imdying);
/* #define DEBUG 1 */
#if DEBUG
	setup_handler(SIGQUIT,imdying);
#else
	setup_handler(SIGQUIT,SIG_IGN);
#endif
	setup_handler(SIGPIPE,SIG_IGN);
#if defined(SIGWINCH) && ! DISP_X11
	setup_handler(SIGWINCH,sizesignal);
#endif
#else
# if SYS_MSDOS
	setup_handler(SIGINT,catchintr);
#  if CC_DJGPP
	_go32_want_ctrl_break(TRUE);
	setcbrk(FALSE);
	want_ctrl_c(TRUE);
	hard_error_catch_setup();
#  else
#   if CC_WATCOM
	{
	/* clean up Warning from Watcom C */
	void *ptrfunc = dos_crit_handler;
	_harderr(ptrfunc);
	}
#   else	/* CC_TURBO */
	_harderr(dos_crit_handler);
#   endif
#  endif
# endif
# if SYS_OS2 || SYS_WINNT
	setup_handler(SIGINT,catchintr);
#endif
#endif

}

static void
siguninit(void)
{
#if SYS_MSDOS
# if CC_DJGPP
	_go32_want_ctrl_break(FALSE);
	want_ctrl_c(FALSE);
	hard_error_teardown();
	setcbrk(TRUE);
# endif
#endif
}

/* do number processing if needed */
static void
do_num_proc(int *cp, int *fp, int *np)
{
	register int c, f, n;
	register int	mflag;
	register int oldn;

	c = *cp;

	if (isCntrl(c) || isspecial(c))
		return;

	f = *fp;
	n = *np;
	if (f)
		oldn = n;
	else
		oldn = 1;
	n = 1;

	if ( isDigit(c) && c != '0' ) {
		n = 0;		/* start with a zero default */
		f = TRUE;	/* there is a # arg */
		mflag = 1;		/* current minus flag */
		while ((isDigit(c) && !isspecial(c)) || (c == '-')) {
			if (c == '-') {
				/* already hit a minus or digit? */
				if ((mflag == -1) || (n != 0))
					break;
				mflag = -1;
			} else {
				n = n * 10 + (c - '0');
			}
			if ((n == 0) && (mflag == -1))	/* lonely - */
				mlwrite("arg:");
			else
				mlwrite("arg: %d",n * mflag);

			c = kbd_seq();	/* get the next key */
		}
		n = n * mflag;	/* figure in the sign */
	}
	*cp = c;
	*fp = f;
	*np = n * oldn;
}

/* do ^U-style repeat argument processing -- vile binds this to 'K' */
static void
do_rept_arg_proc(int *cp, int *fp, int *np)
{
	register int c, f, n;
	register int	mflag;
	register int	oldn;
	c = *cp;

	if (c != reptc)
		return;

	f = *fp;
	n = *np;

	if (f)
		oldn = n;
	else
		oldn = 1;

	n = 4;		/* start with a 4 */
	f = TRUE;	/* there is a # arg */
	mflag = 0;			/* that can be discarded. */
	mlwrite("arg: %d",n);
	while ( (isDigit( c=kbd_seq() ) && !isspecial(c)) 
			|| c==reptc || c=='-'){
		if (c == reptc) {
			/* wow.  what does this do?  -pgf */
			/* (i've been told it controls overflow...) */
			if ((n > 0) == ((n*4) > 0))
				n = n*4;
			else
				n = 1;
		}
		/*
		 * If dash, and start of argument string, set arg.
		 * to -1.  Otherwise, insert it.
		 */
		else if (c == '-') {
			if (mflag)
				break;
			n = 0;
			mflag = -1;
		}
		/*
		 * If first digit entered, replace previous argument
		 * with digit and set sign.  Otherwise, append to arg.
		 */
		else {
			if (!mflag) {
				n = 0;
				mflag = 1;
			}
			n = 10*n + c - '0';
		}
		mlwrite("arg: %d", (mflag >=0) ? n : (n ? -n : -1));
	}
	/*
	 * Make arguments preceded by a minus sign negative and change
	 * the special argument "^U -" to an effective "^U -1".
	 */
	if (mflag == -1) {
		if (n == 0)
			n++;
		n = -n;
	}

	*cp = c;
	*fp = f;
	*np = n * oldn;
}

/* handle all repeat counts */
void
do_repeats(int *cp, int *fp, int *np)
{
	do_num_proc(cp,fp,np);
	do_rept_arg_proc(cp,fp,np);
	if (dotcmdmode == PLAY) {
		if (dotcmdarg)	/* then repeats are done by dotcmdcnt */
			*np = 1;
	} else {
		/* then we want to cancel any dotcmdcnt repeats */
		if (*fp) dotcmdarg = FALSE;
	}
}

/* the vi ZZ command -- write all, then quit */
int
zzquit(int f, int n)
{
	int thiscmd;
	int cnt;
	BUFFER *bp;

	thiscmd = lastcmd;
	cnt = any_changed_buf(&bp);
	if (cnt) {
	    	if (cnt > 1) {
		    mlprompt("Will write %d buffers.  %s ", cnt,
			    clexec ? s_NULL : "Repeat command to continue.");
		} else {
		    mlprompt("Will write buffer \"%s\".  %s ",
			    bp->b_bname,
			    clexec ? s_NULL : "Repeat command to continue.");
		}
		if (!clexec && !isnamedcmd) {
			if (thiscmd != kbd_seq())
				return FALSE;
		}

		if (writeall(f,n,FALSE,TRUE,FALSE) != TRUE) {
			return FALSE;
		}

	} else if (!clexec && !isnamedcmd) {
		/* consume the next char. anyway */
		if (thiscmd != kbd_seq())
			return FALSE;
	}
	return quit(f, n);
}

/*
 * Fancy quit command, as implemented by Norm. If the any buffer has
 * changed do a write on that buffer and exit, otherwise simply exit.
 */
int
quickexit(int f, int n)
{
	register int status;
	if ((status = writeall(f,n,FALSE,TRUE,FALSE)) == TRUE)
		status = quithard(f, n);  /* conditionally quit	*/
	return status;
}

/* Force quit by giving argument */
/* ARGSUSED */
int
quithard(int f GCC_UNUSED, int n GCC_UNUSED)
{
	return quit(TRUE,1);
}

/*
 * Quit command. If an argument, always quit. Otherwise confirm if a buffer
 * has been changed and not written out.
 */
/* ARGSUSED */
int
quit(int f, int n GCC_UNUSED)
{
	int cnt;
	BUFFER *bp;
	const char *sadj, *sverb;

#if OPT_PROCEDURES
	{
	    static int exithooking;
	    if (!exithooking && *exithook) {
		    exithooking = TRUE;
		    run_procedure(exithook);
		    exithooking = FALSE;
	    }
	}
#endif

	if (f == FALSE) {
		cnt = any_changed_buf(&bp);
		sadj = "modified";
		sverb = "Write";
		if (cnt == 0 && global_g_val(GMDWARNUNREAD)) {
			cnt = any_unread_buf(&bp);
			sadj = "unread";
			sverb = "Look at";
		}
		if (cnt != 0) {
			if (cnt == 1)
				mlforce(
				"Buffer \"%s\" is %s.  %s it, or use :q!",
					bp->b_bname,sadj,sverb);
			else
				mlforce(
			  "There are %d %s buffers.  %s them, or use :q!",
					cnt,sadj,sverb);
			return FALSE;
		}
	}
#if OPT_LCKFILES
	/* Release all placed locks */
	if ( global_g_val(GMDUSEFILELOCK) ) {
		for_each_buffer(bp) {
			if ( bp->b_active ) {
				if (!b_val(curbp,MDLOCKED) &&
						!b_val(curbp,MDVIEW))
					release_lock(bp->b_fname);
			}
		}
	}
#endif
	siguninit();
#if OPT_WORKING
	setup_handler(SIGALRM, SIG_IGN);
#if NEEDED	/* i'm not sure when we'd end up with a "working..."
			left on the line, and if we _do_ need to
			clear it, i'd like to figure out how to
			clear just that, so the last message written
			by the editor doesn't get tromped */
	/* force the message line clear */
	mpresf = 1;
	mlerase();
#endif
#endif
#if NO_LEAKS
	{
		beginDisplay();		/* ...this may take a while... */

		/* free all of the global data structures */
		onel_leaks();
		path_leaks();
		kbs_leaks();
		bind_leaks();
		map_leaks();
		itb_leaks();
		tb_leaks();
		wp_leaks();
		bp_leaks();
		vt_leaks();
		ev_leaks();
		mode_leaks();
#if DISP_X11
		x11_leaks();
#endif

		free_local_vals(g_valnames, global_g_values.gv, global_g_values.gv);
		free_local_vals(b_valnames, global_b_values.bv, global_b_values.bv);
		free_local_vals(w_valnames, global_w_values.wv, global_w_values.wv);

		FreeAndNull(gregexp);
		FreeAndNull(patmatch);
#if	OPT_MLFORMAT
    		FreeAndNull(modeline_format);
#endif

#if SYS_UNIX
		if (strcmp(exec_pathname, "."))
			FreeAndNull(exec_pathname);
#endif
		/* whatever is left over must be a leak */
		show_alloc();
	}
#endif
	tidy_exit(GOODEXIT);
	/* NOTREACHED */
	return FALSE;
}

/* ARGSUSED */
int
writequit(int f GCC_UNUSED, int n)
{
	int s;
	s = filesave(FALSE,n);
	if (s != TRUE)
		return s;
	return quit(FALSE,n);
}

/*
 * Abort.
 * Beep the beeper. Kill off any keyboard macro, etc., that is in progress.
 * Sometimes called as a routine, to do general aborting of stuff.
 */
/* ARGSUSED */
int
esc_func(int f GCC_UNUSED, int n GCC_UNUSED)
{
	dotcmdmode = STOP;
	regionshape = EXACT;
	doingopcmd = FALSE;
	doingsweep = FALSE;
	sweephack = FALSE;
	opcmd = 0;
	mlwarn("[Aborted]");
	return ABORT;
}

/* tell the user that this command is illegal while we are in
   VIEW (read-only) mode				*/

int
rdonly(void)
{
	mlwarn("[No changes are allowed while in \"view\" mode]");
	return FALSE;
}

/* ARGSUSED */
int
unimpl(int f GCC_UNUSED, int n GCC_UNUSED)
{
	mlwarn("[Sorry, that vi command is unimplemented in vile ]");
	return FALSE;
}

int
opercopy(int f, int n)
{
	return unimpl(f,n);
}

int
opermove(int f, int n)
{
	return unimpl(f,n);
}

int
opertransf(int f, int n)
{
	return unimpl(f,n);
}

int
operglobals(int f, int n)
{
	return unimpl(f,n);
}

int
opervglobals(int f, int n)
{
	return unimpl(f,n);
}

int
source(int f, int n)
{
	return unimpl(f,n);
}

int
visual(int f, int n)
{
	return unimpl(f,n);
}

int
ex(int f, int n)
{
	return unimpl(f,n);
}

/* ARGSUSED */
int
nullproc(int f GCC_UNUSED, int n GCC_UNUSED)	/* user function that does (almost) NOTHING */
{
	return TRUE;
}

/* ARGSUSED */
int
cntl_a_func(int f GCC_UNUSED, int n GCC_UNUSED)	/* dummy function for binding to control-a prefix */
{
	return TRUE;
}

/* ARGSUSED */
int
cntl_x_func(int f GCC_UNUSED, int n GCC_UNUSED)	/* dummy function for binding to control-x prefix */
{
	return TRUE;
}

/* ARGSUSED */
int
poundc_func(int f GCC_UNUSED, int n GCC_UNUSED)	/* dummy function for binding to poundsign prefix */
{
	return TRUE;
}

/* ARGSUSED */
int
unarg_func(int f GCC_UNUSED, int n GCC_UNUSED) /* dummy function for binding to universal-argument */
{
	return TRUE;
}


/* initialize our version of the "chartypes" stuff normally in ctypes.h */
/* also called later, if charset-affecting modes change, for instance */
void
charinit(void)
{
	register int c;

	(void)memset((char *)_chartypes_, 0, sizeof(_chartypes_));

	/* legal in pathnames */
	_chartypes_['.'] =
		_chartypes_['_'] =
		_chartypes_['~'] =
		_chartypes_['-'] =
		_chartypes_['*'] =
		_chartypes_['/'] = _pathn;

	/* legal in "identifiers" */
	_chartypes_['_'] |= _ident|_qident;
	_chartypes_[':'] |= _qident;
#if SYS_VMS
	_chartypes_['$'] |= _ident|_qident;
#endif

	/* whitespace */
	_chartypes_[' '] =
#if OPT_ISO_8859
		_chartypes_[0xa0] =
#endif
		_chartypes_['\t'] =
		_chartypes_['\r'] =
		_chartypes_['\n'] =
		_chartypes_['\f'] = _space;

	/* control characters */
	for (c = 0; c < ' '; c++)
		_chartypes_[c] |= _cntrl;
	_chartypes_[127] |= _cntrl;

	/* lowercase */
	for (c = 'a'; c <= 'z'; c++)
		_chartypes_[c] |= _lower|_pathn|_ident|_qident;
#if OPT_ISO_8859
	for (c = 0xc0; c <= 0xd6; c++)
		_chartypes_[c] |= _lower|_pathn|_ident|_qident;
#endif
	for (c = 0xd8; c <= 0xde; c++)
		_chartypes_[c] |= _lower|_pathn|_ident|_qident;
	/* uppercase */
	for (c = 'A'; c <= 'Z'; c++)
		_chartypes_[c] |= _upper|_pathn|_ident|_qident;
#if OPT_ISO_8859
	for (c = 0xdf; c <= 0xf6; c++)
		_chartypes_[c] |= _upper|_pathn|_ident|_qident;
	for (c = 0xf8; c <= 0xff; c++)
		_chartypes_[c] |= _upper|_pathn|_ident|_qident;
#endif

	/* digits */
	for (c = '0'; c <= '9'; c++)
		_chartypes_[c] |= _digit|_pathn|_ident|_qident|_linespec;

	/* punctuation */
	for (c = '!'; c <= '/'; c++)
		_chartypes_[c] |= _punct;
	for (c = ':'; c <= '@'; c++)
		_chartypes_[c] |= _punct;
	for (c = '['; c <= '`'; c++)
		_chartypes_[c] |= _punct;
	for (c = LBRACE; c <= '~'; c++)
		_chartypes_[c] |= _punct;
#if OPT_ISO_8859
	for (c = 0xa1; c <= 0xbf; c++)
		_chartypes_[c] |= _punct;
#endif

	/* printable */
	for (c = ' '; c <= '~'; c++)
		_chartypes_[c] |= _print;
	c = global_g_val(GVAL_PRINT_LOW);
	if (c < HIGHBIT) c = HIGHBIT;
	while ( c <= global_g_val(GVAL_PRINT_HIGH) && c < N_chars)
		_chartypes_[c++] |= _print;

	/* backspacers: ^H, rubout */
	_chartypes_['\b'] |= _bspace;
	_chartypes_[127] |= _bspace;

	/* wildcard chars for most shells */
	_chartypes_['*'] |= _wild;
	_chartypes_['?'] |= _wild;
#if !OPT_VMS_PATH
#if SYS_UNIX
	_chartypes_['~'] |= _wild;
#endif
	_chartypes_[LBRACK] |= _wild;
	_chartypes_[RBRACK] |= _wild;
	_chartypes_[LBRACE] |= _wild;
	_chartypes_[RBRACE] |= _wild;
	_chartypes_['$'] |= _wild;
	_chartypes_['`'] |= _wild;
#endif

	/* ex mode line specifiers */
	_chartypes_[','] |= _linespec;
	_chartypes_['%'] |= _linespec;
	_chartypes_['-'] |= _linespec;
	_chartypes_['+'] |= _linespec;
	_chartypes_['.'] |= _linespec;
	_chartypes_['$'] |= _linespec;
	_chartypes_['\''] |= _linespec;

	/* fences */
	_chartypes_[LBRACE] |= _fence;
	_chartypes_[RBRACE] |= _fence;
	_chartypes_[LPAREN] |= _fence;
	_chartypes_[RPAREN] |= _fence;
	_chartypes_[LBRACK] |= _fence;
	_chartypes_[RBRACK] |= _fence;

#if OPT_VMS_PATH
	_chartypes_[LBRACK] |= _pathn;	/* actually, "<", ">" too */
	_chartypes_[RBRACK] |= _pathn;
	_chartypes_['$'] |= _pathn;
	_chartypes_[':'] |= _pathn;
	_chartypes_[';'] |= _pathn;
#endif

#if OPT_MSDOS_PATH
	_chartypes_['\\'] |= _pathn;
	_chartypes_[':'] |= _pathn;
#endif

#if OPT_WIDE_CTYPES
	/* scratch-buffer-names (usually superset of _pathn) */
	_chartypes_[(unsigned)SCRTCH_LEFT[0]]  |= _scrtch;
	_chartypes_[(unsigned)SCRTCH_RIGHT[0]] |= _scrtch;
	_chartypes_[' '] |= _scrtch;	/* ...to handle "[Buffer List]" */
#endif

	for (c = 0; c < N_chars; c++) {
#if OPT_WIDE_CTYPES
		if (isSpace(c) || isPrint(c))
			_chartypes_[c] |= _shpipe;
		if (ispath(c))
			_chartypes_[c] |= _scrtch;
#endif
		if ((_chartypes_[c] & _space) == 0)
			_chartypes_[c] |= _nonspace;
	}

}

/*****		Compiler specific Library functions	****/


#if	OPT_RAMSIZE
/*	These routines will allow me to track memory usage by placing
	a layer on top of the standard system malloc() and free() calls.
	with this code defined, the environment variable, $RAM, will
	report on the number of bytes allocated via malloc.

	with SHOWRAM defined, the number is also posted on the
	end of the bottom mode line and is updated whenever it is changed.
*/

#undef	realloc
#undef	malloc
#undef	free

	/* display the amount of RAM currently malloc'ed */
static void
display_ram_usage (void)
{
	beginDisplay();
	if (global_g_val(GMDRAMSIZE)) {
		char mbuf[20];
		int	saverow = ttrow;
		int	savecol = ttcol;

		if (saverow >= 0 && saverow < term.t_nrow
		 && savecol >= 0 && savecol < term.t_ncol) {
			movecursor(term.t_nrow-1, LastMsgCol);
#if	OPT_COLOR
			TTforg(gfcolor);
			TTbacg(gbcolor);
#endif
			(void)lsprintf(mbuf, "[%ld]", envram);
			kbd_puts(mbuf);
			movecursor(saverow, savecol);
			TTflush();
		}
	}
	endofDisplay();
}

	/* reallocate mp with nbytes and track */
char *reallocate(char *mp, unsigned nbytes)
{
	if (mp != 0) {
		mp -= sizeof(SIZE_T);
		envram -= *((SIZE_T *)mp);
		nbytes += sizeof(SIZE_T);
		mp = realloc(mp, nbytes);
		if (mp != 0) {
			*((SIZE_T *)mp) = nbytes;
			envram += nbytes;
		}
		display_ram_usage();
	} else
		mp = allocate(nbytes);
	return mp;
}

	/* allocate nbytes and track */
char *allocate(
unsigned nbytes)	/* # of bytes to allocate */
{
	char *mp;	/* ptr returned from malloc */

	nbytes += sizeof(SIZE_T);
	if ((mp = malloc(nbytes)) != 0) {
		(void)memset(mp, 0, nbytes);	/* so we can use for calloc */
		*((SIZE_T *)mp) = nbytes;
		envram += nbytes;
		mp += sizeof(SIZE_T);
		display_ram_usage();
	}

	return mp;
}

	/* release malloced memory and track */
void
release(char *mp)	/* chunk of RAM to release */
{
	if (mp) {
		mp -= sizeof(SIZE_T);
		envram -= *((SIZE_T *)mp);
		free(mp);
		display_ram_usage();
	}
}
#endif	/* OPT_RAMSIZE */

#if MALLOCDEBUG
mallocdbg(int f, int n)
{
	int lvl;
	lvl = malloc_debug(n);
	mlwrite("malloc debug level was %d",lvl);
	if (!f) {
		malloc_debug(lvl);
	} else if (n > 2) {
		malloc_verify();
	}
	return TRUE;
}
#endif


/*
 *	the log file is left open, unbuffered.  thus any code can do
 *
 * 	extern FILE *FF;
 *	fprintf(FF, "...", ...);
 *
 *	to log events without disturbing the screen
 */

#ifdef DEBUGLOG
/* suppress the declaration so that the link will fail if someone uses it */
FILE *FF;
#endif

/*ARGSUSED*/
static void
start_debug_log(int ac GCC_UNUSED, char **av GCC_UNUSED)
{
#ifdef DEBUGLOG
	int i;
	FF = fopen("vilelog", "w");
	setbuf(FF,NULL);
	for (i = 0; i < ac; i++)
		(void)fprintf(FF,"arg %d: %s\n",i,av[i]);
#endif
}

#if SYS_MSDOS

#if CC_TURBO
int
showmemory(int f, int n)
{
	extern	long	coreleft(void);
	mlforce("Memory left: %D bytes", coreleft());
	return TRUE;
}
#endif

#if CC_WATCOM
int
showmemory(int f, int n)
{
	mlforce("Watcom C doesn't provide a very useful 'memory-left' call.");
	return TRUE;
}
#endif

#if CC_DJGPP
int
showmemory(int f, int n)
{
	mlforce("Memory left: %D Kb virtual, %D Kb physical",
			_go32_dpmi_remaining_virtual_memory()/1024,
			_go32_dpmi_remaining_physical_memory()/1024);
	return TRUE;
}
#endif
#endif /* SYS_MSDOS */

char *
strncpy0(char *t, const char *f, SIZE_T l)
{
    (void)strncpy(t, f, l);
    if (l)
	t[l-1] = EOS;
    return t;
}

#if defined(SA_RESTART)
/* several systems (SCO, SunOS) have sigaction without SA_RESTART */
/*
 * Redefine signal in terms of sigaction for systems which have the
 * SA_RESTART flag defined through <signal.h>
 *
 * This definition of signal will cause system calls to get restarted for a
 * more BSD-ish behavior.  This will allow us to use the OPT_WORKING feature
 * for such systems.
 */

void
setup_handler(int sig, void (*disp) (int ACTUAL_SIG_ARGS))
{
    struct sigaction act, oact;

    act.sa_handler = disp;
    sigemptyset(&act.sa_mask);
#ifdef SA_NODEFER	/* don't rely on it.  if it's not there, signals
    				probably aren't deferred anyway. */
    act.sa_flags = SA_RESTART|SA_NODEFER ;
#else
    act.sa_flags = SA_RESTART;
#endif

    (void)sigaction(sig, &act, &oact);

}
#else
void
setup_handler(int sig, void (*disp) (int ACTUAL_SIG_ARGS))
{
    (void)signal(sig, disp);
}
#endif


/* put us in a new process group, on command.  we don't do this all the
* time since it interferes with suspending xvile on some systems with some
* shells.  but we _want_ it other times, to better isolate us from signals,
* and isolate those around us (like buggy window/display managers) from
* _our_ signals.  so we punt, and leave it up to the user.
*/
/* ARGSUSED */
int
newprocessgroup(int f GCC_UNUSED, int n GCC_UNUSED)
{
#if DISP_X11

    int pid;

    if (f) {
#ifndef VMS
	    pid = fork();
#else
            pid = vfork();
#endif

	    if (pid > 0)
		tidy_exit(GOODEXIT);
	    else if (pid < 0) {
		fputs("cannot fork\n", stderr);
		tidy_exit(BADEXIT);
	    }
    }
# ifndef VMS
#  ifdef HAVE_SETSID
     (void)setsid();
#  else 
#   ifdef HAVE_BSD_SETPGRP
     (void) setpgrp(0, 0);
#   else
     (void)setpgrp();
#   endif /* HAVE_BSD_SETPGRP */
#  endif /* HAVE_SETSID */
# endif /* VMS */
#endif /* DISP_X11 */
    return TRUE;
}


static int
cmd_mouse_motion(const CMDFUNC *cfp)
{
#if (OPT_XTERM || DISP_X11)
	return cfp && cfp->c_func == mouse_motion;
#else
	return FALSE;
#endif
}
