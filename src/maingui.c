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
#include "xchat.h"
#include "fe.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#include <gdk_imlib.h>
#include <gdk/gdkkeysyms.h>
#include "xtext.h"

GtkWidget *main_window = 0;
GtkWidget *main_book;
GtkStyle *normaltab_style = 0;
GtkStyle *redtab_style;
GtkStyle *bluetab_style;
GtkStyle *inputgad_style;

extern session_t *current_tab; 
extern struct xchatprefs prefs;
extern GtkStyle *channelwin_style;
extern GdkFont *font_normal;
extern gint xchat_is_quitting;

extern int handle_multiline (session_t *sess, char *cmd, int history, int nocommand);
extern int kill_session_callback (void);
extern gint gtk_kill_session_callback (GtkWidget *, void *blah);
extern void clear_user_list (session_t *sess);
extern void handle_inputgad (GtkWidget * igad, void *blah);
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
fe_set_title (void)
{
   char tbuf[200];
   if (!server->connected)
      strcpy (tbuf, "NF-Chat [" VERSION "]");
   else
     {
       if (session->channel[0] == 0 || session->is_server)
         snprintf (tbuf, sizeof tbuf, "NF-Chat [" VERSION "]: %s", server->servername);
       else
         snprintf (tbuf, sizeof tbuf, "NF-Chat [" VERSION "]: %s / %s", server->servername, session->channel);
     }
   if (session->is_tab)
     {
       if (!main_window)
         return;
       if (current_tab == sess)
         gtk_window_set_title ((GtkWindow *) main_window, tbuf);
     } else
       gtk_window_set_title ((GtkWindow *) session->gui->window, tbuf);
}

void
fe_set_channel (void)
{
   gtk_label_set_text (GTK_LABEL (session->gui->changad), session->channel);
}

void
fe_clear_channel (void)
{
   gtk_entry_set_text (GTK_ENTRY (session->gui->topicgad), "");
   gtk_label_set_text (GTK_LABEL (session->gui->namelistinfo), " ");
   gtk_label_set_text (GTK_LABEL (session->gui->changad), "<none>");

   clear_user_list (session);

   if (session->gui->op_xpm)
      gtk_widget_destroy (session->gui->op_xpm);
   session->gui->op_xpm = 0;
}

void
fe_set_nick (char *newnick)
{
   strcpy (server->nick, newnick);
   gtk_label_set_text (GTK_LABEL (session->gui->nickgad), newnick);
}

static void
focus_in (GtkWindow * win, GtkWidget * wid, void *blah)
{
  gtk_widget_grab_focus (session->gui->inputgad);
}

static void
palette_alloc (GtkWidget *widget)
{
  int i;
  
  if (!colors[0].pixel)        /* don't do it again */
    for (i = 0; i < 20; i++)
      {
	colors[i].pixel = (gulong) ((colors[i].red & 0xff00) * 256 + (colors[i].green & 0xff00) + (colors[i].blue & 0xff00) / 256);
	if (!gdk_color_alloc (gtk_widget_get_colormap (widget), &colors[i]))
	  fprintf (stderr, "*** NF-CHAT: cannot alloc colors\n");
      }
}

static void
show_and_unfocus (GtkWidget * wid)
{
   GTK_WIDGET_UNSET_FLAGS (wid, GTK_CAN_FOCUS);
   gtk_widget_show (wid);
}

