/************************************************************************/
/*  File        : menu.c                                                */
/*  Description : Implement a Motif MenuBar into Mxvile                 */
/*                Parse a File (environment variable XVILE_MENU)        */
/*                and create it at the top of the window                */
/*  Auteur      : Philippe CHASSANY                                     */
/*  Date        : 20/02/97                                              */
/************************************************************************/

/*
 * $Header: /users/source/archives/vile.vcs/RCS/menu.c,v 1.20 1998/05/30 12:23:16 tom Exp $
 */

#define NEED_X_INCLUDES 1

/* Vile includes */
#include "estruct.h"
#include "edef.h"

#if DISP_X11

#if MOTIF_WIDGETS
#include <Xm/MainW.h>
#include <Xm/CascadeB.h>
#include <Xm/SeparatoG.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#endif

#if ATHENA_WIDGETS
#include <X11/Xaw/Form.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/SmeBSB.h>
#endif

/* Locals */
#define MAX_ITEM_MENU_LIST  50
static int nb_item_menu_list = 0;
static Widget pm_buffer [MAX_ITEM_MENU_LIST];
static Widget cascade;

/* Tokens of the rc file */
#define MAX_TOKEN   200
struct  _tok_
{
    char    type;
    int     idx_cascade;
    char    libelle [NSTRING];
    char    action  [NSTRING];
    int     macro;
} Token [MAX_TOKEN];
int Nb_Token;

/* Actions of the rc file */
typedef void (*ActionFunc) ( char * );

typedef struct
{
    char                name [30];
    const   ActionFunc  fact;

} TAct;

/* Common Actions = User defined Actions */
/* Why not use dynamic link library to get functions ? */
static void common_action ( char *action );
static TAct Actions [] = {
    { "nop",            0 },
    { "new_xvile",      common_action },
    { "edit_rc",        common_action },
    { "parse_rc",       common_action },
    { "edit_mrc",       common_action },
};
int Nb_Actions = sizeof(Actions)/sizeof(TAct);


static char *
menu_filename(void)
{
        static char default_menu[] = ".vilemenu";
        char *menurc = getenv ("XVILE_MENU");

        if (menurc == NULL || *menurc == EOS)
                menurc = default_menu;

        return flook(menurc, FL_ANYWHERE|FL_READABLE);
}

static char *
startup_filename(void)
{
        return flook(startup_file, FL_ANYWHERE|FL_READABLE);
}

/************************************************************************/
/* All the Common Action are pointed to this function                   */
/************************************************************************/
static void common_action ( char *action )
{
    TRACE(("Action=%s\n", action));

    if (!strcmp(action, "new_xvile"))
    {
        int pid;
        int status;
        char path[NFILEN];

        lsprintf(path, "%s/%s", exec_pathname, prog_arg);

        if ((pid = fork()) > 0) {
            while (wait(&status) >= 0)
                ;
        } else if (pid == 0) {
            if ((pid = fork()) > 0) {
                _exit(0);       /* abandon exec'ing process */
            } else if (pid == 0) {
                newprocessgroup(FALSE, 1);
                execl (path, "xvile", NULL, 0);
                _exit(errno);   /* just in case exec-failed */
            }
        }
    }
    else
    if (!strcmp(action, "edit_rc"))
    {
        splitwind(FALSE, 1);
        getfile (startup_filename(), TRUE);
    }
    else
    if (!strcmp(action, "parse_rc"))
    {
        dofile (startup_filename());
    }
    else
    if (!strcmp(action, "edit_mrc"))
    {
        splitwind(FALSE, 1);

        getfile (menu_filename(), TRUE);
    }
}

/************************************************************************/
/* Test if the action name is part of the Actions of the rc file        */
/************************************************************************/
static int
is_action ( char *action )
{
    int i;

    for (i=0; i<Nb_Actions; i++)
    {
        if (!strcmp(Actions[i].name, action))
            return 1;
    }

    return 0;
}

/************************************************************************/
/* Return the function pointer associated with the action name          */
/************************************************************************/
static ActionFunc
get_action_fonc ( char *action )
{
    int i;

    for (i=0; i<Nb_Actions; i++)
    {
        if (!strcmp(Actions[i].name, action))
            return Actions[i].fact;
    }

    return 0;
}

/************************************************************************/
/* Test if the action name is a bind (inspired from vile sources        */
/************************************************************************/
static int
is_bind ( char *action )
{
    char tok[NSTRING];
    (void) token(action, tok, EOS);
    if (engl2fnc(tok) != NULL)
        return 1;

    return 0;
}

/************************************************************************/
/* Test if the action name begins with the keyword "cmd",               */
/************************************************************************/
static char *
is_cmd ( char *action )
{
    char tok[NSTRING];
    char *result = (char *)token(action, tok, EOS);
    if (!strcmp(tok, "cmd"))
        return result;

    return 0;
}

