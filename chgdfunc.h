/*
 * Prototypes for functions in the mode-tables (must be declared before the
 * point at which proto.h is included).
 *
 * $Header: /users/source/archives/vile.vcs/RCS/chgdfunc.h,v 1.16 2001/06/13 18:55:26 tom Exp $
 */
extern int chgd_autobuf  (CHGD_ARGS);
extern int chgd_buffer   (CHGD_ARGS);
extern int chgd_charset  (CHGD_ARGS);
extern int chgd_disabled (CHGD_ARGS);
extern int chgd_fences   (CHGD_ARGS);
extern int chgd_hilite   (CHGD_ARGS);
extern int chgd_major    (CHGD_ARGS);
extern int chgd_major_w  (CHGD_ARGS);
extern int chgd_mouse    (CHGD_ARGS);
extern int chgd_rs       (CHGD_ARGS);
extern int chgd_status   (CHGD_ARGS);
extern int chgd_undoable (CHGD_ARGS);
extern int chgd_uniqbuf  (CHGD_ARGS);
extern int chgd_window   (CHGD_ARGS);
extern int chgd_working  (CHGD_ARGS);
extern int chgd_xterm    (CHGD_ARGS);

#if OPT_COLOR
extern int chgd_color    (CHGD_ARGS);
#endif

#if OPT_COLOR_SCHEMES
extern int chgd_scheme   (CHGD_ARGS);
#endif

#if OPT_MAJORMODE
extern int chgd_mm_order (CHGD_ARGS);
extern int chgd_filter   (CHGD_ARGS);
#endif

#if OPT_FINDPATH
extern int chgd_find_cfg(CHGD_ARGS);
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
#endif
