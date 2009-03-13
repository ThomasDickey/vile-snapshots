
NOTICE: This change log is officially (sez me) closed.
	All further changes documented (hopefully) in "CHANGES".  This
	file was renamed from "CHANGES" to "CHANGES.R3" on Tue Feb 22, 1994.

Changes for vile 3.65: (released Mon Feb 7, 1994)
(pgf)
    Big change this release is 8-bit cleanliness.  See the help file
    for 8-bit settings.  
    
    NOTE: If you had a FN-key binding which was bound to a "meta" (high
    bit) key, you must change your binding to be a M- binding.  FN- and M-
    are no longer synonymous.  In addition, we now preserve more of the
    termio settings, so you may have to explicitly use "stty" to get
    your meta-bit back.  (We used to clear istrip and the parity settings.)

    NOTE2: pc vile, if built with djgpp or watcom compilers, cannot change
    screens to 43 or 50 line mode.  i'm working on it.

	+ fix bug in charprocreg() that allowed a buffer to be marked
	  modified even if no changes were made and there is nothing to
	  undo.  (region.c)

	+ Brackets '[' and ']' are now treated like braces and parentheses
	  when inserting in c-mode. added new #defines for [LR]BRACK to assist
	  this. (insert.c, estruct.h, main.c)

    	+ A buffer's local "dos" mode is now set to the same as the global
	  "dos" mode when empty, fresh, or when dos-style lines equal unix-
	  style lines. (main.c, file.c, random.c)

	+ giving an argument to "set-dos-mode" causes it to turn dos-mode
	  off rather than on. (file.c)

	+ allow backspacing over autoindented whitespace, whether  or not
	  backspacelimit is set. (insert.c)

	+ new commands, ^A-i, ^A-o, and ^A-O, corresponding to i, o, and O,
	  but which do autoindent-override during an insert.  Most useful
	  when pasting into a buffer, since the pasted text often already
	  has leading whitespace, and autoindent just adds more.   this
	  is a different kind of override than used when pasting in xvile.
	  hmmm.  (insert.c, cmdtbl)

	+ fixed core dump resulting from calculating indentlen and checking
	  for '#' char at start of empty line.  (insert.c)

	+ add directory and host names to "vile died" message.  added ifdef
	  and prototype for usage of gethostname.  currently only used #if
	  BERK is true.  (greg mcfarlane) (file.c, proto.h, estruct.h)

	+ apply (greg mcfarlane's) fix for bug in unqname, when it's being
	  used at startup, and can't ask for input from user. (file.c)

	+ only reclaim or destroy empty unmodified buffers if they don't
	  correspond to existing files.  new routine ffexists() to support
	  this. (buffer.c, file.c, fileio,c, proto.h)

	+ changed calls to kbd_key() to tgetc(), since SPEC is now outside
	  the range that callers can handle.  (window.c spawn.c
	  insert.c history.c exec.c eval.c csrch.c bind.c)

	+ changed all references to last1key to refer to lastkey instead,
	  to accompany above change.  last1key is gone. (basic.c, opers.c,
	  edef.h)

	+ kbd_key() now translates system-specific function-key sequences
	  to canonical '#-c' sequences, which kbd_seq() now interprets.
	  (input.c)

	+ allow 8-bit input, by changing SPEC definition, and some table
	  sizes. (estruct.h, input.c, mktbls.c)

	+ translate X key-events to a default set of xvile FN-bindings,
	  without using the SPEC define, since it won't fit through the
	  8-bit wide pipe we've constructed. (x11.c)

	+ added new modes, printing-low and printing-high, which define
	  the range of 8-bit chars which should _not_ be displayed in
	  octal, but rather as themselves.  (modetbl, modes.c, main.c)

	+ change how we display 8-bit chars. (display.c)

	+ new mode, unprintable-as-octal, to control display of non-printing
	  chars in hex or octal. (display.c, modetbl)

	+ new mode, meta-insert-bindings, which allows meta-bindings (M-c)
	  to work while in insert mode.  when off, all characters, meta
	  or not, are simply inserted as themselves.  when on, meta keys
	  which are bound to functions execute those functions, all others
	  insert themselves. (insert.c, modetbl)

	+ tb_get now returns a true 8 bits, no sign-extension.  (tbuff.c)

	+ allow focusFollowsMouse to be disabled by XDefaults (x11.c)

	+ apply fix (kev) for infinite loop on extended lines with certain
	  kinds of tab values. (display.c)

	+ attempt at better video support, and raw i/o, for djgpp (ibmpc.c)

	+ new routines for ctrl-c/ctrl-break/critical-error handling in
	  djgpp environment. (main.c, proto.h, new file djhandl.c, makefile,
	  makefile.djg)

    	+ added DJGPP to the direct/dirent ifdef in dirstuff.h

	+ fix for sys5 tgetent behavior, when attempting to determine if
	  we're an xterm. (tcap.c)

	+ new targets for motorola delta machines (makefile)

 	+ fixes for core dumps on window resize (from kev) (x11.c)

	+ new define for min no. of lines per window. (estruct.h)

	+ change if_OPT_WORKING to simple ifdefs. (file.c fileio.c display.c
	  estruct.h)

	+ added vanillashar target to makefile, so i can produce shars
	  that the gnuish DOS unshar program will unpack.  (makefile)

	+ clarify help file regarding regions as used by the operator
	  commands (vile.hlp)

	+ applied contributed fix for null fontname (x11.c)

	+ changed logic of tags lookup, so i can understand it, and to
	  correct logic surrounding "taglength" (again).  if taglength is
	  0, matches must be exact (i.e.  all characters significant).  if
	  the user enters less than 'taglength' characters, this match must
	  also be exact.  if the user enters 'taglength' or more
	  characters, only that many characters will be significant in the
	  lookup. (tags.c)

	+ fixed bug with wrap-around reverse searching for a string occurring
	  on the last line of a buffer.  (search.c)

	+ fixed bug in search/replace operations where replacement string
	  was of the form 'text\1moretext'.  the length of the \1 replacement
	  pattern was being obtained incorrectly. (oneliner.c)

	+ changed GO32 ifdef to DJGPP, to avoid name conflict with
	  predefinition. (input.c, fileio.c, estruct.h, ibmpc.c, path.c,
	  random.c, termio.c)

	+ changed the 'interrupted' global to a routine, so we can poll for
	  a control-C character when real signals aren't available. (buffer.c,
	  edef.h, fences.c, main.c, search.c, word.c)

(tom)
	+ corrected EVCURCHAR case in 'gtenv()' that tried to return the
	  characters at the current position, even when the buffer was empty
	  (eval.c).

	+ added ifdef-controls OPT_SHOW_EVAL, OPT_SHOW_REGS, OPT_SHOW_TAGS
	  (cmdtbl, eval.c, estruct.h, line.c, proto.h, tags.c), and renamed
	  OPT_MAP_DISPLAY to OPT_SHOW_MAPS (map.c, estruct.h).

	+ use OPT_UPBUFF in 'killbuffer()' (buffer.c).

	+ added ifdef-control OPT_WIDE_CTYPES (estruct.h, input.c, main.c).

	+ use new macro 'chrBIT()' to fix long/short bitmasks in character
	  tests (estruct.h, line.c).

	+ changed 'set_bname()' to a procedure so that it can trim trailing
	  blanks from the buffer-name.  This is done so that a ^X-k command
	  applied to a buffer-name beginning with "!" will work.

	+ corrected length-computation in 'delins()' so that command
	  ":s/abc\[\(..\))/abc[\1]/" works properly (oneliner.c).

	+ corrected offset-computation in 'scanner()' so that wrapped search
	  finds text on the first line after wrapping (search.c).

	+ added call to 'update()' in 'ms_processing()' so that
	  "show-registers" display will be properly updated at the end of
	  mouse-selection (ibmpc.c).

	+ corrected logic in 'execute_named_command()' that prevented user from
	  typing ":0r foo" in an empty buffer (exec.c).

	+ corrected 'bclear()', so that the b_ulinep member is reset if it is
	  freed (buffer.c).

	+ lint (fileio.c, npopen.c, spawn.c)

	+ absorbed 'fnc2key()' into 'fnc2kcod()' (bind.c), to simplify
	  unbinding of prefix-keys in 'install_bind()'.

	+ corrected 'install_bind()' by new procedure 'reset_prefix()', so that
	  mapping characters that happen to be prefix-keys will override their
	  binding (bind.c).

	+ added definition LTRIMMED, to use in special cases (i.e., scratch
	  buffers) where we don't really want to show "^J" at the end of lines
	  when "list" mode is enabled (estruct.h, display.c, file.c, line.c,
	  map.c).

	+ animated [Mapped Characters] buffer (map.c).

	+ modified display of mappings so that using a repeat-count for the
	  ":map" command causes the actual characters to be shown, e.g.,
	  "#1" instead of "FN1", (map.c, modes.c).

	+ added alternate boolean "glob" mode (in contrast to the normal string
	  mode), for configurations in which vile cannot read filename
	  expansions via a pipe, e.g., VMS or UNIX (mktbls.c, modetbl, glob.c).

	+ moved "glob.h" definitions into estruct.h and proto.h because
	  the preceding change requires some definitions to precede inclusion
	  of "nemode.h" within estruct.h (descrip.mms, eval.c, filec.c, glob.c,
	  main.c, makefile, makefile.djg, makefile.tbc, makefile.wat, proto.h,
	  random.c, revlist, tags.c).

	+ corrected test in 'index2ukb()', which caused the unnamed-buffer
	  to be shown incorrectly when yanking text after lines had been
	  deleted (line.c).

	+ use 'DOT', 'mode_row()', 'buf_head()' and 'win_head()' macros (basic.c, buffer.c,
	  csrch.c, display.c, eval.c, exec.c,
	  file.c, filec.c, finderr.c, globals.c, line.c, random.c, tags.c,
	  tcap.c, tmp.c, undo.c, window.c, word.c) for consistency and readability.

	+ modified lines/cols initialization so that 24x80 is assumed if all
	  other lookups fail, needed for apollo sr10.2 dial-in (tcap.c).

	+ added macro 'KbSize()' (file.c, estruct.h, x11.c, spawn.c, line.c).

	+ corrected 'imdying()' to not save temporary buffers, and to suppress
	  "implybuffer" mode (file.c).

Changes for vile 3.64: (released Wed Dec 23, 1993)
(pgf)
	+ change arithmetic expression in mktbls.c, so we don't assume
	  boolean expression returns exactly 0 and 1.  this exercised
	  a bug in an HP compiler (mktbls,c, from chris sherman)

	+ added fallback definition of VDISABLE to termio.c, in case
	  system doesn'd define it. (termio.c)

	+ increased stack size for Watcom DOS builds to 16K.  probably
	  overkill, but the other DOS builds seems to use 16K as well.

	+ cleaned up some of the scanf nonsense, but not nearly enough.
	  (finderr.c)

	+ don't let display-page changes affect the user's chosen cursor.
	  (ibmpc.c)

	+ use _previous_ indentation level after a line beginning with '#'
	  in cmode.  (insert.c, proto.h)

	+ use COMSPEC environment variable when spawning a DOS shell (spawn.c)

	+ assume we're an xterm if the word "xterm" appears anywhere in the
	  first field of the tcbuf, rather than just in the user's TERM
	  variable. (tcap.c)

	+ increased size of tcbuf since SCO core dumps on overflow (tcap.c)

	+ initialized tb_last in tb_alloc(). (tbuff.c)

	+ couple of minor user suggestions for vile.hlp and the README.

	+ don't beep when motions fail due to being aborted. (main.c)

	+ check modification time on first change to buffer (file.c, exec.c,
	  proto.h)

	+ the filename found for a tags lookup is now passed through doglob(),
	  which means tagfiles can now refer to environment variables etc.
	  (tags.c, from S.Suresh)

	+ new X resource, "focusFollowsMouse", causes xvile's current window
	  to track with the mouse cursor. (x11.c, from S.Suresh)

	+ added '~' to the characters considered part of a pathname, so
	  ^X-e will pick up ~pgf/foo correctly (main.c).

(tom)
	+ corrected last change to allow pipe to vile, in case it is invoked
	  via 'rsh' and cannot open /dev/tty (main.c).

	+ added config flag OPT_TERMCHRS (estruct.h, bind.c).

	+ animated [Terminal] and [Binding List] buffers (bind.c, buffer.c).

	+ corrected call on 'slowreadf()' that resulted in wrong line numbers
	  when a file was piped to vile (main.c).

	+ modified TurboC makefile to merge duplicate strings, saving a little
	  memory (makefile.tbc).

	+ corrected logic that makes ".bak" file on MSDOS so that when
	  appending to a file (e.g., ":w >>foo") the file is copied after
	  renaming (fileio.c).

	+ consolidated some code that sets errno when user attempts to read or
	  write a directory-file (fileio.c).

	+ applied fix from eric krohn to fix uninitialized structure member
	  (tbuff.c).

	+ corrected "EGA" table entry (ibmpc.c)

	+ prevent 'flash' mode from being used until after 'TTopen()' (ibmpc.c)

	+ corrected a hole in 'swbuffer()' that tried to use a null value for
	  'curwp' (buffer.c).

	+ modified to allow "@" startup file to have errors w/o defaulting to
	  "vile.rc", by checking to see if a buffer was created (main.c)

	+ corrected logic of 'vtputc()' and 'vtset()' to make line-wrap display
	  properly when the line contains control characters (display.c).

	+ corrected limits of highlighting in 'change_selection()' (x11.c)

Changes for vile 3.63: (released  Wed Nov 10, 1993)
(tom)
	+ corrected 'pathcat()', to avoid using autoincrement in 'slashc()'
	  macro (path.c).

    	+ cleanup for gcc-2.4.5's -Wall warnings (estruct.h, display.c, exec.c,
	  finderr.c, glob.c, mktbls.c, modes.c, npopen.c, proto.h).

	+ compiled with CenterLine 'clcc' compiler (bind.c, buffer.c,
	  display.c, eval.c, file.c, filec.c, glob.c, globals.c, line.c,
	  tags.c).

	+ corrected 'backpage()' to update mode-line after "previous-page".

	+ modified ibmpc.c to make flash/mouse code more portable.

	+ corrected call on 'strncpy()' in 'kbd_reply()' where 'extbuf' may be
	  copied onto itself.  On HP/UX, this causes the destination to be
	  cleared (input.c).

	+ modified initialization of IBMPC to make "flash" mode work better on
	  MSDOS (ibmpc.c).

	+ modified so that on MSDOS and VMS vile properly handles files that
	  are redirected to it via the standard input (main.c).

	+ corrected bug that caused ":e#" command to ignore the '#' character
	  (exec.c).

	+ corrected handling of ":tag" command so that if a tag's length is
	  shorter than the taglength setting, the shorter value is used
	  (tags.c).

	+ modified 'writereg()' to prevent inadvertant file-write using a
	  read-pipe expression (file.c).
(pgf)
	+ fixed indenting bug in formatregion().  (word.c)

	+ added SHELL=/bin/sh to makefile

Changes for vile 3.62: (released  Mon Oct 11, 1993)
(pgf)
	+ enable DOS mouse processing for Turbo-C only, since some code
	  uses Turbo-isms to get at registers (_AX, etc)  (estruct.h)

	+ change references to x.ax, x.bx, x.di, etc to be x._AX_, x._BX_, etc.
	  to satisfy the Watcom 386 need for eax, ebx, edi, etc.

	+ improper automatic indent on brace insertion fixed (insert.c,
	  fmatch,c, proto.h)

	+ the ls -a output in imdying() was producing a line with a lone '.'
	  on it, which was terminating /bin/mail.  use sort -r to rearrange
	  it. (file.c)

	+ re-ifdefed getscreensize, for machines without SIGWINCH

	+ guard against zero values for tabstop and shiftwidth 	(buffer.c)

	+ added global control variable to make it easy to turn off the
	  "working..." message. (display.c)

	+ don't let hst_append() do anything if clexec is set (history.c)

	+ click the mouse (with xterm-mouse set) no longer calls showcpos().
	  if you want the mouse position, turn on "ruler-mode" (tcap.c)

	+ flash mode is now off by default.  beeps should be beeps until
	  requested otherwise. (main.c)

	+ applied vms path fix in pathcat() (path.c)

	+ don't insert tabs during reformat if notabinsert is set (word.c)

	+ applied patches for djgpp from Jim Crigler. (estruct.h, ibmpc.c)

