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
#include "../pixmaps/xchat_mini.xpm"
#include "fe-gtk.h"
#include "menu.h"
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

extern struct session *current_tab, *menu_sess, *perl_sess;
extern struct xchatprefs prefs;
extern GSList *sess_list;
extern GSList *button_list;
extern GSList *serv_list;
extern GtkStyle *channelwin_style;
/* extern GdkFont *dialog_font_normal; */
extern GdkFont *font_normal;
extern gint xchat_is_quitting;

extern void nick_command_parse (session *sess, char *cmd, char *nick, char *allnick);
extern void goto_url (void *unused, char *url);
extern int key_action_insert (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2, struct session *sess);
extern int handle_multiline (struct session *sess, char *cmd, int history, int nocommand);
extern int kill_session_callback (struct session *sess);
extern gint gtk_kill_session_callback (GtkWidget *, struct session *sess);
extern void palette_alloc (GtkWidget * widget);
extern void menu_urlmenu (struct session *sess, GdkEventButton * event, char *url);
extern void menu_hostmenu (struct session *sess, GdkEventButton * event, char *url);
extern void menu_chanmenu (struct session *sess, GdkEventButton * event, char *url);
extern void userlist_dnd_drop (GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *, guint, guint32, struct session *);
extern int userlist_dnd_motion (GtkWidget *, GdkDragContext *, gint, gint, guint);
extern int userlist_dnd_leave (GtkWidget *, GdkDragContext *, guint time);
extern int tcp_send_len (struct server *serv, char *buf, int len);
extern int tcp_send (struct server *serv, char *buf);
extern void menu_popup (struct session *sess, GdkEventButton * event, char *nick);
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

static void
maingui_set_icon (GtkWidget *win)
{
   static GdkPixmap *xchat_icon = NULL;
   static GdkBitmap *xchat_bmp;

   if (xchat_icon == NULL)
   {
#ifndef USE_IMLIB
      xchat_icon = gdk_pixmap_create_from_xpm_d (win->window, &xchat_bmp,
				       &win->style->bg[GTK_STATE_NORMAL], xchat_mini_xpm);
#else
      gdk_imlib_data_to_pixmap (xchat_mini_xpm, &xchat_icon, &xchat_bmp);
#endif
   }
   gtkutil_set_icon (win->window, xchat_icon, xchat_bmp);
}

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

static void
handle_topicgad (GtkWidget * igad, struct session *sess)
{
   char *topic = gtk_entry_get_text (GTK_ENTRY (igad));
   if (sess->channel[0] && sess->server->connected)
   {
      char *fmt = "TOPIC %s :%s\r\n";
      char *tbuf;
      int len = strlen(fmt)-3 + strlen(sess->channel) + strlen(topic);
      tbuf = malloc (len);
      snprintf (tbuf, len, "TOPIC %s :%s\r\n", sess->channel, topic);
      tcp_send (sess->server, tbuf);
      free(tbuf);
   } else
      gtk_entry_set_text (GTK_ENTRY (igad), "");
}

static char *
find_selected_nick (struct session *sess)
{
   int row;
   struct user *user;

   row = gtkutil_clist_selection (sess->gui->namelistgad);
   if (row == -1)
      return 0;

   user = gtk_clist_get_row_data (GTK_CLIST (sess->gui->namelistgad), row);
   if (!user)
      return 0;
   return user->nick;
}

