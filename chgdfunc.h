/*
 * Prototypes for functions in the mode-tables (must be declared before the
 * point at which proto.h is included).
 *
 * $Header: /users/source/archives/vile.vcs/RCS/chgdfunc.h,v 1.10 2000/02/09 11:34:27 pgf Exp $
 */
extern int chgd_autobuf  (CHGD_ARGS);
extern int chgd_buffer   (CHGD_ARGS);
extern int chgd_charset  (CHGD_ARGS);
extern int chgd_disabled (CHGD_ARGS);
extern int chgd_fences   (CHGD_ARGS);
extern int chgd_hilite   (CHGD_ARGS);
extern int chgd_major    (CHGD_ARGS);
extern int chgd_major_w  (CHGD_ARGS);
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
#endif

#if SYS_WINNT && defined(VILE_OLE)
extern int chgd_redir_keys(CHGD_ARGS);
#endif
