/************************************************************************/
/*  File        : menu.c                                                */
/*  Description : Implement a Motif MenuBar into Mxvile                 */
/*                Parse a File (environment variable XVILE_MENU)        */
/*                and create it at the top of the window                */
/*  Auteur      : Philippe CHASSANY                                     */
/*  Date        : 20/02/97                                              */
/************************************************************************/

/*
 * $Id: menu.c,v 1.82 2025/01/26 16:40:42 tom Exp $
 */

/* Vile includes */
#include "estruct.h"
#include "edef.h"

#if SYS_VMS
#include <processes.h>
#define fork vfork
#endif

/* Tokens of the rc file */
typedef struct _tok_ {
    struct _tok_ *next;
    char type;
    char label[NSTRING];
    char action[NSTRING];
    int macro;
} MenuToken;

/*
 * state, from successive define-menu-entry calls.
 */
static int count_entries = 0;
static void *my_menu_bar = NULL;
static void *parent_menu = NULL;
static MenuToken *all_tokens = NULL;

/* Actions of the rc file */

typedef struct {
    char name[30];
    const ActionFunc fact;
} TAct;

/* Common Actions = User defined Actions */
/* Why not use dynamic link library to get functions ? */
static void common_action(char *action);
static TAct Actions[] =
{
    {"nop", NULL},
    {"new_xvile", common_action},
    {"edit_rc", common_action},
    {"parse_rc", common_action},
    {"edit_mrc", common_action},
    {"parse_mrc", common_action},
};

/*
 * MenuTokens have to be allocated so their contents will be available later in
 * a callback.
 */
static MenuToken *
newToken(void)
{
    MenuToken *result = NULL;

    beginDisplay();
    result = typecalloc(MenuToken);
    if (result != NULL) {
	result->next = all_tokens;
	all_tokens = result;
    }
    endofDisplay();
    return result;
}

static void
free_all_tokens(void)
{
    beginDisplay();
    while (all_tokens != NULL) {
	MenuToken *next = all_tokens->next;
	free(all_tokens);
	all_tokens = next;
    }
    endofDisplay();
}

void
init_menus(void)
{
    free_all_tokens();

    count_entries = 0;
    parent_menu = NULL;
}

static char *
menu_filename(void)
{
    char *result = cfg_locate(menu_file, LOCATE_SOURCE);
#if SYS_UNIX
    /* just for backward compatibility */
    if (result == NULL && strcmp(menu_file, "vilemenu.rc")) {
	static char *param;
	if (param == NULL)
	    param = strmalloc("vilemenu.rc");
	result = cfg_locate(param, FL_STARTPATH | FL_READABLE);
    }
#endif
    return result;
}

static char *
startup_filename(void)
{
    return cfg_locate(startup_file, LOCATE_SOURCE);
}

static void
edit_file(char *fname)
{
    if (fname != NULL && *fname != EOS && ffexists(fname)) {
	splitwind(FALSE, 1);
	getfile(fname, TRUE);
    } else
	no_file_found();
}

/*
 * Reload/parse the menu-file.
 */
static void
parse_menus(char *fname)
{
    gui_remove_menus(FALSE, 1);
    init_menus();
    if (dofile(fname)) {
	gui_show_menus(FALSE, 1);
    }
}

/************************************************************************/
/* All the common actions are handled by this function                  */
/************************************************************************/
static void
common_action(char *action)
{
    TRACE(("Action=%s\n", action));

    if (!strcmp(action, "new_xvile")) {
	int pid;
	int status;
	char path[NFILEN];

	pathcat(path, exec_pathname, prog_arg);

	if ((pid = fork()) > 0) {
	    while (wait(&status) >= 0)
		/* */ ;
	} else if (pid == 0) {
	    if ((pid = fork()) > 0) {
		free_all_leaks();
		_exit(0);	/* abandon exec'ing process */
	    } else if (pid == 0) {
		newprocessgroup(FALSE, 1);
		execl(path,
		      "xvile",
		      "-display",
		      x_get_display_name(),
		      (void *) 0);
		_exit(errno);	/* just in case exec-failed */
	    }
	}
    } else if (!strcmp(action, "edit_rc")) {
	edit_file(startup_filename());
    } else if (!strcmp(action, "parse_rc")) {
	dofile(startup_filename());
    } else if (!strcmp(action, "edit_mrc")) {
	edit_file(menu_filename());
    } else if (!strcmp(action, "parse_mrc")) {
	parse_menus(menu_filename());
    }
}