static void
ul_button_rel (GtkWidget * widget, GdkEventButton * even, struct session *sess)
{
   char *nick;
   int row, col;
   char buf[67];

   if (!even)
      return;
   if (even->button == 3)
   {
      if (gtk_clist_get_selection_info (GTK_CLIST (widget), even->x, even->y, &row, &col) < 0)
         return;
      gtk_clist_unselect_all (GTK_CLIST (widget));
      gtk_clist_select_row (GTK_CLIST (widget), row, 0);
      nick = find_selected_nick (sess);
      if (nick)
      {
         menu_popup (sess, even, nick);
      }
      return;
   }
   if (even->button == 2)
   {
      if (gtk_clist_get_selection_info (GTK_CLIST (widget), even->x, even->y, &row, &col) != -1)
      {
         gtk_clist_select_row (GTK_CLIST (widget), row, col);
         nick = find_selected_nick (sess);
         if (nick)
         {
            snprintf (buf, sizeof (buf), "%s: ", nick);
            gtk_entry_set_text (GTK_ENTRY (sess->gui->inputgad), buf);
            gtk_widget_grab_focus (sess->gui->inputgad);
         }
      }
   }
}

void
add_tip (GtkWidget * wid, char *text)
{
   GtkTooltips *tip = gtk_tooltips_new ();
   gtk_tooltips_set_tip (tip, wid, text, 0);
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
         menu_sess = current_tab;
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
      menu_sess = sess;
   }
}

void
show_and_unfocus (GtkWidget * wid)
{
   GTK_WIDGET_UNSET_FLAGS (wid, GTK_CAN_FOCUS);
   gtk_widget_show (wid);
}

GtkStyle *
my_widget_get_style (char *bg_pic)
{
   char buf[256];
   GtkStyle *style;
   GdkPixmap *pixmap;
#ifdef USE_IMLIB
   GdkImlibImage *img;
#endif

   style = gtk_style_new ();

   gdk_font_unref (style->font);
   gdk_font_ref (font_normal);
   style->font = font_normal;

   style->base[GTK_STATE_NORMAL] = colors[19];
   style->bg[GTK_STATE_NORMAL] = colors[19];
   style->fg[GTK_STATE_NORMAL] = colors[18];

   if (bg_pic[0])
   {
      if (access (bg_pic, R_OK) == 0)
      {
#ifdef USE_IMLIB
         img = gdk_imlib_load_image (bg_pic);
         if (img)
         {
            gdk_imlib_render (img, img->rgb_width, img->rgb_height);
            pixmap = gdk_imlib_move_image (img);
            gdk_imlib_destroy_image (img);
            style->bg_pixmap[GTK_STATE_NORMAL] = pixmap;
         }
#else
         pixmap = gdk_pixmap_create_from_xpm (0, 0,
                                              &style->bg[GTK_STATE_NORMAL], bg_pic);
         style->bg_pixmap[0] = pixmap;
#endif
      } else
      {
         snprintf (buf, sizeof buf, "Cannot access %s", bg_pic);
         gtkutil_simpledialog (buf);
      }
   }
   return style;
}

int
textgad_get_focus_cb (GtkWidget * wid, GdkEventKey * event, struct session *sess)
{
   char text[2] = " ";

   if (event->keyval >= GDK_space && event->keyval <= GDK_asciitilde)
   {
      text[0] = event->keyval;
      gtk_entry_append_text (GTK_ENTRY (sess->gui->inputgad), text);
      gtk_widget_grab_focus (sess->gui->inputgad);
      return FALSE;
   } else if (event->keyval == GDK_BackSpace)
   {
      gtk_widget_grab_focus (sess->gui->inputgad);
      return FALSE;
   }
   return TRUE;
}

#define WORD_URL     1
#define WORD_NICK    2
#define WORD_CHANNEL 3
#define WORD_HOST    4
#define WORD_EMAIL   5
#define WORD_DIALOG  -1

/* check if a word is clickable */

