/************************************************************************/
/*  File        : menu.c                                                */
/*  Description : Implement a Motif MenuBar into Mxvile                 */
/*                Parse a File (environment variable XVILE_MENU)        */
/*                and create it at the top of the window                */
/*  Auteur      : Philippe CHASSANY                                     */
/*  Date        : 20/02/97                                              */
/************************************************************************/

/*
 * $Id: x11menu.c,v 1.25 2025/01/26 16:54:07 tom Exp $
 */

#define NEED_X_INCLUDES 1

/* Vile includes */
#include "estruct.h"
#include "edef.h"

#if DISP_X11

#if ATHENA_WIDGETS
#if defined(HAVE_LIB_XAW3DXFT)
#include	<X11/Xaw3dxft/Form.h>
#include	<X11/Xaw3dxft/SimpleMenu.h>
#include	<X11/Xaw3dxft/MenuButton.h>
#include	<X11/Xaw3dxft/SmeLine.h>
#include	<X11/Xaw3dxft/SmeBSB.h>
#elif defined(HAVE_LIB_XAW3D)
#include	<X11/Xaw3d/Form.h>
#include	<X11/Xaw3d/SimpleMenu.h>
#include	<X11/Xaw3d/MenuButton.h>
#include	<X11/Xaw3d/SmeLine.h>
#include	<X11/Xaw3d/SmeBSB.h>
#elif defined(HAVE_LIB_XAWPLUS)
#include	<X11/XawPlus/Form.h>
#include	<X11/XawPlus/SimpleMenu.h>
#include	<X11/XawPlus/MenuButton.h>
#include	<X11/XawPlus/SmeLine.h>
#include	<X11/XawPlus/SmeBSB.h>
#elif defined(HAVE_LIB_NEXTAW)
#include	<X11/neXtaw/Form.h>
#include	<X11/neXtaw/SimpleMenu.h>
#include	<X11/neXtaw/MenuButton.h>
#include	<X11/neXtaw/SmeLine.h>
#include	<X11/neXtaw/SmeBSB.h>
#elif defined(HAVE_LIB_XAW)
#include	<X11/Xaw/Form.h>
#include	<X11/Xaw/SimpleMenu.h>
#include	<X11/Xaw/MenuButton.h>
#include	<X11/Xaw/SmeLine.h>
#include	<X11/Xaw/SmeBSB.h>
#endif
#elif MOTIF_WIDGETS
#include	<Xm/MainW.h>
#include	<Xm/CascadeB.h>
#include	<Xm/SeparatoG.h>
#include	<Xm/RowColumn.h>
#include	<Xm/PushBG.h>
#include	<Xm/ToggleB.h>
#endif

#define Nval(name,value) name, (XtArgVal)(value)
#define Sval(name,value) name, (value)

#define MY_MENUS struct MyMenus
MY_MENUS {
    MY_MENUS *next;
    Widget parent;
    Widget child;
};

/* Locals */
static Widget cascade;
static MY_MENUS *my_menus;

/************************************************************************/
/* Motif function to make a cascade button into a menubar               */
/************************************************************************/
void *
gui_make_menu(void *menubar, const char *nom, int the_class GCC_UNUSED)
{
    MY_MENUS *remember;
    Widget pm;
    Widget menub = (Widget) menubar;
#if MOTIF_WIDGETS
    XmString xms;
    char str[50];

    sprintf(str, "%sMenu", nom);
    pm = (Widget) XmCreatePulldownMenu(menub, str, NULL, 0);
#if OPT_MENUS_COLORED
    XtVaSetValues(pm,
		  Nval(XtNforeground, x_menu_foreground()),
		  Nval(XtNbackground, x_menu_background()),
		  NULL);
    XtVaSetValues(menub,
		  Nval(XtNforeground, x_menu_foreground()),
		  Nval(XtNbackground, x_menu_background()),
		  NULL);
#endif /* OPT_MENUS_COLORED */

    xms = XmStringCreateSimple(nom);

    cascade = XtVaCreateManagedWidget("menuHeader",
				      xmCascadeButtonWidgetClass, menub,
				      Nval(XmNlabelString, xms),
				      Nval(XmNsubMenuId, pm),
#if OPT_MENUS_COLORED
				      Nval(XtNforeground, x_menu_foreground()),
				      Nval(XtNbackground, x_menu_background()),
#endif /* OPT_MENUS_COLORED */
				      NULL);
    XmStringFree(xms);

    if (the_class == 'H') {
	XtVaSetValues(menub,
		      Nval(XmNmenuHelpWidget, cascade),
		      NULL);
    }
#elif ATHENA_WIDGETS
    static Widget last;
    char *str = XtMalloc((Cardinal) strlen(nom) + 10);

    sprintf(str, "%sMenu", nom);
    pm = XtVaCreatePopupShell(str,
			      simpleMenuWidgetClass,
			      menub,
			      Nval(XtNgeometry, NULL),
			      NULL);

    cascade = XtVaCreateManagedWidget("menuHeader",
				      menuButtonWidgetClass,
				      menub,
				      Nval(XtNheight, x_menu_height()),
				      Nval(XtNlabel, nom),
				      Nval(XtNfromHoriz, last),
				      Nval(XtNmenuName, str),
				      NULL);
#if OPT_MENUS_COLORED
    XtVaSetValues(pm,
		  Nval(XtNforeground, x_menu_foreground()),
		  Nval(XtNbackground, x_menu_background()),
		  NULL);
    XtVaSetValues(menub,
		  Nval(XtNforeground, x_menu_foreground()),
		  Nval(XtNbackground, x_menu_background()),
		  NULL);
    XtVaSetValues(cascade,
		  Nval(XtNforeground, x_menu_foreground()),
		  Nval(XtNbackground, x_menu_background()),
		  NULL);
#endif /* OPT_MENUS_COLORED */
    last = cascade;

#endif /* ATHENA_WIDGETS */

    /* remember the widgets we created, so we can destroy them */
    if ((remember = typecalloc(MY_MENUS)) != NULL) {
	remember->parent = pm;
	remember->child = cascade;
	remember->next = my_menus;
	my_menus = remember;
    }

    return (void *) pm;
}