/************************************************************************/
/* Main function to parse the rc file                                   */
/************************************************************************/
int parse_menu ( const char *rc_filename )
{
    FILE    *fp;
    char    line [1000];
    char    *ptr_tok;
    int     n, tmp;
    int     cascade_token = 0;
    int     nlig = 0;

    if ((fp = fopen(rc_filename, "r")) == NULL)
        return FALSE;

    Nb_Token = 0;
    while (fgets (line, sizeof(line), fp) != NULL)
    {
        nlig++;

        /* Let a tab begin inline comment */
        if ((ptr_tok = strchr(line, '\t')) != 0)
                *ptr_tok = EOS;

        ptr_tok = strtok(line, ":");
        if (ptr_tok == NULL)
            continue;

        switch (*ptr_tok)
        {
        case ';': /* FALLTHRU */
        case '#':
                continue;

        case 'C':
                Token[Nb_Token].type = *ptr_tok;
                if ((ptr_tok = strtok(NULL, ":\n")) != NULL)
                {
                    strcpy(Token[Nb_Token].libelle, ptr_tok);
                    cascade_token = Nb_Token;
                    if ((ptr_tok = strtok(NULL, ":\n")) != NULL)
                    {
                        Token[Nb_Token].type = 'H';
                    }
                    Nb_Token++;
                }
                else
                {
                    fclose (fp);
                    return FAILED;
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
                if ((ptr_tok = strtok(NULL, ":\n")) != NULL)
                {
                    strcpy(Token[Nb_Token].action, ptr_tok);
                    cascade_token = Nb_Token;
                }
                Nb_Token++;
                break;

        case 'B':
                Token[Nb_Token].type = *ptr_tok;
                Token[Nb_Token].idx_cascade = cascade_token;
                n = 0;
                while ((ptr_tok = strtok(NULL, ":\n")) != NULL)
                {
                    switch (n)
                    {
                    case 0:
                        strcpy(Token[Nb_Token].libelle, ptr_tok);
                        break;
                    case 1:
                        if (isDigit((int)*ptr_tok))
                        {
                            tmp = (int)atoi(ptr_tok);
                            Token[Nb_Token].macro = tmp;
                        }
                        else
                        {
                            if (is_action(ptr_tok)
                             || is_bind(ptr_tok)
                             || is_cmd(ptr_tok))
                            {
                                strcpy(Token[Nb_Token].action, ptr_tok);
                            }
                            else
                            {
                                printf("Error in line %d : '%s' is not an action\n",
                                            nlig, ptr_tok);
                                strcpy(Token[Nb_Token].action, "beep");
                            }
                        }
                        break;
                    }
                    n++;
                }
                if (n != 2)
                {
                    fclose (fp);
                    return FAILED;
                }
                Nb_Token++;
                break;
        }
    }
    fclose (fp);

    return TRUE;
}

/************************************************************************/
/* Debug function to print the  tokens of the rc file                   */
/************************************************************************/
#if OPT_TRACE
static void
print_token (void)
{
    int     i;

    for (i=0; i<Nb_Token; i++)
    {
        switch (Token[i].type)
        {
        case 'C':
        case 'H':
            Trace("%c %s\n",
                        Token[i].type,
                        Token[i].libelle);
            break;
        case 'S':
            Trace("\t%c (%s)\n",
                        Token[i].type,
                        Token[Token[i].idx_cascade].libelle);
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
                        Token[i].libelle,
                        Token[Token[i].idx_cascade].libelle,
                        Token[i].macro);
            else
                Trace("\t%c %s(%s) -> Action-%s\n",
                        Token[i].type,
                        Token[i].libelle,
                        Token[Token[i].idx_cascade].libelle,
                        Token[i].action);
        }
    }
}
#endif

/************************************************************************/
/* Motif function to make a cascade button into a menubar               */
/************************************************************************/
static Widget do_cascade ( Widget menub, char *nom, int the_class )
{
#if MOTIF_WIDGETS
    Widget      pm;
    XmString    xms;
    char        str [50];

    sprintf(str, "%sMenu", nom);
    pm = (Widget) XmCreatePulldownMenu (menub, str, NULL, 0);

    xms = XmStringCreateSimple (nom);

    cascade = XtVaCreateManagedWidget ("menuHeader",
                xmCascadeButtonWidgetClass,     menub,
                XmNlabelString,         xms,
                XmNsubMenuId,           pm,
                NULL);
    XmStringFree (xms);

    if (the_class == 'H')
    {
        XtVaSetValues (menub,
            XmNmenuHelpWidget,  cascade,
            NULL);
    }
    return pm;
#endif /* MOTIF_WIDGETS */
#if ATHENA_WIDGETS
    static      Widget last;
    Widget      pm;
    String      str = XtMalloc(strlen(nom) + 10);

    sprintf(str, "%sMenu", nom);
    pm = XtVaCreatePopupShell (str,
        simpleMenuWidgetClass,
        menub,
        XtNgeometry,                    NULL,
        NULL);

    cascade = XtVaCreateManagedWidget ("menuHeader",
        menuButtonWidgetClass,
        menub,
        XtNheight,                      x_menu_height(),
        XtNlabel,                       nom,
        XtNfromHoriz,                   last,
        XtNmenuName,                    str,
        NULL);
    last = cascade;

    return pm;
#endif /* ATHENA_WIDGETS */
}

