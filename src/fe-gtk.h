/*
 * NF-Chat: A cut down version of X-chat, cut down by _Death_
 * Largely based upon X-Chat 1.4.2 by Peter Zelezny. (www.xchat.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef SKIPCONFIG
#include "../config.h"
#endif

#include <gtk/gtk.h>
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

struct session_gui
{
   GtkWidget *window;
   GtkWidget *vbox;   
   GtkWidget *tbox;
   GtkWidget *topicgad;         /* FIXME: Pseudo union with ipgad --AGL */
   GtkWidget *textgad;
   GtkWidget *namelistgad;
   GtkWidget *nickgad;
   GtkWidget *inputgad;
   GtkWidget *namelistinfo;
   GtkWidget *vscrollbar;
   GtkWidget *op_box;
   GtkWidget *op_xpm;
   GtkWidget *userlistbox;
   GtkWidget *nl_box;
   GtkWidget *bar;				/* progressbar */
   GtkWidget *leftpane;
};
