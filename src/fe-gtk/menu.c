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
#include <fcntl.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "../common/xchat.h"
#include "fe-gtk.h"
#include "menu.h"
#include "../common/cfgfiles.h"
#include "gtkutil.h"
#include "../common/ignore.h"
#include "../common/fe.h"
#include "../common/util.h"
#include "xtext.h"

extern struct session *current_tab;
extern struct xchatprefs prefs;
extern GdkColor colors[];
extern GSList *sess_list;
extern GdkFont *font_normal;
extern GtkWidget *main_window, *main_book;

extern void my_system (char *cmd);
extern void show_and_unfocus (GtkWidget * wid);
extern char *default_file (void);
extern void connect_server (struct session *sess, char *server, int port, int quiet);
extern GdkFont *my_font_load (char *fontname);
extern int tcp_send_len (struct server *serv, char *buf, int len);
extern int tcp_send (struct server *serv, char *buf);
extern void fkeys_edit (void);
extern void ctcp_gui_load (GtkWidget * list);
extern void ctcp_gui_save (GtkWidget * igad);
extern void editlist_gui_open (GSList * list, char *, char *, char *, char *help);
extern void ctcp_gui_open (void);
extern void cmd_help (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
extern void xchat_cleanup (GtkWidget * wid, gpointer sess);
extern struct server *new_server (void);
extern struct session *new_session (struct server *, char *from);
extern void PrintText (struct session *, char *);
extern char *find_selected_nick (struct session *sess);
extern int handle_command (char *cmd, struct session *sess, int, int);
extern int cmd_unloadall (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
extern int module_unload (char *name, struct session *sess);
extern void module_glist (struct session *sess);
extern void pevent_dialog_show ();
extern void key_dialog_show ();

extern struct session *menu_sess;
extern GSList *popup_list;
extern GSList *button_list;
extern GSList *command_list;
extern GSList *ctcp_list;
extern GSList *replace_list;
extern GSList *urlhandler_list;


void
goto_url (void *unused, char *url)
{
   char tbuf[256];
   snprintf (tbuf, sizeof tbuf,
             "netscape -remote 'openURL(%s)'", url);
   my_system (tbuf);
}

/* execute a userlistbutton/popupmenu command */

static void
nick_command (session *sess, char *cmd)
{
   if (*cmd == '!')
      my_system (cmd + 1);
   else 
      handle_command (cmd, sess, FALSE, FALSE);
}

/* fill in the %a %s %n etc and execute the command */

void
nick_command_parse (session *sess, char *cmd, char *nick, char *allnick)
{
   struct user *user;
   char *buf;
   int i = 0;

   /* this can't overflow, since popup->cmd is only 256 */
   buf = malloc (strlen (cmd) + strlen (nick) + strlen (allnick) + 256);

   while (*cmd)
   {
      if (*cmd == '%')
      {
         cmd++;
         switch (*cmd)
         {
         case 0:
            cmd--;
            break;
         case 'a':
            strcpy (&buf[i], allnick);
            break;
         case 'c':
            strcpy (&buf[i], sess->channel);
            break;
         case 'h':
            user = find_name (sess, nick);
            if (user && user->hostname)
               strcpy (&buf[i], user->hostname);
            else
               strcpy (&buf[i], "Host unknown");
            break;
         case 'n':
            strcpy (&buf[i], sess->server->nick);
            break;
         case 's':
            strcpy (&buf[i], nick);
            break;
         case 'v':
            strcpy (&buf[i], VERSION);
            break;
         default:
            cmd--;
            buf[i] = *cmd;
            i++;
            buf[i] = 0;
            break;
         }
         i = strlen (buf);
      } else
      {
         buf[i] = *cmd;
         i++;
         buf[i] = 0;
      }
      cmd++;
   }

   nick_command (sess, buf);
   free (buf);
}

/* userlist button has been clicked */

void
userlist_button_cb (GtkWidget *button, char *cmd)
{
   int nicks, using_allnicks = FALSE;
   char *nick = NULL, *allnicks;
   struct user *user;
   GList *sel_list;
   session *sess;

   /* this is set in maingui.c userlist_button() */
   sess = gtk_object_get_user_data (GTK_OBJECT (button));

   if (strstr (cmd, "%a"))
      using_allnicks = TRUE;

   /* count number of selected rows */
   sel_list = (GTK_CLIST (sess->gui->namelistgad))->selection;
   nicks = g_list_length (sel_list);

   if (nicks == 0)
   {
      nick_command_parse (sess, cmd, "", "");
      return;
   }

   /* create "allnicks" string */
   allnicks = malloc (65 * nicks);
   *allnicks = 0;

   nicks = 0;
   while (sel_list)
   {
      user = gtk_clist_get_row_data (GTK_CLIST (sess->gui->namelistgad),
                                     (gint)sel_list->data);
      if (!nick)
         nick = user->nick;
      if (nicks > 0)
         strcat (allnicks, " ");
      strcat (allnicks, user->nick);
      sel_list = sel_list->next;
      nicks++;

     /* if not using "%a", execute the command once for each nickname */
      if (!using_allnicks)
         nick_command_parse (sess, cmd, user->nick, "");

   }

   if (using_allnicks)
   {
      if (!nick)
         nick = "";
      nick_command_parse (sess, cmd, nick, allnicks);
   }

   free (allnicks);
}

/* a popup-menu-item has been selected */

static void
popup_menu_cb (GtkWidget *item, char *cmd)
{
   char *nick;

   /* the userdata is set in menu_quick_item() */
  nick = gtk_object_get_user_data (GTK_OBJECT (item));

   nick_command_parse (menu_sess, cmd, nick, nick);
} 

 GtkWidget *
menu_quick_item (char *cmd, char *label, GtkWidget *menu, int flags, gpointer userdata)
{
   GtkWidget *item;
   if (!label)
      item = gtk_menu_item_new ();
   else
      item = gtk_menu_item_new_with_label (label);
   gtk_menu_append (GTK_MENU (menu), item);
   gtk_object_set_user_data (GTK_OBJECT (item), userdata);
   if (cmd)
      gtk_signal_connect (GTK_OBJECT (item), "activate",
                          GTK_SIGNAL_FUNC (popup_menu_cb), cmd);
   if (flags & (1 << 0))
      gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
   gtk_widget_show (item);

   return item;
}

void
menu_quick_item_with_callback (void *callback, char *label, GtkWidget *menu, void *arg)
{
   GtkWidget *item;

   item = gtk_menu_item_new_with_label (label);
   gtk_menu_append (GTK_MENU (menu), item);
   gtk_signal_connect (GTK_OBJECT (item), "activate",
                       GTK_SIGNAL_FUNC (callback), arg);
   gtk_widget_show (item);
}