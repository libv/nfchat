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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "../common/xchat.h"
#include "../common/dcc.h"
#include "../common/fe.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#include "menu.h"
#include "../common/util.h"
#include "xtext.h"

extern GSList *sess_list;
extern struct xchatprefs prefs;
extern GdkFont *dialog_font_normal;
extern GdkFont *dialog_font_bold;
extern GdkColor colors[];
extern GtkStyle *dialogwin_style;
extern GtkStyle *inputgad_style;
extern GtkWidget *main_window;
extern GtkWidget *main_book;
extern GtkStyle *greentab_style;
extern struct session *current_tab;
extern struct session *menu_sess;
extern GtkStyle *normaltab_style;

extern void maingui_configure (GtkWidget *unused);
extern int maingui_word_check (GtkXText *xtext, char *word);
extern void maingui_word_clicked (GtkXText *xtext, char *word, GdkEventButton *even, session *sess);
extern void maingui_moveright (GtkWidget *wid);
extern void maingui_moveleft (GtkWidget *wid);
extern struct session *find_dialog (struct server *serv, char *nick);
extern void X_session (GtkWidget * button, struct session *sess);
extern int maingui_pagetofront (int page);
extern void show_and_unfocus (GtkWidget * wid);
extern void term_change_pos (GtkWidget * widget);
extern void add_tip (GtkWidget * wid, char *text);
extern int textgad_get_focus_cb (GtkWidget * wid, GdkEventKey * event, struct session *sess);
extern int tcp_send_len (struct server *serv, char *buf, int len);
extern int tcp_send (struct server *serv, char *buf);
extern int handle_command (char *cmd, struct session *sess, int history, int);
extern unsigned char *strip_color (unsigned char *text);
extern int open_log_file (char *servname, char *channame);
struct session *new_dialog (struct session *sess);
extern void dcc_send (struct session *sess, char *tbuf, char *to, char *file);
extern int relink_window (GtkWidget * w, struct session *sess);
extern void handle_inputgad (GtkWidget * igad, struct session *sess);
extern gint gtk_kill_session_callback (GtkWidget * win, struct session *sess);
extern void gui_new_tab (GtkWidget * widget, GtkNotebookPage * nbpage, guint page);
extern void focus_in (GtkWindow * win, GtkWidget * wid, struct session *sess);
extern void maingui_textgad_clicked_gtk (GtkWidget * wid, GdkEventButton * even, struct session *sess);
extern GtkStyle *my_widget_get_style (char *bg_pic);
int key_handle_key_press (GtkWidget, GdkEventKey, gpointer);


void
fe_change_nick (struct server *serv, char *nick, char *newnick)
{
   struct session *sess = find_dialog (serv, nick);
   if (sess)
   {
      strcpy (sess->channel, newnick);
      if (!sess->is_tab)
         gtk_window_set_title (GTK_WINDOW (sess->gui->window), newnick);
   }
}

static void
dialog_whois (GtkWidget *wid, struct session *sess)
{
   char *tbuf = malloc (strlen (sess->channel) + 9);
   sprintf (tbuf, "WHOIS %s\r\n", sess->channel);
   tcp_send (sess->server, tbuf);
   free (tbuf);
}

static void
dialog_send (GtkWidget *wid, struct session *sess)
{
   fe_dcc_send_filereq (sess, sess->channel);
}

static void
dialog_chat (GtkWidget *wid, struct session *sess)
{
   dcc_chat (sess, sess->channel);
}

static void
dialog_ping (GtkWidget *wid, struct session *sess)
{
   char *cmd = malloc (strlen (sess->channel) + 7);
   sprintf (cmd, "/ping %s", sess->channel);
   handle_command (cmd, sess, FALSE, FALSE);
   free (cmd);
}

static void
dialog_clear (GtkWidget *wid, struct session *sess)
{
   fe_text_clear (sess);
}