int
maingui_word_check (GtkXText *xtext, char *word)
{
   session *sess;
   char *at, *dot;
   int i, dots;
   int len = strlen (word);

   if ((word[0] == '@' || word[0] == '+') && word[1] == '#')
      return WORD_CHANNEL;

   if (word[0] == '#' && word[1] != '#' && word[1] != 0)
      return WORD_CHANNEL;

   if (!strncasecmp (word, "irc://", 6))
      return WORD_URL;

   if (!strncasecmp (word, "irc.", 4))
      return WORD_URL;

   if (!strncasecmp (word, "ftp.", 4))
      return WORD_URL;

   if (!strncasecmp (word, "ftp:", 4))
      return WORD_URL;

   if (!strncasecmp (word, "www.", 4))
      return WORD_URL;

   if (!strncasecmp (word, "http:", 5))
      return WORD_URL;

   if (!strncasecmp (word, "https:", 6))
      return WORD_URL;

   sess = gtk_object_get_user_data (GTK_OBJECT (xtext));

   if (find_name (sess, word))
      return WORD_NICK;

   at = strchr (word, '@');  /* check for email addy */
   dot = strrchr (word, '.');
   if (at && dot)
   {
      if ((unsigned long) at < (unsigned long) dot)
      {
         if (strchr (word, '*'))
            return WORD_HOST;
         else
            return WORD_EMAIL;
      }
   }

   /* check if it's an IP number */
   dots = 0;
   for (i = 0; i < len; i++)
   {
      if (word[i] == '.')
         dots++;
   }
   if (dots == 3)
   {
      if (inet_addr (word) != -1)
         return WORD_HOST;
   }

   if (!strncasecmp (word + len - 5, ".html", 5))
      return WORD_HOST;

   if (!strncasecmp (word + len - 4, ".org", 4))
      return WORD_HOST;

   if (!strncasecmp (word + len - 4, ".net", 4))
      return WORD_HOST;

   if (!strncasecmp (word + len - 4, ".com", 4))
      return WORD_HOST;

   if (!strncasecmp (word + len - 4, ".edu", 4))
      return WORD_HOST;

   if (len > 5)
   {
      if (word[len-3] == '.' &&
          isalpha(word[len-2]) &&
          isalpha(word[len-1])
         )
         return WORD_HOST;
   }

   return 0;
}

/* mouse click inside text area */

void
maingui_word_clicked (GtkXText *xtext, char *word, GdkEventButton *even, session *sess)
{

   if (even->button == 1)  /* left button */
   {
      if (even->state & GDK_CONTROL_MASK)
      {
         switch (maingui_word_check (xtext, word))
         {
         case WORD_URL:
         case WORD_HOST:
            goto_url (0, word);
         }
      }
      return;
   }

   switch (maingui_word_check (xtext, word))
   {
   case WORD_URL:
   case WORD_HOST:
      menu_urlmenu (sess, even, word);
      break;
   case WORD_NICK:
      menu_popup (sess, even, word);
      break;
   case WORD_CHANNEL:
      if (*word == '@' || *word == '+')
         word++;
      menu_chanmenu (sess, even, word);
      break;
   case WORD_EMAIL:
      {
         char *newword = malloc (strlen (word) + 10);
         if (*word == '~')
            word++;
         sprintf (newword, "mailto:%s", word);
         menu_urlmenu (sess, even, newword);
         free (newword);
      }
      break;
   case WORD_DIALOG:
      menu_popup (sess, even, sess->channel);
      break;
   }
}

void
maingui_configure (GtkWidget *unused)
{
   if (menu_sess)
      gtk_widget_queue_draw (menu_sess->gui->textgad);
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
   ((GtkXText*)sess->gui->textgad)->urlcheck_function = maingui_word_check;

   ((GtkXText*)sess->gui->textgad)->tint_red = prefs.tint_red;
   ((GtkXText*)sess->gui->textgad)->tint_green = prefs.tint_green;
   ((GtkXText*)sess->gui->textgad)->tint_blue = prefs.tint_blue;

   if (prefs.timestamp && prefs.indent_nicks)
      ((GtkXText*)sess->gui->textgad)->time_stamp = TRUE;

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

   if (!sess->is_tab)
      gtk_signal_connect_object (GTK_OBJECT (sess->gui->window),
                                 "configure_event",
                                 GTK_SIGNAL_FUNC (maingui_configure),
                                 GTK_OBJECT (sess->gui->textgad));

   gtk_signal_connect (GTK_OBJECT (sess->gui->textgad), "word_click",
                       GTK_SIGNAL_FUNC (maingui_word_clicked), sess);
}

