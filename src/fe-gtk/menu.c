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
extern void userlist_hide (GtkWidget * igad, struct session *sess);
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
extern void menu_themehandler ();

extern struct session *menu_sess;
extern GSList *popup_list;
extern GSList *button_list;
extern GSList *command_list;
extern GSList *ctcp_list;
extern GSList *replace_list;
extern GSList *urlhandler_list;
static GSList *submenu_list;

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

static GtkWidget *
menu_quick_sub (char *name, GtkWidget *menu)
{
   GtkWidget *sub_menu;
   GtkWidget *sub_item;

   if (!name)
      return menu;

   /* Code to add a submenu */
   sub_menu = gtk_menu_new ();
   sub_item = gtk_menu_item_new_with_label (name);
   gtk_menu_append (GTK_MENU (menu), sub_item);
   gtk_widget_show (sub_item);
   gtk_menu_item_set_submenu (GTK_MENU_ITEM (sub_item), sub_menu);

   /* We create a new element in the list */
   submenu_list = g_slist_prepend (submenu_list, sub_menu);
   return (sub_menu);
}

static GtkWidget *
menu_quick_endsub ()
{
  /* Just delete the first element in the linked list pointed to by first */
  if (submenu_list)
    submenu_list = g_slist_remove (submenu_list, submenu_list->data);
  return (submenu_list->data);
}

static char *nick_copy = 0;

void
menu_popup (struct session *sess, GdkEventButton * event, char *nick)
{
   GtkWidget *wid;
   GtkWidget *menu;
   GtkWidget *tempmenu;
   GSList *list = popup_list;
   struct popup *pop;
   struct user *user;
   char *buf;
   int free_me = 0;

   user = find_name_global (menu_sess->server, nick);

   if (nick_copy)
      free (nick_copy);
   nick_copy = strdup (nick);

   /* First create the menu so that we have something to add items to */
   menu = gtk_menu_new ();
   submenu_list = g_slist_prepend (submenu_list, menu);
   
   /* Then a submenu with the name of the user and some info */
   tempmenu = menu_quick_sub (nick_copy, menu);
   if (!user)
   {
      user = malloc (sizeof (struct user));
      memset (user, 0, sizeof (struct user));
      free_me = 1;
   }
   buf = malloc (256);
   snprintf (buf, 255,
                "User: %s\n"
                "Country: %s\n"
                "Realname: %s\n"
                "Server: %s\n"
                "Last Msg: %s",
                user->hostname ? user->hostname : "Unknown",
                user->hostname ? country (user->hostname) : "Unknown",
                user->realname ? user->realname : "Unknown",
                user->servername ? user->servername : "Unknown",
                user->lasttalk ? ctime(&(user->lasttalk)) : "Unknown-");
   buf[strlen (buf) - 1] = 0;
   wid = menu_quick_item (0, buf, tempmenu, 0, 0);
   gtk_label_set_justify (GTK_LABEL (GTK_BIN (wid)->child), GTK_JUSTIFY_LEFT);
   free (buf);
   if (free_me)
      free (user);
   tempmenu = menu_quick_endsub ();
   menu_quick_item (0, 0, tempmenu, 1, nick_copy);

   /* Lets do the user specified part of the list */
   while (list)
   {
      pop = (struct popup *) list->data;
      if (!strncasecmp (pop->name, "SUB", 3))
         tempmenu = menu_quick_sub (pop->cmd, tempmenu);
      else if (!strncasecmp (pop->name, "ENDSUB", 6))
      {
         if (tempmenu != menu)
            tempmenu = menu_quick_endsub ();
	 /* If we get here and tempmenu equals menu that means we havent got any submenus to exit from */
      } 
      else if (!strncasecmp (pop->name, "SEP", 3))
	menu_quick_item (0, 0, tempmenu, 1, nick_copy);
      else
	menu_quick_item (pop->cmd, pop->name, tempmenu, 0, nick_copy);

      list = list->next;

   }
   
   /* Menu done...show it to the world */
   gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
		   event->button, event->time);
   gtk_widget_show (menu);

   /* Let's clean up the linked list from mem */
   while (submenu_list)
     submenu_list = g_slist_remove (submenu_list, submenu_list->data);
}

static char *url_copy = 0;

void
menu_urlmenu (struct session *sess, GdkEventButton *event, char *url)
{
   GtkWidget *menu;
   struct popup *pop;
   GSList *list;

   if (url_copy)
      free (url_copy);

   url_copy = strdup (url);

   menu = gtk_menu_new ();

   menu_quick_item (0, url, menu, 1, url_copy);
   menu_quick_item (0, 0, menu, 1, url_copy);

   list = urlhandler_list;
   while (list)
   {
      pop = list->data;
      menu_quick_item (pop->cmd, pop->name, menu, 0, url_copy);
      list = list->next;
   }

   gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);
}

static void
menu_chan_cycle (GtkWidget *menu, char *chan)
{
   char tbuf[256];

   if (menu_sess)
   {
      snprintf (tbuf, sizeof tbuf, "PART %s\r\nJOIN %s\r\n", chan, chan);
      tcp_send (menu_sess->server, tbuf);
   }
}

static void
menu_chan_part (GtkWidget *menu, char *chan)
{
   char tbuf[256];

   if (menu_sess)
   {
      snprintf (tbuf, sizeof tbuf, "PART %s\r\n", chan);
      tcp_send (menu_sess->server, tbuf);
   }
}

static void
menu_chan_join (GtkWidget *menu, char *chan)
{
   char tbuf[256];

   if (menu_sess)
   {
      snprintf (tbuf, sizeof tbuf, "JOIN %s\r\n", chan);
      tcp_send (menu_sess->server, tbuf);
   }
}

void
menu_chanmenu (struct session *sess, GdkEventButton * event, char *chan)
{
   GtkWidget *menu;
   GSList *list = sess_list;
   int is_joined = FALSE;
   struct session *s;

   while (list)
   {
      s = (struct session *) list->data;
      if (s->server == sess->server)
      {
         if (!strcasecmp (chan, s->channel))
         {
            is_joined = TRUE;
            break;
         }
      }
      list = list->next;
   }

   if (url_copy)
      free (url_copy);

   url_copy = strdup (chan);

   menu = gtk_menu_new ();

   menu_quick_item (0, chan, menu, 1, url_copy);
   menu_quick_item (0, 0, menu, 1, url_copy);

   if (!is_joined)
      menu_quick_item_with_callback (menu_chan_join, "Join Channel", menu, url_copy);
   else {
      menu_quick_item_with_callback (menu_chan_part, "Part Channel", menu, url_copy);
      menu_quick_item_with_callback (menu_chan_cycle, "Cycle Channel", menu, url_copy);
   }

   gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
          event->button, event->time);
}