/************************************************************************/
/* Motif function to make a button into a cascade (with accelarator)    */
/************************************************************************/
static Widget do_button ( Widget pm, char *nom, char *accel, int the_class )
{
#if MOTIF_WIDGETS
    Widget      w;
    XmString    xms_accl;
    XmString    xms_name;
    WidgetClass wc;

    switch (the_class) {
        case 'S':
            wc = xmSeparatorGadgetClass;
            break;
        case 'B':
            wc = xmPushButtonGadgetClass;
            break;
    }

    if (accel != NULL)
        xms_accl = XmStringCreateSimple (accel);
    else
        xms_accl = XmStringCreateSimple ("");

    xms_name = XmStringCreateSimple (nom);
    w = XtVaCreateManagedWidget ("menuEntry",
                wc,
                pm,
                XmNacceleratorText,     xms_accl,
                XmNlabelString,         xms_name,
                NULL);

    XmStringFree (xms_accl);
    XmStringFree (xms_name);

    return w;
#endif /* MOTIF_WIDGETS */
#if ATHENA_WIDGETS
    Widget      w;

    w = XtVaCreateManagedWidget ("menuEntry",
                (the_class == 'B') ? smeBSBObjectClass : smeLineObjectClass,
                pm,
                XtNlabel,               nom,
                NULL);

    return w;
#endif /* ATHENA_WIDGETS */
}

/************************************************************************/
/* Function called when a button is clicked into the menu               */
/* Bind and Action are tested                                           */
/************************************************************************/
static void proc_back ( Widget w GCC_UNUSED, XtPointer arg, XtPointer call  GCC_UNUSED)
{
    char    *macro_action = (char *)arg;
    ActionFunc fact;
    int     oldflag = im_waiting(-1);
    int     exec_flag = TRUE;
    char    *s;

    TRACE(("Macro/Action=%s\n", macro_action));

    if ((s = is_cmd(macro_action)) != 0)
    {
        macro_action = s;
        exec_flag = FALSE;
    }

    if (is_bind (macro_action))
    {
        docmd (macro_action, exec_flag, FALSE, 1);
    }
    else
    {
        /* Action predefined */
        fact = get_action_fonc (macro_action);
        if (fact == NULL)
            return;
        fact(macro_action);
    }

    (void)im_waiting(oldflag);
    (void)update(TRUE);
}

/************************************************************************/
/* Function called when a buffer button is clicked into the menu        */
/* A buffer button takes part from the list tokens (L)                  */
/************************************************************************/
static void list_proc_back ( Widget w, XtPointer bname, XtPointer call GCC_UNUSED )
{
    int     num_buff;
    int     oldflag = im_waiting(-1);

#if MOTIF_WIDGETS
    char *accel;
    XmString xms;
    XtVaGetValues (w, XmNacceleratorText, &xms, NULL);
#ifndef XmFONTLIST_DEFAULT_TAG
#define XmFONTLIST_DEFAULT_TAG "XmFONTLIST_DEFAULT_TAG_STRING"
#endif
    XmStringGetLtoR (xms,  XmFONTLIST_DEFAULT_TAG, &accel);
    num_buff = atoi(&accel[1]);
#endif
#if ATHENA_WIDGETS
    num_buff = (int)bname;
#endif

    (void)histbuff(TRUE, num_buff);
    (void)im_waiting(oldflag);
    (void)update(TRUE);
}

static void add_callback(Widget w, XtCallbackProc function, XtPointer closure)
{
#if MOTIF_WIDGETS
    XtAddCallback (w, XmNactivateCallback, function, closure);
#endif
#if ATHENA_WIDGETS
    XtAddCallback (w, XtNcallback, function, closure);
#endif /* ATHENA_WIDGETS */
}

