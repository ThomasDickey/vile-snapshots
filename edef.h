/*	EDEF:		Global variable definitions for vile
			

			written for MicroEMACS 3.9 by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			greatly modified by Daniel Lawrence
			modified even more than that by Paul Fox.  honest.
*/

/*
 * $Header: /users/source/archives/vile.vcs/RCS/edef.h,v 1.199 1996/09/10 12:36:57 pgf Exp $
 */

/* I know this declaration stuff is really ugly, and I probably won't ever
 *	do it again.  promise.  but it _does_ make it easy to add/change
 *	globals. -pgf
 */
#ifdef realdef
# define decl_init(thing,value) thing = value
# define decl_uninit(thing) thing
#else
# define decl_init(thing,value) extern thing
# define decl_uninit(thing) extern thing
#endif

decl_uninit( char *prog_arg );		/* argv[0] from main.c */

#if DISP_X11
decl_init( const char prognam[], "xvile");
#else
decl_init( const char prognam[], "vile");
#endif

decl_init( const char version[], "version 6.1");

#ifdef SYSTEM_NAME
decl_init( const char opersys[], SYSTEM_NAME);
#else
#if SYS_UNIX
decl_init( const char opersys[], "unix");
#endif
#if SYS_VMS
decl_init( const char opersys[], "vms");
#endif
#if SYS_MSDOS
decl_init( const char opersys[], "dos");
#endif
#if SYS_WIN31
decl_init( const char opersys[], "windows 3.1");
#endif
#if SYS_OS2
decl_init( const char opersys[], "os/2");
#endif
#if SYS_WINNT
decl_init( const char opersys[], "win32");
#endif
#endif

decl_uninit( int am_interrupted );	/* have we been interrupted/ */


decl_init( int autoindented , -1);	/* how many chars (not cols) indented */
decl_uninit( int isnamedcmd );		/* are we typing a command name */
decl_uninit( int calledbefore );	/* called before during this command? */
decl_uninit( CHARTYPE _chartypes_[N_chars] );	/* character types	*/
decl_uninit( int displaying );		/* flag set during screen updates */
decl_uninit( int doing_kbd_read );	/* flag set during keyboard reading */
decl_uninit( int reading_msg_line );	/* flag set during msgline reading */
decl_uninit( jmp_buf read_jmp_buf );	/* for setjmp/longjmp on SIGINT */
#ifndef insertmode
decl_uninit( int insertmode );		/* are we inserting or overwriting? */
#endif
decl_uninit( int lastkey );		/* last keystoke (tgetc)	*/
decl_uninit( int lastcmd );		/* last command	(kbd_seq)	*/
decl_uninit( REGIONSHAPE regionshape );	/* shape of region		*/
#if OPT_VIDEO_ATTRS
decl_uninit( VIDEO_ATTR videoattribute );
					/* attribute to set in call to
					   attributeregion()		*/
#endif
decl_uninit( int doingopcmd );		/* operator command in progress */
decl_uninit( int doingsweep );		/* operator command in progress */
decl_uninit( int sweephack );		/* don't include dot when sweeping */
decl_uninit( MARK pre_op_dot );		/* current pos. before operator cmd */

decl_uninit( MARK scanboundpos );	/* where do searches end? */
decl_uninit( int scanbound_is_header);	/* is scanboundpos the header line? */

decl_uninit( short opcmd );		/* what sort of operator?	*/
decl_uninit( const CMDFUNC *havemotion ); /* so we can use "oper" routines
					   internally */
decl_uninit( int currow );		/* Cursor row                   */
decl_uninit( int curcol );		/* Cursor column                */
decl_uninit( WINDOW *curwp );		/* Current window               */
decl_uninit( BUFFER *curbp );		/* Current buffer               */
decl_uninit( WINDOW *wheadp );		/* Head of list of windows      */
decl_uninit( BUFFER *bheadp );		/* Head of list of buffers      */