(tom)
	+ TurboC compiler warnings (history.c, mktbls.c, oneliner.c, search.c).

	+ added TurboC compiler warning patterns (finderr.c).

	+ implemented mouse highlighting for MSDOS (display.c, ibmpc.c).

	+ made the table-driven combinations for $sres work properly (ibmpc.c).

	+ corrected 'repointstuff()', so that undo works properly for multiple
	  windows; other window's dot wasn't being updated (undo.c).

	+ modified 'execute_named_command()' to set dot-moved flag if the
	  range-computation moves it, fixes warning from 'updpos()' (exec.c).

	+ linted remaining size_t usage (basic.c bind.c display.c estruct.h
	  eval.c exec.c filec.c glob.c history.c isearch.c mktbls.c npopen.c
	  oneliner.c proto.h regexp.c search.c spawn.c vmsvt.c).

	+ guarded &left, &right, related functions against negative indices,
	  etc., (eval.c).

	+ changed sizes in regexp-struct to SIZE_T (from short, int) to support
	  lint (estruct.h).

	+ added command-line option "-wm" to set X11 window-manager title
	  (main.c, version.c, x11.c).

	+ modified prompts to make ^V echo until the character is followed by
	  what we want to see a la vi (input.c).

	+ modified treatment of '!' character (in shell prompts) to expand it
	  immediately, more like vi (main.c, history.c, input.c, spawn.c).

	+ corrected some ifdefs that broke !-commands when compiling small
	  configuration for testing (cmdtbl, bind.c, eval.c).

	+ whitespace spawn.c

	+ corrected redisplay logic in 'execute_named_command()' when repeated
	  backspace characters were received (exec.c).

	+ modified logic in 'imworking()' to be a little more conservative when
	  erasing leftover text in the message line, e.g., when it happens
	  during filename-completion on large directories (display.c).

	+ corrected a hole in 'hst_append()' that allowed non-interactive use
	  of the bind-key function to erroneously set the 'clexec' flag
	  breaking subsequent named-commands (history.c).

	+ trailing blank, e.g., "int<sp><nl>tgetc(quotec)<nl>" breaks apollo
	  ctags by making it generate a pattern only for the "int<sp>" (bind.c,
	  input.c, isearch.c, search.c).

	+ modified 'imply_buffer()' so that c-mode is set as in 'readin()'
	  (buffer.c).

	+ corrected 'getfile()' to treat names longer than NBUFN as filenames,
	  and 'unqname()' to assume its 'bname' argument can have a trailing
	  null (buffer.c, exec.c, file.c, main.c).  (This fixes a bug I
	  introduced in 3.60 that prevented using ":e %+" properly, where "%"
	  expands to a long buffer name, because the buffer names are compared
	  with 'strncmp()').

	+ modified 'getfile()' to suppress redundant prompt by using returned
	  state of 'unqname()' (file.c).

	+ modified 'mlreply_file()' to prompt if a wildcard expands to multiple
	  files (e.g., when reading a file) (filec.c).

	+ modified 'tokval()' to expand ~-paths so macros can refer to user's
	  home paths (eval.c).

	+ corrections to vms filename-completion (path.c).

	With Purify demo

	+ corrected memory leak in 'FreeBuffer()' (buffer.c).
	
	+ corrected uninit-memory reference in 'kbd_show_response()' (input.c,
	  history.c), in 'dfputsn()' (display.c).

	+ corrected reference to freed memory in 'listmodes()' and modified
	  logic to use 'animated' logic, and variables 'relisting_b_vals' and
	  'relisting_w_vals'.  As a side-effect, this allows local buffer and
	  window modes to be set in [Settings] (buffer.c, display.c, edef.h,
	  main.c, modes.c).

	+ corrected a null-pointer reference to 'fromline' when a ":0" command
	  was entered in an empty buffer (exec.c).

Changes for vile 3.61: (released  Thu Sep 16, 1993)
(pgf)
	+ when files are saved in /tmp, and mail is sent, the list is now
	  created with "ls -a" to make all files visible. (file.c)

	+ when trying to get back to the current buffer in globber(), use
	  set_curwp() instead of swbuffer(), since swbuffer() may not take
	  us back to the same window we started in, if the buffer is
	  multiply displayed. (globals.c)

	+ moving the cursor with the mouse (xterm-mouse) causes showcpos()
	  to be called, as if a ^G had been typed.  the message line needed
	  to be cleared in any case, to delete old ^G output, so i thought
	  i'd try updating it instead (should a similar change be made in
	  x11.c?). (tcap.c)

	+ in cmode, parentheses are treated like braces, in that if a line
	  ends with a left one, the next line is indented, and if it starts
	  with a right one, the line is aligned with the line containing the
	  match.  this may be useful for parentheses-heavy languages like

(tom)
	+ nits from 3.60 integration (file.c, mktbls.c, proto.h, glob.h)

	+ allow "number" mode to be set/unset in scratch buffers (modetbl,
	  display.c, random.c).

	+ use 'delay()' function in TurboC for 'catnap()' (random.c)

	+ ifdef'd "dirc" and "history" modes so that they don't appear in
	  [Settings] when not configured (modetbl, main.c).

	+ brought TurboC makefile up-to-date (makefile.tbc).

	+ corrected logic of 'makemodelist()' so that both buffer and window
	  local modes are shown (modes.c).

	+ corrected my last change to 'unqname()' to ensure that if the b_bname
	  field is short enough, it will have a null terminator (file.c).

	+ moved logic that reports lines-deleted from 'ldelete()' to common
	  logic of 'operdel()' and 'operlinedel()', so that using 'ldelete()'
	  internally won't generate spurious messages (line.c, opers.c).

	+ used 'doingopcmd' to modify return-status of motion commands (h,l) to
	  get rid of special-case handling so that when used internally they
	  don't fail; obsoleted 'forwchar_in_line()' (basic.c).

	+ use macro 'cmdBIT()' to clarify flags for CMDFUNC, added flag OPTREG
	  to simplify test in 'execute_named_command()', and make test for
	  optional register and/or line-count more explicit by allowing it only
	  when non-punctuation follows the command (fixes ":s/pattern")
	  (cmdtbl, estruct.h, exec.c, input.c).

	+ corrected my last change to make "2p work correctly, by using new
	  function 'index2ukb()' (line.c).

	+ modified 'mlreply_file()' so that filename completion works on the ^W
	  and ^R screen commands (filec.c).

	+ modified 'getregion()' to use line-number to simplify computation,
	  and to work with left-margin.  Also whitespaced (region.c).

	+ corrected test in 'loop()' that caused j/k commands to go awry in the
	  [Registers] buffer at blank lines (main.c).

	+ corrected test in 'kbd_reply()' to avoid name-completion when the
	  current-response is a shell-command, e.g., "!foo -?" (input.c).

	+ corrected logic in 'forwchar_to_eol()' and 'ins_anytime()' that lets
	  arrow-keys to go past the end of the line while in insert (or
	  replace) mode (basic.c, insert.c).

	+ corrected logic that allowed a user to begin an insertion in one
	  buffer, then click the mouse to reposition into a readonly buffer
	  and continue inserting -- in the readonly buffer (basic.c, insert.c).

	+ added mode 'flash' (ibmpc.c, main.c, tcap.c, x11.c)

	+ added logic to support msdos-mouse, using defines OPT_MS_MOUSE and
	  OPT_MOUSE.  This version only supports repositioning clicks (basic.c,
	  display.c, estruct.h, ibmpc.c, termio.c).

	+ use symbol ALLOC_T to clarify distinction between size_t/int/unsigned
	  (display.c, edef.h, estruct.h, eval.c, filec.c, fileio.c, finderr.c,
	  line.c, oneliner.c, proto.h, regexp.c, tbuff.c, undo.c, x11.c)

Changes for vile 3.60: (unreleased)
(pgf)
	+ added "stop" command (synonym for "suspend-vile", aka ^Z) (cmdtbl)

	+ ensure shiftwidth and tabstop are always set correctly after modes
	  are adjusted -- any of cmode, cts/ts, csw/sw can affect. (modes.c)

	+ added support to finderr() for OSF1 (DEC Alpha's, at least) compiler
	  errors, of the form:
		/usr/lib/cmplrs/cc/cfe: Error: test.c, line 8: Syntax Error

	+ took out leftover dbwrite() call from rangespec() -- may fix errant
	  behavior on ":.$d" (exec.c)

	+ fixed helpfile note about X11 -name option -- it sets the name
	  used for resource lookups, not the window name.  (vile.hlp)
	  how do we set the window name?

	+ under DOS, leave the cursor shape and the keyboard repeat rate
	  alone.  if we get complaints, perhaps we should mode-ify these,
	  but in general, settings like that are personal, and we shouldn't
	  muck with them.  (ibmpc.c)

	+ don't introduce extra path separators in pathcat() (path.c)

	+ protect against NULL returns from getcwd, in case the directory
	  we're in has been removed. (random.c)

	+ changed symbol "glob()" to "doglob()" to stop name conflict with
	  standard libraries (glob.c, eval.c, random.c, filec.c)

	+ added prototypes for glob.c to proto.h

	+ eliminated infinite loop when using goto-beginning-of-sentence from
	  end of buffer (basic.c)

	+ added mode "multibeep" to control the "one beep per failed motion"
	  logic -- if multibeep is on (default), then every failed motion
	  produces a beep.  if it's off, then only the first (of a set of
	  identical) causes a beep. (modetbl, main.c)

	+ corrected possible use of uninited locals in file.c (was really
	  only a warning suppression) and  display.c

	+ turned off beeping in search.c and csrch.c, now that all failed
	  motions cause beeps.

	+ change "Date" to "installed" in version.c, since modtime really
	  only reflects installation date.

	+ don't let vile use a screen dimension of less than 2 in either
	  direction (display.c, tcap.c)

	+ fixed bug that made all TERM types be thought of as xterms. (tcap.c)

	+ fixed documentation of the help commands.  it's been wrong
	  for over three years now, and it's in the first screen of
	  help! (^X-^H and ^A-^H should be ^X-h and ^A-h) (vile.hlp)

	+ added AIX to the machines that need sys/ioctl.h, to pick up
	  TIOCGWINSIZE (display.c)

	+ reset the "calledbefore" flag in docmd().  fixes chris sherman's
	  problem where a second substitute-til in a macro would appear to
	  do nothing.  (actually it was reusing its previous args, so there
	  was simply nothing to do)  (exec.c)

	+ added '>' to the characters known to the reformatting code, so
	  you can reformat most email replies. (word.c)

	+ added automatic patchfile generation to the makefile.  hopefully
	  i can generate patches for all future releases, in addition to
	  shar files. (makefile)

(tom)
	+ made ansi.c work with TurboC (ansi.c, main.c, display.c, termio.c),
	  to use as test-case for COLOR.

	+ corrected highlighting code for ":%s" command with ibmpc display
	  (display.c, ibmpc.c).

	+ modified 'bfind()' to always return a buffer (unless it cannot create
	  the buffer), use 'find_b_name()' for the no-create mode (bind.c,
	  buffer.c, eval.c, exec.c, file.c, filec.c, finderr.c, history.c,
	  main.c, map.c, modes.c, oneliner.c, random.c, spawn.c, tags.c).

	+ use macros 'set_bname()' and 'eql_bname()' and function 'get_bname()'
	  to encapsulate buffer name, which does not necessarily have a
	  trailing null (bind.c, buffer.c, display.c, estruct.h, eval.c,
	  exec.c, file.c, finderr.c, input.c, main.c, modes.c, oneliner.c,
	  spawn.c, tags.c, tmp.c).

	+ removed nonstandard 'strncpy()' (main.c).

	+ corrected 'kbd_show_response()' to work with expand-chars mode
	  (input.c).

	+ corrected 'kbd_reply()' to make the number of characters shown have
	  the same limit as the caller's buffer-length, and to make it always
	  return the data with a trailing null (input.c).

	+ made 'animated' mode work for [Registers] buffer (line.c), and
	  modified the logic so that numbered-buffers are displayed in a
	  vi-compatible manner.

	+ make shift-commands work more like vi, allowing repeated '<' or '>'
	  characters in a :-command to imply a repeat count (bind.c, exec.c,
	  opers.c).

	+ corrected logic of 'dofile()' so that if an error is found in the
	  startup file, vile properly sets curbp and curwp (exec.c).

	+ simplified/corrected command-history logic of to fix error that
	  prevented scrolling past ":e!" command, and to allow scrolling of
	  '<', '>' command even when repeated (bind.c, exec.c, history.c,
	  spawn.c).

	+ added flag KBD_EXPCMD to handle the case (from rewrite of history)
	  where %,#,: expansion must be done for shell commands (estruct.h,
	  bind.c, input.c).

	+ added OPT_HISTORY to allow ifdef'ing of history.c code (estruct.h,
	  history.c)

	+ cache home-directories looked up in 'home_path()' to make it run
	  faster (path.c)

	+ modified glob-expansion to allow "~" expansion.  This doesn't work
	  with wildcards in the user-portion (e.g., "~ab?"), but suffices for
	  most cases (glob.c).

	+ added a find-error case for apollo's C++ compiler (finderr.c).

	+ modified logic for 'displaying' vs 'imworking()' slightly, to get rid
	  of erroneous cursor movement (display.c).

	+ added function 'slowtime()' to force updates in 'slowreadf()' more
	  often (file.c).

	+ adjusted definitions for better lint checks on apollo sr10.2
	  (estruct.h, makefile).

	+ make piped-input also set error-buffer (main.c).

	+ revised logic that sets VAL-structs for modes to correct places where
	  buffer & window traits are saved without converting to local values.
	  Also removed code (e.g., copy_val) that checks reference counts,
	  since they aren't really necessary (buffer.c estruct.h main.c modes.c
	  proto.h)

	+ modified behavior of 'unsetl' command to delete local setting
	  (eval.c, modes.c).  It was only modifying (toggling) boolean modes.

	+ corrected 'reframe()' to handle linewrapping case submitted by
	  alistair (display.c).

	+ corrected cursor-update in 'x_scroll()' that left stray cursor images
	  remaining after ^D/^U commands (x11.c).

	+ corrected logic that caused selection-text to be lost when keypress
	  was received (x11.c).

	+ modified X-windows pasting to trim unneeded whitespace when inserting
	  into cmode or autoindent buffer (bind.c, x11.c).

	+ corrected kill-buffer flag KAPPEND, which had the same value as
	  KLINES (estruct.h, line.c).

	+ corrected error in 'doindent()' that caused a "<nl>}" insertion to
	  delete nonwhite characters (insert.c).

	+ corrected ":tags" command for exact (taglen=0) lookups (tags.c)

	+ make failed motion-commands sound an alarm like vi (main.c).

	+ modified logic for h,j,k,l,^D,^U,^F,^B commands to return false when
	  they fail, so failed-motion alarm works (basic.c).

	+ corrected logic for b,B,w,W,e,E commands to return false only when
	  backing/forwarding past the buffer ends (word.c).

	+ modified alternate-window motions 'scrnextup()', 'scrnextdw()',
	  'mvdnnxtwind()', 'mvupnxtwind()' to also return failed-motion status,
	  for consistency (window.c)

	+ made new function 'forwchar_in_line()' to do the vi "l" command,
	  dissociating it from the internal motion-command (basic.c, cmdtbl).

	+ made 'backhpage()', 'forwhpage()' motion-commands (cmdtbl).

	+ make failed quit-commands return false (main.c).

	+ make version-command show UNIX or VMS executable's date (main.c)

	+ corrected behavior of 'histbuff()' when user simply toggles with the
	  "_" command.  This did not handle the case in which the alternate
	  buffer was temporary, and thus had no number (buffer.c).

	+ introduced new buffer-attribute 'b_lim_left', macro
	  'w_left_margin()', ifdef'd with OPT_B_LIMITS, use this to control
	  cut/paste selection in [Registers] (basic.c, buffer.c, csrch.c,
	  display.c, random.c, region.c, window.c, x11.c).

	+ modified 'execute_named_command()' to recognize register name and
	  line-count for commands such as ":yank", making it handle cases such
	  as ":5>6", ":y a 6", ":10,20y b" (exec.c, input.c, line.c).

	+ modified ":put" command to report the number of lines added (line.c).

	+ split out 'version.c' because main.c got too large for apollo lint
	  (main.c, makefile*, vms_link.opt, link.msc).

