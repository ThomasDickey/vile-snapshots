/************************************************************************/
/*  File        : menu.c                                                */
/*  Description : Implement a Motif MenuBar into Mxvile                 */
/*                Parse a File (environment variable XVILE_MENU)        */
/*                and create it at the top of the window                */
/*  Auteur      : Philippe CHASSANY                                     */
/*  Date        : 20/02/97                                              */
/************************************************************************/

/*
 * $Header: /users/source/archives/vile.vcs/RCS/menu.c,v 1.46 2005/05/20 23:01:32 tom Exp $
 */

/* Vile includes */
#include "estruct.h"
#include "edef.h"

#if SYS_VMS
#include <processes.h>
#define fork vfork
#define Token MenuTokens
#endif

/* Tokens of the rc file */
#define MAX_TOKEN   200
static struct _tok_ {
    char type;
    int idx_cascade;
    char label[NSTRING];
    char action[NSTRING];
    int macro;
} Token[MAX_TOKEN];
static int Nb_Token = 0;

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
    {"nop", 0},
    {"new_xvile", common_action},
    {"edit_rc", common_action},
    {"parse_rc", common_action},
    {"edit_mrc", common_action},
};

static char *
menu_filename(void)
{
    char *result = cfg_locate(menu_file, LOCATE_SOURCE);
#if SYS_UNIX
    /* just for backward compatibility */
    if (result == 0 && strcmp(menu_file, "vilemenu.rc")) {
	result = cfg_locate("vilemenu.rc", FL_STARTPATH | FL_READABLE);
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
    if (fname != 0 && *fname != EOS && ffexists(fname)) {
	splitwind(FALSE, 1);
	getfile(fname, TRUE);
    } else
	no_file_found();
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
		_exit(0);	/* abandon exec'ing process */
	    } else if (pid == 0) {
		newprocessgroup(FALSE, 1);
		execl(path, "xvile", "-display", x_get_display_name(), NULL, 0);
		_exit(errno);	/* just in case exec-failed */
	    }
	}
    } else if (!strcmp(action, "edit_rc")) {
	edit_file(startup_filename());
    } else if (!strcmp(action, "parse_rc")) {
	dofile(startup_filename());
    } else if (!strcmp(action, "edit_mrc")) {
	edit_file(menu_filename());
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

    return 0;
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

    return 0;
}

/************************************************************************/
/* Main function to parse the rc file                                   */
/************************************************************************/
int
parse_menu(const char *rc_filename)
{
    FILE *fp;
    char line[1000];
    char *ptr_tok;
    int n, tmp;
    int cascade_token = 0;
    int nlig = 0;

    TRACE(("parse_menu(%s)\n", rc_filename));

    if ((fp = fopen(rc_filename, "r")) == NULL)
	return FALSE;

    Nb_Token = 0;
    while (fgets(line, sizeof(line), fp) != NULL && Nb_Token < MAX_TOKEN - 2) {
	nlig++;

	/* Let a tab begin inline comment */
	if ((ptr_tok = strchr(line, '\t')) != 0)
	    *ptr_tok = EOS;

	ptr_tok = strtok(line, ":");
	if (ptr_tok == NULL)
	    continue;

	switch (*ptr_tok) {
	case ';':		/* FALLTHRU */
	case '"':
	case '#':
	    continue;

	case 'C':
	    Token[Nb_Token].type = *ptr_tok;
	    if ((ptr_tok = strtok(NULL, ":\n")) != NULL) {
		strcpy(Token[Nb_Token].label, ptr_tok);
		cascade_token = Nb_Token;
		if ((ptr_tok = strtok(NULL, ":\n")) != NULL) {
		    Token[Nb_Token].type = 'H';
		}
		Nb_Token++;
	    } else {
		fclose(fp);
		return FALSE;
	    }
	    break;

	case 'S':
	    Token[Nb_Token].type = *ptr_tok;
	    Token[Nb_Token].idx_cascade = cascade_token;
	    Nb_Token++;
	    break;

	case 'L':
	    Token[Nb_Token].type = 'S';
	    Token[Nb_Token].idx_cascade = cascade_token;
	    Nb_Token++;
	    Token[Nb_Token].type = *ptr_tok;
	    Token[Nb_Token].idx_cascade = cascade_token;
	    if ((ptr_tok = strtok(NULL, ":\n")) != NULL) {
		strcpy(Token[Nb_Token].action, ptr_tok);
		cascade_token = Nb_Token;
	    }
	    Nb_Token++;
	    break;

	case 'B':
	    Token[Nb_Token].type = *ptr_tok;
	    Token[Nb_Token].idx_cascade = cascade_token;
	    n = 0;
	    while ((ptr_tok = strtok(NULL, ":\n")) != NULL) {
		switch (n) {
		case 0:
		    strcpy(Token[Nb_Token].label, ptr_tok);
		    break;
		case 1:
		    if (isDigit((int) *ptr_tok)) {
			tmp = (int) atoi(ptr_tok);
			Token[Nb_Token].macro = tmp;
		    } else {
			if (is_action(ptr_tok)
			    || vlmenu_is_bind(ptr_tok)
			    || vlmenu_is_cmd(ptr_tok)) {
			    strcpy(Token[Nb_Token].action, ptr_tok);
			} else {
			    printf("Error in line %d : '%s' is not an action\n",
				   nlig, ptr_tok);
			    strcpy(Token[Nb_Token].action, "beep");
			}
		    }
		    break;
		}
		n++;
	    }
	    if (n != 2) {
		fclose(fp);
		return FALSE;
	    }
	    Nb_Token++;
	    break;
	}
    }
    fclose(fp);

    return TRUE;
}