static void
gui_new_tab (session *sess)
{
   current_tab = sess;
   menu_sess = sess;
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
      /*gtk_widget_set_style (sess->gui->changad, normaltab_style);*/
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
   menu_sess = 0;
}

int
maingui_pagetofront (int page)
{
   gtk_notebook_set_page (GTK_NOTEBOOK (main_book), page);
   return 0;
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
   char *nick;
   void *unused;

   if (even)
   {
      if (even->type == GDK_2BUTTON_PRESS)
      {
         if (prefs.doubleclickuser[0])
         {

            if (gtk_clist_get_cell_type (GTK_CLIST (clist), row, column) == GTK_CELL_PIXTEXT)
            {
               gtk_clist_get_pixtext (GTK_CLIST (clist), row, column, &nick, (guint8 *) & unused, (GdkPixmap **) & unused, (GdkBitmap **) & unused);
            } else
               gtk_clist_get_text (GTK_CLIST (clist), row, column, &nick);
            nick_command_parse (menu_sess, prefs.doubleclickuser, nick, nick);
         }
      } else
      {
         if (!(even->state & GDK_SHIFT_MASK))
         {
            gtk_clist_unselect_all (GTK_CLIST (clist));
            gtk_clist_select_row (GTK_CLIST (clist), row, column);
         }
      }
   }
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
      maingui_set_icon (main_window);
      gtk_signal_connect ((GtkObject *) main_window, "destroy",
                          GTK_SIGNAL_FUNC (gui_main_window_kill), sess);
      gtk_signal_connect ((GtkObject *) main_window, "focus_in_event",
                          GTK_SIGNAL_FUNC (focus_in), 0);
      gtk_signal_connect_object (GTK_OBJECT (main_window), "configure_event",
                          GTK_SIGNAL_FUNC (maingui_configure), 0);
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
      maingui_set_icon (sess->gui->window);
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
   gtk_signal_connect (GTK_OBJECT (sess->gui->topicgad), "activate",
                       GTK_SIGNAL_FUNC (handle_topicgad), sess);
   gtk_container_add (GTK_CONTAINER (tbox), sess->gui->topicgad);
   gtk_widget_show (sess->gui->topicgad);

   add_tip (sess->gui->topicgad, "The channel topic");

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

   gtk_clist_set_selection_mode (GTK_CLIST (sess->gui->namelistgad),
              GTK_SELECTION_MULTIPLE);
   gtk_clist_set_column_width (GTK_CLIST (sess->gui->namelistgad), 0, 10);
   gtk_widget_set_usize (sess->gui->namelistgad->parent, 115, 0);
   gtk_signal_connect (GTK_OBJECT (sess->gui->namelistgad), "button_press_event",
                       GTK_SIGNAL_FUNC (ul_button_rel), sess);

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

      if (prefs.newtabstofront && !justopened)
      {
         maingui_pagetofront (gtk_notebook_page_num (GTK_NOTEBOOK (main_book), sess->gui->window));
         sess->new_data = TRUE;
         gui_new_tab (sess);
      }

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

void
my_window_set_title (GtkWidget *window, char *title)
{
   if (GTK_IS_WINDOW(window))
      gtk_window_set_title (GTK_WINDOW(window), title);
   else
      gtk_label_set_text (gtk_object_get_user_data (GTK_OBJECT(window)), title);
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
   int page;
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

   if (prefs.newtabstofront)
   {
      page = gtk_notebook_page_num (GTK_NOTEBOOK (main_book), box);
      maingui_pagetofront (page);
   }

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
   if (menu_sess == sess && sess_list)
      menu_sess = (struct session *) sess_list->data;

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
