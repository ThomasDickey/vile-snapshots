/************************************************************************/
/*  File	: menu.c						*/
/*  Description : Implement a Motif MenuBar into Mxvile 		*/
/*		  Parse a File (environment variable XVILE_MENU)	*/
/*		  and create it at the top of the window		*/
/*  Auteur	: Philippe CHASSANY					*/
/*  Date	: 20/02/97						*/
/************************************************************************/

/* Vile includes */
#include "estruct.h"
#include "edef.h"

#if MOTIF_WIDGETS
#include <Xm/MainW.h>
#include <Xm/CascadeB.h>
#include <Xm/SeparatoG.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#endif

/* Xvile copy */
static	int show_all;
#define update_on_chg(bp) (!b_is_temporary(bp) || show_all)

/* Patch (PC) of bind.c file */
extern char *give_accelerator ( char * );

/* Locals */
#define MAX_ITEM_MENU_LIST  50
int	nb_item_menu_list = 0;
Widget	cascade;

/* Tokens of the rc file */
#define MAX_TOKEN   200
struct	_tok_
{
    char    type;
    int     idx_cascade;
    char    libelle [100];
    char    action  [50];
    int     flag_bind;
    int     macro;
} Token [MAX_TOKEN];
int Nb_Token;

/* Actions of the rc file */
typedef int (*ActionFunc) ( char * );

typedef struct 
{
    char		name [30];
    const   ActionFunc	fact;

} TAct;

/* Common Actions = User defined Actions */
/* Why not use dynamic link library to get functions ? */
int common_action ( char *action );
static TAct Actions [] = {
    "nop",	    NULL,
    "new_xvile",    common_action,
    "edit_rc",	    common_action,
    "parse_rc",     common_action,
    "edit_mrc",     common_action,
};
int Nb_Actions = sizeof(Actions)/sizeof(TAct);


/************************************************************************/
/* All the Common Action are pointed to this function			*/
/************************************************************************/
int common_action ( char *action )
{
    FILE    *pp;
    char    str [256];

#ifdef DEBUG
    printf("Action=%s\n", action);
#endif

    if (!strcmp(action, "new_xvile"))
    {
	pp = popen ("which xvile", "r");
	fscanf(pp, "%s", str);
	pclose (pp);
	if (fork() == 0)
	    execl (str, "xvile", NULL, 0);
    }
    else
    if (!strcmp(action, "edit_rc"))
    {
	splitwind(FALSE, 1);
	sprintf(str, "%s/.vilerc", getenv ("HOME"));
	getfile (str, TRUE);
    }
    else
    if (!strcmp(action, "parse_rc"))
    {
	sprintf(str, "%s/.vilerc", getenv ("HOME"));
	dofile (str);
    }
    else
    if (!strcmp(action, "edit_mrc"))
    {
	char	*menurc;

	splitwind(FALSE, 1);

	menurc = getenv ("XVILE_MENU");
	if (menurc == NULL)
	{
	    puts ("Error: XVILE_MENU variable undefined");
	    return;
	}
	getfile (menurc, TRUE);
    }
}
/************************************************************************/
/* Inspired from vile sources						*/
/************************************************************************/
static BUFFER *find_b_hist(int number)
{
    register BUFFER *bp;

    if (number >= 0) {
	for_each_buffer(bp)
	    if (!b_is_temporary(bp) && (number-- <= 0))
		break;
    } else
	bp = 0;
    return bp;
}

/************************************************************************/
/* Inspired from vile sources						*/
/************************************************************************/
static char *hist_lookup(int c)
{
    register BUFFER *bp = find_b_hist(c);
    return (bp != 0) ? bp->b_bname : NULL;
}

/************************************************************************/
/* Test if the action name is part of the Actions of the rc file	*/
/************************************************************************/
int is_action ( char *action )
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
/* Return the function pointer associated with the action name		*/
/************************************************************************/
const ActionFunc get_action_fonc ( char *action )
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
/* Test if the action name is a bind (inspired from vile sources	*/
/************************************************************************/
int is_bind ( char *action )
{
    if (engl2fnc(action) != NULL)
	return 1;

    return 0;
}