Changes for vile 3.59: (released Wed Aug 18, 1993)
(pgf)
	+ added VIEWOK flag for functions that execute macros.  it says
	  it's okay to execute them in view mode, even though they have the
	  UNDO bit set.  this fixes not being able to run @, :map'ed, or
	  ^X-& macros in view-only buffers.

	+ made the xterm-mouse mode available in xvile as well as vile, to
	  make it easier to share .vilerc between the two versions.  the
	  alternative is to insist that people put ~force set xterm-mouse
	  in .vilerc, to override the error they'll get in xvile from the
	  mode not being present. (input.c, modes.c, estruct.h)

	+ changed default value of "xterm-mouse" to off, to make vile behave
	  like it used to, and like other character applications, by default.
	  (main.c)

	+ turned off support for the "working..." message for builds that
	  define USG, since they probably have non-restartable system calls.
	  (estruct.h)

	+ accept any TERM variable that ends in "xterm" to be an xterm.
	  (tcap.c, vile.hlp)

	+ fixed "uninitialized variable 'req'" when compiling without REVSTA
	  defined. (display.c)

	+ fixed missing closing double quote in "make all" help output.
	  (makefile)

	+ bumped the maximum row count from 100 to 200.  i personally can't
	  read more than about 70 lines on a 19" mono monitor, but maybe
	  i need glasses. (tcap.c, x11.c)

	+ took out extraneous trace.h include from x11.c

	+ corrected 'col2offs()' to make it easier to position the mouse on a
	  tab character (display.c).

Changes for vile 3.58: (released Fri Aug 13, 1993)
(pgf)
	+ added "sys$login:" to VMS file search path for vile.rc

(tom)
	+ refined 'imworking()' to get rid of leftover working-message
	  (display.c, file.c).  Make working-message show up only when 'terse'
	  mode is off (the default).

	+ added code to support xterm mouse-clicking (to move dot), with
	  additional code for later use with highlighted selections.  Made this
	  a mode (xterm-mouse) because it does not implement cut & paste yet
	  (basic.c, display.c, input.c, main.c, modes.c, tcap.c).

	+ added calls to 'TTkopen()' and 'TTkclose()' to termio.c so that (for
	  all systems) we call these within 'ttclean()' and 'ttunclean()'
	  (termio.c, tcap.c).

	+ optimized display of 'ruler' (display.c)

	+ use 'displaying' as a semaphore rather than true/false flag, as a
	  guard against interrupt by 'imworking()' which modifies ttcol and
	  ttrow (bind.c, display.c, main.c, tcap.c, x11.c).

	+ make ":=" command accept address (like vi) to show arbitrary address
	  value (cmdtbl, estruct.h, exec.c, random.c).

	+ don't let 'write-til' move dot (cmdtbl).

	+ modified 'doindent()' to trim leading whitespace (like vi) from the
	  portion of the line after dot (insert.c).

	+ modified insert-code to treat the combination of repeat-count and
	  newline like vi, causing the repeat count to apply only to the
	  insertion-string before a newline.  (edef.h, input.c, insert.c)

	+ corrected handling of '.' repeat-count for r,R commands (insert.c)

	+ modified user-variables to make them dynamically-allocated, and to
	  remove the restriction on the length of their names (mktbls.c,
	  estruct.h, eval.c, main.c).  Also corrected treatment of newline in
	  prompt for user-variable name.

	+ used 'strncpy()' to copy return-values from 'kbd_reply()', just in
	  case macro-tokens are too long (input.c).

	+ don't expand '%' or '#' in 'kbd_reply()' if it would be the name of
	  an invisible buffer (input.c).

	+ added universal-modes 'expand-chars' to list the characters to expand
	  in 'kbd_reply()', and 'expand-path' to control whether %/# are shown
	  as full, or shortened paths (modetbl, input.c, main.c).

	+ modified X-windows config to allow processing of events when the
	  working-message code is executed (display.c, x11.c).  This makes the
	  screen refresh and keyboard handling work better.

	+ added 'animated' buffer-mode to control whether vile updates
	  scratch buffers when their contents would change (e.g., the buffer
	  list), made this work for buffer-list (buffer.c, display.c,
	  estruct.h, modes.c, modetbl).

	+ revised the 'killbuffer()' code to work properly with the animated
	  mode (buffer.c).

	+ modified mktbls to check for illegal mode-names (mktbls.c).

	+ linted with SMALLER set (to get rid of unused functions), introduced
	  new ifdef with OPT_EVAL to simplify (basic.c bind.c buffer.c cmdtbl
	  estruct.h eval.c insert.c line.c lint.out modes.c npopen.c proto.h
	  window.c).

Changes for vile 3.57: (released Fri Aug 06, 1993)
(pgf)
	+ the bug check for b_ulinep pointing at a newly inserted line is
	  no longer valid, since we can now insert a line, make changes (which
	  will set b_ulinep), undo those changes, and then undo the insert.
	  we now just null out b_ulinep when we remove the line it references.
	  (undo.c)

	+ changed name of setmode() to avoid library conflict with djgpp, and
	  changed the other del/setg/delg versions as well.  (cmdtbl, modes.c,
	  proto.h)

	+ took out #define USG 0 since djgpp uses an #ifdef on this in a header
	  (estruct.h)
(tom)
	+ whitespace in basic.c

	+ corrected definition of ScratchName macro for apollo SR10.2
	  (estruct.h) and a compiler warning (in glob.c).

	+ corrected gcc warnings about 'signal()'-arguments for SunOs.

	+ use new macros ACTUAL_SIG_ARGS, etc., instead of ACTUAL_SIGNAL so
	  that the tag-file can find the signal-handling procedures (estruct.h,
	  display.c, file.c, main.c, proto.h, spawn.c, x11.c).

	+ customized the c-suffix default values for VMS, MSDOS and UNIX.

	+ VAX/VMS C does not allow continuation-lines in ifdefs (mktbls.c).

	+ VAX/VMS C may incorrectly set ferror() when feof() is true (fileio.c)

	+ VAX/VMS C 'rewind()' does not work correctly (fileio.c)

	+ added code to show "working..." message for time-consuming operations
	  (display.c, npopen.c, vmspipe.c), and to allow 'slowreadf()' to show
	  its progress (file.c, fileio.c).

	+ linted calls on 'signal()', (main.c, display.c, spawn.c, termio.c).

	+ linted calls on 'firstnonwhite()' (basic.c, exec.c, insert.c, line.c,
	  opers.c, region.c, spawn.c, word.c).

	+ use new functions 'end_named_cmd()', 'more_named_cmd()' for clarity
	  (exec.c, file.c, map.c, oneliner.c)

	+ added new mode 'ruler' (to display line+column in a window on the
	  modeline), and new window-flag WFSTAT to support it (by allowing
	  non-modal information to affect the modeline).  (display.c)

	+ corrected error I introduced into 'reframe()', and in 'row2window()'
	  while adding 'linewrap' mode (display.c).

	+ corrected code that frees buffer's fname-field for the special case
	  of out-of-memory (buffer.c, random.c).

	+ added new command 'tagstack' to show the contents of the tag-stack.
	  Modified 'tags()' to use module-level 'tagname[]' rather than local
	  copy of tag-string.  (tags.c)

	+ always update the modtime-at-warn buffer field when prompting
	  for the check-modtime mode (file.c).

	+ removed (obsolete) code for DEC Rainbow (estruct.h, display.c,
	  termio.c).

	+ modified 'line_no()' to return values in the range 1..MAX+1
	  instead of 0..MAX (random.c).

	+ corrections to make X11 version work with 'number', 'linewrap' modes,
	  to guard against selection extending outside a window, to highlight
	  only the actual text within a window, and to use 'yankregion()' to
	  save the paste-text.  (basic.c, display.c, window.c, x11.c)

	+ modified 'typahead()' to know when X-windows is pasting, and the
	  paste-init code to supply an escape to end insert-mode, and to use
	  TBUFF's so that nulls can be inserted (termio.c, x11.c).

	+ put the repeat-count in 'map_proc()' back the way Otto Lind had it
	  (map.c).

Changes for vile 3.56:	(released Tue Jul 27, 1993)
(pgf)
	+ after a confirm substitute operation, we now mlerase the confirm
	  prompt (oneliner.c)

	+ be sure the [vileinit] buffer is marked as scratch while doing the
	  bprintf into it, so undo record-keeping will be suppressed. (main.c)

	+ rearranged calls to chg_buff and ldelnewline in ldelete, so that
	  the buffer will still be marked clean on first toss/copy/tag to
	  undo.  unmodified state is now restored correctly after a :g/^$/d is
	  undone. (line.c)

	+ In regular expressions, \s and \S are now true atoms.  they both
	  must now be followed by * or + to match more than one.  also, \s
	  no longer matches BOL or EOL (thanks to alistair and eric for
	  pointing out and fixing) (regexp.c)  In practice this means that
	  one should use \s* or \s\+ where one used to use just \s.  also,
	  if a match against ^ or $ is necessary, it must be done explicitly,
	  e.g. /\(^\s*\|\s\+\)word/ instead of /\sword/

	+ adjusted paragraph/section/comment/sentence regexps, with help
	  from eric, to reflect change to \s and \S

	+ make the #ifdef USE_TERMIO version of ttclean() be a no-op under
	  X11, as it is in the other cases.  eric's right -- termio.c needs
	  a rewrite.  (termio.c)

	+ don't pass the (0) to getpgrp if we're POSIX and ANSI -- the
	  library is probably the right one, and there's a high probability
	  of a prototype conflict if we _do_ pass it.  (spawn.c)

	+ When looking up X defaults, the old code checked the class first,
	  then the specific program name.  This is backwards since if the
	  class value exists, there is no way to override it with the -name
	  option.  (x11.c)

	+ AIX and pyramid think we have non-constant case expressions (eval.c)

	+ There is no requirement in ANSI C for SCRTCH_LEFT and SCRTCH_RIGHT
	  to be expanded before the ##.  Some preprocessors do and some don't.
	  However, the other #define using just #s and string concatenation
	  seems to work everywhere.  (estruct.h)

	+ clarified usage of &sin function, which searches its first argument
	  for an occurrence of the second.  (vile.hlp)

(tom)
	+ renamed UCHARAT to UCHAR_AT; added defs for UCHAR USHORT, UINT and
	  ULONG for easier linting/reading (bind.c, dirstuff.h, estruct.h,
	  file.c, history.c, ibmpc.c, input.c, isearch.c, line.c, oneliner.c,
	  proto.h, tbuff.c, vmalloc.c, vmsvt.c, x11.c, z_ibmpc.c)

	+ make 'prc2kcod()' return an int again, some optimization of 'token()'
	  (bind.c, file.c).

	+ clean-compiled/tested X11 + DEBUGM + OPT_MAP_MEMORY with gcc
	  (basic.c, edef.h, exec.c).

	+ disable filename-completion when OPT_MAP_MEMORY is enabled.

	+ added mode 'newline', set to true when reading files that have a
	  trailing newline (the normal vi-convention), modified (file.c,
	  buffer.c, random.c).

	+ fixed place where ":ww" garbages the screen before reporting
	  "wrote 0 buffers" (buffer.c).

	+ simplified 'writereg()' and 'ffputline()' (fileio.c, file.c) to allow
	  'newline' mode to control whether a trailing newline is written to
	  output files.

	+ modified 'kifile()' so that it does not insert the trailing newline
	  if it wasn't present in the file.

	+ made 'ldelete()' return from one point (line.c) to simplify 'report'
	  mode.

	+ added mode 'report', to specify the number of changes before a
	  message is emitted:
		NN lines deleted		6dd
		NN more lines			u
		NN fewer lines			u
		NN fewer lines			:g/^$/d
		NN lines yanked			"a8yy
		NN substitutions on MM lines	:%s/x/XX/g
		NN substitutions		:%s/x/X/
	  (globals.c, line.c, undo.c, oneliner.c, random.c, region.c)

	+ corrected unnecessary screen-update in 'substline()' (oneliner.c)
	  introduced in confirm-changes.

	+ added 'linewrap' mode (basic.c, buffer.c, display.c, windows.c).

	+ corrected cursor movement for large repeat counts with 'H' command
	  (basic.c).

	+ ifdef'd 'screen_string()' to handle apollo compiler warning when
	  testing SMALLER configuration.

	+ added stubs for modes 'popup-msgs', 'popup-choices'.

Changes for vile 3.55: (released Mon Jul 19, 1993)
(pgf)
	+ new routine, mlquickask(), which prompts for and returns a single
	  key answer from a user.  use this when getting a confirm from a
	  user for a substitute command. (input.c, oneliner.c, proto.h)

	+ documented the 'c' for confirm (vile.hlp)

	+ the token() routine now expands numeric escape codes, like \nnn and
	  \xNNN where nnn are octal and NNN are hex digits.  this may affect
	  vilerc files that have \ sequences in them.  support for mapping
	  to key-sequences like 0xNN is gone -- use \xNN instead.  this will
	  make it easier to do multi-char maps in the future.

	  note that \000 will cause internal problems, since things are null-
	  terminated, and the sequences "<sp>", "<tab>", and "^C" are all
	  problematic when multi-char sequences come around.

(alistair)
	+ added highlighted confirm to ":s" command.  (oneliner.c, display.c)