decl_uninit( WINDOW *wminip );		/* window for command-line      */
decl_uninit( BUFFER *bminip );		/* buffer for command-line      */

decl_uninit( TBUFF *save_shell[2] );	/* last ":!" or ^X-!  command	*/

decl_uninit( char sres[NBUFN] );	/* current screen resolution	*/

decl_uninit( char pat[NPAT] );		/* Search pattern		*/
decl_uninit( char rpat[NPAT] );		/* replacement pattern		*/
decl_uninit( int  last_srch_direc );		/* Direction of last search */

#if OPT_PROCEDURES
decl_uninit( char cdhook[NBUFN] );	/* proc to run when change dir */
decl_uninit( char readhook[NBUFN] );	/* proc to run when read file  */
decl_uninit( char writehook[NBUFN] );	/* proc to run when write file */
decl_uninit( char bufhook[NBUFN] );	/* proc to run when change buf */
decl_uninit( char exithook[NBUFN] );	/* proc to run when exiting */
#endif

decl_uninit( regexp *gregexp );		/* compiled version of pat */

/* patmatch holds the string that satisfied the search command.  */
decl_uninit( char *patmatch );

decl_uninit( int ignorecase );

decl_init( int curgoal, -1 );           /* column goal			*/
decl_uninit( const char *execstr );	/* pointer to string to execute	*/
#if OPT_EVAL
decl_uninit( char golabel[NPAT] );	/* current line to go to	*/
#endif
#if OPT_MLFORMAT
decl_uninit( char *modeline_format );	/* modeline formatting string */
#endif
decl_uninit( int execlevel );		/* execution IF level		*/
decl_init( int	eolexist, TRUE );	/* does clear to EOL exist	*/
decl_uninit( int revexist );		/* does reverse video exist?	*/
#if DISP_IBMPC || OPT_EVAL
decl_uninit( int flickcode );		/* do flicker suppression?	*/
#endif
decl_uninit( int curtabval );		/* current tab width		*/
decl_uninit( int curswval );		/* current shiftwidth		*/

#ifdef realdef
	MARK	nullmark = { NULL, 0 };
#else
extern	MARK	nullmark;
#endif

#if ! WINMARK
decl_uninit( MARK Mark );		/* the worker mark */
#endif

/* these get their initial values in main.c, in global_val_init() */
decl_uninit( G_VALUES global_g_values );
decl_uninit( B_VALUES global_b_values );
decl_uninit( W_VALUES global_w_values );

decl_init( int sgarbf, TRUE );          /* TRUE if screen is garbage	*/
decl_uninit( int mpresf );              /* zero if message-line empty	*/
decl_uninit( int clexec	);		/* command line execution flag	*/
decl_uninit( int mstore	);		/* storing text to macro flag	*/
decl_init( int discmd, TRUE );		/* display command flag		*/
decl_init( int disinp, TRUE );		/* display input characters	*/
decl_uninit( struct BUFFER *bstore );	/* buffer to store macro text to*/
decl_uninit( int vtrow );               /* Row location of SW cursor	*/
decl_uninit( int vtcol );               /* Column location of SW cursor */
decl_init( int ttrow, HUGE );           /* Row location of HW cursor	*/
decl_init( int ttcol, HUGE );           /* Column location of HW cursor */
decl_uninit( int taboff	);		/* tab offset for display	*/
decl_init( int ntildes, 100 );		/* number of tildes displayed at eob
					  (expressed as percent of window) */

/* Special characters, used in keyboard control (some values are set on
 * initialization in termio.c).
 */