/************************************************************************/
/* Main function to parse the rc file					*/
/************************************************************************/
int parse_menu ( char *filename )
{
    FILE    *fp;
    char    line [1000];
    char    *ptr_tok;
    int     n, cascade_token, tmp;
    int     nlig = 0;

    if ((fp=fopen(filename, "r")) == NULL)
	return 0;

    Nb_Token = 0;
    while (fgets (line, sizeof(line), fp) != NULL)
    {
	nlig++;

#if 1
	if ((ptr_tok = strchr(line, '\t')) != 0)
		*ptr_tok = EOS;
#endif

	ptr_tok = strtok(line, ":");
	if (ptr_tok == NULL)
	    continue;

	switch (*ptr_tok)
	{
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
		    return 0;
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
			if (isdigit((int)*ptr_tok))
			{
			    tmp = (int)atoi(ptr_tok);
			    Token[Nb_Token].macro = tmp;
			}
			else
			{
			    if (is_action(ptr_tok))
			    {
				strcpy(Token[Nb_Token].action, ptr_tok);
				Token[Nb_Token].flag_bind = 0;
			    }
			    else
			    if (is_bind(ptr_tok))
			    {
				strcpy(Token[Nb_Token].action, ptr_tok);
				Token[Nb_Token].flag_bind = 1;
			    }
			    else
			    {
				printf("Error in line %d : '%s' is not an action\n",
					    nlig, ptr_tok);
			    }
			}
			break;
		    }
		    n++;
		}
		if (n != 2)
		{
		    fclose (fp);
		    return 0;
		}
		Nb_Token++;
		break;
	}
    }
    fclose (fp);

    return 1;
}

/************************************************************************/
/* Debug function to print the	tokens of the rc file			*/
/************************************************************************/
void print_token ()
{
    int     i;

    for (i=0; i<Nb_Token; i++)
    {
	switch (Token[i].type)
	{
	case 'C':
	case 'H':
	    printf("%c %s\n", 
			Token[i].type,
			Token[i].libelle);
	    break;
	case 'S':
	    printf("\t%c (%s)\n", 
			Token[i].type,
			Token[Token[i].idx_cascade].libelle);
	    break;
	case 'L':
	    printf("\t%c %s\n", 
			Token[i].type,
			Token[i].action);
	    break;
	case 'B':
	    if (Token[i].macro > 0)
		printf("\t%c %s(%s) -> Macro-%d\n", 
			Token[i].type,
			Token[i].libelle,
			Token[Token[i].idx_cascade].libelle,
			Token[i].macro);
	    else
		printf("\t%c %s(%s) -> Action-%s\n", 
			Token[i].type,
			Token[i].libelle,
			Token[Token[i].idx_cascade].libelle,
			Token[i].action);
	}
    }
}

/************************************************************************/
/* Motif function to make a cascade button into a menubar		*/
/************************************************************************/
Widget do_cascade ( Widget menub, char *nom )
{
    Widget	pm;
    XmString	xms;
    char	str [50];

    sprintf(str, "%s_pm", nom);
    pm = XmCreatePulldownMenu (menub, str, NULL, 0);

    xms = XmStringCreateSimple (nom);

    cascade = XtVaCreateManagedWidget (nom,
		xmCascadeButtonWidgetClass,	menub,
		XmNlabelString, 	xms,
		XmNsubMenuId,		pm,
		NULL);
    XmStringFree (xms);

    return pm;
}

/************************************************************************/
/* Motif function to make a button into a cascade (with accelarator)	*/
/************************************************************************/
Widget do_button ( Widget pm, char *nom, char *accel, WidgetClass wc )
{
    Widget	w;
    XmString	xms;

    if (accel != NULL)
	xms = XmStringCreateSimple (accel);
    else
	xms = XmStringCreateSimple ("");

    w = XtVaCreateManagedWidget (nom,
		wc, 
		pm,
		XmNacceleratorText,	xms,
		NULL);

    XmStringFree (xms);

    return w;
}

/************************************************************************/
/* Function called when a button is clicked into the menu		*/
/* Bind and Action are tested						*/
/************************************************************************/
void proc_back ( Widget w, XtPointer arg, XtPointer call )
{
    char    *macro_action = (char *)arg;
    const CMDFUNC   *cmd;
    ActionFunc fact;

#ifdef DEBUG
    printf("Macro/Action=%s\n", macro_action);
#endif

    if (is_bind (macro_action))
    {	
	/* Binding */
	if ((cmd = engl2fnc (macro_action)) == NULL)
	    return;
	execute (cmd, FALSE, 1);
    }
    else
    {
	/* Action predefined */
	fact = get_action_fonc (macro_action);
	if (fact == NULL)
	    return;
	fact(macro_action);
    }

    (void)update(TRUE);
}