(tom)
	+ modified 'makebindlist()' (in bind.c) to format using tabs rather
	  than spaces.

	+ many mods to map.c (and bind.c) to eliminate redundancy between it
	  and bind.c

	+ modified 'prc2kcod()' to look for only one prefix, but allow
	  hexadecimal code in form 0xff.

	+ corrected a recursion bug in 'install_map()' (file map.c)

	+ modified ":map" (file map.c) to show show mappings if no values are
	  given.

	+ fix strict gcc warnings in x11.c, consolidated some initialization
	  code and corrected font-ascent value (so 9x15 works properly).

	+ added "on" and "off" to literals in 'stol()' (file eval.c) to use
	  this in x11.c

	+ modified 'killbuffer()' in buffer.c to recognize a repeat-count
	  (e.g., allows ^X-k within the buffer list to remove a succession of
	  buffers).

	+ fix strict gcc warnings in ansi.c

	+ make command-history record properly for bind-key and map commands
	  (files bind.c and map.c).

	+ modified 'engl2fnc()' (in bind.c) to allow unique abbreviation (so
	  that scripts don't have to be spelled out), added "se" symbol to
	  cmdtbl for vi-compatibility.

	+ modified 'docmd()' (file exec.c) to allow leading ':' in scripts
	  for vi-compatibility.

	+ corrected logic in modes.c to (re)allow trailing whitespace on
	  the end of set-commands.

	+ merged files makmktbl.tbc and makvile.tbc into makefile.tbc

	+ eliminated 'VIDEO *' arguments from 'updateline()' in file display.c
	  because the 'row' argument already provides this information.

	+ added 'offs2col()' to correct position of highlighting when
	  confirming substitutions.

Changes for vile 3.54: (released Fri Jul 09, 1993)
(pgf)
	+ will now call setvbuf instead of setbuffer if USG defined in the
	  POSIX version of ttopen.  this to support SVR4 (well, solaris)
	  (termio.c)

	+ took out the more stringent prototype warnings (-Wstuff) from the
	  gcc OPTFLAGS, since we don't pass them yet (someday, alistair).
	  (makefile)

	+ mode and variable symbol names generated by mktbls are now of the
	  form "s_name__" instead of "s_name", to be more unique.  (there
	  was a conflict with a (naturally named) typedef of signed char to
	  "s_char".)  (mktbls.c)

	+ upped the max number of screen columns for x11.c.  was 150, now is
	  200.  reduced the max rows number from 200 to 100.  the screen
	  buffers should be made re-allocatable, so we wouldn't eat the space
	  unless necessary.  made the normal (tcap.c) maximums match these.
	  (tcap.c x11.c)

	+ applied Tuan Dang's fixes for my merging of his watcom and djgcc
	  patches.  Thanks Tuan!  (makefile.wat, ibmpc.c, estruct.h, path.c,
	  mktbls.c)

	+ turn DOS mode on by default for DOS versions of vile. (main.c,
	  vile.hlp)

Changes for vile 3.53: 	(released Thu Jul 07, 1993)
(pgf)
	+ stop core dump on file completion (":e fi<tab>") by making sure
	  BFSIZES bit is cleared when building MyBuff

	+ in rtfrmshell(), there was a "curwp->w_flag = WFHARD" statement,
	  with a comment asking if it was needed.  in fact, it's wrong, so
	  i took it out.  it was tromping other flags, like WFMODE, for that
	  window only (spawn.c).

	+ in update(), don't add nu_width to screencol if the buffer is empty,
	  since there is no line number in that case. (display.c)

	+ fixed bug when undoing the first insertion into an empty buffer.
	  the value of dot was being restored to the inserted (and now gone)
	  line.  (line.c)

Changes for vile 3.52:
(pgf)
	+ added a couple of tweaks to tom's check-modtime changes -- prompts
	  are not suppressed if you're writing the buffer out, and running
	  a shell only checks visible buffers for modtime changes, rather
	  than all buffers. (mostly file.c, estruct.h, random.c)

	+ changed name of "timeout-value" to "timeoutlen", to match some
	  versions of vi.  (modetbl, vile.hlp)

	+ applied patches from Tuan DANG (dang@cogit.ign.fr) to support
	  DJGCC v1.09 (display.c, estruct.h fileio.c ibmpc.c main.c mktbls.c
	  modes.c path.c random.c spawn.c termio.c)

	+ consolidated TURBO, WATCOM, MSC, ZTC, and GO32 under a new,
	  presumptuous #define, called NEWDOSCC. (mostly same files as djgpp
	  changes)

	+ fixed calloc macro (estruct.h) in RAMSIZE ifdef

	+ added map.c/map.obj source file for VMS (descrip.mms, vms_link.opt)

	+ removed bogus dbgwrite in exec.c, where linespec returns a value
	  for "toline" which can be null.

Changes for vile 3.51:
(pgf)
	+ added a couple of tweaks to tom's check-modtime changes -- prompts
	  are not suppressed if you're writing the buffer out, and running
	  a shell only checks visible buffers for modtime changes, rather
	  than all buffers.

	+ added regexp metacharacter "index" in margin of vile.hlp.

	+ renested some limit setting in scanner(), so I can understand it
	  again. (search.c)

	+ changed name of ABS macro to ABSM to avoid namespace conflict
	  on OSF/1. (cmdtbl, exec.c estruct.h)

	+ made :map to SPEC key-sequences work (map.c insert.c)

	+ undo now preserves current column as well as row (undo.c estruct.h)

	+ changed name of "naptime" to "timeout-value"  (main.c modetbl
	  vile.hlp input.c)

	+ added new variable $mode, which is the name of the user-mode we
	  e're in: "command", "insert", "overwrite".  the value is set
	  when we enter insertmode, and restored when we leave.  this lets
	  macros invoked from within insert mode find out if they should
	  enter insertmode.  (eval.c, modetbl, insert.c, proto.h, vile.hlp)

	+ prc2kcod will now accept literal control chars, and also will
	  accept hex specifications.  so now key-sequences can be specified
	  as follows:		(bind.c map.c vile.hlp)
	  	an optional prefix:
			^A	(one char)
			^A-	(three chars)
			^X	(one char)
			^X-	(three chars)

		followed by an optional "function" prefix:
			FN-	(three chars)
			M-	(two chars)
		    (these two are synonymous)

		followed by a character:
			C	(one char)
			^C	(one char)
			^C	(two chars)
			0xNN	(four chars)
			0XNN	(four chars)
			<tab>	(five chars)
			<sp>	(four chars)


	+ removed old BEFORE ifdefs (input.c insert.c)

	+ better error checking on parsing of key-sequences in exec'ed files.
	  (bind.c)

	+ if dobuf() encounters an error, it switches to that buffer.  but
	  until now, this error propagated back through nested dobuf()'s,
	  and we ended up looking at the 'source "filename"' line in the
	  outermost buffer.  now we only switch on the most deeply nested
	  dobuf().  also, we no longer set file buffers that we're executing
	  to view-mode.  (exec.c)

	+ if we get an error on startup, we skip right to the editing loop,
	  so the user gets confronted with the error right away.  (main.c)

	+ gave adjustmode a real return value -- used to always return TRUE.
	  (modes.c)

	+ when looping backwards backspacing, check if the input char was
	  killc or wkillc before checking isbackspace(c), since otherwise
	  if killc is DEL, we'll treat it as backspace first.  (this is
	  because ^H and DEL are both assumed to be backspacers initially)
	  (insert.c input.c)

	+ implemented new universal mode, "naptime", which is the number of
	  milliseconds to delay before accepting a lone ESC character,
	  rather than as the start of a function key sequence.  also added
	  boolean arg to catnap(), which controls whether detection of user
	  input will abort the catnap delay.  the default for this mode is
	  500 milliseconds.  the old value was 50, which was fine for fast
	  local displays, but didn't work well for serial terminals or
	  rlogin sessions.  (which surprises me somewhat -- 9600 baud
	  translates to roughly 1 character per millisecond -- why is a
	  delay of 50 ms not enough for the second character of a function
	  key generated by a terminal?) (proto.h fences.c input.c random.c
	  modetbl vile.hlp main.c)

	+ applied patches for Watcom C/386, from Tuan DANG (dang@cogit.ign.fr).
	  I haven't had a chance to try them yet. ( mktbls.c cmdtbl estruct.h
	  dirstuff.h ibmpc.c main.c fileio.c random.c termio.c display.c
	  glob.c npopen.c path.c proto.h spawn.c makefile)

	+ renamed turbo makefiles, to be more similar to the watcom makefiles.

	+ fixed insert-string to really insert instead of overwrite.

(tom (3.51))
	+ added buffer-mode 'check-modtime' to check if the disk-file is newer
	  than the buffer when reading, writing or popping a window (adapted
	  from a patch by Randy Winney, modifies (files file.c, fileio.c,
	  buffer.c, random.c, spawn.c).  Note that moving the cursor between
	  buffers does not perform this check.

	+ use macro 'for_each_line()' in some more places (files: buffer.c,
	  globals.c, random.c, spawn.c, vmalloc.c).

	+ use new function 'SetCurrentWindow()' in file window.c.

	+ added checking in 'SetVarValue()' (file eval.c) to guard readonly
	  buffers from modification.

	+ added checks in file mode.c to prevent major-mode changes to scratch
	  buffers (and corrected the settings of WFMODE in file modetbl).

	+ tested/augmented RAMSIZE configuration option (files main.c,
	  display.c), made a mode 'showram' to control it.

	+ merged field that shows major-modes in the modeline (e.g.,
	  "view-only"), and added "cmode" when it is active.

	+ added buffer-flag BFSIZES; use it in 'bsizes()' to compute byte/line
	  counts only when buffer has been changed.  Also, added function
	  'add_line_at()' to merge file-reading logic which initializes the
	  byte/line counts (files: buffer.c, file.c).

	+ if dos-mode is set on a buffer, trim carriage-returns from files that
	  are inserted into it (file file.c).

	+ renamed macros in file buffer.c that test BUFFER.b_flags, added
	  macros for setting/clearing these flags, moved the macros to
	  estruct.h to use in files bind.c, buffer.c, file.c, history.c,
	  modes.c, random.c, spawn.c, tags.c, undo.c.

	+ corrected size shown for buffer-list within the buffer-list display
	  (file buffer.c).

	+ use window-colors for modelines rather than global values (file
	  display.c).

	+ disable ":p" command from within buffer-list display (oneliner.c)

Changes for vile 3.50:
(pgf)
	+ finished (yeah, right) the changes for infinite undo. implemented
	  mode to control how many changes are stored (called "undolimit").
	  the modified bit (BFCHG) now gets reset if the right number of
	  undos is done.  (estruct.h main.c modetbl undo.c buffer.c vile.hlp)

	+ rearranged calls to copy_for/tag_for/toss_to_undo and chg_buff
	  so that the BFCHG flag is unmodified before the first call to
	  preundocleanup().  this gives undo the true state of BFCHG.
	  (line.c region.c)

Changes for vile 3.49:
(pgf)
	+ implemented infinite undo, on keys ^X-u (undo) and ^X-r (redo).
	  probably affected location of cursor after some undo operations --
	  don't if this will be significant or not -- i doubt we matched
	  vi before in this regard anyway.  (undo.c, cmdtbl, buffer.c, proto.h
	  estruct.h, vile.hlp)

	+ use an explicit %s when printing lone filename, to protect against
	  filenames with % in name. (display.c)  (eric krohn)

	+ don't allow kbd_putc to print to last column of display, since if
	  the terminal autowraps, this causes a scroll.  this should check
	  the :am: capability, but we're at the wrong layer to do that, and
	  it doesn't seem worth adding to the TERM struct to make that info
	  available (unix-pc behavior pointed out by eric krohn).

	+ added Otto Lind's initial :map implementation (map.c, main.c,
	  cmdtbl, makefile, proto.h)  This is a good start, but has the
	  following limitations:  1) no string to string maps.  only simple
	  keysequences are mappable.  2) not integrated with the keyboard
	  macro routines -- this would make sense, and allow for recursion
	  to be detected more properly.  but the keyboard macros are pretty
	  tied into the named registers, which are stored in KILLREGS,
	  which doesn't seem right for the :map'ed things.  perhaps we could
	  dispense with the KILLREGS, and replace them with tbufs.  then some
	  small changes the layering of start/finish_kbm and kbm_started
	  could allow the :map code to share those routines.

	+ added alistair crooks' lint stuff and change for the NeXT. (proto.h,
	  mktbls.c)

(tom)
	+ modified file undo.c so that changes to scratch buffers can be
	  undone.

	+ modified 'imply_alt()' in file buffer.c so that the top/current line
	  numbers are propagated into the alternate buffer.

	+ changed environment variable 'directory' to 'cwd' so that 'directory'
	  can be used for vi-compatible directory variable (used to control
	  where the temp-file is written).

	+ implemented environment variable 'directory' (files modetbl, eval.c)
	  and used it to control temp-file location (npopen.c, tmp.c).  This is
	  set by the environment variable TMP.

	+ changed logic that opens/closes temp-file in tmp.c to use 'tempnam()'
	  to derive the filename; added entrypoint 'tmp_cleanup()' to remove
	  the temp-file on exit, and 'exit_program()' to file main.c to make a
	  single exit-point from which to call 'tmp_cleanup()'.

	+ Use macro 'ExitProgram()' to hide whether we call 'exit_program()' or
	  'exit()' (files main.c, display.c, ibmpc.c, spawn.c, vmsvt.c, vt52.c,
	  termio.c, termcap.c, file.c, window.c, x11.c).

	+ modified 'mktbls.c' to generate definitions for 'nemode.h' in a
	  form that allows modes to be conditionally compiled.

	+ added a column to 'modetbl' to ifdef out modes that are not used.

	+ made 'crypt', 'fcolor' and 'bcolor' modes ifdef-able (files main.c,
	  modes.c, modetbl).

	+ simplified 'promptpattern()' and 'expandp()' in file isearch.c

	+ corrected USE_D_NAMLEN ifdef in 'expand_pattern()' (file glob.c)

	+ corrected behavior when user inserts a newline into an empty buffer
	  (file line.c).

	+ moved environment variable definitions from cmdtbl to modetbl to
	  ensure that name-completion table for modes+vars is updated in one
	  place.  Sorted the environment variable names in modetbl and vile.hlp

	+ include "nevars.h" within main.c to allow common name-completion
	  table (without introducing lots of extra string-pointer variables).

	+ modified mktbls.c to put the environment-variable names into the
	  'all_modes[]' array in nemode.h (to support common name-completion).

	+ modified 'listvars()' in file eval.c to optionally show all modes
	  and environment variables in one list (if the user supplies a numeric
	  argument before the "show-variables" command).

	+ modified 'gtenv()' in file eval.c to recognize mode-names as well as
	  environment names, and logic in file modes.c to recognize environment
	  names.  Thus, for example, the commands
	  	":setl dos" and
		":setv $dos=true".
	  are equivalent.

	+ changed use of 'sprintf()' to 'lsprintf()' (files buffer.c, modes.c)

	+ use macros 'FreeAndNull()' and 'FreeIfNeeded()' to simplify code
	  (buffer.c, eval.c, file.c, filec.c, main.c, modes.c,
	  npopen.c, oneliner.c, search.c, random.c, x11.c)

	+ changed case-statements for EV-variables to if-then-else-if in
	  file eval.c because VAX-C cannot handle constant-expression
	  OFFSETOF as case-values.

	+ added 'tempnam()' to file vms2unix.c (to support testing of
	  mapped-data configuration).

Changes for vile 3.48:
(pgf)
	+ added FILEC_PROMPT flag to mlreply_file call in writeregion, to
	  make ^W file-writing operator work.

	+ folded some long lines (opers.c filec.c file.c)
(tom)
	+ corrections to make undo work properly with mapped-memory option
	  (files buffer.c and tmp.c).

	+ use symbol 'null_ptr' to tidy up references to '(LINE *)0'
	  (files buffer.c fences.c line.c main.c undo.c)

	+ changed LINE.l_size to SIZE_T (to make it lintable)

	+ corrected "poison" code in 'lfree()' (file line.c)

	+ added logic to finderr.c to handle lint output on apollo.

	+ added logic to handle line truncation (files buffer.c, file.c,
	  line.c, tmp.c)

	+ added function 'l_region()' to file tmp.c; use this (with some
	  reorganization) in files file.c, oneliner.c, region.c, word.c to
	  track LINEPTR adjustments in REGION structs.  (Converted most of the
	  calling procedures to single-return to clarify this).

	+ moved options-logic in 'substreg1()' (file oneliner.c) down to make
	  this simpler (and avoid unnecessary goto).  (This procedure has too
	  much logic to convert to single-return).  Also fixed a place that
	  should have used 'end_string()'.

	+ used macro 'same_ptr()' to compare LINEPTR's directly (a bit more
	  efficient).

	+ implemented page-swapping logic in file tmp.c

	+ modified ifdefs in files bind.c, eval.c, finderr.c to get rid of
	  compiler warnings when SMALLER, REBIND and APROP ifdefs are turned
	  off.

	+ used 'memcpy()' and 'memset()' in files buffer.c, line.c to simplify
	  loops copying text into LINE's.

	+ modified interfaces of functions that may modify 'LINE *' pointers
	  (or, equivalently, cause swapping of a LINE to a different address)
	  to use LINEPTR arguments (for example, 'line_no()' can iterate over
	  all lines, causing swapping).  Also modified functions (especially in
	  files line.c and undo.c) to use LINEPTR variables where they call
	  lower-level functions that may page-swapping.  Affects files
	  buffer.c, display.c, exec.c, file.c, filec.c, input.c, line.c,
	  oneliner.c, random.c, region.c, search.c, vmalloc.c, word.c

	+ replaced call on 'lalloc()' in file exec.c with 'addline()' (only
	  difference is that the latter also sets b_dot).

	+ converted several functions in file undo.c to static (to simplify
	  tinkering with LINE/LINEPTR tradeoffs).

	+ modified calls on 'lalloc()' in file undo.c to use special codes for
	  l_used member directly.

	+ added options -Wconversion -Wstrict-prototypes -Wmissing-prototypes
	  to gcc flags in makefile; addressed these warnings (except for
	  'signal()' arguments, which cannot be fixed) on sunos.  Modified
	  files bind.c, file.c, filec.c, fileio.c, glob.c, insert.c, line.c,
	  path.c, random.c, region.c, spawn.c, tcap.c.

	+ corrected ifdef in 'string_has_wildcards()' (file glob.c) for VMS.

