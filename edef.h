/*
 * edef.h - some global variable definitions for vile
 *
 *  based on microemacs code worked originally by Dave G. Conroy,
 *  modified by Steve Wilhite, George Jones, Daniel Lawrence.
*/

/*
 * $Header: /users/source/archives/vile.vcs/RCS/edef.h,v 1.291 2001/12/21 12:36:24 tom Exp $
 */

#ifndef VILE_EDEF_H
#define VILE_EDEF_H 1

#ifdef __cplusplus
extern "C" {
#endif

/* I know this declaration stuff is really ugly, and I probably won't ever
 *	do it again.  promise.  but it _does_ make it easy to add/change
 *	globals. -pgf
 */
#ifdef realdef
# define decl_init_const(thing,value) EXTERN_CONST thing = value
# define decl_init(thing,value) thing = value
# define decl_uninit(thing) thing
#else
# define decl_init_const(thing,value) extern const thing
# define decl_init(thing,value) extern thing
# define decl_uninit(thing) extern thing
#endif

decl_uninit( char *prog_arg );		/* argv[0] from main.c */
decl_init( char *exec_pathname, ".");	/* replaced at runtime with path-head of argv[0] */

#ifndef PROGRAM_NAME
# if DISP_X11
# define PROGRAM_NAME "xvile"
# else
#  if DISP_NTWIN
# define PROGRAM_NAME "winvile"
#  else
# define PROGRAM_NAME "vile"
#  endif
# endif
#endif /* PROGRAM_NAME */

decl_init( char prognam[], PROGRAM_NAME);
decl_init( char version[], "version 9.2");

#ifdef SYSTEM_NAME
decl_init( char opersys[], SYSTEM_NAME);
#else
#if SYS_UNIX
decl_init( char opersys[], "unix");
#endif
#if SYS_VMS
decl_init( char opersys[], "vms");
#endif
#if SYS_MSDOS
decl_init( char opersys[], "dos");
#endif
#if SYS_OS2
decl_init( char opersys[], "os/2");
#endif
#if SYS_WINNT
decl_init( char opersys[], "win32");
#endif
#endif

#if SYS_WINNT
decl_uninit(int nowait_pipe_cmd);	/* if set, don't slowreadf() this pipe
					 * command.
					 */
#endif

decl_uninit( int am_interrupted );	/* have we been interrupted/ */


decl_init( int autoindented , -1);	/* how many chars (not cols) indented */
decl_uninit( int isnamedcmd );		/* are we typing a command name */
decl_uninit( int calledbefore );	/* called before during this command? */
decl_uninit( CHARTYPE vl_chartypes_[N_chars] );	/* character types	*/
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
#if OPT_LINE_ATTRS
decl_uninit( LINE_ATTR_ENTRY line_attr_tbl[N_chars] );
					/* Line attribute hash table	*/
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
decl_uninit( int currow );		/* Cursor row			*/
decl_uninit( int curcol );		/* Cursor column		*/
decl_uninit( WINDOW *curwp );		/* Current window		*/
decl_uninit( BUFFER *curbp );		/* Current buffer		*/
decl_uninit( WINDOW *wheadp );		/* Head of list of windows      */
decl_uninit( BUFFER *bheadp );		/* Head of list of buffers      */

decl_uninit( WINDOW *wminip );		/* window for command-line      */
decl_uninit( BUFFER *bminip );		/* buffer for command-line      */

decl_uninit( TBUFF *tb_save_shell[2] );	/* last ":!" or ^X-!  command	*/

decl_uninit( char screen_desc[NBUFN] );	/* rough description of screen  */

decl_init( TBUFF *mlsave, 0 );		/* last message, if postponed	*/
decl_uninit( char searchpat[NPAT] );	/* Search pattern		*/
decl_init( TBUFF *replacepat, 0 );	/* replacement pattern		*/
decl_uninit( int  last_srch_direc );	/* Direction of last search */
decl_uninit( regexp *gregexp );		/* compiled version of searchpat */
decl_init( TBUFF *tb_matched_pat, 0);	/* text that scan found */
decl_init( TBUFF *lastfileedited, 0);

#if OPT_HOOKS
decl_uninit( HOOK cdhook );		/* proc to run when change dir */
decl_uninit( HOOK readhook );		/* proc to run when read file  */
decl_uninit( HOOK writehook );		/* proc to run when write file */
decl_uninit( HOOK bufhook );		/* proc to run when change buf */
decl_uninit( HOOK exithook );		/* proc to run when exiting */
decl_uninit( HOOK autocolorhook );	/* proc to run for autocoloring */
decl_uninit( HOOK majormodehook );	/* proc to run for majormodes */
#endif

decl_uninit( int ignorecase );

decl_init( int curgoal, -1 );		/* column goal			*/
decl_uninit( TBUFF *prompt_string );	/* command-line prompt-string	*/
decl_uninit( char *execstr );		/* string being executed	*/
#if OPT_MLFORMAT
decl_uninit( char *modeline_format );	/* modeline formatting string */
#endif
#if OPT_POSFORMAT
decl_uninit( char *position_format );	/* position formatting string */
#endif
decl_init( int	eolexist, TRUE );	/* does clear to EOL exist	*/
decl_uninit( int revexist );		/* does reverse video exist?	*/
decl_uninit( int curtabval );		/* current tab width		*/

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

decl_init( int sgarbf, TRUE );		/* TRUE if screen is garbage	*/
decl_init( int need_update, TRUE );	/* TRUE if screen is not updated*/
decl_uninit( int clexec	);		/* command line execution flag	*/
decl_uninit( int clhide );		/* hide results of this command	*/
decl_uninit( int miniedit );		/* editing minibuffer with vi-cmds */
decl_init( int vl_msgs, TRUE);		/* suppress command output?	*/
decl_uninit( int no_errs);		/* suppress bells/alarms?	*/
decl_init( int vl_echo, TRUE);		/* echo user input 		*/
decl_init( int qpasswd, FALSE);		/* querying for password	*/
decl_init( int in_autocolor, FALSE );	/* Autocoloring			*/

decl_uninit( int vtrow );		/* Row location of SW cursor	*/
decl_uninit( int vtcol );		/* Column location of SW cursor */
decl_init( int ttrow, VL_HUGE );	/* Row location of HW cursor	*/
decl_init( int ttcol, VL_HUGE );	/* Column location of HW cursor */
decl_uninit( int horscroll );		/* line offset when displaying	*/
decl_init( int ntildes, 100 );		/* number of tildes displayed at eob
					  (expressed as percent of window) */
#if OPT_COLOR
decl_init( int ncolors, NCOLORS );	/* total number of colors displayable */
#if DISP_NTWIN
decl_init( int rgb_gray, 140 );
decl_init( int rgb_normal, 180 );
decl_init( int rgb_bright, 255 );
#endif
#endif

#if OPT_TITLE
decl_init( int auto_set_title, TRUE );	/* automatically set title	*/
decl_init ( TBUFF * title_format, 0 );	/* format, if any		*/
#endif

/* Special characters, used in keyboard control (some values are set on
 * initialization in termio.c).
 */

/* these five are rebindable in the "usual" way */
decl_init( int cntl_a, tocntrl('A') );	/* rebindable control-A prefix	*/
decl_init( int cntl_x, tocntrl('X') );	/* rebindable control-X prefix	*/
decl_init( int reptc, 'K' );		/* rebindable "repeat" arg	*/
decl_init( int esc_c, tocntrl('[') );	/* rebindable vi esc char	*/
decl_init( int poundc, '#' );		/* pseudo function key prefix	*/

/* rebindable via the "set-terminal" interface */
decl_init( int editc, tocntrl('G') );	/* toggle edit-mode in minibuffer */
decl_init( int quotec, tocntrl('V') );	/* current quoting char		*/
decl_init( int killc, tocntrl('U') );	/* current line kill char	*/
decl_init( int wkillc, tocntrl('W') );	/* current word kill char	*/
decl_init( int intrc, tocntrl('C') );	/* current interrupt char	*/
decl_init( int suspc, tocntrl('Z') );	/* current suspend char	*/
decl_init( int startc, tocntrl('Q') );	/* current output start char	*/
decl_init( int stopc, tocntrl('S') );	/* current output stop char	*/
decl_init( int backspc, '\b');		/* current backspace char	*/
decl_init( int name_cmpl, '\t');	/* do name-completion		*/
decl_init( int test_cmpl, '?');		/* show name-completion		*/

decl_uninit( KILLREG kbs[NKREGS] );	/* all chars, 1 thru 9, and default */
decl_uninit( short ukb );		/* index of current kbuffs */
decl_uninit( USHORT kregflag );		/* info for pending kill into reg */
decl_uninit( C_NUM kregwidth );		/* max width of current kill */
decl_uninit( int kchars );		/* how much did we kill? */
decl_uninit( int klines );
decl_uninit( int lines_deleted );	/* from 'ldelete()', for reporting */
decl_uninit( int warnings );		/* from 'mlwarn()', for reporting */

#if !SMALLER
decl_uninit( WINDOW *swindow );		/* saved window pointer		*/
decl_init( TBUFF *with_prefix, 0);	/* prefix set by "~with"	*/
#endif

#if OPT_ENCRYPT
decl_init( char * cryptkey, 0 );	/* top-level crypt-key, if any	*/
#endif

decl_uninit( int dotcmdactive );	/* current dot command mode	*/
decl_init( int dotcmdarg, FALSE);	/* was there an arg to '.'? */
decl_uninit( short dotcmdkreg);		/* original dot command kill reg */
decl_uninit( ITBUFF *dotcmd );		/* recorded-text of dot-commands */
decl_uninit( int dotcmdcnt );		/* down-counter for dot-commands */
decl_uninit( int dotcmdrep );		/* original dot-command repeat-count */

#if OPT_EVAL
decl_init( int seed, 123 );		/* random number seed		*/
#endif

decl_init( int cmd_count, 0 );		/* 1..n for procedure execution	*/

#if OPT_EVAL || OPT_DEBUGMACROS
decl_init( int tracemacros, FALSE );	/* macro tracing flag		*/
#endif

#if OPT_WORKING
decl_uninit( B_COUNT max_working );	/* 100% value for slowreadf	*/
decl_uninit( B_COUNT cur_working );	/* current-value for slowreadf	*/
decl_uninit( B_COUNT old_working );	/* previous-value for slowreadf	*/
#endif
decl_uninit( int vile_is_busy );	/* disabling flag, e.g., working */
decl_uninit( int signal_was );		/* what was the last signal */

#if OPT_SHELL
decl_init ( int cd_on_open, FALSE );
#endif

	/* These pointers are nonnull only while animating a given buffer or
	 * window.  They are used to obtain local mode-values.
	 */
#if OPT_UPBUFF
decl_uninit( struct VAL *relisting_b_vals );
decl_uninit( struct VAL *relisting_w_vals );
#endif

decl_init( char out_of_mem[], "Out of Memory" );
decl_init( char error_val[], "ERROR" );

#if OPT_EVAL || OPT_COLOR
decl_uninit( TBUFF *tb_curpalette );	/* current colormap palete	*/
#endif
#if OPT_COLOR
decl_uninit( int ctrans[NCOLORS] );	/* color translation table	*/
#endif
decl_uninit( int kbd_expand );		/* -1 kbd_putc shows tab as space */
					/* +1 kbd_putc shows cr as ^M */


decl_uninit( FILE *ffp );		/* File pointer, all functions. */
decl_uninit( char *fflinebuf );		/* current buffer for file reads */
decl_uninit( ALLOC_T fflinelen );	/* fflinebuf length */
decl_uninit( int count_fline );		/* # of lines read with 'ffgetline()' */

/*--------------------------------------------------------------------------*/

decl_uninit( int fileispipe );
decl_uninit( int fileeof );		/* found eof */
decl_init ( L_NUM help_at, -1 );	/* position in help-file */
decl_uninit( char *helpfile );
decl_init( char vl_pathsep, PATHCHR );	/* $pathlist-separator */

decl_uninit( char *startup_file );
decl_uninit( char *startup_path );

decl_uninit( char *libdir_path );

decl_init_const( char hexdigits[], "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
#if SYS_WINNT
decl_init_const( char ctrldigits[], "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_");
#endif

/*--------------------------------------------------------------------------*/

decl_init_const( char HELP_BufName[],		"[Help]");
decl_init_const( char BINDINGLIST_BufName[],	"[Binding List]");
decl_init_const( char CMD_BINDINGS_BufName[],	"[Command-Bindings]");
decl_init_const( char INS_BINDINGS_BufName[],	"[Insert-Bindings]");
decl_init_const( char SEL_BINDINGS_BufName[],	"[Select-Bindings]");
#if OPT_REBIND
decl_init_const( char KEY_NAMES_BufName[],	"[Key Names]");
# if OPT_TERMCHRS
decl_init_const( char TERMINALCHARS_BufName[],	"[Terminal Chars]");
# endif
#endif
#if OPT_COLOR_SCHEMES
decl_init_const( char COLOR_SCHEMES_BufName[],	"[Color Schemes]");
#endif
#if OPT_SHELL
decl_init_const( char DIRSTACK_BufName[],	"[DirStack]");
#endif
#if OPT_SHOW_COLORS
decl_init_const( char PALETTE_COLORS_BufName[],	"[Color Palette]");
#endif
#if OPT_SHOW_CTYPE
decl_init_const( char PRINTABLECHARS_BufName[],	"[Printable Chars]");
#endif
#if OPT_POPUPCHOICE
decl_init_const( char COMPLETIONS_BufName[],	"[Completions]");
#endif
decl_init_const( char BUFFERLIST_BufName[],	"[Buffer List]");
#if OPT_SHOW_EVAL
decl_init_const( char VARIABLES_BufName[],	"[Variables]");
#endif
decl_init_const( char MACRO_N_BufName[],	"[Macro %d]");
#if COMPLETE_FILES
decl_init_const( char FILECOMPLETION_BufName[],	"[FileCompletion]");
#endif
#if COMPLETE_DIRS
decl_init_const( char DIRCOMPLETION_BufName[],	"[DirCompletion]");
#endif
decl_init_const( char OUTPUT_BufName[],		"[Output]");
#if OPT_EVAL || OPT_DEBUGMACROS
decl_init_const( char TRACE_BufName[],		"[Trace]");
#endif
#if OPT_FINDERR
decl_init( TBUFF *filename_expr, 0 );
decl_init( TBUFF *error_expr, 0 );
decl_init( TBUFF *error_match, 0 );
decl_init_const( char ERRORS_BufName[],		"[Error Expressions]");
decl_init_const( char ERR_REGEX_BufName[],	"[Error Patterns]");
#endif
#if OPT_HISTORY
decl_init_const( char HISTORY_BufName[],	"[History]");
#endif
#if OPT_SHOW_REGS
decl_init_const( char REGISTERS_BufName[],	"[Registers]");
#endif
decl_init_const( char STDIN_BufName[],		"[Standard Input]");
decl_init_const( char UNNAMED_BufName[],	"[unnamed]");
decl_init_const( char VILEINIT_BufName[],	"[vileinit]");
#if OPT_SHOW_MAPS
decl_init_const( char MAP_BufName[],		"[Map Sequences]");
decl_init_const( char MAPBANG_BufName[],	"[Map! Sequences]");
decl_init_const( char ABBR_BufName[],		"[Abbreviations]");
decl_init_const( char SYSMAP_BufName[],		"[System Maps]");
#else
/* needed anyway, since they're passed around as args */
decl_init_const( char MAP_BufName[],		"");
decl_init_const( char MAPBANG_BufName[],	"");
decl_init_const( char ABBR_BufName[],		"");
decl_init_const( char SYSMAP_BufName[],		"");
#endif
#if OPT_SHOW_MARKS
decl_init_const( char MARKS_BufName[],		"[Named Marks]");
#endif
#if OPT_SHOW_WHICH
decl_init_const( char WHICH_BufName[],		"[Which Files]");
#endif
decl_init_const( char SETTINGS_BufName[],	"[Settings]");
#if OPT_MAJORMODE
decl_init_const( char MAJORMODES_BufName[],	"[Major Modes]");
#endif
#if OPT_POPUP_MSGS
decl_init_const( char MESSAGES_BufName[],	"[Messages]");
#endif
decl_init_const( char P_LINES_BufName[],	"[p-lines]");
#if OPT_SHOW_TAGS
decl_init_const( char TAGSTACK_BufName[],	"[Tag Stack]");
#endif
#if OPT_TAGS
decl_init_const( char TAGFILE_BufName[],	"[Tags %d]");
#endif

/* defined in nebind.h and nename.h */
extern const NTAB nametbl[];
extern const CMDFUNC *asciitbl[];
extern KBIND kbindtbl[];

/* defined in bind.c */
extern BINDINGS dft_bindings;
extern BINDINGS ins_bindings;
extern BINDINGS cmd_bindings;
extern BINDINGS sel_bindings;

/* vars useful for writing procedures that are : commands */
decl_uninit(int ev_end_of_cmd);

/* terminal structure is defined in the configured screen driver */
#ifndef	termdef
extern	TERM	term;			/* Terminal information.	*/
#endif
#if OPT_DUMBTERM
extern	TERM	dumb_term;
#endif
extern	TERM	null_term;

#if DISP_IBMPC || DISP_BORLAND || DISP_VIO
decl_init( char *current_res_name, "default");
#endif	/* IBMPC */

#ifdef __cplusplus
}
#endif

#endif /* VILE_EDEF_H */