/************************************************************************/
/* Function called when a buffer button is clicked into the menu	*/
/* A buffer button takes part from the list tokens (L)			*/
/************************************************************************/
void list_proc_back ( Widget w, XtPointer bname, XtPointer call )
{
    char    *sbname = (char *)bname;
    int     num_buff = (int)atoi(sbname);
    register BUFFER *bp;
    char    *ptr;

#ifdef DEBUG
    printf("BUFF %d\n", num_buff);
#endif

    if ((ptr = hist_lookup(num_buff)) == NULL) {
	mlwarn("[No such buffer.]");
	return;
    }
    if ((bp = find_b_name (ptr)) == NULL)
    {
#ifdef DEBUG
	printf("BP NULL\n");
#endif
    }
    else
    {
#ifdef DEBUG
	printf("BP = %s\n", bp->b_bname);
#endif
	swbuffer(bp);
    }
    (void)update(TRUE);
}

/************************************************************************/
/* Function called when the cascade containing the List (buffers)	*/
/* is clicked => Post the PullDown associated				*/
/* It's dirty ...							*/
/************************************************************************/
void post_menu ( Widget w, XtPointer client, XEvent *ev )
{
    int     i;
    BUFFER  *bp;
    char    *arg, string[256], temp[256], *p;
    Widget  pm = (Widget) client;
    XButtonPressedEvent *bev = (XButtonPressedEvent *)ev;
    static Widget   wlist[MAX_ITEM_MENU_LIST];

    for (i=0; i<nb_item_menu_list; i++)
    {
	if (wlist[i] != NULL)
	    XtDestroyWidget (wlist[i]);
	wlist[i] = NULL;
    }

    nb_item_menu_list = 0;

    for_each_buffer(bp) 
    {
	if (!update_on_chg(bp))
	    continue;

	p = shorten_path(strcpy(temp, bp->b_fname), FALSE);
	sprintf(string, "%*.*-s %s", NBUFN-1, NBUFN-1, 
		bp->b_bname, 
		lengthen_path(p));

	sprintf(temp, "_%d", nb_item_menu_list);
	wlist [nb_item_menu_list] =
	    do_button (pm, string, temp, xmPushButtonGadgetClass);

	arg = (char *)malloc(10*sizeof(char));
	sprintf(arg, "%d", nb_item_menu_list);
	
	XtAddCallback (wlist[nb_item_menu_list], 
			XmNactivateCallback, 
			list_proc_back,
			arg);
	nb_item_menu_list++;
    }
    XmUpdateDisplay(pm);
}

/************************************************************************/
/* Main function : Take the MotifBar as argument and create all the	*/
/* Cascades, PullDowns Menus and Buttons readed from the rc file	*/
/************************************************************************/
void do_menu ( Widget menub )
{
    int     i, n;
    char    *arg, *accel, *macro;
    Widget  pm;
    Widget  pm_w [50];
    char    *menurc;

    menurc = getenv ("XVILE_MENU");
    if (menurc == NULL)
    {
	puts ("Error: XVILE_MENU variable undefined");
	return;
    }
    if (!parse_menu (menurc))
    {
	puts ("Erreur");
	return;
    }
    else
    {
#ifdef DEBUG
	print_token();
#endif
    }

    for (i=0; i<Nb_Token; i++)
    {
	switch (Token[i].type)
	{
	/* HELP CASCADE */
	case 'C':
	case 'H':
	    pm = do_cascade (menub, Token[i].libelle);
	    if (Token[i].type == 'H')
	    {
		XtVaSetValues (menub, 
				XmNmenuHelpWidget,  cascade,
				NULL);
	    }
	    n=0;
	    break;

	/* SEPARATOR WIDGET */
	case 'S':
	    do_button (pm, "sep", NULL, xmSeparatorGadgetClass);
	    break;

	/* LIST (BUFFER LIST) */
	case 'L':
	    if (!strcmp(Token[i].action, "list_buff"))
	    {
		XtAddEventHandler (cascade, 
			ButtonPressMask, False, post_menu, pm);
	    }
	    break;

	/* BUTTON WIDGET */
	case 'B':
	    if (Token[i].macro > 0)
	    {
		macro = (char *)malloc(50*sizeof(char));
		sprintf(macro, "execute-macro-%d", Token[i].macro);
		accel = give_accelerator (macro);
		pm_w[n] = do_button (pm, Token[i].libelle, accel, xmPushButtonGadgetClass);

		arg = (char *)malloc(10*sizeof(char));
		sprintf(arg, "%d", Token[i].macro);
		XtAddCallback (pm_w[n], XmNactivateCallback, 
			proc_back, macro);
	    }
	    else
	    {
		accel = give_accelerator (Token[i].action);
		pm_w[n] = do_button (pm, Token[i].libelle, accel, xmPushButtonGadgetClass);
		XtAddCallback (pm_w[n], XmNactivateCallback, 
		    proc_back, Token[i].action);
	    }
	    n++;
	    break;
	}
    }
}
/************************************************************************/