Changes for vile 3.47:
(tom)

    (part a)
	+ introduced 'l_ref()' and 'l_ptr()' functions, rippled-through changes
	  to support mapped data (files basic.c buffer.c csrch.c display.c
	  estruct.h eval.c exec.c fences.c file.c filec.c finderr.c globals.c
	  history.c insert.c line.c oneliner.c path.c proto.h random.c region.c
	  search.c spawn.c tags.c tmp.c undo.c window.c word.c)

	+ added logic to file tmp.c to allocate LINE structs from a pool of
	  pages (note that deallocation isn't done, nor is swapping to a
	  tmp-file as yet).

    (part b)
	+ added code to test return-value of 'glob()' in file eval.c

	+ corrected 'get_recorded_char()' (file input.c) to reset 'buffer'
	  variable after 'finish_kbm()' makes it obsolete.

	+ provided more useful defaults for TERMCAP, IBMPC and COLOR
	  definitions in file estruct.h

	+ corrected value of MK in 'FreeBuffer()' (file buffer.c), which caused
	  multiple frees of the same LINE struct.

	+ supplied initialization for wp->w_dot.l and wp->w_line.l in
	  'reframe()' (file display.c) to handle the case when vile is invoked
	  with no filename parameters.

Changes for vile 3.46:
(pgf)
	+ suppress attempts at reading a null file into the [vileinit]
	  buffer (main.c)

	+ don't require the d_namlen field in a DIRENT struct -- some systems
	  don't have it, and most of those that do null terminate the name
	  anyway.  (dirstuff.h, filec.c, glob.c, path.c)

	+ changed compile flags for uts, on user suggestion (makefile)

	+ in ffread(), after doing read(), do an fseek to sync up.  this
	  keeps some stdio implementations from not bothering to seek back
	  to the start in case we have to discard the read and resort to
	  slowreadf() due to long lines. (fileio.c)

	+ avoid realloc(ptr,0) when long lines are encountered in quickreadf()
	  (file.c)

	+ make M and L commands take account of too few lines in window (i.e.
	  do the right thing when end of file is on screen). (basic.c)

	+ even if the global dos-mode is on, empty (new) buffers should come
	  up as non-dos.  (file.c)

	+ fnc2engl() now tries to return a longer, meaningful name for a
	  function (i.e. "exit" rather than "Q"). (bind.c)

(tom)
	+ test-compiled VMALLOC, DEBUGM options on apollo.

	+ introduced 'set_lforw()' and 'set_lback()' macros to use in mapped
	  memory option.  Added stubs in new file 'tmp.c'.  Modified files
	  buffer.c exec.c file.c filec.c line.c undo.c, verified mechanically.

	+ added definition of LINEPTR and 'lsync()' to files estruct.h, proto.h

	+ fixed a memory leak in 'glob()', and changed interface to
	  'glob_free()' (files filec.c, glob.c, glob.h).

	+ revised logic in ibmpc.c that sets screen resolution ($sres) to
	  make it more table-driven.

	+ corrected 'forwline()' in file basic.c (repeated movement down
	  sometimes reset the cursor to column 1 unlike vi).

	+ added prototypes to 'mktbls.c'.

	+ modified file main.c so that if the standard input isn't a terminal
	  then vile reads it into a buffer and reopens /dev/tty (unix only).

	+ modified files main.c and epath.h to allow (for unix systems) vile
	  to find vile.hlp even if vile isn't in the path.

Changes for vile 3.45:
(pgf)
	+ got rid of LOOKTAGS mode, and all of its ifdefs (filec.c, main.c,
	  modetbl, proto.h, tags.c)

	+ got rid of NeWS ifdefs (basic.c, bind.c, buffer.c, edef.h,
	  estruct.h, exec.c, file.c, input.c, isearch.c, main.c, modes.c,
	  spawn.c, termio.c)

	+ resolved vile.hlp modes documentation and modetbl (vile.hlp)

	+ added missing mlreply call in goto-named-mark processing, to
	  allow "goto-named-mark a" to be used in a command file. (basic.c,
	  proto.h)

	+ added man page (don't get excited -- it's short), and turbo makefiles
	  vile.mak, mktbls.mak.  (makefile)

	+ fix to ins/inschar/istring to correct for bad backspace limiting
	  (insert.c)

	+ fixed off-by-one bug for column count under X. (x11.c)

	+ turned off typahead detection for X11.  updates were being held
	  off incorrectly. (termio.c)  related change, possibly unnecessary:
	  reordered initial mlforce() and initial update() in main, because
	  xvile was not starting up properly.

	+ no longer set _bspace bit in _chartypes[backspc], and isbackspace()
	  macro tests backspc directly.  will make rebinding of backspace
	  simpler. (estruct.h, termio.c)

	+ some changes to set-terminal stuff -- took out the keys that can
	  be rebound with bind-key, i.e. those that have effect only in
	  command mode.  added help.  (bind.c, vile.hlp)

	+ no longer strip the SPEC bit in ttgetc, so a meta key can be used
	  to generate "function keys"  (termio.c, vile.hlp)

	+ updated README

Changes for vile 3.45:
(tom)

	+ more fixes for long/int complaints from TurboC (files tags.c spawn.c
	  random.c main.c display.c bind.c).

	+ added logic to fileio.c so that (MSDOS only) overwriting a file
	  saves the old version renamed to ".bak" suffix.

	+ simplified argument processing in file main.c; reduced size a little.

	+ reorganized code that uses tungetc so that it isn't used as often.
	  This also makes 'tpeekc()' obsolete.  Moved extern-defs for 'tungotc'
	  to edef.h (files input.c, exec.c, globals.c, oneliner.c, termio.c).

	+ modified 'setvar()' in eval.c to show warning message if attempt is
	  made to modify a readonly variable, or if for some other
	  reason an environment variable cannot be set.

	+ modified 'ibmcres()' in ibmpc.c to allow user to reset using value of
	  CDSENSE as argument.

	+ corrected inequality-test in 'newscreensize()' (file display.c) that
	  prevented user from correctly setting $sres back to VGA in IBMPC
	  config.

	+ renamed entrypoints in file crypt.c to 'ue_setkey()' and 'ue_crypt()'
	  to avoid confusion with unix crypt(3) (files fileio.c, file.c
	  crypt.c, main.c, cmdtbl).

	+ corrected core dump when using encryption from command line by
	  splitting out 'ue_makekey()' from 'ue_setkey()'; used this from
	  'resetkey()' in file.c

	+ corrected handling of -k (crypt option); added a global 'cryptkey'
	  which is used to set the encryption key only in files listed as
	  command arguments (files edef.h, main.c, buffer.c).

	+ overwrite -k parameter value in main.c (for those systems where this
	  has an effect, it changes the value shown in a "ps" command).

	+ made 'quickreadf()' decrypt file.

	+ corrected 'resetkey()' so that it doesn't automatically use crypt key
	  for files that aren't the same as the buffer.

	+ The prompt added to 'resetkey()' also required moving some calls on
	  'ttclean()' (file.c).  Simplified some of the ttclean code using
	  CleanToPipe and CleanAfterPipe macros.

	+ corrected 'resetkey()' so that if no password is provided (at least
	  for pipes), the operation will proceed properly (i.e., read/write
	  without encryption).

	+ modified 'modeline()' in display.c to show if the current buffer is
	  encrypted.

	+ made NAMEC and TESTC define to variables name_cmpl and test_cmpl,
	  which can be modified at run-time (files estruct.h, edef.h).

	+ added commands "set-terminal" and "show-terminal" to set/show special
	  characters (including NAMEC/TESTC values!).  Put this into file
	  bind.c (reusing pieces of 'bindkey()').

	+ make '#' (special prefix-key) rebindable, like cntl_a-prefix.

	+ make display-routines in bind.c show the actual characters to which
	  CTLA and CTLX are bound (function 'kcod2str()').  Note that
	  'prc2kcod()' still assumes that CTLA and CTLX are bound to control
	  characters.

	+ changed argument type of TERM.t_cres to 'char *' to allow consistent
	  usage of variable 'sres[]' (files tcap.c, ibmpc.c, z_ibmpc.c).

	+ changed values recognized by ibmpc.c for "$sres" to be the names
	  that it returns (e.g., "VGA") and the number-of-lines (from main.c's
	  arguments).

	+ finished off 'npopen()' for MSDOS (allows write-pipes) and
	  implemented 'inout_popen()'; used in file spawn.c for filter-region
	  command.

Changes for vile 3.44: (pgf)
	+ fixed indentation if openup used in cmode, with a single word on
	  the current line -- indentation was coming from the _next_ line,
	  instead (insert.c).

	+ finderrbuf() now remembers its last argument (finderr.c)

