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
#include "fe-gtk.h"
#include <gdk_imlib.h>
#include <gdk/gdkkeysyms.h>
#include "xtext.h"

GtkStyle *inputgad_style;

extern struct xchatprefs prefs;
extern GtkStyle *channelwin_style;
extern GdkFont *font_normal;
extern gint xchat_is_quitting;

extern int handle_multiline (char *cmd);
extern int kill_session_callback (void);
extern gint gtk_kill_session_callback (GtkWidget *, void *blah);
extern void clear_user_list (void);
extern void handle_inputgad (GtkWidget * igad, void *blah);
extern void fe_progressbar_end (void);
extern GdkFont *my_font_load (char *fontname);

extern int key_handle_key_press (GtkWidget *, GdkEventKey *, gpointer);

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
   {0, 0xeeee, 0xeeee, 0xeeee}, /* 15 light grey */
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
   else if (session->channel[0] == 0)
     snprintf (tbuf, sizeof tbuf, "NF-Chat [" VERSION "]: %s", server->servername);
   else
     snprintf (tbuf, sizeof tbuf, "NF-Chat [" VERSION "]: %s / %s", server->servername, session->channel);
   gtk_window_set_title ((GtkWindow *) session->gui->window, tbuf);
}

void
fe_clear_channel (void)
{
   gtk_entry_set_text (GTK_ENTRY (session->gui->topicgad), "");
   gtk_label_set_text (GTK_LABEL (session->gui->namelistinfo), " ");

   clear_user_list ();

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
focus_in (GtkWidget * win, GtkWidget * wid, void *blah)
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

static GtkStyle *
namelist_style (void)
{
  GtkStyle *style = gtk_style_new ();
  
  style->base[GTK_STATE_INSENSITIVE] = style->base[GTK_STATE_NORMAL];
  style->bg[GTK_STATE_INSENSITIVE] = style->bg[GTK_STATE_NORMAL];
  style->fg[GTK_STATE_INSENSITIVE] = style->fg[GTK_STATE_NORMAL];
  style->light[GTK_STATE_INSENSITIVE] = style->light[GTK_STATE_NORMAL];
  style->dark[GTK_STATE_INSENSITIVE] = style->dark[GTK_STATE_NORMAL];
  style->text[GTK_STATE_INSENSITIVE] = style->text[GTK_STATE_NORMAL];

  return (style);
}

static GtkStyle *
topic_style (void)
{
  GtkStyle *style = gtk_style_new ();

  style->font = my_font_load ("-etl-fixed-medium-r-normal-*-*-140-*-*-c-*-iso8859-9");

  style->base[GTK_STATE_INSENSITIVE] = style->base[GTK_STATE_NORMAL];
  style->bg[GTK_STATE_INSENSITIVE] = style->bg[GTK_STATE_NORMAL];
  style->fg[GTK_STATE_INSENSITIVE] = style->fg[GTK_STATE_NORMAL];
  style->light[GTK_STATE_INSENSITIVE] = style->light[GTK_STATE_NORMAL];
  style->dark[GTK_STATE_INSENSITIVE] = style->dark[GTK_STATE_NORMAL];
  style->text[GTK_STATE_INSENSITIVE] = style->text[GTK_STATE_NORMAL];

  return (style);
}

static GtkWidget *
gtkutil_clist_new (GtkWidget * box, int policy)
{
   GtkWidget *clist, *win;

   win = gtk_scrolled_window_new (0, 0);
   gtk_container_add (GTK_CONTAINER (box), win);
   gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (win), GTK_POLICY_AUTOMATIC, policy);
   gtk_widget_show (win);

   clist = gtk_clist_new (1);
   
   gtk_clist_column_titles_passive (GTK_CLIST (clist));
   gtk_container_add (GTK_CONTAINER (win), clist);
   gtk_widget_show (clist);

   return clist;
}

void
maingui_configure (GtkWidget *unused)
{
  gtk_widget_queue_draw (session->gui->textgad);
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
   gtk_signal_connect_object ((GtkObject *) session->gui->window, "configure_event", GTK_SIGNAL_FUNC (maingui_configure), 0);
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
   
   session->gui->topicgad = gtk_entry_new ();
   gtk_entry_set_editable (GTK_ENTRY (session->gui->topicgad), FALSE);
   gtk_container_add (GTK_CONTAINER (tbox), session->gui->topicgad);
   gtk_widget_set_sensitive ((GtkWidget *) session->gui->topicgad, FALSE);
   gtk_widget_set_style (session->gui->topicgad, topic_style ());
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
   session->gui->leftpane = leftpane;

   session->gui->namelistgad = gtkutil_clist_new (nlbox, GTK_POLICY_AUTOMATIC);
   gtk_widget_set_sensitive ((GtkWidget *) session->gui->namelistgad, FALSE);
   gtk_clist_set_column_width (GTK_CLIST (session->gui->namelistgad), 0, 10);
   gtk_widget_set_usize (session->gui->namelistgad->parent, 115, 0);
   gtk_widget_set_style (session->gui->namelistgad, namelist_style ());
   bbox = gtk_hbox_new (FALSE, 0);
   gtk_container_set_border_width (GTK_CONTAINER (bbox), 0);
   gtk_box_pack_end (GTK_BOX (vbox), bbox, FALSE, TRUE, 2);
   gtk_widget_show (bbox);

   session->gui->op_box = gtk_hbox_new (0, 0);
   gtk_box_pack_start (GTK_BOX (bbox), session->gui->op_box, FALSE, FALSE, 2);

   session->gui->nickgad = gtk_label_new (server->nick);
   gtk_box_pack_start (GTK_BOX (bbox), session->gui->nickgad, FALSE, FALSE, 4);
   gtk_widget_show (session->gui->nickgad);

   session->gui->inputgad = gtk_entry_new_with_max_length (2048);
   gtk_container_add (GTK_CONTAINER (bbox), session->gui->inputgad);
   gtk_signal_connect (GTK_OBJECT (session->gui->inputgad), "activate", GTK_SIGNAL_FUNC (handle_inputgad), session);
   gtk_signal_connect (GTK_OBJECT (session->gui->inputgad), "key-press-event", GTK_SIGNAL_FUNC (key_handle_key_press), session);
   gtk_widget_set_style (session->gui->inputgad, inputgad_style);
   gtk_widget_show (session->gui->inputgad);
   if (justopened)
      gtk_widget_grab_focus (session->gui->inputgad);

   gtk_widget_show (session->gui->window);

   if (prefs.mainwindow_left || prefs.mainwindow_top)
     gdk_window_move (session->gui->window->window, prefs.mainwindow_left, prefs.mainwindow_top);
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
}

void
handle_inputgad (GtkWidget * igad, void *blah)
{
   char *cmd = gtk_entry_get_text (GTK_ENTRY (igad));

   if (cmd[0] == 0)
      return;

   handle_multiline (cmd);

   gtk_entry_set_text (GTK_ENTRY (igad), "");
}
