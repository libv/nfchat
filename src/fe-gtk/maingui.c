/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
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

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <stdio.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include "../common/xchat.h"
#include "../common/fe.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#ifdef USE_IMLIB
#include <gdk_imlib.h>
#endif
#include <gdk/gdkkeysyms.h>
#include "xtext.h"

GtkWidget *main_window = 0;
GtkWidget *main_book;
GtkStyle *normaltab_style = 0;
GtkStyle *redtab_style;
GtkStyle *bluetab_style;
GtkStyle *inputgad_style;

extern struct session *current_tab; 
extern struct xchatprefs prefs;
extern GSList *sess_list;
extern GtkStyle *channelwin_style;
extern GdkFont *font_normal;
extern gint xchat_is_quitting;

extern int handle_multiline (struct session *sess, char *cmd, int history, int nocommand);
extern int kill_session_callback (struct session *sess);
extern gint gtk_kill_session_callback (GtkWidget *, struct session *sess);
extern void palette_alloc (GtkWidget * widget);
extern void clear_user_list (struct session *sess);
extern void handle_inputgad (GtkWidget * igad, struct session *sess);
int key_handle_key_press (GtkWidget *, GdkEventKey *, gpointer);

struct relink_data {
   GtkWidget *win, *hbox;
   gchar *ltitle;
   gchar *stitle;
   void (*close_callback) (GtkWidget *, void *);
   void *userdata;
};

GdkColor colors[] =
{
   {0, 0, 0, 0},                /* 0  black */
   {0, 0xcccc, 0xcccc, 0xcccc}, /* 1  white */
   {0, 0, 0, 0xcccc},           /* 2  blue */
   {0, 0, 0xcccc, 0},           /* 3  green */
   {0, 0xcccc, 0, 0},           /* 4  red */
   {0, 0xbbbb, 0xbbbb, 0},      /* 5  yellow/brown */
   {0, 0xbbbb, 0, 0xbbbb},      /* 6  purple */
   {0, 0xffff, 0xaaaa, 0},      /* 7  orange */
   {0, 0xffff, 0xffff, 0},      /* 8  yellow */
   {0, 0, 0xffff, 0},           /* 9  green */
   {0, 0, 0xcccc, 0xcccc},      /* 10 aqua */
   {0, 0, 0xffff, 0xffff},      /* 11 light aqua */
   {0, 0, 0, 0xffff},           /* 12 blue */
   {0, 0xffff, 0, 0xffff},      /* 13 pink */
   {0, 0x7777, 0x7777, 0x7777}, /* 14 grey */
   {0, 0x9999, 0x9999, 0x9999}, /* 15 light grey */
   {0, 0, 0, 0xcccc},           /* 16 blue markBack */
   {0, 0xeeee, 0xeeee, 0xeeee}, /* 17 white markFore */
   {0, 0xcccc, 0xcccc, 0xcccc}, /* 18 foreground (white) */
   {0, 0, 0, 0},                /* 19 background (black) */
};

void
fe_set_title (struct session *sess)
{
   char tbuf[200];
   if (!sess->server->connected)
      strcpy (tbuf, "NF-Chat [" VERSION "]");
   else
   {
      if (sess->channel[0] == 0 || sess->is_server)
         snprintf (tbuf, sizeof tbuf, "NF-Chat [" VERSION "]: %s", sess->server->servername);
      else
         snprintf (tbuf, sizeof tbuf, "NF-Chat [" VERSION "]: %s / %s", sess->server->servername, sess->channel);
   }
   if (sess->is_tab)
   {
      if (!main_window)
         return;
      if (current_tab == sess)
         gtk_window_set_title ((GtkWindow *) main_window, tbuf);
   } else
      gtk_window_set_title ((GtkWindow *) sess->gui->window, tbuf);
}

void
fe_set_channel (struct session *sess)
{
   gtk_label_set_text (GTK_LABEL (sess->gui->changad), sess->channel);
}

void
fe_clear_channel (struct session *sess)
{
   gtk_entry_set_text (GTK_ENTRY (sess->gui->topicgad), "");
   gtk_label_set_text (GTK_LABEL (sess->gui->namelistinfo), " ");
   gtk_label_set_text (GTK_LABEL (sess->gui->changad), "<none>");

   clear_user_list (sess);

   if (sess->gui->op_xpm)
      gtk_widget_destroy (sess->gui->op_xpm);
   sess->gui->op_xpm = 0;
}