decl_init( int cntl_a, tocntrl('A') );	/* current meta character	*/
decl_init( int cntl_x, tocntrl('X') );	/* current control X prefix char */
decl_init( int reptc, 'K' );		/* current universal repeat char */
decl_init( int abortc, tocntrl('[') );	/* ESC: current abort command char */
decl_init( int poundc, '#' );		/* pseudo function key prefix */
decl_init( int quotec, tocntrl('V') );	/* quote char during mlreply()	*/
decl_init( int killc, tocntrl('U') );	/* current line kill char	*/
decl_init( int wkillc, tocntrl('W') );	/* current word kill char	*/
decl_init( int intrc, tocntrl('C') );	/* current interrupt char	*/
decl_init( int suspc, tocntrl('Z') );	/* current suspend char	*/
decl_init( int startc, tocntrl('Q') );	/* current output start char	*/
decl_init( int stopc, tocntrl('S') );	/* current output stop char	*/
decl_init( int backspc, '\b');		/* current backspace char	*/
decl_init( int name_cmpl, '\t');	/* do name-completion		*/
decl_init( int test_cmpl, '?');		/* show name-completion		*/

#if OPT_MSDOS_PATH
decl_init( int slashc, '\\');		/* default path delimiter	*/
#endif

decl_uninit( KILLREG kbs[NKREGS] );	/* all chars, 1 thru 9, and default */
decl_uninit( short ukb );		/* index of current kbuffs */
decl_uninit( short kregflag );		/* info for pending kill into reg */
decl_uninit( C_NUM kregwidth );		/* max width of current kill */
decl_uninit( int kchars );		/* how much did we kill? */
decl_uninit( int klines );
decl_uninit( int lines_deleted );	/* from 'ldelete()', for reporting */
decl_uninit( int warnings );		/* from 'mlwarn()', for reporting */

#if !SMALLER
decl_uninit( WINDOW *swindow );		/* saved window pointer		*/
#endif

#if OPT_ENCRYPT
decl_init( int cryptflag, FALSE );	/* currently encrypting?	*/
decl_init( char * cryptkey, 0 );	/* top-level crypt-key, if any	*/
#endif

decl_init( int dotcmdmode, RECORD );	/* current dot command mode	*/
decl_init( int dotcmdarg, FALSE);	/* was there an arg to '.'? */
decl_uninit( short dotcmdkreg);		/* original dot command kill reg */
decl_uninit( ITBUFF *dotcmd );		/* recorded-text of dot-commands */
decl_uninit( int dotcmdcnt );		/* down-counter for dot-commands */
decl_uninit( int dotcmdrep );		/* original dot-command repeat-count */

decl_init( int	kbdmode, STOP );	/* current keyboard macro mode	*/
#if OPT_EVAL
decl_uninit( int seed );		/* random number seed		*/
#endif

#if OPT_RAMSIZE
decl_uninit( long envram );		/* # of bytes current used malloc */
#endif

#if OPT_EVAL || OPT_DEBUGMACROS
decl_uninit( int macbug );		/* macro debugging flag		*/
#endif

#if OPT_WORKING
decl_uninit( B_COUNT max_working );	/* 100% value for slowreadf	*/
decl_uninit( B_COUNT cur_working );	/* current-value for slowreadf	*/
decl_uninit( B_COUNT old_working );	/* previous-value for slowreadf	*/
decl_uninit( int no_working );		/* disabling flag */
#endif
decl_uninit( int signal_was );		/* what was the last signal */

	/* These pointers are nonnull only while animating a given buffer or
	 * window.  They are used to obtain local mode-values.
	 */
#if OPT_UPBUFF
decl_uninit( struct VAL *relisting_b_vals );
decl_uninit( struct VAL *relisting_w_vals );
#endif

decl_init( const char out_of_mem[], "OUT OF MEMORY" );
decl_init( const char errorm[], "ERROR" ); /* error literal		*/
decl_init( const char truem[], "TRUE" );   /* true literal		*/
decl_init( const char falsem[], "FALSE" ); /* false literal		*/

decl_init( int	cmdstatus, TRUE );	/* last command status		*/
#if OPT_EVAL || OPT_COLOR
decl_uninit( char palstr[NSTRING] );	/* palette string		*/
#endif
decl_uninit( char *fline );		/* dynamic return line		*/
decl_uninit( ALLOC_T flen );		/* current length of fline	*/