/************************************************************************/
/* Test if the action name is part of the Actions of the rc file        */
/************************************************************************/
static int
is_action(char *action)
{
    unsigned i;

    for (i = 0; i < TABLESIZE(Actions); i++) {
	if (!strcmp(Actions[i].name, action))
	    return 1;
    }

    return 0;
}

/************************************************************************/
/* Return the function pointer associated with the action name          */
/************************************************************************/
ActionFunc
vlmenu_action_func(char *action)
{
    unsigned i;

    for (i = 0; i < TABLESIZE(Actions); i++) {
	if (!strcmp(Actions[i].name, action))
	    return Actions[i].fact;
    }

    return NULL;
}

/************************************************************************/
/* Test if the action name is a bind (inspired from vile sources        */
/************************************************************************/
int
vlmenu_is_bind(char *action)
{
    static TBUFF *tok;

    (void) get_token(action, &tok, eol_null, EOS, (int *) 0);
    if (tb_length(tok) != 0
	&& engl2fnc(tb_values(tok)) != NULL)
	return 1;

    return 0;
}

/************************************************************************/
/* Test if the action name begins with the keyword "cmd",               */
/************************************************************************/
char *
vlmenu_is_cmd(char *action)
{
    static TBUFF *tok;
    char *result = (char *) get_token(action, &tok, eol_null, EOS, (int *) 0);

    if (tb_length(tok) != 0
	&& !strcmp(tb_values(tok), "cmd"))
	return result;

    return NULL;
}

/*
 * Parse a menu-entry string, filling in the token if an entry was found.
 * If an error occurs, return false.
 * If only a comment-line was found, return sort-of-true.
 */
static int
parse_menu_entry(MenuToken * token, const char *source, size_t slen)
{
    char *ptr_tok;
    int n, tmp;
    int result = TRUE;
    char buffer[sizeof(MenuToken) + NLINE];

    /* make a copy of the source, since strtok modifies it */
    if (slen > sizeof(buffer) - 1)
	slen = sizeof(buffer) - 1;
    memcpy(buffer, source, slen);
    buffer[slen] = EOS;

    /* Let a tab begin inline comment */
    if ((ptr_tok = strchr(buffer, '\t')) != NULL)
	*ptr_tok = EOS;

    ptr_tok = strtok(buffer, ":");
    memset(token, 0, sizeof(*token));
    if (ptr_tok != NULL) {
	switch (*ptr_tok) {
	case ';':		/* FALLTHRU */
	case '~':
	case '"':
	case '#':
	    result = SORTOFTRUE;
	    break;

	case 'C':
	    token->type = *ptr_tok;
	    if ((ptr_tok = strtok(NULL, ":\n")) != NULL) {
		vl_strncpy(token->label, ptr_tok, sizeof(token->label));
		if (strtok(NULL, ":\n") != NULL) {
		    token->type = 'H';
		}
	    } else {
		result = FALSE;
	    }
	    break;

	case 'S':
	    token->type = *ptr_tok;
	    break;

	case 'L':
	    token->type = *ptr_tok;
	    if ((ptr_tok = strtok(NULL, ":\n")) != NULL) {
		vl_strncpy(token->action, ptr_tok, sizeof(token->action));
	    }
	    break;

	case 'B':
	    token->type = *ptr_tok;
	    n = 0;
	    while ((ptr_tok = strtok(NULL, ":\n")) != NULL) {
		switch (n) {
		case 0:
		    vl_strncpy(token->label, ptr_tok, sizeof(token->label));
		    break;
		case 1:
		    if (isDigit((int) *ptr_tok)) {
			tmp = (int) atoi(ptr_tok);
			token->macro = tmp;
		    } else {
			if (is_action(ptr_tok)
			    || vlmenu_is_bind(ptr_tok)
			    || vlmenu_is_cmd(ptr_tok)) {
			    vl_strncpy(token->action, ptr_tok, sizeof(token->action));
			} else {
			    mlwarn("'%s' is not an action", ptr_tok);
			    vl_strncpy(token->action, "beep", sizeof(token->action));
			    result = FALSE;
			}
		    }
		    break;
		}
		n++;
	    }
	    if (n != 2) {
		result = FALSE;
	    }
	    break;
	}
    }
    return result;
}