void
fe_set_nick (struct server *serv, char *newnick)
{
   GSList *list = sess_list;
   struct session *sess;
   strcpy (serv->nick, newnick);
   while (list)
   {
      sess = (struct session *) list->data;
      if (sess->server == serv) 
         gtk_label_set_text (GTK_LABEL (sess->gui->nickgad), newnick);
      list = list->next;
   }
}

void
focus_in (GtkWindow * win, GtkWidget * wid, struct session *sess)
{
   if (!sess)
   {
      if (current_tab)
      {
         if (!current_tab->is_shell)
            gtk_widget_grab_focus (current_tab->gui->inputgad);
         else
            gtk_widget_grab_focus (current_tab->gui->textgad);
         if (!prefs.use_server_tab)
            current_tab->server->front_session = current_tab;
      }
   } else
   {
      if (!prefs.use_server_tab)
         sess->server->front_session = sess;
      if (!sess->is_shell)
         gtk_widget_grab_focus (sess->gui->inputgad);
      else
         gtk_widget_grab_focus (sess->gui->textgad);
   }
}

void
show_and_unfocus (GtkWidget * wid)
{
   GTK_WIDGET_UNSET_FLAGS (wid, GTK_CAN_FOCUS);
   gtk_widget_show (wid);
}

static void
maingui_create_textlist (struct session *sess, GtkWidget *leftpane)
{
   sess->gui->textgad = gtk_xtext_new (prefs.indent_pixels*prefs.indent_nicks,
                                       prefs.show_separator);

   gtk_object_set_user_data (GTK_OBJECT (sess->gui->textgad), sess);

   ((GtkXText*)sess->gui->textgad)->double_buffer = prefs.double_buffer;
   ((GtkXText*)sess->gui->textgad)->wordwrap = prefs.wordwrap;
   ((GtkXText*)sess->gui->textgad)->max_auto_indent = prefs.max_auto_indent;
   ((GtkXText*)sess->gui->textgad)->auto_indent = prefs.auto_indent;
   ((GtkXText*)sess->gui->textgad)->thinline = prefs.thin_separator;
   ((GtkXText*)sess->gui->textgad)->max_lines = prefs.max_lines;
   ((GtkXText*)sess->gui->textgad)->error_function = gtkutil_simpledialog;
  
   ((GtkXText*)sess->gui->textgad)->tint_red = prefs.tint_red;
   ((GtkXText*)sess->gui->textgad)->tint_green = prefs.tint_green;
   ((GtkXText*)sess->gui->textgad)->tint_blue = prefs.tint_blue;

  /* if (prefs.timestamp && prefs.indent_nicks)
      ((GtkXText*)sess->gui->textgad)->time_stamp = TRUE; */

   gtk_xtext_set_palette (GTK_XTEXT (sess->gui->textgad), colors);
   gtk_xtext_set_font (GTK_XTEXT (sess->gui->textgad), font_normal, 0);
   gtk_xtext_set_background (GTK_XTEXT (sess->gui->textgad),
                             channelwin_style->bg_pixmap[0],
                             prefs.transparent,
                             prefs.tint);

   gtk_widget_set_usize (sess->gui->textgad,
                         prefs.mainwindow_width - 115,
                         prefs.mainwindow_height - 105);
   gtk_container_add (GTK_CONTAINER (leftpane), sess->gui->textgad);
   show_and_unfocus (sess->gui->textgad);

   sess->gui->vscrollbar = gtk_vscrollbar_new (GTK_XTEXT (sess->gui->textgad)->adj);
   gtk_box_pack_start (GTK_BOX (leftpane), sess->gui->vscrollbar, FALSE, FALSE, 1);
   show_and_unfocus (sess->gui->vscrollbar);
}

static void
gui_new_tab (session *sess)
{
   current_tab = sess;
   if (!prefs.use_server_tab)
      sess->server->front_session = sess;
   fe_set_title (sess);
   if (!sess->is_shell)
      gtk_widget_grab_focus (sess->gui->inputgad);

   if (sess->new_data || sess->nick_said)
   {
      sess->nick_said = FALSE;
      sess->new_data = FALSE;
      gtk_widget_set_rc_style (sess->gui->changad);
   }
}

static void
gui_new_tab_callback (GtkWidget * widget, GtkNotebookPage * nbpage, guint page)
{
   struct session *sess;
   GSList *list = sess_list;

   if (xchat_is_quitting)
      return;

   while (list)
   {
      sess = (struct session *) list->data;
      if (sess->gui->window == nbpage->child)
      {
         gui_new_tab (sess);
         return;
      }
      list = list->next;
   }

   current_tab = 0;
}