decl_uninit( int kbd_expand );		/* -1 kbd_putc shows tab as space */
					/* +1 kbd_putc shows cr as ^M */


decl_uninit( FILE *ffp );		/* File pointer, all functions. */
decl_uninit( int fileispipe );
decl_uninit( int eofflag );		/* end-of-file flag */
decl_init ( L_NUM help_at, -1 );	/* position in help-file */

decl_init( const char hexdigits[], "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");

decl_init( const char HELP_BufName[],	 	"[Help]");
#if OPT_REBIND
decl_init( const char BINDINGLIST_BufName[],	"[Binding List]");
# if OPT_TERMCHRS
decl_init( const char TERMINALCHARS_BufName[],	"[Terminal Chars]");
# endif
#endif
#if OPT_POPUPCHOICE
decl_init( const char COMPLETIONS_BufName[],	"[Completions]");
#endif
decl_init( const char BUFFERLIST_BufName[],	"[Buffer List]");
#if OPT_SHOW_EVAL
decl_init( const char VARIABLES_BufName[],	"[Variables]");
#endif
decl_init( const char MACRO_N_BufName[],	"[Macro %d]");
#if COMPLETE_FILES
decl_init( const char FILECOMPLETION_BufName[],	"[FileCompletion]");
#endif
#if COMPLETE_DIRS
decl_init( const char DIRCOMPLETION_BufName[],	"[DirCompletion]");
#endif
decl_init( const char OUTPUT_BufName[],		"[Output]");
#if OPT_FINDERR
decl_init( const char ERRORS_BufName[],		"[Error Expressions]");
#endif
#if OPT_HISTORY
decl_init( const char HISTORY_BufName[],	"[History]");
#endif
#if OPT_SHOW_REGS
decl_init( const char REGISTERS_BufName[],	"[Registers]");
#endif
decl_init( const char STDIN_BufName[],		"[Standard Input]");
decl_init( const char UNNAMED_BufName[],	"[unnamed]");
decl_init( const char VILEINIT_BufName[],	"[vileinit]");
#if OPT_SHOW_MAPS
decl_init( const char MAP_BufName[],		"[Map Sequences]");
decl_init( const char MAPBANG_BufName[],	"[Map! Sequences]");
decl_init( const char ABBR_BufName[],		"[Abbreviations]");
decl_init( const char SYSMAP_BufName[],		"[System Maps]");
#else
/* needed anyway, since they're passed around as args */
decl_init( const char MAP_BufName[],		"");
decl_init( const char MAPBANG_BufName[],	"");
decl_init( const char ABBR_BufName[],		"");
decl_init( const char SYSMAP_BufName[],		"");
#endif
decl_init( const char SETTINGS_BufName[],	"[Settings]");
#if OPT_POPUP_MSGS
decl_init( const char MESSAGES_BufName[],	"[Messages]");
#endif
decl_init( const char P_LINES_BufName[],	"[p-lines]");
#if OPT_SHOW_TAGS
decl_init( const char TAGSTACK_BufName[],	"[Tag Stack]");
#endif
#if OPT_TAGS
decl_init( const char TAGFILE_BufName[],	"[Tags %d]");
#endif

/* defined in nebind.h and nename.h */
extern const NTAB nametbl[];
extern const CMDFUNC *asciitbl[];
extern KBIND kbindtbl[];

/* terminal table defined only in TERM.C */

#ifndef	termdef
extern  TERM    term;                   /* Terminal information.        */
#endif
#if OPT_DUMBTERM
extern	TERM	dumb_term;
#endif
extern	TERM	null_term;

#if DISP_IBMPC || DISP_BORLAND || DISP_VIO
decl_init( char *current_res_name, "default");
#endif	/* IBMPC */