/************************************************************************/
/* Debug function to print the  tokens of the rc file                   */
/************************************************************************/
#if OPT_TRACE
static void
print_token(MenuToken * token)
{
    switch (token->type) {
    case 'C':
    case 'H':
	Trace("%c %s\n",
	      token->type,
	      token->label);
	break;
    case 'S':
	Trace("\t%c\n",
	      token->type);
	break;
    case 'L':
	Trace("\t%c %s\n",
	      token->type,
	      token->action);
	break;
    case 'B':
	if (token->macro > 0)
	    Trace("\t%c %s -> Macro-%d\n",
		  token->type,
		  token->label,
		  token->macro);
	else
	    Trace("\t%c %s -> Action-%s\n",
		  token->type,
		  token->label,
		  token->action);
	break;
    }
}
#endif

static void *
make_header(void *menub, int count)
{
    char label[80];
    sprintf(label, "Menu%d", count);
    return gui_make_menu(menub, label, 'C');
}

static void
define_menu_entry(MenuToken * token)
{
    char *accel, *macro_text;
    void *pm_w;

#if OPT_TRACE
    print_token(token);
#endif
    switch (token->type) {
	/* HELP CASCADE */
    case 'C':
    case 'H':
	parent_menu = gui_make_menu(my_menu_bar, token->label, token->type);
	break;

	/* SEPARATOR WIDGET */
    case 'S':
	if (parent_menu == NULL)
	    parent_menu = make_header(my_menu_bar, ++count_entries);
	gui_add_menu_item(parent_menu, "sep", NULL, token->type);
	break;

	/* LIST (BUFFER LIST) */
    case 'L':
	if (!strcmp(token->action, "list_buff")) {
	    if (parent_menu == NULL)
		parent_menu = make_header(my_menu_bar, ++count_entries);
	    gui_add_list_callback(parent_menu);
	}
	break;

	/* BUTTON WIDGET */
    case 'B':
	if (parent_menu == NULL)
	    parent_menu = make_header(my_menu_bar, ++count_entries);
	if (token->macro > 0) {
	    if ((macro_text = castalloc(char, 50)) != NULL) {
		sprintf(macro_text, "execute-macro-%d", token->macro);
		accel = give_accelerator(macro_text);
		pm_w = gui_add_menu_item(parent_menu, token->label, accel, token->type);

		gui_add_func_callback(pm_w, macro_text);
	    }
	} else {
	    accel = give_accelerator(token->action);
	    pm_w = gui_add_menu_item(parent_menu, token->label, accel, token->type);
	    gui_add_func_callback(pm_w, token->action);
	}
	break;
    }
}

static void
add_menu_separator(void)
{
    MenuToken *myToken = newToken();
    if (myToken != NULL) {
	myToken->type = 'S';
	define_menu_entry(myToken);
    }
}

static int
add_menu_entry(const char *text, size_t slen)
{
    int result;
    MenuToken myToken;
    MenuToken *hisToken;
    MenuToken *links;

    result = parse_menu_entry(&myToken, text, slen);
    if (result == FALSE) {
	result = FALSE;
    } else if (result == TRUE) {
	if (myToken.type == 'L') {
	    add_menu_separator();
	}
	hisToken = newToken();
	links = hisToken->next;
	*hisToken = myToken;
	hisToken->next = links;
	define_menu_entry(hisToken);
    }
    return result;
}