/************************************************************************/
/* Debug function to print the  tokens of the rc file                   */
/************************************************************************/
#if OPT_TRACE
static void
print_token(void)
{
    int i;

    for (i = 0; i < Nb_Token; i++) {
	switch (Token[i].type) {
	case 'C':
	case 'H':
	    Trace("%c %s\n",
		  Token[i].type,
		  Token[i].label);
	    break;
	case 'S':
	    Trace("\t%c (%s)\n",
		  Token[i].type,
		  Token[Token[i].idx_cascade].label);
	    break;
	case 'L':
	    Trace("\t%c %s\n",
		  Token[i].type,
		  Token[i].action);
	    break;
	case 'B':
	    if (Token[i].macro > 0)
		Trace("\t%c %s(%s) -> Macro-%d\n",
		      Token[i].type,
		      Token[i].label,
		      Token[Token[i].idx_cascade].label,
		      Token[i].macro);
	    else
		Trace("\t%c %s(%s) -> Action-%s\n",
		      Token[i].type,
		      Token[i].label,
		      Token[Token[i].idx_cascade].label,
		      Token[i].action);
	}
    }
}
#endif

/************************************************************************/
/* Main function : Take the menu-bar as argument and create all the     */
/* Cascades, PullDowns Menus and Buttons read from the rc file          */
/************************************************************************/
int
do_menu(void *menub)
{
    int i;
    char *accel, *macro;
    void *pm = 0;
    void *pm_w;
    int rc;
    char *menurc = menu_filename();

    if (menurc == 0) {
	mlforce("No menu-file found");
	return FALSE;
    }
    if ((rc = parse_menu(menurc)) != TRUE) {
	mlforce("Error parsing menu-file");
	return FALSE;
    }
#if OPT_TRACE
    print_token();
#endif

    for (i = 0; i < Nb_Token; i++) {
	switch (Token[i].type) {
	    /* HELP CASCADE */
	case 'C':
	case 'H':
	    pm = gui_make_menu(menub, Token[i].label, Token[i].type);
	    break;

	    /* SEPARATOR WIDGET */
	case 'S':
	    gui_add_menu_item(pm, "sep", NULL, Token[i].type);
	    break;

	    /* LIST (BUFFER LIST) */
	case 'L':
	    if (!strcmp(Token[i].action, "list_buff")) {
		gui_add_list_callback(pm);
	    }
	    break;

	    /* BUTTON WIDGET */
	case 'B':
	    if (Token[i].macro > 0) {
		if ((macro = castalloc(char, 50)) != 0) {
		    sprintf(macro, "execute-macro-%d", Token[i].macro);
		    accel = give_accelerator(macro);
		    pm_w = gui_add_menu_item(pm, Token[i].label, accel,
					     Token[i].type);

		    gui_add_func_callback(pm_w, macro);
		}
	    } else {
		accel = give_accelerator(Token[i].action);
		pm_w = gui_add_menu_item(pm, Token[i].label, accel,
					 Token[i].type);
		gui_add_func_callback(pm_w, Token[i].action);
	    }
	    break;
	}
    }
    return TRUE;
}

/************************************************************************/

int
vlmenu_load(int f, int n)
{
    if (gui_create_menus()) {
	return gui_show_menus(f, n);
    }
    return FALSE;
}