static void
gui_mainbook_invalid (GtkWidget * w, GtkWidget * main_window)
{
   main_book = NULL;
   gtk_widget_destroy (main_window);
}

static void
gui_main_window_kill (GtkWidget * win, struct session *sess)
{
   GSList *list;

   xchat_is_quitting = TRUE;

   /* see if there's any non-tab windows left */
   list = sess_list;
   while (list)
   {
      sess = (session *) list->data;
      if (!sess->is_tab)
      {
         xchat_is_quitting = FALSE;
         break;
      }
      list = list -> next;
   }

   main_window = 0;
   current_tab = 0;
}

static void
maingui_userlist_selected (GtkWidget *clist, gint row, gint column,
                           GdkEventButton *even)
{
   gtk_clist_unselect_all (GTK_CLIST (clist));
}

void
maingui_set_tab_pos (int pos)
{
   gtk_notebook_set_show_tabs ((GtkNotebook*) main_book, TRUE);
   switch (pos)
   {
   case 0:
      gtk_notebook_set_tab_pos ((GtkNotebook *) main_book, GTK_POS_BOTTOM);
      break;
   case 1:
      gtk_notebook_set_tab_pos ((GtkNotebook *) main_book, GTK_POS_TOP);
      break;
   case 2:
      gtk_notebook_set_tab_pos ((GtkNotebook *) main_book, GTK_POS_LEFT);
      break;
   case 3:
      gtk_notebook_set_tab_pos ((GtkNotebook *) main_book, GTK_POS_RIGHT);
      break;
   case 4:
      gtk_notebook_set_show_tabs ((GtkNotebook*) main_book, FALSE);
      break;
   }
}

static void
gui_make_tab_window (struct session *sess)
{
   GtkWidget *main_box;
 
   if (!main_window)
   {
      current_tab = 0;
      main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_widget_realize (main_window);
      gtk_signal_connect ((GtkObject *) main_window, "destroy",
                          GTK_SIGNAL_FUNC (gui_main_window_kill), sess);
      gtk_signal_connect ((GtkObject *) main_window, "focus_in_event",
                          GTK_SIGNAL_FUNC (focus_in), 0);
      gtk_window_set_policy ((GtkWindow *) main_window, TRUE, TRUE, FALSE);

      main_box = gtk_vbox_new (0, 0);

      gtk_container_add (GTK_CONTAINER (main_window), main_box);

      gtk_widget_show (main_box);

      main_book = gtk_notebook_new ();

      maingui_set_tab_pos (prefs.tabs_position);

      gtk_notebook_set_scrollable ((GtkNotebook *) main_book, TRUE);
      gtk_signal_connect ((GtkObject *) main_book, "switch_page",
                          GTK_SIGNAL_FUNC (gui_new_tab_callback), 0);
      gtk_signal_connect ((GtkObject *) main_book, "destroy",
                          GTK_SIGNAL_FUNC (gui_mainbook_invalid), main_window);
    
      gtk_container_add (GTK_CONTAINER (main_box), main_book);
      gtk_widget_show (main_book);
   }
}

static void
maingui_init_styles (GtkStyle * style)
{
   normaltab_style = gtk_style_new ();
   normaltab_style->font = style->font;

   redtab_style = gtk_style_new ();
   redtab_style->font = style->font;
   memcpy (redtab_style->fg, &colors[4], sizeof (GdkColor));

   bluetab_style = gtk_style_new ();
   bluetab_style->font = style->font;
   memcpy (bluetab_style->fg, &colors[12], sizeof (GdkColor));
}

