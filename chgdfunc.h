/*
 * Prototypes for functions in the mode-tables (must be declared before the
 * point at which proto.h is included).
 *
 * $Header: /users/source/archives/vile.vcs/RCS/chgdfunc.h,v 1.5 1998/10/24 15:05:11 cmorgan Exp $
 */
extern int chgd_autobuf  (CHGD_ARGS);
extern int chgd_buffer   (CHGD_ARGS);
extern int chgd_charset  (CHGD_ARGS);
#if OPT_COLOR
extern int chgd_color    (CHGD_ARGS);
#endif
extern int chgd_disabled (CHGD_ARGS);
extern int chgd_fences   (CHGD_ARGS);
extern int chgd_major    (CHGD_ARGS);
extern int chgd_major_w  (CHGD_ARGS);
extern int chgd_status   (CHGD_ARGS);
extern int chgd_window   (CHGD_ARGS);
extern int chgd_working  (CHGD_ARGS);
extern int chgd_xterm    (CHGD_ARGS);
extern int chgd_hilite   (CHGD_ARGS);
#if SYS_WINNT && defined(VILE_OLE)
extern int chgd_redir_keys(CHGD_ARGS);
#endif