/************************************************************************/
/* Motif function to make a button into a cascade (with accelarator)    */
/************************************************************************/
void *
gui_add_menu_item(void *pm, const char *nom, char *accel GCC_UNUSED, int the_class)
{
    Widget w;
#if MOTIF_WIDGETS
    XmString xms_accl;
    XmString xms_name;
    WidgetClass wc;

    switch (the_class) {
    case 'S':
	wc = xmSeparatorGadgetClass;
	break;
    default:
	/* FALLTHRU */
    case 'B':
	wc = xmPushButtonGadgetClass;
	break;
    }

    if (accel != NULL)
	xms_accl = XmStringCreateSimple(accel);
    else
	xms_accl = XmStringCreateSimple("");

    xms_name = XmStringCreateSimple(nom);
    w = XtVaCreateManagedWidget("menuEntry",
				wc,
				(Widget) pm,
				Nval(XmNacceleratorText, xms_accl),
				Nval(XmNlabelString, xms_name),
				NULL);

    XmStringFree(xms_accl);
    XmStringFree(xms_name);

#elif ATHENA_WIDGETS
    w = XtVaCreateManagedWidget("menuEntry",
				((the_class == 'B')
				 ? smeBSBObjectClass
				 : smeLineObjectClass),
				(Widget) pm,
				Nval(XtNlabel, nom),
				NULL);

#endif /* ATHENA_WIDGETS */
    return (void *) w;
}

/************************************************************************/
/* Function called when a buffer button is clicked into the menu        */
/* A buffer button takes part from the list tokens (L)                  */
/************************************************************************/
static void
list_proc_back(Widget w GCC_UNUSED,
	       XtPointer bname GCC_UNUSED,
	       XtPointer call GCC_UNUSED)
{
    int num_buff;
    int oldflag = im_waiting(-1);

#if MOTIF_WIDGETS
    char *accel;
    XmString xms;
    XtVaGetValues(w, XmNacceleratorText, &xms, NULL);
#ifndef XmFONTLIST_DEFAULT_TAG
#define XmFONTLIST_DEFAULT_TAG "XmFONTLIST_DEFAULT_TAG_STRING"
#endif
    XmStringGetLtoR(xms, XmFONTLIST_DEFAULT_TAG, &accel);
    num_buff = atoi(&accel[1]);
#elif ATHENA_WIDGETS
    num_buff = (int) (long) bname;
#endif

    if (vile_is_busy)
	return;

    (void) histbuff(TRUE, num_buff);
    (void) im_waiting(oldflag);
    (void) update(TRUE);
}

/************************************************************************/
/* Function called when a button is clicked into the menu               */
/* Bind and Action are tested                                           */
/************************************************************************/
static void
proc_back(Widget w GCC_UNUSED, XtPointer arg, XtPointer call GCC_UNUSED)
{
    char *macro_action = (char *) arg;
    ActionFunc fact;
    int oldflag = im_waiting(-1);
    int exec_flag = TRUE;
    char *s;

    TRACE(("Macro/Action=%s\n", macro_action));

#if OPT_WORKING
    if (vile_is_busy)
	return;
#endif

    if ((s = vlmenu_is_cmd(macro_action)) != NULL) {
	macro_action = s;
	exec_flag = FALSE;
    }

    if (vlmenu_is_bind(macro_action)) {
	docmd(macro_action, exec_flag, FALSE, 1);
    } else {
	/* Action predefined */
	fact = vlmenu_action_func(macro_action);
	if (fact != NULL)
	    fact(macro_action);
    }

    (void) im_waiting(oldflag);
    (void) update(TRUE);
}

void
gui_add_func_callback(void *w, void *closure)
{
#if MOTIF_WIDGETS
    XtAddCallback((Widget) w, XmNactivateCallback, proc_back, closure);
#elif ATHENA_WIDGETS
    XtAddCallback((Widget) w, XtNcallback, proc_back, closure);
#endif /* ATHENA_WIDGETS */
}

