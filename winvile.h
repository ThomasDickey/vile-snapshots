/* definitions for winvile.rc */

#define IDM_OPEN                 101
#define IDM_FONT                 102
#define IDM_MENU                 103
#define IDM_SAVE_AS              104
#define IDM_COPY                 105
#define IDM_CUT                  106
#define IDM_DELETE               107
#define IDM_PASTE                108
#define IDM_ABOUT                109
#define IDM_UNDO                 110
#define IDM_REDO                 111
#define IDM_FAVORITES            112
#define IDM_SELECT_ALL           113
#define IDM_PRINT                114
#define IDM_PAGE_SETUP           115
#define IDM_SEP_AFTER_RCNT_FLDRS 116
#define IDM_CHDIR                117

/* range between 118 and 200 is available for future menu commands */

/*
 * NB: range between IDM_RECENT_FILES and (IDM_NEXT_MNU_ITEM_ID - 1)
 * is "in use".
 */
#define IDM_RECENT_FILES         200
#define IDM_RECENT_FLDRS         (IDM_RECENT_FILES + MAX_RECENT_FILES)

/* if yet more menu item IDs are required, start here... */
#define IDM_NEXT_MNU_ITEM_ID     (IDM_RECENT_FLDRS + MAX_RECENT_FLDRS)


/* About dialog box defns. */

#define IDM_ABOUT_COPYRIGHT 1001
#define IDM_ABOUT_PROGNAME  1002

/* custom ntwinio WINDOWS messages */

#define WM_WVILE_CURSOR_ON               (WM_USER + 1)
#define WM_WVILE_CURSOR_OFF              (WM_USER + 2)
#define WM_WVILE_FLASH_START             (WM_USER + 3)
#define WM_WVILE_FLASH_STOP              (WM_USER + 4)