/* FIXME - merge this with chunk from gettagsfile() */
static BUFFER *
re_read_in(char *fname, BUFFER *bp, int *did_read)
{
#ifdef MDCHK_MODTIME
    time_t current = 0;
#endif

    *did_read = FALSE;
#ifdef MDCHK_MODTIME
    if (strcmp(fname, NonNull(bp->b_fname))) {
	bp->b_modtime = 0;
    }
    if ((global_b_val(MDCHK_MODTIME)
	 && get_modtime(bp, &current)
	 && bp->b_modtime != current)) {
	if (readin(fname, FALSE, bp, FALSE) != TRUE) {
	    zotbuf(bp);
	    bp = NULL;
	} else {
	    set_modtime(bp, bp->b_fname);
	    *did_read = TRUE;
	}
    } else if (!bp->b_active || strcmp(fname, NonNull(bp->b_fname)))
#endif
    {
	if (readin(fname, FALSE, bp, FALSE) != TRUE) {
	    zotbuf(bp);
	    bp = NULL;
	} else {
	    set_modtime(bp, bp->b_fname);
	    *did_read = TRUE;
	}
    }
    TRACE(("re_read_in(%s) %s\n", fname, *did_read ? "read" : "skipped"));
    return bp;
}

/*
 * Get a pointer to the menu-buffer.  If the external file is newer, read that
 * into memory.
 */
static BUFFER *
get_menu_buffer(void)
{
    int did_read;
    char *menurc;
    BUFFER *bp = NULL;

    if ((menurc = menu_filename()) == NULL) {
	mlforce("No menu-file found");
    } else if ((bp = bfind(VILEMENU_BufName, BFEXEC | BFINVS)) != NULL) {
	if ((bp = re_read_in(menurc, bp, &did_read)) != NULL) {
	    b_set_invisible(bp);
	    set_vilemode(bp);
	}
    }

    TRACE(("get_menu_buffer(%s) %s\n",
	   menurc,
	   (bp != 0) ? "success" : "fail"));

    return bp;
}

static int
has_expression(LINE *lp)
{
    int rc = FALSE;
    int len = llength(lp);
    int n;

    if (len > 0) {
	for (n = 0; n < len; ++n) {
	    int ch = lgetc(lp, n);
	    if (!isSpace(ch)) {
		rc = (ch == '~');
		break;
	    }
	}
    }
    return rc;
}

/************************************************************************/
/* Main function : Take the menu-bar as argument and create all the     */
/* Cascades, PullDowns Menus and Buttons read from the rc file          */
/************************************************************************/
int
do_menu(void *menub)
{
    static int first = TRUE;

    BUFFER *bp;
    LINE *lp;
    int rc = TRUE;

    TRACE((T_CALLED "do_menu\n"));
    my_menu_bar = menub;

    init_menus();

    /*
     * As called from xvile's initialization, this would be too soon to do
     * expression-evaluation.  Check if there are any expressions - if so, we
     * will delay loading the menus.  Otherwise (the old scheme), do it right
     * away.
     */
    if ((bp = get_menu_buffer()) != NULL) {
	delay_menus = FALSE;
	for_each_line(lp, bp) {
	    if (has_expression(lp)) {
		static const char *fallback[] =
		{
		    "C:Help:help",
		    "B:About:version"
		};
		unsigned n;

		/*
		 * Provide a fallback to simplify making the menubar the
		 * right height during initialization.
		 */
		for (n = 0; n < TABLESIZE(fallback); ++n) {
		    if ((rc = add_menu_entry(fallback[n],
					     strlen(fallback[n]))) == FALSE) {
			break;
		    }
		}
		delay_menus = TRUE;
		break;
	    }
	}

	if (delay_menus && !first) {
	    gui_remove_menus(FALSE, 1);
	    dobuf(bp, 1, -1);
	} else if (!delay_menus) {
	    for_each_line(lp, bp) {
		rc = add_menu_entry(lvalue(lp), (size_t) llength(lp));
		if (rc == FALSE)
		    break;
	    }
	}
	first = FALSE;
    }
    returnCode(rc);
}

/************************************************************************/

/*
 * Define a menu-entry for the current menu-bar.
 */
int
vlmenu_entry(int f, int n)
{
    int result;
    char buffer[NLINE];

    if (clexec) {
	result = add_menu_entry(execstr, strlen(execstr));
    } else {
	*buffer = EOS;
	result = mlreply("Menu entry:", buffer, (UINT) sizeof(buffer));
	if (result == TRUE)
	    result = add_menu_entry(buffer, strlen(buffer));
	if (result == TRUE)
	    result = gui_show_menus(f, n);
    }
    return result;
}

int
vlmenu_load(int f, int n)
{
    int result = FALSE;

    gui_remove_menus(f, n);
    if (gui_create_menus()) {
	result = gui_show_menus(f, n);
    }
    return result;
}