static void
maingui_create_textlist (GtkWidget *leftpane)
{
   session->gui->textgad = gtk_xtext_new (prefs.indent_pixels);

   gtk_object_set_user_data (GTK_OBJECT (session->gui->textgad), session);

   ((GtkXText*)session->gui->textgad)->max_auto_indent = prefs.max_auto_indent;
   ((GtkXText*)session->gui->textgad)->max_lines = prefs.max_lines;
  
   ((GtkXText*)session->gui->textgad)->tint_red = prefs.tint_red;
   ((GtkXText*)session->gui->textgad)->tint_green = prefs.tint_green;
   ((GtkXText*)session->gui->textgad)->tint_blue = prefs.tint_blue;

   gtk_xtext_set_palette (GTK_XTEXT (session->gui->textgad), colors);
   gtk_xtext_set_font (GTK_XTEXT (session->gui->textgad), font_normal, 0);
   gtk_xtext_set_background (GTK_XTEXT (session->gui->textgad), channelwin_style->bg_pixmap[0], prefs.transparent, prefs.tint);

   gtk_widget_set_usize (session->gui->textgad, prefs.mainwindow_width - 115, prefs.mainwindow_height - 105);
   gtk_container_add (GTK_CONTAINER (leftpane), session->gui->textgad);
   show_and_unfocus (session->gui->textgad);

   session->gui->vscrollbar = gtk_vscrollbar_new (GTK_XTEXT (session->gui->textgad)->adj);
   gtk_box_pack_start (GTK_BOX (leftpane), session->gui->vscrollbar, FALSE, FALSE, 1);
   show_and_unfocus (session->gui->vscrollbar);
}

static void
gui_new_tab ()
{
   fe_set_title ();
   gtk_widget_grab_focus (session->gui->inputgad);

   if (session->new_data || session->nick_said)
   {
      session->nick_said = FALSE;
      session->new_data = FALSE;
      gtk_widget_set_rc_style (session->gui->changad);
   }
}

static void
gui_new_tab_callback (GtkWidget * widget, GtkNotebookPage * nbpage, guint page)
{
   if (xchat_is_quitting)
      return;
   if (session->gui->window == nbpage->child)
     {
       gui_new_tab ();
       return;
     }
}

static void
gui_mainbook_invalid (GtkWidget * w, GtkWidget * main_window)
{
   main_book = NULL;
   gtk_widget_destroy (main_window);
}

