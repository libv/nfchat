#ifndef SKIPCONFIG
#include "../../config.h"
#endif

#ifdef USE_GNOME

#include <gnome.h>
#undef GNOME_APP
#define GNOME_APP(n) ((GnomeApp*)n)

#else

#include <gtk/gtk.h>
#include "fake_gnome.h"
#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(x) gettext(x)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define N_(String) (String)
#  define _(x) (x)
#endif

#endif

#undef GTK_BIN
#undef GTK_WINDOW
#undef GTK_BOX
#undef GTK_OBJECT
#undef GTK_CONTAINER
#undef GTK_CLIST
#undef GTK_TEXT
#undef GTK_LABEL
#undef GTK_ENTRY
#undef GTK_WIDGET
#undef GTK_MENU_BAR
#undef GTK_DIALOG
#undef GTK_FILE_SELECTION
#undef GTK_PANED
#undef GTK_TABLE
#undef GTK_DRAWING_AREA
#undef GTK_FONT_SELECTION_DIALOG
#undef GTK_SCROLLED_WINDOW
#undef GTK_TOGGLE_BUTTON
#undef GTK_NOTEBOOK
#undef GTK_MENU_ITEM
#undef GTK_CTREE
#undef GTK_COLOR_SELECTION_DIALOG

#define GTK_BIN(n) ((GtkBin *)n)
#define GTK_WINDOW(n) ((GtkWindow *)n)
#define GTK_BOX(n) ((GtkBox *)n)
#define GTK_OBJECT(n) ((GtkObject *)n)
#define GTK_CONTAINER(n) ((GtkContainer *)n)
#define GTK_CLIST(n) ((GtkCList *)n)
#define GTK_TEXT(n) ((GtkText *)n)
#define GTK_LABEL(n) ((GtkLabel *)n)
#define GTK_ENTRY(n) ((GtkEntry *)n)
#define GTK_WIDGET(n) ((GtkWidget *)n)
#define GTK_MENU_BAR(n) ((GtkMenuBar *)n)
#define GTK_DIALOG(n) ((GtkDialog *)n)
#define GTK_FILE_SELECTION(n) ((GtkFileSelection *)n)
#define GTK_PANED(n) ((GtkPaned *)n)
#define GTK_TABLE(n) ((GtkTable *)n)
#define GTK_DRAWING_AREA(n) ((GtkDrawingArea *)n)
#define GTK_FONT_SELECTION_DIALOG(n) ((GtkFontSelectionDialog *)n)
#define GTK_SCROLLED_WINDOW(n) ((GtkScrolledWindow *)n)
#define GTK_TOGGLE_BUTTON(n) ((GtkToggleButton *)n)
#define GTK_NOTEBOOK(n) ((GtkNotebook*)n)
#define GTK_MENU_ITEM(n) ((GtkMenuItem*)n)
#define GTK_CTREE(n) ((GtkCTree*)n)
#define GTK_COLOR_SELECTION_DIALOG(n) ((GtkColorSelectionDialog *)n)

#define flag_t flag_wid[0]
#define flag_n flag_wid[1]
#define flag_s flag_wid[2]
#define flag_i flag_wid[3]
#define flag_p flag_wid[4]
#define flag_m flag_wid[5]
#define flag_l flag_wid[6]
#define flag_k flag_wid[7]

struct server_gui
{
   GtkWidget *rawlog_window;
   GtkWidget *rawlog_textlist;

   GtkWidget *chanlist_wild;
   GtkWidget *chanlist_window;
   GtkWidget *chanlist_list;
   GtkWidget *chanlist_refresh;

   int chanlist_minusers;
};

struct session_gui
{
   GtkWidget *window;
   GtkWidget *vbox;
   GtkWidget *menu;
   GtkWidget *usermenu;
   GtkWidget *tbox;
   GtkWidget *changad;
   GtkWidget *topicgad;         /* FIXME: Pseudo union with ipgad --AGL */
   GtkWidget *textgad;
   GtkWidget *namelistgad;
   GtkWidget *nickgad;
   GtkWidget *inputgad;
   GtkWidget *namelistinfo;
   GtkWidget *paned;
   GtkWidget *vscrollbar;
   GtkWidget *op_box;
   GtkWidget *op_xpm;
   GtkWidget *userlistbox;
   GtkWidget *nl_box;
   GtkWidget *button_box;
   GtkWidget *toolbox;
   GtkWidget *bar;
   GtkWidget *leftpane;
   GtkWidget *confbutton;       /* conference mode button */
   GtkWidget *flag_wid[8];
   GtkWidget *limit_entry;      /* +l */
   GtkWidget *key_entry;        /* +k */
   GtkWidget *delink_button;
};