/************************************************************************/
/* Function called when the cascade containing the List (buffers)       */
/* is clicked => Post the PullDown associated                           */
/* It's dirty ...                                                       */
/************************************************************************/
static void
post_buffer_list(Widget w GCC_UNUSED,
		 XtPointer client GCC_UNUSED,
		 XEvent *ev GCC_UNUSED,
		 Boolean *ok GCC_UNUSED)
{
    static unsigned in_item_menu_list = 0;	/* number allocated */
    static unsigned nb_item_menu_list = 0;	/* number in use */
    static Widget *pm_buffer;

    unsigned i, n = nb_item_menu_list;
    BUFFER *bp;
    char string[NBUFN + 2 + NFILEN], temp[1 + NFILEN], *p;
    Widget pm = (Widget) client;

    TRACE(("post_buffer_list\n"));
    nb_item_menu_list = 0;

    for_each_buffer(bp) {
	if (b_is_temporary(bp))	/* cf: hist_show() */
	    continue;

	p = shorten_path(vl_strncpy(temp, bp->b_fname, sizeof(temp)), FALSE);
	sprintf(string, "%-*.*s %s", NBUFN - 1, NBUFN - 1,
		bp->b_bname, p);

	sprintf(temp, "_%d", nb_item_menu_list);
	TRACE(("ACCEL(%s) = %s\n", temp, string));

	if (nb_item_menu_list + 2 >= in_item_menu_list) {
	    unsigned m = in_item_menu_list;

	    in_item_menu_list = (in_item_menu_list + 3) * 2;

	    if (pm_buffer != NULL)
		pm_buffer = typereallocn(Widget, pm_buffer, in_item_menu_list);
	    else
		pm_buffer = typeallocn(Widget, in_item_menu_list);

	    if (pm_buffer == NULL) {
		no_memory("post_buffer_list");
		return;
	    }

	    while (m < in_item_menu_list)
		pm_buffer[m++] = NULL;
	}

	if (pm_buffer[nb_item_menu_list] == NULL) {
	    pm_buffer[nb_item_menu_list] = (Widget) gui_add_menu_item(pm, string,
								      temp, 'B');
	} else {
#if MOTIF_WIDGETS
	    XmString xms = XmStringCreateSimple(string);
	    XtVaSetValues(pm_buffer[nb_item_menu_list],
			  Nval(XmNlabelString, xms),
			  NULL);
	    XmStringFree(xms);
	    XtRemoveCallback(pm_buffer[nb_item_menu_list],
			     XmNactivateCallback, list_proc_back, NULL);
#elif ATHENA_WIDGETS
	    XtVaSetValues(pm_buffer[nb_item_menu_list],
			  Nval(XtNlabel, string),
			  NULL);
	    XtRemoveAllCallbacks(pm_buffer[nb_item_menu_list],
				 XtNcallback);
#endif
	}

#if MOTIF_WIDGETS
	XtAddCallback(pm_buffer[nb_item_menu_list],
		      XmNactivateCallback,
		      list_proc_back,
		      NULL);
#elif ATHENA_WIDGETS
	XtAddCallback(pm_buffer[nb_item_menu_list],
		      XtNcallback,
		      list_proc_back,
		      (XtPointer) (long) nb_item_menu_list);
#endif
	nb_item_menu_list++;
    }
    for (i = nb_item_menu_list; i < n; i++) {
	XtDestroyWidget(pm_buffer[i]);
	pm_buffer[i] = NULL;
    }

#if MOTIF_WIDGETS
    XmUpdateDisplay(pm);
#endif
}

void
gui_add_list_callback(void *pm)
{
    if (cascade != NULL)
	XtAddEventHandler(cascade,
			  ButtonPressMask,
			  False,
			  post_buffer_list,
			  (Widget) pm);
}

/*
 * Hide/show menus, useful for avoiding thrashing while regenerating menus
 */
int
gui_hide_menus(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int rc = FALSE;
    Widget w = x_menu_widget();

    if (w != NULL) {
	XtUnmapWidget(w);
	rc = TRUE;
    }
    TRACE(("gui_hide_menus %d\n", rc));
    return rc;
}

int
gui_show_menus(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int rc = FALSE;
    Widget w = x_menu_widget();

    if (w != NULL) {
	XtMapWidget(w);
	rc = TRUE;
    }
    TRACE(("gui_show_menus %d\n", rc));
    return rc;
}

/*
 * Remove children of the current menu widget, so we can regenerate them.
 */
int
gui_remove_menus(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int rc = TRUE;
    int count = 0;

    gui_hide_menus(f, n);
    while (my_menus != NULL) {
	MY_MENUS *next = my_menus->next;

	XtUnmanageChild(my_menus->parent);
	XtDestroyWidget(my_menus->parent);
	XtDestroyWidget(my_menus->child);

	free(my_menus);
	my_menus = next;
	++count;
    }
    TRACE(("gui_remove_menus %d\n", count));
    return rc;
}

/*
 * (Re)create the menus.
 */
int
gui_create_menus(void)
{
    int rc = do_menu(x_menu_widget());
    TRACE(("gui_create_menus %d\n", rc));
    return rc;
}

#endif /* DISP_X11 */
