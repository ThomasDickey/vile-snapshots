/*
 * Prototypes for functions in the mode-tables (must be declared before the
 * point at which proto.h is included).
 *
 * $Id: chgdfunc.h,v 1.25 2025/01/26 11:43:46 tom Exp $
 */
extern int chgd_autobuf  (CHGD_ARGS);
extern int chgd_buffer   (CHGD_ARGS);
extern int chgd_charset  (CHGD_ARGS);
extern int chgd_disabled (CHGD_ARGS);
extern int chgd_dos_mode (CHGD_ARGS);
extern int chgd_fences   (CHGD_ARGS);
extern int chgd_hilite   (CHGD_ARGS);
extern int chgd_major    (CHGD_ARGS);
extern int chgd_mouse    (CHGD_ARGS);
extern int chgd_percent  (CHGD_ARGS);
extern int chgd_rs       (CHGD_ARGS);
extern int chgd_status   (CHGD_ARGS);
extern int chgd_undoable (CHGD_ARGS);
extern int chgd_uniqbuf  (CHGD_ARGS);
extern int chgd_win_mode (CHGD_ARGS);
extern int chgd_window   (CHGD_ARGS);
extern int chgd_working  (CHGD_ARGS);
extern int chgd_xterm    (CHGD_ARGS);
extern int chgd_xtermkeys(CHGD_ARGS);

#if OPT_COLOR
extern int chgd_color    (CHGD_ARGS);
#endif

#if OPT_COLOR_SCHEMES
extern int chgd_scheme   (CHGD_ARGS);
#endif

#if OPT_CURTOKENS
extern int chgd_curtokens(CHGD_ARGS);
#endif

#if OPT_FINDPATH
extern int chgd_find_cfg(CHGD_ARGS);
#endif

#if OPT_MAJORMODE
extern int chgd_mm_order (CHGD_ARGS);
extern int chgd_filter   (CHGD_ARGS);
#endif

#if OPT_MULTIBYTE
extern int chgd_byteorder(CHGD_ARGS);
extern int chgd_fileencode(CHGD_ARGS);
#endif

#if OPT_TITLE
extern int chgd_swaptitle(CHGD_ARGS);
#endif

#if SYS_WINNT && defined(VILE_OLE)
extern int chgd_redir_keys(CHGD_ARGS);
#endif

#if SYS_WINNT
extern int chgd_icursor(CHGD_ARGS);
extern int chgd_popupmenu(CHGD_ARGS);
extern int chgd_rcntfiles(CHGD_ARGS);
extern int chgd_rcntfldrs(CHGD_ARGS);
#endif