Changes for vile 3.44:
(tom)
	+ corrected file descrip.mms; the vms_link.opt file no longer applies
	  to mktbls.exe; also fixed 'clobber' rule.

	+ corrected file glob.c (don't treat '$' as a wildcard on vms).

	+ correction: moved call on 'global_val_init()' before
	  'expand_wild_args()', to make wildcard globbing work on vms and
	  ms-dos (file main.c).

	+ corrected logic of 'kbd_reply()'; last_eolchar was not being set
	  properly when NAMEC wasn't space (file input.c).

	+ modified 'kbd_engl_stat()' in file bind.c so that if NAMEC is not
	  bound to space, then name-completion for :-commands is turned on.

	+ added option KBD_NULLOK to 'kbd_reply()' and used it in function
	  'kbd_engl_stat()' to re-allow commands to be empty (files bind.c,
	  input.c, estruct.h), needed after decoupling NAMEC and space.

	+ corrected 'slowreadf()' in file file.c so that dos-mode files are
	  read properly.  (The code stripped CR's even when dos-mode wasn't
	  set).

	+ implemented 'npopen()' and 'npclose()' for MSDOS; used this to make
	  MSDOS logic for ^X-!  the same as on UNIX (modified files fileio.c,
	  npopen.c, spawn.c).

	+ modified 'makename()' to yield essentially the same result for
	  MSDOS as for UNIX (in file.c), because 'npopen()' works similarly.

	+ added tests in 'ffropen()' and 'ffwopen()' to prevent accidentally
	  opening a directory-file.

	+ corrected handling of FIOFNF in 'readin()'; made this work on MSDOS
	  as well as UNIX (files file.c, fileio.c, display.c).  The value of
	  'errno' was not being saved to report in the error message.  Added
	  'mlerror()' to display.c to encapsulate the error reporting better.

	+ moved 'errno' declarations into 'estruct.h' (files display.c, file.c,
	  npopen.c, termio.c).

	+ added function 'set_febuff()' to encapsulate the 'febuff[]' and
	  'newfebuff' variables; moved these to finderr.c (files file.c,
	  finderr.c, spawn.c).

	+ corrected test in 'filename()' (file.c) for ":f" command.

	+ corrected test in filec.c for pre-existing file (don't prompt if it
	  is a pipe-command).

	+ corrected tests in filec.c for prompt for re-read (didn't look for
	  abort-status).

	+ test-compiled CRYPT, VMALLOC configuration flags on TurboC.  Modified
	  files crypt.c, estruct.h, vmalloc.c

	+ began addressing TurboC warnings for conversion between long/int.

	+ deleted obsolete defines in file estruct.h (CLRMSG, CTRLZ).

	+ added 'set_rdonly()' to encapsulate the places where an automatically
	  created buffer is made read-only.  This makes them more consistent.
	  Files: bind.c, exec.c, main.c, oneliner.c, random.c, spawn.c

	+ modified 'update()' in file display.c so that '.' commands don't
	  redisplay if VISMAC is configured (previously, only keyboard macro
	  redisplay was suppressed).

Changes for vile 3.43:
(pgf)
	+ changed NAMEC from SPACE to TAB (estruct.h).  changed eolchar arg
	  passed to kbd_reply in kbd_engl_stat from NAMEC to ' ', to
	  compensate.  added check against command term'ed with ' ' in
	  execute_named_cmd() (exec.c).

	+ added check for iswild() when scanning for wildcard chars in glob.c

	+ allow nulls as well as newlines to separate filenames in glob-pipe
	  output.

	+ added finderrbuf() routine (command is "error-buffer" or "find-next-
	  error-buffer-name"), to set the name of the buffer to scan for errors.
	  (finderr.c, proto.h, cmdtbl, vile.hlp)

	+ fixed pathname bug when scanning output that contains Entering/
	  Leaving pairs.  We no longer just keep a pointer to where we
	  found the directory name, but now malloc space for it.

	+ horizontal scrolling moves by half screens by default (window.c,
	  display.c)

	+ added AIX to machines that need sys/ioctl.h (termio.c, display.c)

	+ further fix to repeat counts and the '.' command, so that all cases
	  ( 6dw , 3d2w , etc.) work.

	+ support for letting a named register on a '.' command override
	  what was originally typed:  "adw"b.  will put one word in register
	  a and the nex in register b.  (input.c line.c exec.c)

(tom)
	+ corrected define in dirstuff.h to make TurboC compile again.

	+ corrected logic of 'makename()' in file.c for MS-DOS.

	+ added type CMASK to correct use of _chartypes[] array on MS-DOS
	  (e.g., in 'screen_string()' in input.c).

	+ deleted VMS-DCL file 'vile.com', since this causes conflict on
	  MS-DOS.

	+ lowercased color names in file modes.c

	+ modified ibmpc.c to get true white and yellow colors (had gray and
	  brown).

	+ modified files ibmpc.c and display.c to allow MS-DOS version to do
	  scrolling.

	+ removed unused assignments from file insert.c

	+ substituted all l_fp references to lforw macro, l_bp references to
	  lback macro, verified mechanically.

	+ make special case in 'dotcmdplay()' to allow (like vi) the repeat
	  count on '.' command to replace that on the replayed command.

	+ modified logic of 'writereg()' to allow its call on 'imply_alt()' to
	  correctly set the 'copy' parameter (only if a whole file is written).

	+ introduced new function 'same_fname()' to correct places where
	  matches between a filename and the buffer's filename broke (either
	  because it was not in canonical form, or because VMS version numbers
	  were not stripped).

	+ modified fileio.c to read/write binary files on MS-DOS to avoid
	  truncating files at ^Z.  This does not make ^M visible however.

	+ ifdefd out contents of 'dot_replays_macro()' in input.c to avoid
	  side-effects

	+ corrected logic of 'tungetc()' in file input.c; this had broken
	  replay of keyboard macro "iX<esc>/Y^MaZ<esc>".

	+ split out 'expand_wild_args()' from main.c into a new file glob.c,
	  which contains globbing code.  Moved 'glob()' from path.c to glob.c
	  also.

	+ moved function 'is_directory()' from filec.c to path.c; made it
	  public.

	+ added function 'no_memory()' to encapsulate the more common places
	  that vile/msdos runs out of memory; use this in several places to
	  simplify error reporting.

	+ added universal-mode "history", to allow turning that function off,
	  and instrumented history.c to automatically turn logging off if vile
	  runs out of memory.  (It is annoying to get out-of-memory errors on
	  _every_ command).

	+ modified display.c to fill in the long space before program & version
	  identifier for the scratch buffer (on the mode line).

	+ added option to 'mlreply_file()' to allow expansion of wildcard to
	  multiple files.  Use this in file.c (for ":e" and ":view" commands).

	+ modified files descrip.mms and vms_link.opt because (with new module
	  glob.c) the link-command line got too long.  Other mods to file
	  descrip.mms to make file vile.com generated (to avoid leaving it
	  around for ms-dos configuration), and the 'clean' and '.last' rules
	  (to make them less noisy).

	+ corrected 'ffronly()' in fileio.c; on VMS this caused some files to
	  be touched.

	+ implemented scrolling in file vmsvt.c

	+ implemented 'glob' mode to control how wildcards are expanded
	  (affects files main.c, modes.c, glob.c) and the corresponding piped
	  commands for glob-mode.

	+ added logic to 'makename()' to handle embedded blanks in filenames.

Changes for vile 3.42:
(pgf)
	+ added routine tb_stuff() (tbuff.c), which is roughly the
	  converse of tb_peek(), to allow simple replacement of a character
	  in a tbuff.

	+ used tb_stuff() to increment the buffer number when referencing
	  the numbered buffers -- i.e. "1p becomes "2p in a '.' command.
	  (input.c)

	+ backed out change to 'get_recorded_char()' (input.c) that caused
	  dd..."1P... to continue deleting lines after the P operation.

	+ added horizontal scrolling mode, to cause whole screen to shift
	  instead of just one line when cursor goes "off screen".  this
	  mode ("set hs", or "set horizscroll" is now the default. (display.c,
	  vile.hlp, main.c, modetbl, proto.h)

	+ allow cursor to rest in last column, if line is just that long, to
	  prevent extra horizontal scrolling. (display.c)

	+ split up the ins() function, to provide an inschar() function that
	  istring could use.  istring() now called by insstring() and
	  overwstring(), which are bound to "insert-string" and new
	  "overwrite-string" commands.  all this to allow "insert-string"
	  to take advantage of autoindent, cmode, wordwrap, etc.  will
	  need tweaks later when tty chars are rebindable (^U, ^W, etc.)

	+ compiled with "-Wmissing-prototypes", which turned up a bunch of 'em.
	  (proto.h)

	+ some linux gcc cleanup, mostly just including some more headers
	  (display.c file.c main.c)

	+ ifdefed out the onamedcmd() function, and it's binding (cmdtbl,
	  exec.c)

	+ took out the extern declarations of getpwnam and getpwuid that i
	  put in recently.  they seem to be causing more problems than they
	  were fixing.  in general, providing prototypes for system routines
	  is a real pain.  i wish there were an "ifnprototype" construct.

Changes for vile 3.41:
(tom)
	+ ifdef ioctl.h for APOLLO (note: you shouldn't use '"'-form for system
	  includes, since that could pick up files in the current directory,
	  and also because it does weird things to the include-path).

	+ moved code to support USE_LS_FOR_DIRS from filec.c to path.c, making
	  it look like unix-style directory routines.  Added ifdef for
	  OLD_STYLE_DIRS just in case...

	+ modified 'slowreadf()' in file.c so that piped input scrolls from the
	  bottom of the window, rather than the middle.

Changes for vile 3.40:
(pgf)

	+ the makefile no longer supplies any -I options.  this will cause some
	  machines to not find "ioctl.h" where it's in the sys subdirectory,
	  but I just spent a day tracking a bug caused by picking up the wrong
	  system headers (i needed the gcc "fixincludes", and they had moved).

	+ now honor LINES and COLUMNS in getscreensize (display.c)

	+ now handle searching multiple tags files specified in "set tags"

	+ bugfix to fences.c, which fixes indentation when inserting '}'

	+ added firstnonwhite() calls to godotplus, so we can publish it as
	  "whole-lines" (files basic.c, cmdtbl).

	+ added 'setjmp()' code to files main.c, input.c to handle ^C with
	  BSD-style signals.

	+ added define USE_LS_FOR_DIRS to files dirstuff.h and filec.c to
	  accommodate systems w/o a 'readdir()' function.

	+ corrected $pagewid (was returning length instead of width) in file
	  eval.c

	+ use lsprintf to form [Macro nn] names (file exec.c)

(tom)
	+ corrected 'ffwopen()' in fileio.c so that write of current buffer on
	  VMS works properly (forgot to strip version number).

	+ made 'flook()' work properly for vms (file bind.c)

	+ moved 'dname[]' table into file exec.c and 'cname[]' table into file
	  modes.c; did this to get rid of 'comma' macro, which does not work as
	  intended on Turbo-C (the comma got evaluated too soon).

	+ to prevent redefinition error in Turbo-C, removed redundant includes
	  for <stdio.h>, <string.h> and <signal.h> (already in file estruct.h)
	  from the ".c" files: ansi.c at386.c basic.c bind.c buffer.c crypt.c
	  dg10.c display.c eval.c exec.c file.c globals.c hp110.c hp150.c
	  ibmpc.c input.c isearch.c line.c main.c modes.c npopen.c oneliner.c
	  regexp.c region.c search.c spawn.c st520.c tcap.c termio.c tipc.c
	  vmsvt.c vt52.c window.c word.c wordmov.c x11.c z309.c z_ibmpc.c

	+ moved macro 'P' from file proto.h to estruct.h, adjusted definitions
	  in file 'estruct.h' to allow use of 'P' macro in CMDFUNC definition.
	  Added definition of type 'CmdFunc' for the actual pointer-type.

	+ use P-macro for TERM struct definitions (and added TTscroll),
	  modified files estruct.h and display.c

	+ corrected logic of 'dot_replays_macro()' in file input.c (for the
	  special case of macro "@w" followed by "dd" command, I noticed that
	  '.' replayed both commands).

	+ compiled with Turbo-C, turned on warning if function was called w/o a
	  prototype.  Added prototypes (including those for static procedures)
	  to get rid of these and other compiler warnings.  Doing this found an
	  error in eval.c and one in history.c (files modified: bind.c buffer.c
	  display.c eval.c exec.c fences.c file.c fileio.c history.c ibmpc.c
	  input.c insert.c main.c mktbls.c modes.c npopen.c opers.c path.c
	  random.c regexp.c region.c spawn.c tbuff.c window.c).

	+ corrected MSDOS-specific pathname translation in files path.c and
	  random.c

	+ corrected d_namlen value supplied in readdir function for VMS (file
	  vms2unix.c)

	+ implemented first version of filename completion for VMS.  (Directory
	  names are not completed; there are still some ambiguities between
	  scratch buffer names and VMS directory paths).

	+ added option KBD_UPPERC to input.c; use this to force prompts for VMS
	  filenames to uppercase.

	+ removed redundant declarations for 'getenv()'; it is already in file
	  estruct.h

	+ added new file 'vmspipe.c' which provides a piped-read facility
	  'vms_rpipe()'.

	+ modified file finderr.c to recognize lint output on SUNOS; also
	  corrected it to ensure that sscanf doesn't scan past the string
	  limits (no EOS was formerly on each line).

	+ added/used new functions 'pathleaf()' and 'pathcat()' to file path.c
	  to hide vms/unix pathname differences.

Changes for vile 3.39 (from Tom Dickey):

	+ corrected error in 'get_recorded_char()' (file input.c) that caused
	  command "ix^[j.j.j.j." to not repeat properly.

	+ corrected prompt-string in 'ShellPrompt()' (file spawn.c)

	+ corrected behavior of filename completion (file filec.c) so that
	  internal names and shell names can have blanks in them.

	+ provided a last-replay value for 'execfile()' in file exec.c (it was
	  defaulting to the current buffer, which is wrong).

	+ corrected pointer test in 'tb_sappend()', which was null in the
	  special case of a ":w" command before any ":e" or other read.

	+ simplified MSDOS-drive translation (untested) in file random.c to
	  accommodate changes made for VMS in other files.

	+ modified 'cd()' procedure in file random.c to chdir to home directory
	  on systems that have one.

	The following are changes to support VMS:

	+ added new files (shown in makefile and descrip.mms): dirstuff.h,
	  vms2unix.c, vile.com

	+ use new file 'dirstuff.h' for definitions in filec.c and path.c

	+ corrected some dependencies in descrip.mms

	+ corrected logic in 'imply_alt()' (file buffer.c) that lost the
	  original filename argument (replaced it with the lengthen-path).

	+ corrected logic in 'getfile()2' (file file.c) to account for the
	  distinction between buffer-names and file-names.  Simplified
	  'getfile()' and 'getfile2()' because of modifications to
	  'shorten_path()' and 'lengthen_path()'.

	+ corrected logic of 'ffread()' (file fileio.c); on VMS a single read
	  does not necessarily read an entire file.

	+ corrected definition of macro 'isready_c()' for VMS.

	+ modified 'shorten_path()' (file path.c) to provide the "./" prefix
	  that was added in files buffer.c and display.c; also absorbed the
	  test for MSDOS device prefix (e.g., "c:") there.  This was needed to
	  allow VMS port to show native pathnames.  The equivalent of "./" on
	  VMS is "[]".

	+ use VMS function 'fgetname()' and 'getname()' to determine the
	  fully-resolved path in 3 cases: after writing a file, after reading a
	  file, and in 'lengthen_path()' (files file.c and path.c).  This makes
	  it know the current version number of each file.

	+ modified 'makename()' (file file.c) to account for VMS pathnames.
	  Note that DecShell names (which the C runtime library accepts as
	  arguments to fopen, etc.) look much like unix filenames.

	+ added new function 'is_internalname()' to file path.c; this serves
	  the same purpose as the (now obsolete) macro isScratchName.  This
	  makes a better check (to keep VMS paths distinct from scratch buffer
	  names) than the macro.

	+ deleted macro isScratchName from file estruct.h; changed macro
	  isInternalName to use function 'is_internalname()'.  Adjusted other
	  macros in estruct.h and display.c to make VMS port work properly.

	+ added new function 'non_filename()' to file path.c; use this in files
	  bind.c and random.c to encapsulate the program+version string.  Use
	  'non_filename()' in 'is_internalname()'.

	+ corrected logic of 'screen_string()' (file input.c) to handle the
	  distinction between "[" and "]" used to delimit pathnames on VMS, and
	  to indicate scratch-buffer names.

	+ corrected wildcard, path and scratch-buffer characters for VMS (file
	  main.c).

	+ corrected/modified file path.c so that the functions therein support
	  VMS filenames.  Added several new functions to do this.

	+ corrected code for 'catnap()' (file random.c) for VMS.

	+ corrected several 'exit()' arguments (file termio.c)

	+ corrected VMS's ttgetc function (its interaction with 'typahead()'
	  was wrong; also 'typahead()' was not really implemented (file
	  termio.c).

	+ ifdef'd out a redundant 'ttclean()' call (which caused the output on
	  VMS to be closed more than once, which is really an error).

	+ guard 'vtinit()' (file vmsvt.c) against t_mcol or t_mrow too small

Changes for vile 3.38:
(pgf)
	+ cleaned up internal naming and help for alt-tabpos mode.
	  alt-tabpos on implies emacs-style, and it's off by default