/************************************************************************/
/* Function called when the cascade containing the List (buffers)       */
/* is clicked => Post the PullDown associated                           */
/* It's dirty ...                                                       */
/************************************************************************/
static void post_buffer_list ( Widget w GCC_UNUSED, XtPointer client GCC_UNUSED, XEvent *ev GCC_UNUSED, Boolean *ok  GCC_UNUSED)
{
    int     i, n=nb_item_menu_list;
    BUFFER  *bp;
    char    string[NBUFN+2+NFILEN], temp[1+NFILEN], *p;
    Widget  pm = (Widget) client;

    TRACE(("post_buffer_list\n"));
    nb_item_menu_list = 0;

    for_each_buffer(bp)
    {
        if (b_is_temporary(bp)) /* cf: hist_show() */
            continue;

        p = shorten_path(strcpy(temp, bp->b_fname), FALSE);
        sprintf(string, "%-*.*s %s", NBUFN-1, NBUFN-1,
                bp->b_bname, p);

        sprintf(temp, "_%d", nb_item_menu_list);
        TRACE(("ACCEL(%s) = %s\n", temp, string));

        if (pm_buffer [nb_item_menu_list] == NULL)
        {
            pm_buffer [nb_item_menu_list] = do_button (pm, string, temp, 'B');
        }
        else
        {
#if MOTIF_WIDGETS
            XmString xms = XmStringCreateSimple (string);
            XtVaSetValues (pm_buffer [nb_item_menu_list],
                        XmNlabelString, xms, NULL);
            XmStringFree (xms);
            XtRemoveCallback (pm_buffer[nb_item_menu_list],
                              XmNactivateCallback, list_proc_back, NULL);
#endif
#if ATHENA_WIDGETS
            XtVaSetValues (pm_buffer [nb_item_menu_list],
                        XtNlabel, string, NULL);
            XtRemoveAllCallbacks (pm_buffer[nb_item_menu_list],
                        XtNcallback);
#endif
        }

#if MOTIF_WIDGETS
        XtAddCallback (pm_buffer[nb_item_menu_list],
                        XmNactivateCallback,
                        list_proc_back,
                        NULL);
#endif
#if ATHENA_WIDGETS
        XtAddCallback (pm_buffer[nb_item_menu_list],
                        XtNcallback,
                        list_proc_back,
                        (XtPointer)nb_item_menu_list);
#endif
        nb_item_menu_list++;
    }
    for (i=nb_item_menu_list; i<n; i++)
    {
        XtDestroyWidget (pm_buffer[i]);
        pm_buffer[i] = NULL;
    }

#if MOTIF_WIDGETS
    XmUpdateDisplay(pm);
#endif
}

/************************************************************************/
/* Main function : Take the MotifBar as argument and create all the     */
/* Cascades, PullDowns Menus and Buttons read from the rc file          */
/************************************************************************/
void do_menu ( Widget menub )
{
    int     i, n = 0;
    char    *arg, *accel, *macro;
    Widget  pm = 0;
    Widget  pm_w [50];
    int     rc;
    char   *menurc = menu_filename();

    if ((rc = parse_menu (menurc)) != TRUE)
    {
        if (rc == FAILED)
            puts ("Error parsing menu-file");
        return;
    }
#if OPT_TRACE
    print_token();
#endif

    for (i=0; i<Nb_Token; i++)
    {
        switch (Token[i].type)
        {
        /* HELP CASCADE */
        case 'C':
        case 'H':
            pm = do_cascade (menub, Token[i].libelle, Token[i].type);
            n = 0;
            break;

        /* SEPARATOR WIDGET */
        case 'S':
            do_button (pm, "sep", NULL, Token[i].type);
            break;

        /* LIST (BUFFER LIST) */
        case 'L':
            if (!strcmp(Token[i].action, "list_buff")
             && cascade != 0)
            {
                XtAddEventHandler (cascade,
                        ButtonPressMask, False, post_buffer_list, pm);
            }
            break;

        /* BUTTON WIDGET */
        case 'B':
            if (Token[i].macro > 0)
            {
                macro = castalloc(char,50);
                sprintf(macro, "execute-macro-%d", Token[i].macro);
                accel = give_accelerator (macro);
                pm_w[n] = do_button (pm, Token[i].libelle, accel, Token[i].type);

                arg = castalloc(char,10);
                sprintf(arg, "%d", Token[i].macro);
                add_callback (pm_w[n], proc_back, macro);
            }
            else
            {
                accel = give_accelerator (Token[i].action);
                pm_w[n] = do_button (pm, Token[i].libelle, accel, Token[i].type);
                add_callback (pm_w[n], proc_back, Token[i].action);
            }
            n++;
            break;
        }
    }

    for (i=0; i<MAX_ITEM_MENU_LIST; i++)
        pm_buffer[i] = NULL;
}
/************************************************************************/

#endif /* DISP_X11 */