void
create_window (struct session *sess)
{
   GtkWidget *leftpane, *rightpane;
   GtkWidget *vbox, *tbox, *bbox, *nlbox, *wid;
   int justopened = FALSE;
  
   if (!sess->server->front_session)
      sess->server->front_session = sess;

   if (prefs.tabchannels)
   {
      sess->is_tab = TRUE;
      if (!main_window)
      {
         justopened = TRUE;
         gui_make_tab_window (sess);
      }
      sess->gui->window = gtk_hbox_new (0, 0);
      gtk_signal_connect ((GtkObject *) sess->gui->window, "destroy",
                          GTK_SIGNAL_FUNC (gtk_kill_session_callback), sess);
      if (!current_tab)
      {
         current_tab = sess;
         fe_set_title (sess);
      }
   } else
   {
      sess->gui->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      fe_set_title (sess);
      gtk_widget_realize (sess->gui->window);
      gtk_signal_connect ((GtkObject *) sess->gui->window, "destroy",
                          GTK_SIGNAL_FUNC (gtk_kill_session_callback), sess);
      gtk_signal_connect ((GtkObject *) sess->gui->window, "focus_in_event",
                          GTK_SIGNAL_FUNC (focus_in), sess);
      gtk_window_set_policy ((GtkWindow *) sess->gui->window, TRUE, TRUE, FALSE);
   }

   palette_alloc (sess->gui->window);

   vbox = gtk_vbox_new (FALSE, 0);
   sess->gui->vbox = vbox;
   gtk_container_set_border_width ((GtkContainer *) vbox, 2);
   if (!prefs.tabchannels)
   {
      gtk_container_add (GTK_CONTAINER (sess->gui->window), vbox);

   } else
      gtk_container_add ((GtkContainer *) sess->gui->window, vbox);
   gtk_widget_show (vbox);

   tbox = gtk_hbox_new (FALSE, 0);
   sess->gui->tbox = tbox;
   gtk_container_set_border_width (GTK_CONTAINER (tbox), 0);
   gtk_box_pack_start (GTK_BOX (vbox), tbox, FALSE, TRUE, 2);
   gtk_widget_show (tbox);
   
   if (!prefs.tabchannels)
   {
      sess->gui->changad = gtk_label_new ("<none>");
      gtk_box_pack_start (GTK_BOX (tbox), sess->gui->changad, FALSE, FALSE, 5);
      gtk_widget_show (sess->gui->changad);
   }

   sess->gui->topicgad = gtk_entry_new ();
   gtk_entry_set_editable (GTK_ENTRY (sess->gui->topicgad), FALSE);
   gtk_container_add (GTK_CONTAINER (tbox), sess->gui->topicgad);
   gtk_widget_show (sess->gui->topicgad);
   
   leftpane = gtk_hbox_new (FALSE, 0);
   gtk_widget_show (leftpane);

   rightpane = gtk_hbox_new (FALSE, 8);
   gtk_widget_show (rightpane);
   sess->gui->userlistbox = rightpane;

   wid = gtk_hbox_new (0, 2);
   gtk_container_add (GTK_CONTAINER (vbox), wid);
   gtk_widget_show (wid);
   gtk_container_add (GTK_CONTAINER (wid), leftpane);
   gtk_box_pack_end (GTK_BOX (wid), rightpane, 0, 0, 0);

   sess->gui->nl_box = nlbox = gtk_vbox_new (FALSE, 2);
   gtk_container_add (GTK_CONTAINER (rightpane), nlbox);
   gtk_widget_show (nlbox);

   wid = gtk_frame_new (0);
   gtk_box_pack_start (GTK_BOX (nlbox), wid, 0, 0, 0);
   gtk_widget_show (wid);

   sess->gui->namelistinfo = gtk_label_new (" ");
   gtk_container_add (GTK_CONTAINER (wid), sess->gui->namelistinfo);
   gtk_widget_show (sess->gui->namelistinfo);

   maingui_create_textlist (sess, leftpane);
   sess->gui->leftpane = leftpane;

   sess->gui->namelistgad = gtkutil_clist_new (1, 0, nlbox, GTK_POLICY_AUTOMATIC,
               maingui_userlist_selected, sess, 0, 0, GTK_SELECTION_MULTIPLE);
   
   gtk_clist_set_column_width (GTK_CLIST (sess->gui->namelistgad), 0, 10);
   gtk_widget_set_usize (sess->gui->namelistgad->parent, 115, 0);
 
   bbox = gtk_hbox_new (FALSE, 0);
   gtk_container_set_border_width (GTK_CONTAINER (bbox), 0);
   gtk_box_pack_end (GTK_BOX (vbox), bbox, FALSE, TRUE, 2);
   gtk_widget_show (bbox);

   sess->gui->op_box = gtk_hbox_new (0, 0);
   gtk_box_pack_start (GTK_BOX (bbox), sess->gui->op_box, FALSE, FALSE, 2);
   gtk_widget_show (sess->gui->op_box);

   sess->gui->nickgad = gtk_label_new (sess->server->nick);
   gtk_box_pack_start (GTK_BOX (bbox), sess->gui->nickgad, FALSE, FALSE, 4);
   gtk_widget_show (sess->gui->nickgad);

   sess->gui->inputgad = gtk_entry_new_with_max_length (2048);
   gtk_container_add (GTK_CONTAINER (bbox), sess->gui->inputgad);
   gtk_signal_connect (GTK_OBJECT (sess->gui->inputgad), "activate",
                       GTK_SIGNAL_FUNC (handle_inputgad), sess);
   gtk_signal_connect (GTK_OBJECT (sess->gui->inputgad), "key_press_event",
                       GTK_SIGNAL_FUNC (key_handle_key_press), sess);
   if (prefs.style_inputbox)
      gtk_widget_set_style (sess->gui->inputgad, inputgad_style);
   gtk_widget_show (sess->gui->inputgad);
   if (prefs.newtabstofront || justopened)
      gtk_widget_grab_focus (sess->gui->inputgad);

   gtk_widget_show (sess->gui->window);

   if (prefs.tabchannels)
   {
      sess->gui->changad = gtk_label_new ("<none>");
      gtk_widget_show (sess->gui->changad);

      gtk_notebook_append_page (GTK_NOTEBOOK (main_book), sess->gui->window, sess->gui->changad);
      gtk_widget_realize (sess->gui->textgad);

      if (justopened)
      {
         gtk_widget_show (main_window);
         if (prefs.mainwindow_left || prefs.mainwindow_top)
            gdk_window_move (main_window->window,
                  prefs.mainwindow_left,
                  prefs.mainwindow_top);
      }

      if (!normaltab_style)
         maingui_init_styles (gtk_widget_get_style (sess->gui->changad));

      /* make switching tabs super smooth! */
      gtk_widget_realize (rightpane);
      gdk_window_set_background (rightpane->window,
                                 &rightpane->style->bg[GTK_STATE_NORMAL]);
      gdk_window_set_back_pixmap (main_book->window, 0, 0);
   } else
   {
      if (!normaltab_style)
         maingui_init_styles (gtk_widget_get_style (sess->gui->changad));

      if (prefs.mainwindow_left || prefs.mainwindow_top)
         gdk_window_move (sess->gui->window->window,
                prefs.mainwindow_left,
                prefs.mainwindow_top);
   }

}