(tom)
	+ corrected status-check in 'ffclose()' (in fileio.c) that generated
	  spurious 'Error on close' message, visible when nothing was read from
	  a pipe.  (Also eliminated redundant defs for 'npopen.c' in fileio.c
	  and proto.h).

	+ corrected place in 'globals.c' where (if the buffer was readonly),
	  the '/' delimiter after an s-command was not eaten.

	+ modified loop-limit in "show-registers" command to display the
	  numbered registers.

	+ corrected file fences.c, allowing C preprocessor lines to have
	  leading whitespace before '#'.  The cursor may be on any character
	  from the first column through the '#'.

	+ rewrote procedure 'cpp_fence()' in file fences.c, allowing it to
	  recognize "elif" keyword.  Added 'nextchar()' to file basic.c to
	  support this.

	+ modified procedure 'ShellPrompt()' in file spawn.c to eliminate the
	  ": " before prompts for ^X-!.

	+ corrected 'kbd_reply()' in file input.c to invoke 'tgetc()' directly
	  when ^V is used to escape a character.

	+ corrected 'kbd_kill_response()' in file input.c; ^W is not supposed
	  to trim blanks before a word.

	+ corrected 'update()' in file display.c; the VISMAC define does not
	  apply to "." command.  This fix allows showmatch to work with "."
	  command.  Also, in 'fmatch()' in file fences.c; test return-value of
	  'update()' to avoid catnap when update fails.

	+ corrected 'insert()' and 'insertbol()' in file insert.c; the cursor
	  was not being advanced after repeated insertions like vi.

	+ corrected file input.c so that it can handle a "." command within a
	  keyboard macro.  The macro was "iA^[l2." (for testing).

	+ modified file input.c (new procedure 'dot_replays_macro()' so that
	  the "." command after defining or executing a keyboard macro causes
	  the macro to be (re)invoked.

	+ split 'kcod2prc()' in file bind.c into 2 parts; the new one (i.e.,
	  'kcod2str()') is used to support 'dot_replays_macro()'.  Also use new
	  function 'fnc2kcod()'.

	+ added new function 'kbm_started()' to file input.c to get rid of then
	  non-recursive variable 'kbdplayreg'.

	+ save/restore "." repeat counts in 'start_kbm()' and 'finish_kbm()' so
	  that repeat works properly for keyboard macros.

Changes for vile 3.37:
(pgf)
	+ works on osf/1 alpha now, with hacks applied to termio.c and
	  display.c
(tom)
	+ added files 'descrip.mms' and 'vms_link.opt'; compiled on VAX/VMS
	  (but did not make it run) (tom dickey)

	+ pathname tracking fixes (calling shorten_path() correctly everywhere)

Changes for vile 3.36 (from Tom Dickey):

	+ supplied initial value for dirc-mode

	+ guarded 'mlreply_file()' against missing curbp->b_fname (for use in
	  .vilerc startup).

	+ corrected logic in 'kbd_reply()' (file input.c) that caused backspace
	  to be treated as a command-delimiter.

	+ corrected logic in 'kbd_reply()' (file input.c) that caused '/'
	  character in complex substitutions to be eaten, e.g.,
		:g/pattern/s//newpattern/g
	  broke after the 's'.

	+ corrected calls (broke in 3.35) on 'mlreply_file()' from 'insfile()'
	  and 'operwrite()' (mods to files estruct.h, filec.c, file.c and
	  opers.c).  The ^R and ^W commands always prompt for name, rather than
	  taking it from the screen.

	+ corrected end-of-line logic in 'do_a_mode()' (file modes.c).  This
	  had allowed history scrolling to display keywords that were separated
	  by blanks; fixed this by testing for both space and "=" characters.

	+ added calls to 'hst_glue()' in 'unqname()' and 'getfile2()' to add
	  blank in command history before the responses these functions solicit
	  from 'mlreply()'

	+ added macro 'char2int()' to file estruct.h, use this in fileio.c and
	  tbuff.c to hide byte-masking.

	+ changed data in TBUFF to 'char' (unsigned char does not buy that much
	  conciseness); absorbed macros 'tb_values()' and 'tb_length()' into
	  file tbuff.c as functions.

	+ corrected logic of 'tb_leaks()' (file tbuff.c); I had only exercised
	  this at the end of execution.

	+ removed unused define for TMPSTOP

	+ recoded handling of keyboard macros in file input.c to use TBUFF
	  structs (so there is no limit on length of keyboard macros).

	+ modified 'execkreg()' to remove length-limit on macros there as well
	  (file line.c)

	+ modified 'execkreg()' to allow invoking registers 0-9 (file line.c)

	+ modified files input.c and line.c so that "." and @-commands can be
	  invoked from within a keyboard-macro (i.e., ^X-&).

	+ modified files insert.c and input.c so that repeated "." command
	  works properly for i,I,R commands (the cursor must appear at the end
	  of the insertion).

	+ modified so that ^Z (job suspension) works properly on apollo sr10.3
	  (files termio.c and spawn.c).

	+ modified 'ttgetc()' in file termio.c so that (for apollo) it reads in
	  raw-mode.

	+ added flag to 'tgetc()', allowing it to suppress the conversion of
	  intrc to abortc (files basic.c csrch.c eval.c exec.c input.c
	  insert.c).  This change also lets users quote ESC characters.

	+ corrected unused args in 'globber()' (file globals.c)

	+ corrected special case in use of 'canonpath()' on apollo (user could
	  type in path beginning with "/", which was not getting "//" portion
	  of pathname prepended.

	+ renamed "vitabpos" mode to "alt-tabpos" mode, avoiding conflict with
	  "view" mode.

	+ added "implybuffer" (ib) mode to use in splitting out vi-style
	  buffer-implication from 'autobuffer' mode (files modetbl and main.c).

	+ added "samebangs" (sb) mode to allow the ^X-!!  and :!!  commands to
	  be optionally the same.

	+ Corrected logic of 'imply_alt()' so that it works with files read, as
	  well as written (files buffer.c and file.c).

	+ split out the pathname functions into file path.c

	+ added function 'is_appendname()' to path.c to better encapsulate the
	  isAppendToName macro's knowlege of the ">>" length (files path.c,
	  filec.c, fileio.c, random.c).  Used this to make functions
	  'canonpath()', 'lengthen_path()', 'shorten_path()' and
	  'is_pathname()' work properly with ">>" prefix.

	+ added code to 'path.c' to resolve "~" as user's home-directory.

	+ corrected code in 'makebufflist()' that caused b_bfname member to
	  be modified (file buffer.c).

	+ moved MSDOS drive-logic from 'ch_fname()' into 'lengthen_path()'
	  (files random.c and path.c)

	+ added procedure 'copy_val()' to modes.c to encapsulate the make-local
	  and make-global macros.  This fixes a case in which memory could be
	  freed more than once: if the user had done 'setl' commands for two
	  different buffers.

	+ made variables 'doslines' and 'unixlines' local to the procedures
	  'slowreadf()' 'readin()' in file.c; initialized these so that
	  successive reads of a set of files will treat the dos/unix line
	  counts separately.

	+ added command "set-dos-mode" to force the current buffer to be
	  in DOS-mode (trimming CR's).

Changes for vile 3.35 (from Tom Dickey):

	+ addressed compiler warnings from gcc 1.36 on apollo sr10.3.  (I know
	  it is old, but it gives useful warnings).  This required some minor
	  adjustments to defines/includes in files estruct.h, proto.h, main.c,
	  fileio.c, npopen.c, random.c to accommodate order-of-definition
	  problems in stdio.h and signal.h

	+ tested with gcc 1.40 on sunos 4.1.1

	+ tested compiling with SMALLER=1; fixes to files eval.c, input.c,
	  mktbls.c, npopen.c, proto.h, random.c (On my machine, this only cuts
	  about 10% from the binary).

	+ added EOS at mismatch-point in 'fill_partial()' in file bind.c, fixes
	  bug that caused incorrect display after repeated name completion and
	  query operations.

	+ guarded 'kbd_complete()' against embedded nulls in its buf-argument,
	  by testing for nulls before completion logic in file input.c

	+ added a flag 'kbd_expand' to make calls on 'kbd_putc()' via
	  'mlforce()' and related functions show tabs as a space.  This
	  modifies files bind.c, edef.h, display.c to fix a problem I
	  introduced in 3.32

	+ use 'kbd_expand' flag in file input.c to fix non-displaying ^M; other
	  fixes include quoting and backslash fixes, as well as correction to
	  wkillc handling.

	+ modified finderr.c to make the embedded tabs as "\t" digraphs, so
	  they are visible.  Added pattern to use with apollo C compiler.

	+ corrected logic in 'updpos()' (file display.c) so that vile displays
	  cursor location just-like-vi for tabs and control characters.

	+ made 'wordcount()' code (file word.c) work properly.

	+ corrected 'join()' (file word.c) so that blanks are not inserted when
	  joining after a space, or joining a blank line.  Also (like 'vi'), if
	  there is trailing whitespace on the first line, don't modify it.

	+ corrected flags for join-lines in file cmdtbl so that the region-
	  oriented code can work.

	+ made join-lines work with regions (files opers.c, word.c).

	+ restored GLOBOK flag for join-lines-til (regions work now).

	+ interchanged order of calls on 'toss_to_undo()' and 'lremove()' in
	  'ldelnewline()', since this was causing incorrect cursor placement
	  after an undo.  (The particular case arose in the rewritten
	  join-lines, when joining a blank line to a non-blank line).

	+ modified 'edithistory()' in file history.c to ifdef out the ESC code
	  (with new symbol KSH_HISTORY), merge the loops to use 'escaped' flag
	  to avoid redundant logic, and pick up control chars (e.g., ^N and ^P)
	  for scrolling.  This also modified files input.c and exec.c

	+ corrected logic in history.c (from 3.34) that uses the function
	  'WillExtend()' to modify/macroize !-commands.  The code did not
	  provide for the case in which the user would modify the text after
	  scrolling to it.

	+ corrected call on 'hst_flush()' in file pipecmd.c (it was not called
	  if the pipe-command failed).

	+ revised 'tgetc()' in file input.c to make it use a single return
	  point (simplifies testing and debugging).

	+ corrected file modes.c so that user can abort from a set command.

	+ deleted code for unused global 'lineinput' (files bind.c, input.c,
	  edef.h).

	+ renamed ifdef for LAZINESS to (global mode) TAGSLOOK, added an entry
	  to modetbl for "looktags" to test/update this code.  Note that this
	  is turned off by commenting out the "LookTags" line in modetbl.  This
	  affects also files main.c, file.c modes.c and tags.c

	+ added file 'filec.c' to perform filename completion.  Moved code that
	  was ifdef'd LAZINESS there, and recoded so that it stores paths
	  normally (not reversed!).

	+ created function 'mlreply_file()' in filec.c, used this to
	  consolidate prompting for filenames in file.c (Note that now all
	  filename prompts can get data from the screen).

	+ made most of the calls on 'mlreply_file()' default to the current
	  filename; this is simpler than the last name given to the command.

	+ moved check for writing view-only buffer from 'filewrite()' as new
	  local function 'writable()', before calls on 'writereg()' This
	  affects 'filewrite()', 'filesave()', 'imdying()' and 'writeregion()'
	  implicitly.

	+ use 'mlreply_file()' in 'execfile()' (file exec.c)

	+ use 'mlreply_file()' in 'operwrite()' (file opers.c)

	+ added 'no_such_file()' to file.c; use this in 'execfile()' so that
	  ":source" will report an error if the file is not found.

	+ created function 'mkreply_dir()' modeled after 'mlreply_file()' in
	  filec.c, used this to allow pathname completion in 'cd()' in random.c

	+ added mode "dirc" to file modetbl, to support directory name
	  completion.

	+ added l_number to LINE struct, b_bytecount to BUFFER struct (file
	  estruct.h) to use in caches for # of bytes and lines displays.

	+ added macro 'for_each_line' to file estruct.h

	+ added procedure 'bsizes()' to buffer.c to compute values for l_number
	  and b_numchars, b_numlines values.  Will later get this to cache
	  results to speed up 'line_no()' and related code.

	+ corrected column-splitting in file modes.c (error in 3.34 changes)
	  which caused dos-mode to not be shown.

	+ corrected flag in 'modetbl' for dos-mode: this makes dos-mode show in
	  the status-line.

	+ added new entrypoints to tbuff.c: tb_copy, tb_scopy, tb_bappend

	+ merged code in 'spawn1()' and 'pipecmd()' (file spawn.c) so that
	  !-commands and !!-commands work consistently.

Changes for vile 3.34 (from Tom Dickey):

	+ modified mktbls.c to use 3rd column of modetbl (window-flags).  Moved
	  window-flag defines in estruct.h to allow their use in nemode.h

	+ in modes.c, moved the code that updates window hints-flags from the
	  'adjustmode()' to 'adjvalueset()' procedure so that it can use the
	  new window-flags entry in nemode.h

	+ modified mktbls.c (and estruct.h, edef.h) to support global modes.

	+ added to modes.c: 'new_regexval()', 'free_regexval()', and
	  'free_val()'.  Use these in main.c and modes.c to simplify
	  leak-testing.

	+ added to estruct.h defines to use in testing memory leaks using both
	  dbmalloc as well as apollo-specific traces.

	+ added to main.c ifdef for NO_LEAKS; used this in testing for
	  permanent memory leaks (freeing data also finds specific types of
	  allocator corruption).  Added test-code for this purpose to:
	  buffer.c, display.c, line.c, regexp.c, tbuff.c, window.c

	+ plugged memory-leak in 'getfile()' in file.c by splitting it after
	  the temporary-buffer allocation, making tbp a static value, and
	  deleting the line that set the tbp.b_fname pointer to null.

	+ revised the value-checking in 'adjvalset()' (file modes.c) so that
	  unnecessary changes are ignored.

	+ further modified file modes.c so that typing ' ' (except after string
	  or pattern values) will continue the prompting for further set-modes,
	  as in noninteractive use.

	+ put the TTputc for '\r' back into 'kbd_putc()'; omitted this when
	  taking code for 'mlmsg()' from display.c

	+ added new function 'GetEnv()' to eval.c to guard against
	  null-return from 'getenv()'.

	+ added new command (to file line.c) "show-registers" to show the
	  contents of kill-buffers and registers.

	+ added new module 'history.c'.  This implements up/down scrolling in
	  an invisible buffer "[History]", which logs all :-commands.

	+ coded-around apollo sr10.3 optimizer-bug in 'execkreg()':
		kbdend = &kp->d_chunk[i];
	  resulted in kbdend == kp->d_chunk.

	+ moved autobuffer and c-suffixes modes to global-table.  Deleted 'cwd'
	  mode.

	+ modified mktbls.c so that each table will be sorted by name (not by
	  type&name).  Modified modes.c to take advantage of this and display
	  the modes sorted in columns (more like 'vi'), except for long strings
	  (which are deferred to the bottom of the display, still).

	+ make modes.c show the actual color name in addition to the index.

	+ rewrote 'kbd_string()' to interface it with 'edithistory()'.  Split
	  out entrypoint 'kbd_reply()' to support integration of history with
	  'namedcmd()'.

	+ redefined 'kcod2key()' in terms of N_chars; added macro 'isspecial()'
	  to estruct.h

	+ revised 'kbd_engl_stat()' and 'namedcmd()' to use the 'kbd_reply()'
	  entrypoint so that user can scroll through history of any portion of
	  commands.

	+ modified 'search.c' to log history for '/' and '?' commands.

	+ modified x11.c to use memcpy rather than bcopy (for consistency).

	+ made x11.c compile properly on apollo sr10.3 (there is a conflict
	  between prototypes in <string.h> and <strings.h>).

Changes for vile 3.33:

    	* many: cleanup for gcc-2.3's -Wall warnings

    	* bind.c: added new function, insertion_cmd(), which returns char
	that gives us simple insert mode.  used in x11.c and input.c, for
	pasting and for insert-mode arrow keys

    	* estruct.h: don't redefine "const" on linux

    	* makefile: added aix warnings, changed name of ENV to ENVIR, due
	to name conflict with linux make

    	* random.c: add include of sys/select.h for AIX

    	* x11.c: added code to support arrow keys and function keys from
	Phil Rubini

Changes for vile 3.32 (from Tom Dickey):

	* modified estruct.h, proto.h and display.c to compile vile on HP/Apollo
	  SR10.2 (CC 6.7).  Added new definitions ANSI_PROTOS and ANSI_VARARGS
	  to simplify this.

	* modified 'mktbls.c' to produce nemode.h; use these definitions
	  to replace buffer/window mode definitions in estruct.h and edef.h
	  (note that this obsoletes symbol MAX_STRING_W_VALUE, reordered
	  window int-modes).  Further modified 'mktbls.c' to only create
	  ".h" files for the cases where it has data (so that I can split
	  modetbl off from cmdtbl, avoiding unnecessary recompiles).

	* supplied an ifdef (at least for APOLLO!) in termio.c so that ^C
	  and ^U work properly.

	* added buffer-mode 'wrapmargin' to file insert.c (which overrides
	  wrapwords iff it is set to nonzero).

	* added new module 'tbuff.c' to manage dynamic buffers in places
	  where local static buffers exist now.

	* modified 'ins()' in insert.c so that it reallocates 'insbuff[]',
	  allowing arbitrarily-large re-inserts.

	* modified 'input.c' to dynamically allocate 'tmpcmd' and 'dotcmd'
	  buffers.  This allows arbitrarily-large commands to be repeated
	  with '.'.

	* guard 'lengthen_path()' in file.c against shell/pipe/internal names.

	* added variable "$shell" (files cmdtbl and eval.c) to override
	  environment-variable "SHELL" in pipe-commands.

	* made 'show-vars' (file eval.c) show leading '$' or '%' to be
	  consistent with the way the user must type them.

	* suppress 'show-vars' (file eval.c) ERROR values (these correspond to
	  unimplemented variables)

	* corrected error in nextbuffer/swbuffer (file buffer.c) that allowed
	  vile to crash if "set noautobuffer" was specified in .vilerc

	* corrected display in 'show-vars' (file eval.c) of "$kill" (reference
	  to volatile data).

	* made new function 'kbd_complete()' (file bind.c) to allow keyboard
	  completion in tables other than NTAB.

	* modified logic in new function 'kbd_complete()' to show permissible
	  completions in response to '?'.

	* consolidated logic in exec.c, display.c, bind.c, input.c with new
	  procedures 'kbd_putc()', 'kbd_puts()', 'kbd_erase()', 'kbd_alarm()'
	  These hide random use of 'ttcol'.

	* make 'kbd_engl_stat()' (file bind.c) recognize word-delete character.

	* eliminated redundant unget/get (files bind.c, exec.c) in the interface
	  of 'kbd_engl_stat()'; this caused an infinite loop in the call from
	  'namedcmd()'.

	* modified bug-test in 'updpos()' (file display.c); I had a process hang
	  in that loop for some reason.

	* collapsed expand/dobackslashes args of 'kbd_string' (file input.c)
	  into one mask.

	* modified 'mktbls.c' further to produce table 'all_modes[]', to use
	  this for name-completion of mode.

	* modified 'mktbls.c' to recognize color-types so I could get rid of
	  upper/lower case kludge in modes.c

	* modified 'modes.c' to use color-types.  The interactive behavior now
	  uses 'kbd_string()' to split the input at the '=' mark (if given).

	* added option-flag KBD_LOWERC to 'kbd_string()' (file input.c) so that
	  things that are forced to lowercase will be shown in that way on
	  input.

	* modified 'token()' (file exec.c) to handle the 'eolchar' for
	  non-interactive calls on 'kbd_string()'.

	- modified 'eval.c' to recognize '=' as a separator between
	  variable-name and value, as in 'modes.c'.

	- modified 'eval.c' to perform name-completion for environment
	  variables (i.e., those beginning with '$').

	- modified 'modes.c' to accept vi-style multiple assignments in a ":set"
	  command (this works non-interactively).

	- corrected 'imply_alt()' in buffer.c; ensure that the fname-argument is
	  non-null.  If called via 'filterregion()', it will be null.

Changes in 3.31:  (Tom Dickey)

	* corrected test in 'find_alt()', which caused the %/# toggle to work
	  incorrectly in noautobuffer mode.

	* updatelistbuffers after 'cd' command

	* added 'chg_buff()' & 'unchg_buff()' to encapsulate logic to update
	  buffer-list after buffers are modified.  The function 'lchange()'
	  is replaced by 'chg_buff()'.

	* modified 'histbuffer' to toggle according to 'find_alt()'

	* make the filename/buffer/number change for 'killbuffer' apply to
	  'usebuffer()'

	* corrected handling of repeat-count in 'C' command.

	* updated vile.hlp

	* revised 'mktbls.c':

		* filter out redundant ifdef's
		* tab-align data items for readability
		* modify 'nefunc.h' to use prototypes rather than K&R style
		  definition, also split the extern-definitions from the
		  data items for readability.
		* make procedures static, linted
		* modified so that 'evar.h' definitions are automatically
		  generated, so that "show-variables" can show a sorted
		  list of variables.  The new filename is "nevars.h".

	* corrected window refresh flag for 'U' (lineundo) command.

	* added an apollo-specific code in 'imdying()' to generate a walkback
	  for debugging.

	* corrected an error in 'zotwp()': reference to data that had been
	  deleted.

	* corrected an error in 'onlywind()': it did not delink window-pointer
	  after freeing it.

	* noticed that references to 'dfoutfn' weren't reentrant:  I did a
	  ":show-var" from the buffer-list and got only part of the heading.
	  Reorganized display.c to ensure that it is reentrant by using a
	  local copy of 'dfoutfn'.  Note that I didn't add 'dfoutfn' as an
	  argument to 'dofmt()' to avoid complications with varargs.

	* corrected 'dfputli()': it didn't do hexadecimal.

	* corrected 'updateline()': it sometimes failed to highlight the ends
	  of the mode-line.

	* corrected (my last change) to 'vtset()' so that "^J" is shown in
	  list-mode.

	* moved 'writeall()' from main.c to buffer.c; it belonged there, and
	  I used local macros for simplifying it.

	* remove SIGHUP handler before normal call on 'exit()' to avoid
	  vile-died messages when running it in an xterm.

	* corrected VMS definition of exit codes; added def of BAD-macro.
	  modified main.c, display.c, window.c accordingly.

Changes in 3.30, mostly from Tom Dickey:

This is lots of changes, mostly in buffer.c:

	* lint-clean: buffer.c

	* linted also: file.c, modes.c (though I left size_t alone for now).

	* added "list-vars" & "show-vars" to show the current variables

	* modified 'killbuffer' so I could add binding ^X-k to point to
	  a buffer-name in the buffer-list and kill it.

	* also modified 'killbuffer' to recognize filename if buffer-name is
	  not found, also buffer-number.

	* modify buffer.c so that the number shown by '_' and '*' is consistent

	* fixed ^X-o and ^X-O so they update buffer-list, as well as some
	  other places I missed before.

	* make some functions static (where they are really private), because
	  my per-module lint library got too large.  (I believe there are
	  too many externals).  In modes.c, this involved reordering, since
	  my practice is not to have forward-refs on static functions, to
	  make this work with some awful compilers.  This meant that I
	  deleted lines from 'proto.h'.

	* introduced new macro 'isreturn()', used this in correction in
	  'histbuff()'.  I used it in one bug fix & also where I could find
	  explicit compare for \r, \n.

	* added mode 'autobuffer':

		* the default is 'autobuffer' (the vile lru buffering)
		* curbp no longer is necessarily equal to bheadp
		* added members b_created, b_last_used, b_relink to support
		  the autobuffer/noautobuffer mode.
		* if "noautobuffer" is set, ":rewind" and ":n" apply only
		  to the original command-line files.

	* added macros SCRTCH_LEFT, SCRTCH_RIGHT, ScratchName, IsInternalName
	  to encapsulate use of '[' in names.

	* added macros SHPIPE_LEFT, isShellOrPipe to encapsulate use of '!' in
	  names.

	* added function is_pathname to encapsulate tests for pathnames
	  beginning with '.'.  This fixes the display of ".vilerc" from the current
	  directory as "./.vilerc".

	* added new char-types _scrtch and _shpipe to support killbuffer from
	  screen.  Use these types in new macro 'screen_to_bname()' which uses
	  modification of 'screen_string()'.  Used 'screen_to_bname()' to
	  make ^X-e and ^X-k "correctly" parse the bname, fname columns from
	  buffer-list.

	* fixed the error you mentioned in modeline (the position of col-80 mark
	  taking into account sideways and number modes).

	* supplied a missing TTflush in input.c

	* modified 'makebufflist()' to show only the flags actually used in the
	  footnote.

	* modified 'makebufflist()' to show '%' and '#' by the corresponding
	  buffer/filename.

	* added macros 'for_each_buffer()' and 'for_each_window()' to simplify
	  code as well as to keep visible references to bheadp & wheadp
	  confined to buffer.c and window.c respectively.

	* added/used macro SIZEOF

-------------------------------------------------------------------------------

Changes in 3.29:
-----------------

(fixes supplied by tom dickey, eric krohn, and willem kasdorp)

buffer.c: changes for keeping the buffer list more up to date when
	on screen
fences.c: allow match of fence that is first char. in buffer
file.c: avoid inf. loop when choosing unique name for buffers that have names
	containing "x" in the NBUFN-1 position and allow ":e!" with no file
	to default to current file
globals.c: implemented 'v' command
isearch.c, line.c, display.c: ifdef of some unused code
main.c: added .C, .i to cmode suffixes, for C++ and cpp output files
makefile, install target: test for presence of vile in destdir before
	trying to move it to ovile
oneliner.c: don't allow pregion to be used if the plines buffer is current,
	and set the WINDOW list mode, not the BUFFER list mode, so :l works
	again, and fix for s/.//g bug
regexp.c: fixes for interaction of BOL and \s etc.
spawn.c: lint cleanup  mostly casting strcXX() to void.
tcap.c: hack to supply CS or SR for scrolling region control, missing
	from some xterms
termio.c: added missing fflush in two of the ttclean() routines
word.c: fix for counts on 'J' command

------------------------------------------

The rest of the changes lister here are grouped only roughly, and represent
the delta of version 3.28 from vile version three.  -pgf Dec 30 1992

------------------------------------------------------

VI COMPLIANCE:
===============

- made ^N, ^P, and ^T do exactly what they do in vi.  (next line,
	previous-line, and tag-pop) Use ^X-o to switch windows, and ^X-2 to
	split them.

- ANSI style arrow keys now work from either command or insert mode.  Any
	ANSI function key of the form ESC [ c can be used as a command
	key -- the binding is FN-c

- the '#' key may now be used to force the following character to be treated
	as a function key.  Thus #1 is now equivalent to FN-1.

- some modes/values have changed names: aindent is now autoindent, swrap is
	now wrapscan, exact is now noignorecase, and they all have the vi
	shorthands (e.g.  tabstop is the same as ts, ic is the same as
	ignorecase).

- the "paragraphs" and "sections" values are now implemented, but as
	regular expressions -- they look pretty ugly, but I think they
	do what vi does.

- the sentence motions ')' and '(' now work.  they may not do exactly what
	vi does, but they're close.  they're controlled by regular
	expressions that find the ends of sentences -- then they move
	forward to the start of the next one.  The regular expressions are
	set in the new "sentences" value setting.

- "showmatch" mode now works, and momentarily shows the previous matching '('
	or '{'.

- "magic" mode, or rather "nomagic" mode, now works more like vi:  special
	characters are still available by escaping them in nomagic mode.

- there is a new mode, settable as "set terse" and "set noterse", which will
	suppress some of the extra messages that vile prints, reducing
	traffic for slow terminals.

- the '%' command now scans forward for a fence character if you're not on
	one to begin with (thanks to Dave Lemke).  I also added '^X-%', which
	scans backward instead.

- the ":&" command now works, and repeats the last ":s" or '^X-s'
	substitution.  it re-uses trailing p and l options, which vi does
	not.

- the '&' now works, and repeats the last ":s" or '^X-s' substitution.

- replacement sub-expressions work (\( and \), \1, etc.), as do \U \L \u \l

- there is a new operator, '^A-&', which is similar to '&' but acts on a
	region.  I expect that if you want it, you'll probably rebind it to
	something else (like '&') since typing it is awkward, to say the least.

- ^W and ^U are now both read from the tty settings, and they work in both
	insert mode and on the : line.  There is a "backspacelimit" setting
	that controls whether these (and regular backspacing (^H or DEL)
	will backspace past the insert point in insert mode.

- The '@' command can now be used to execute the contents of a named register,
	as if it were entered at the keyboard.  Thus @k executes register k.

- The "keyboard macro" obtained with ^X-( and ^X-) can be saved into a named
	register with the ^X-^ command.  This lets you record a keyboard
	macro and save it for use with the new '@' command.

- And, along to further complement the '@' command, it is possible to pre-load
	the named registers with the "load-register" command.  In a .vilerc
	you can say:
		use-register a load-register ihello^[
	(where ^[ is really an ESC entered with ^V-ESC), and then from
	within vile you can execute '@a', and the word "hello" will be
	inserted.

- the H, M, and L commands are now absolute motions

- insert mode now takes a repeat count, so '80i=ESC' inserts 80 '=' chars

- two new functions, to make normal motions more vi compliant --
	forward-character-to-eol and backward-character-to-bol, which won't
	move past the boundaries of the current line.

- you can no longer backspace from the beginning of a line to the end of
	the previous.  (rebind 'h' and '^h' from "backward-character-to-bol"
	to "backward-character"

- a motion caused by an undo is considered absolute, and the "last dot"
	mark is reset.

- added 'vi +/searchstring file.c' invocation syntax

- you can now specify a count to the ~ command.

- backslashes now have a protective effect on the : line -- they guard
	against expansion of #, %, :, and \.

- a lone ":f" without filename now gives same info as ^G

- 'x', 's', 'r' will no longer delete a line if used on an empty line.

- after a ":e!" command, the buffer name changes to match the filename, as
	if this were a new file read.  It used to just suck a new file into
	an old buffer, and the names didn't match.

- there is now a "taglength" setting, and the name of the tags file is in
	the "tags" setting.

- the "cd" and "pwd" commands now work

- shiftwidth has been implemented, so ^T, ^D, '<', and '>' all do pretty
	much what they do in vi

- there is a separate c-shiftwidth, used when in cmode, much as c-tabstop
	replaces tabstop when in cmode.

- ": args" is now similar in spirit to the vi command of that name


MISCELLANEOUS
===============

- reading files in general is _much_ faster, unless the file contains lines
	longer than 255 characters, in which case things slow down again.

- regular expressions are more powerful and much, much faster than they
	were.  vile now uses Henry Spencer's regexp code.  This has the
	side effect of making '\|' a new metacharacter, for alternation
	of expressions.  '\+' and '\?' are also new, and stand for "one
	or more" and "zero or one" occurrences.  they can be used from
	the I-search commands as well.

- in addition, \< and \> find beginnings and ends of words, and \w, \s,
	and \d find "word" characters, "space" characters, and digits.
	\W, \S, and \D find the converse

- related to the above change, replacement metacharacters now work, so you
	can use '&' and '\1' through '\9' in the replacement string, and
	you'll get the right thing.

- Dave Lemke contributed code to support vile under X windows -- including
	support for cutting/pasting, etc.  Thanks Dave!

- the '%' command will now find matching #if/#else/#endif sets.

- the '%' command will now find matching C comment sets.  Yes, it does
	the (non-)nesting correctly.

- tabstops can now be set to any value, and there is a separate value for
	tabstops in C mode.  (c-tabstop)

- there is now some support for horizontal scrolling, using ^X-^R and ^X-^L

- the ! filter command for running text through an external filter now
	works

- quite a bit of code cleanup, at least I think so -- it has ported pretty
	easily to a R6000, linux, 386bsd, Sony NeWS, NeXT, and the UNIXPC.
	it's close to lint-free, and Saber has very few problems with the
	code.  it'll now use prototypes if __STDC__ is defined, and it
	passes gcc -Wall -Wshadow with no warnings, if you ignore implicit
	declarations of system calls and lib routines.  the code should now
	be more portable, to various compilers and systems.  there are new
	targets in the makefile to make building on various platforms
	easier.  the makefile even supports microsoft C.

- the code now builds and runs under DOS, using (at least) Microsoft C 6.0.
	Peter Ruczynski has done a great job porting vile to DOS -- he
	calls that version "pcvile".  He has used the Zortech compiler in
	the past -- I didn't try to break that, but you never know.
	NOTE THAT THE DOS CODE IS KNOWN TO BE MUCH BUGGIER THAN THE UNIX CODE!

- quite a few bug fixes, including all (I think) of those reported by users

- the "last dot" mark, accessed with the names ' and ` doesn't get reset
	needlessly as often -- that is, if an absolute motion results in no
	move (e.g.  a failed search or tags operation, or an undo) then the
	"last dot" mark is unchanged.

- the "source" ("execfile") command now does globbing on the file being run
	-- be sure to put it in quotes, as in :source "$HOME/.vilestuff"

- entab, detab, and line-trim are now all operators (^A-tab, ^A-space, and
	^A-t)

- "make install" installs to /usr/local/bin or $HOME/bin, whichever is
	writable

- the english names of functions, particularly new ones, have been
	"rationalized", hopefully making it easier to remember their names.

- if you invoke a substitute command with ^X-s, it will always act globally
	across lines.  (i.e.  it behaves like "s/s1/s2/g", not like
	"s/s1/s2/")

- a QUIT signal will no longer cause core dump unless built with DEBUG on

- the display optimizes scrolling if possible when the window scroll
	commands ^E and ^Y are used.

- values/modes have been reworked.  Some are now attached to buffers, some
	to windows.  By default, all values use the global settings of
	those values, and will track changes to the global settings.  If a
	local setting is specified, it breaks the link between the local
	and global value, and the local value will no longer track the
	global value.  ":set all" is now more informative.

- the "set list" value is now attached to a window, not a buffer, so a
	single buffer can be displayed in two windows, "list"ed in one but
	not the other.

- reading from other processes into a buffer is now _much_ faster

- it is no longer legal to write out the contents of a buffer that has view
	mode set.

- the format region command (^A-f) now restarts with each fresh paragraph,
	so you can format an entire file at once, without collapsing it all
	into a single paragraph.  It also knows a little bit about C and
	shell comments, so you can now reformat blocks of commentary text.
	(There can't be any non-commentary stuff in front of the comments.)

- the position of the cursor within a window (the framing) is now preserved
	while the window if "off-screen" -- it no longer reframes when made
	visible again.

- there is now a mode indicator on the modeline: I for insert, O for
	overwrite, and R for replace-char.

- a line starting with '#' won't shift-right if the buffer is in C mode.

- :s/s1/s2/5 now works, to change the 5th occurrence on a line

- there are new commands to write all buffers: :ww is a synonym for
	"write-changed-buffers", and :wwq is now a synonym for
	"write-changed-buffers-and-quit"

- it is now legal to "kill-buffer" a displayed buffer

- there are new variables, $word, $pathname, and $identifier, which return
	the appropriate type of string from the cursor's location

- all of the variables, and the functions and directives of the extension
	language have been documented, and most have been tried, if not
	tested

- two new functions: &rd and &wr, return whether a file is readable or writable

- a new mode, "tabinsert", has been created to fill the needs of the
	"I hate tabs" camp.

- filenames are now put into canonical form -- this keeps "junk", "./junk",
	"./somedir/../junk" from appearing as different files to the editor.

- there is a new mode, "tagsrelative", which, when set, causes filenames
	looked up via tags to have the directory name of the tags file
	prepended to them before being accessed.  This allows one to cd into
	a build tree from within the editor, to facilitate a grep or a local
	build, and have tags lookups continue to work.

- the find-next-error code now honors the "Entering/Leaving directory"
	messages that GNU make (and others?) puts out.

- job control now works right

IMPLEMENTORS NOTES:
===================

- internally, most pointers into the buffer, consisting of a LINE * and an
	int offset are now represented with a MARK structure -- in the
	nineties, it's time to start using structure assignment.

- the set of values that are frequently handed about between buffers and
	windows, or windows and their "children" are now grouped into
	B_TRAITS and W_TRAITS structures, for easier bookkeeping.  There
	are lots of macros to hide the added data structure depth.

- if you add a new setting that should be inherited by and selectable
	on a per-buffer or per-window basis, add it to the W_VALUES or
	B_VALUES structs.  Otherwise, if its just a global value, like the
	X font, make it a variable.  I intend to merge the setting of
	variables in with the other values one of these days.

- there is now a generic printf facility built in, which is layered under
	mlwrite() for the message line, bprintf() to print into a buffer,
	and lsprintf() to print into a string

- LINE structures can now have NULL l_text pointers -- empty lines don't
	have memory associated with them (except for the LINE struct
	itself)

- line text is now "block malloc"ed -- now for most files there is are two
	mallocs and a single read, no copies.  previously there were two
	mallocs per line, and two copies (stdio's and ours).  This change
	implies that LINE structures and their text should not be moved
	between buffers -- the space they occupy may have come from the big
	chunks attached to the buffer structure, and not from the malloc
	pool directly.  So lfree() and lalloc() now take a buffer pointer
	argument.

- there is "line poisoning" code, in line.c, which trashes the contents of
	freed lines and LINE structs, to make it easier to find bad usage.

- do_fl_region() is a convenient wrapper for writing operator commands that
	work on full lines -- there isn't yet a similar wrapper for partial
	line operators.

- use mlwrite() for informative messages that terse mode will suppress,
	mlforce() for messages that _must_ be seen, and mlprompt() for
	messages that need a response from the user unless they're in a
	command file.