void
open_dialog_window (struct session *sess)
{
   GtkWidget *hbox, *vbox, *wid;
   int page = prefs.privmsgtab;
   struct user *user;

   if (!main_book)
      page = 0;
   if (!page)
   {
      sess->gui->window = gtkutil_window_new (sess->channel, "NF-Chat", 300, 100,
                                              gtk_kill_session_callback, sess, FALSE);
      gtk_signal_connect ((GtkObject *) sess->gui->window, "focus_in_event",
                          GTK_SIGNAL_FUNC (focus_in), sess);
      gtk_window_set_policy ((GtkWindow *) sess->gui->window, TRUE, TRUE, FALSE);
      sess->is_tab = FALSE;
   } else
   {
      sess->gui->window = gtk_hbox_new (0, 0);
      sess->is_tab = TRUE;
   }
   vbox = gtk_vbox_new (0, 0);
   sess->gui->vbox = vbox;
   gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);

   if (!page)
   {
      gtk_container_add (GTK_CONTAINER (sess->gui->window), vbox);
   } else
      gtk_container_add (GTK_CONTAINER (sess->gui->window), vbox);
   if (page)
   {
      sess->gui->changad = gtk_label_new (sess->channel);
      gtk_notebook_append_page (GTK_NOTEBOOK (main_book), sess->gui->window, sess->gui->changad);
   } else
      sess->gui->changad = NULL;
   gtk_widget_show (vbox);

   if (page)
   {
      gtk_signal_connect ((GtkObject *) sess->gui->window, "destroy",
                          GTK_SIGNAL_FUNC (gtk_kill_session_callback), sess);
   }
   hbox = gtk_hbox_new (FALSE, 0);
   gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 2);
   gtk_widget_show (hbox);

   wid = gtk_button_new_with_label ("X");
   gtk_box_pack_start (GTK_BOX (hbox), wid, 0, 0, 0);
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
   GTK_SIGNAL_FUNC (X_session), sess);
   gtk_widget_show (wid);

   add_tip (wid, "Close Dialog");
   wid = gtk_button_new_with_label ("^");
   gtk_box_pack_start (GTK_BOX (hbox), wid, 0, 0, 0);
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
                       GTK_SIGNAL_FUNC (relink_window), sess);

   add_tip (wid, "Link/DeLink this tab");
   gtk_widget_show (wid);

   wid = gtk_button_new_with_label ("<");
   gtk_box_pack_start (GTK_BOX (hbox), wid, 0, 0, 0);
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
			     GTK_SIGNAL_FUNC (maingui_moveleft), 0);
   gtk_widget_show (wid);
   add_tip (wid, "Move tab left");

   wid = gtk_button_new_with_label (">");
   gtk_box_pack_start (GTK_BOX (hbox), wid, 0, 0, 0);
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
			     GTK_SIGNAL_FUNC (maingui_moveright), 0);
   gtk_widget_show (wid);
   add_tip (wid, "Move tab right");

   sess->gui->topicgad = gtk_entry_new ();
   gtk_entry_set_editable ((GtkEntry *) sess->gui->topicgad, FALSE);
   gtk_container_add (GTK_CONTAINER (hbox), sess->gui->topicgad);
   gtk_widget_show (sess->gui->topicgad);

   wid = gtk_button_new_with_label ("WhoIs");
   gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
                       GTK_SIGNAL_FUNC (dialog_whois), sess);
   gtk_widget_show (wid);

   wid = gtk_button_new_with_label ("Send");
   gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
                       GTK_SIGNAL_FUNC (dialog_send), sess);
   gtk_widget_show (wid);

   wid = gtk_button_new_with_label ("Chat");
   gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
                       GTK_SIGNAL_FUNC (dialog_chat), sess);
   gtk_widget_show (wid);

   wid = gtk_button_new_with_label ("Ping");
   gtk_box_pack_end (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
                       GTK_SIGNAL_FUNC (dialog_ping), sess);
   gtk_widget_show (wid);

   wid = gtk_button_new_with_label ("Clear");
   gtk_box_pack_end (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
                       GTK_SIGNAL_FUNC (dialog_clear), sess);
   gtk_widget_show (wid);

   hbox = gtk_hbox_new (FALSE, 0);
   sess->gui->leftpane = hbox;
   gtk_container_add (GTK_CONTAINER (vbox), hbox);
   gtk_widget_show (hbox);

   sess->gui->textgad = gtk_xtext_new (prefs.dialog_indent_pixels*prefs.dialog_indent_nicks, 
                                       prefs.dialog_show_separator);

   gtk_object_set_user_data (GTK_OBJECT (sess->gui->textgad), sess);

   ((GtkXText*)sess->gui->textgad)->double_buffer = prefs.double_buffer;
   ((GtkXText*)sess->gui->textgad)->wordwrap = prefs.dialog_wordwrap;
   ((GtkXText*)sess->gui->textgad)->max_auto_indent = prefs.max_auto_indent;
   ((GtkXText*)sess->gui->textgad)->auto_indent = prefs.auto_indent;
   ((GtkXText*)sess->gui->textgad)->thinline = prefs.thin_separator;
   ((GtkXText*)sess->gui->textgad)->max_lines = prefs.max_lines;
   ((GtkXText*)sess->gui->textgad)->error_function = gtkutil_simpledialog;
   ((GtkXText*)sess->gui->textgad)->urlcheck_function = maingui_word_check;

   ((GtkXText*)sess->gui->textgad)->tint_red = prefs.dialog_tint_red;
   ((GtkXText*)sess->gui->textgad)->tint_green = prefs.dialog_tint_green;
   ((GtkXText*)sess->gui->textgad)->tint_blue = prefs.dialog_tint_blue;

   if (prefs.timestamp && prefs.dialog_indent_nicks)
      ((GtkXText*)sess->gui->textgad)->time_stamp = TRUE;

   gtk_xtext_set_palette (GTK_XTEXT (sess->gui->textgad), colors);
   gtk_xtext_set_font (GTK_XTEXT (sess->gui->textgad), dialog_font_normal, 0);
   gtk_xtext_set_background (GTK_XTEXT (sess->gui->textgad),
                             dialogwin_style->bg_pixmap[0], 
                             prefs.dialog_transparent,
                             prefs.dialog_tint);

   gtk_container_add (GTK_CONTAINER (hbox), sess->gui->textgad);
   show_and_unfocus (sess->gui->textgad);

   sess->gui->vscrollbar = gtk_vscrollbar_new (GTK_XTEXT (sess->gui->textgad)->adj);
   gtk_box_pack_start (GTK_BOX (hbox), sess->gui->vscrollbar, FALSE, FALSE, 1);
   show_and_unfocus (sess->gui->vscrollbar);

   if (!sess->is_tab)
      gtk_signal_connect_object (GTK_OBJECT (sess->gui->window),
                                 "configure_event",
                                 GTK_SIGNAL_FUNC (maingui_configure),
                                 GTK_OBJECT (sess->gui->textgad));

   gtk_signal_connect (GTK_OBJECT (sess->gui->textgad), "word_click",
                       GTK_SIGNAL_FUNC (maingui_word_clicked), sess);

   sess->gui->inputgad = gtk_entry_new_with_max_length (2048);
   gtk_box_pack_start (GTK_BOX (vbox), sess->gui->inputgad, FALSE, FALSE, 2);
   gtk_signal_connect (GTK_OBJECT (sess->gui->inputgad), "activate",
                       GTK_SIGNAL_FUNC (handle_inputgad), sess);
   gtk_signal_connect (GTK_OBJECT (sess->gui->inputgad), "key_press_event",
                       GTK_SIGNAL_FUNC (key_handle_key_press), sess);
   if (prefs.style_inputbox)
      gtk_widget_set_style (sess->gui->inputgad, inputgad_style);
   gtk_widget_show (sess->gui->inputgad);

   gtk_widget_show (sess->gui->window);

   if (page)
   {
      if (prefs.newtabstofront)
      {
         page = gtk_notebook_page_num (GTK_NOTEBOOK (main_book), sess->gui->window);
         maingui_pagetofront (page);

         gtk_widget_grab_focus (sess->gui->inputgad);
         current_tab = sess;
         menu_sess = sess;
         if (!prefs.use_server_tab)
            sess->server->front_session = sess;
         sess->nick_said = FALSE;
         sess->new_data = FALSE;
         gtk_widget_set_style (sess->gui->changad, normaltab_style);
      } else
         sess->new_data = TRUE;
   }

   fe_set_title (sess);

   user = find_name_global (sess->server, sess->channel);
   if (user)
   {
      if (user->hostname)
         gtk_entry_set_text (GTK_ENTRY (sess->gui->topicgad), user->hostname);
   }
}

struct session *
new_dialog (struct session *sess)
{
   if (prefs.logging)
      sess->logfd = open_log_file (sess->server->servername, sess->channel);
   else
      sess->logfd = -1;
   open_dialog_window (sess);
   sess->new_data = FALSE;

   return (sess);
}