static int
maingui_box_close (GtkWidget *wid, struct relink_data *rld)
{
	if (rld->win)
	   gtk_widget_destroy (rld->win);
	if (rld->hbox)
	   gtk_widget_destroy (rld->hbox);
	
	if (rld->close_callback)
	   rld->close_callback (NULL, rld->userdata);
   
   free (rld->ltitle);
	g_free (rld);
	return 1;
}

GtkWidget *
maingui_new_tab (char *title, char *name, void *close_callback, void *userdata)
{
   /* int page; */
   GtkWidget *wid, *box, *label, *topbox;
   struct relink_data *rld = g_new0 (struct relink_data, 1);
   char *buf;

   topbox = gtk_hbox_new (0, 0);
   gtk_widget_show (topbox);

   box = gtk_vbox_new (0, 0);
   gtk_box_pack_start (GTK_BOX(box), topbox, 0, 0, 0);
   gtk_widget_show (box);

   wid = gtk_label_new (title);
   gtk_container_add (GTK_CONTAINER(topbox), wid);
   gtk_widget_show (wid);
   
   gtk_signal_connect (GTK_OBJECT(box), "destroy",
		       GTK_SIGNAL_FUNC(maingui_box_close), rld);

   buf = malloc (strlen(name) + 3);
   sprintf (buf, "(%s)", name);
   label = gtk_label_new (buf);
   free (buf);
   gtk_widget_show (label);

   gtk_notebook_append_page (GTK_NOTEBOOK (main_book), box, label);

   rld->win = NULL;
   rld->stitle = name;
   rld->ltitle = strdup (title);
   rld->close_callback = close_callback;
   rld->userdata = userdata;
   
   gtk_object_set_user_data (GTK_OBJECT (box), rld);
   return box;
}

gint
gtk_kill_session_callback (GtkWidget *win, struct session *sess)
{
   kill_session_callback (sess);
   return TRUE;
}

void
fe_session_callback (struct session *sess)
{
   if (sess->gui->bar)
   {
      fe_progressbar_end (sess);
      sess->server->connecting = TRUE;
   }

   if (sess->is_tab && main_book)
   {
      if (gtk_notebook_get_nth_page (GTK_NOTEBOOK (main_book), 0) == NULL)
         gtk_widget_destroy (main_book);
   }
}

void
handle_inputgad (GtkWidget * igad, struct session *sess)
{
   char *cmd = gtk_entry_get_text (GTK_ENTRY (igad));

   if (cmd[0] == 0)
      return;

   handle_multiline (sess, cmd, TRUE, FALSE);

   gtk_entry_set_text (GTK_ENTRY (igad), "");
}