static void
gui_main_window_kill (GtkWidget * win, void *blah)
{
   xchat_is_quitting = TRUE;
   if (!sess->is_tab)
     {
       xchat_is_quitting = FALSE;
       break;
     }
   main_window = 0;
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
gui_make_tab_window ()
{
   GtkWidget *main_box;
 
   if (!main_window)
   {
      current_tab = 0;
      main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_widget_realize (main_window);
      gtk_signal_connect ((GtkObject *) main_window, "destroy",
                          GTK_SIGNAL_FUNC (gui_main_window_kill), session);
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
create_window (void)
{
   GtkWidget *leftpane, *rightpane;
   GtkWidget *vbox, *tbox, *bbox, *nlbox, *wid;
   int justopened = FALSE;
  
   session->gui->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   
   fe_set_title ();
   gtk_widget_realize (session->gui->window);
   gtk_signal_connect ((GtkObject *) session->gui->window, "destroy", GTK_SIGNAL_FUNC (gtk_kill_session_callback), session);
   gtk_signal_connect ((GtkObject *) session->gui->window, "focus_in_event", GTK_SIGNAL_FUNC (focus_in), session);
   gtk_window_set_policy ((GtkWindow *) session->gui->window, TRUE, TRUE, FALSE);
   
   palette_alloc (session->gui->window);
   
   vbox = gtk_vbox_new (FALSE, 0);
   session->gui->vbox = vbox;
   gtk_container_set_border_width ((GtkContainer *) vbox, 2);
   gtk_container_add (GTK_CONTAINER (session->gui->window), vbox);

   gtk_widget_show (vbox);

   tbox = gtk_hbox_new (FALSE, 0);
   session->gui->tbox = tbox;
   gtk_container_set_border_width (GTK_CONTAINER (tbox), 0);
   gtk_box_pack_start (GTK_BOX (vbox), tbox, FALSE, TRUE, 2);
   gtk_widget_show (tbox);
   
   session->gui->changad = gtk_label_new ("<none>");
   gtk_box_pack_start (GTK_BOX (tbox), session->gui->changad, FALSE, FALSE, 5);
   gtk_widget_show (session->gui->changad);

   session->gui->topicgad = gtk_entry_new ();
   gtk_entry_set_editable (GTK_ENTRY (session->gui->topicgad), FALSE);
   gtk_container_add (GTK_CONTAINER (tbox), session->gui->topicgad);
   gtk_widget_show (session->gui->topicgad);
   
   leftpane = gtk_hbox_new (FALSE, 0);
   gtk_widget_show (leftpane);

   rightpane = gtk_hbox_new (FALSE, 8);
   gtk_widget_show (rightpane);
   session->gui->userlistbox = rightpane;

   wid = gtk_hbox_new (0, 2);
   gtk_container_add (GTK_CONTAINER (vbox), wid);
   gtk_widget_show (wid);
   gtk_container_add (GTK_CONTAINER (wid), leftpane);
   gtk_box_pack_end (GTK_BOX (wid), rightpane, 0, 0, 0);

   session->gui->nl_box = nlbox = gtk_vbox_new (FALSE, 2);
   gtk_container_add (GTK_CONTAINER (rightpane), nlbox);
   gtk_widget_show (nlbox);

   wid = gtk_frame_new (0);
   gtk_box_pack_start (GTK_BOX (nlbox), wid, 0, 0, 0);
   gtk_widget_show (wid);

   session->gui->namelistinfo = gtk_label_new (" ");
   gtk_container_add (GTK_CONTAINER (wid), session->gui->namelistinfo);
   gtk_widget_show (session->gui->namelistinfo);

   maingui_create_textlist (leftpane);
   sess->gui->leftpane = leftpane;

   sess->gui->namelistgad = gtkutil_clist_new (1, 0, nlbox, GTK_POLICY_AUTOMATIC, maingui_userlist_selected, session, 0, 0, GTK_SELECTION_MULTIPLE);
   
   gtk_clist_set_column_width (GTK_CLIST (session->gui->namelistgad), 0, 10);
   gtk_widget_set_usize (session->gui->namelistgad->parent, 115, 0);
 
   bbox = gtk_hbox_new (FALSE, 0);
   gtk_container_set_border_width (GTK_CONTAINER (bbox), 0);
   gtk_box_pack_end (GTK_BOX (vbox), bbox, FALSE, TRUE, 2);
   gtk_widget_show (bbox);

   session->gui->op_box = gtk_hbox_new (0, 0);
   gtk_box_pack_start (GTK_BOX (bbox), session->gui->op_box, FALSE, FALSE, 2);
   gtk_widget_show (session->gui->op_box);

   session->gui->nickgad = gtk_label_new (server->nick);
   gtk_box_pack_start (GTK_BOX (bbox), session->gui->nickgad, FALSE, FALSE, 4);
   gtk_widget_show (session->gui->nickgad);

   session->gui->inputgad = gtk_entry_new_with_max_length (2048);
   gtk_container_add (GTK_CONTAINER (bbox), session->gui->inputgad);
   gtk_signal_connect (GTK_OBJECT (session->gui->inputgad), "activate", GTK_SIGNAL_FUNC (handle_inputgad), session);
   gtk_signal_connect (GTK_OBJECT (session->gui->inputgad), "key_press_event", GTK_SIGNAL_FUNC (key_handle_key_press), session);
   gtk_widget_set_style (session->gui->inputgad, inputgad_style);
   gtk_widget_show (session->gui->inputgad);
   if (justopened)
      gtk_widget_grab_focus (session->gui->inputgad);

   gtk_widget_show (session->gui->window);

   if (!normaltab_style)
     maingui_init_styles (gtk_widget_get_style (session->gui->changad));
   if (prefs.mainwindow_left || prefs.mainwindow_top)
     gdk_window_move (session->gui->window->window, prefs.mainwindow_left, prefs.mainwindow_top);
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
gtk_kill_session_callback (GtkWidget *win, void *blah)
{
   kill_session_callback ();
   return TRUE;
}

void
fe_session_callback (void)
{
   if (session->gui->bar)
   {
      fe_progressbar_end ();
      server->connecting = TRUE;
   }
   if (session->is_tab && main_book && gtk_notebook_get_nth_page (GTK_NOTEBOOK (main_book), 0) == NULL)
     gtk_widget_destroy (main_book);
}

void
handle_inputgad (GtkWidget * igad, void *blah)
{
   char *cmd = gtk_entry_get_text (GTK_ENTRY (igad));

   if (cmd[0] == 0)
      return;

   handle_multiline (session, cmd, TRUE, FALSE);

   gtk_entry_set_text (GTK_ENTRY (igad), "");
}
