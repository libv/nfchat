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
extern void settings_opengui (struct session *sess);
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

static void
menu_settings (GtkWidget * wid, gpointer sess)
{
   settings_opengui (menu_sess);
}

static void
menu_newserver_window (GtkWidget * wid, gpointer sess)
{
   int old = prefs.tabchannels;
   struct server *serv;

   prefs.tabchannels = 0;
   serv = new_server ();
   if (serv && !prefs.use_server_tab)
      new_session (serv, 0);
   prefs.tabchannels = old;
}

static void
menu_newchannel_window (GtkWidget * wid, gpointer sess)
{
   int old = prefs.tabchannels;

   prefs.tabchannels = 0;
   new_session (menu_sess->server, 0);
   prefs.tabchannels = old;
}

static void
menu_newserver_tab (GtkWidget * wid, gpointer sess)
{
   int old = prefs.tabchannels;
   struct server *serv;

   prefs.tabchannels = 1;
   serv = new_server ();
   if (serv && !prefs.use_server_tab)
      new_session (serv, 0);
   prefs.tabchannels = old;
}

static void
menu_newchannel_tab (GtkWidget * wid, gpointer sess)
{
   int old = prefs.tabchannels;

   prefs.tabchannels = 1;
   new_session (menu_sess->server, 0);
   prefs.tabchannels = old;
}

static void
menu_autorejoin (GtkWidget * wid, gpointer sess)
{
   prefs.autorejoin = !prefs.autorejoin;
}

static void
menu_autoreconnect (GtkWidget * wid, gpointer sess)
{
   prefs.autoreconnect = !prefs.autoreconnect;
}

static void
menu_autoreconnectonfail (GtkWidget * wid, gpointer sess)
{
   prefs.autoreconnectonfail = !prefs.autoreconnectonfail;
}

static void
menu_autodialog (GtkWidget * wid, gpointer sess)
{
   prefs.autodialog = !prefs.autodialog;
}

static void
menu_autodccchat (GtkWidget * wid, gpointer sess)
{
   prefs.autodccchat = !prefs.autodccchat;
}

static void
menu_autodccsend (GtkWidget * wid, gpointer sess)
{
   prefs.autodccsend = !prefs.autodccsend;
   if (prefs.autodccsend)
   {
      if (!strcasecmp (g_get_home_dir (), prefs.dccdir))
      {
         gtkutil_simpledialog ("*WARNING*\n"
                               "Auto accepting DCC to your home directory\n"
                               "can be dangerous and is exploitable. Eg:\n"
                               "Someone could send you a .bash_profile");
      }
   }
}

static void
menu_saveexit (GtkWidget * wid, gpointer sess)
{
   prefs.autosave = !prefs.autosave;
}

static void
menu_close (GtkWidget * wid, gpointer sess)
{
   gtk_widget_destroy (menu_sess->gui->window);
}

static void
menu_flushbuffer (GtkWidget * wid, gpointer sess)
{
   fe_text_clear (menu_sess);
}

static GtkWidget *freq = 0;

static void
close_savebuffer (void)
{
   gtk_widget_destroy (freq);
   freq = 0;
}

static void
savebuffer_req_done (GtkWidget * wid, struct session *sess)
{
   int fh;
   char *buf, *sbuf;

   fh = open (gtk_file_selection_get_filename (GTK_FILE_SELECTION (freq)),
              O_TRUNC | O_WRONLY | O_CREAT, 0600);

   close_savebuffer ();

   if (fh != -1)
   {
      buf = gtk_xtext_get_chars (GTK_XTEXT (sess->gui->textgad));
      if (buf)
      {
         sbuf = gtk_xtext_strip_color (buf, strlen (buf));
         write (fh, sbuf, strlen (sbuf));
         free (sbuf);
         g_free (buf);
      }

      close (fh);
   } else
      gtkutil_simpledialog ("Cannot write to that file.");
}

static void
menu_savebuffer (GtkWidget * wid, gpointer sess)
{
   if (freq)
      close_savebuffer ();
   freq = gtk_file_selection_new ("Select an output filename");
   gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (freq)->cancel_button),
                              "clicked", (GtkSignalFunc) close_savebuffer, 0);
   gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (freq)->ok_button),
                       "clicked", (GtkSignalFunc) savebuffer_req_done, menu_sess);
   gtk_widget_show (freq);
}

static void
menu_wallops (GtkWidget * wid, gpointer sess)
{
   char tbuf[128];
   prefs.wallops = !prefs.wallops;
   if (menu_sess->server->connected)
   {
      if (prefs.wallops)
         sprintf (tbuf, "MODE %s +w\r\n", menu_sess->server->nick);
      else
         sprintf (tbuf, "MODE %s -w\r\n", menu_sess->server->nick);
      tcp_send (menu_sess->server, tbuf);
   }
}

static void
menu_servernotice (GtkWidget * wid, gpointer sess)
{
   char tbuf[128];
   prefs.servernotice = !prefs.servernotice;
   if (menu_sess->server->connected)
   {
      if (prefs.servernotice)
         sprintf (tbuf, "MODE %s +s\r\n", menu_sess->server->nick);
      else
         sprintf (tbuf, "MODE %s -s\r\n", menu_sess->server->nick);
      tcp_send (menu_sess->server, tbuf);
   }
}

static void
menu_away (GtkWidget * wid, gpointer sess)
{
   char tbuf[256];
   prefs.away = !prefs.away;
   if (menu_sess->server->connected)
   {
      if (prefs.away)
      {
         snprintf (tbuf, 255, "/away %s", prefs.awayreason);
         handle_command (tbuf, menu_sess, FALSE, FALSE);
      } else
      {
         handle_command ("/away", menu_sess, FALSE, FALSE);
      }
   }
}

static void
menu_invisible (GtkWidget * wid, gpointer sess)
{
   char tbuf[128];
   prefs.invisible = !prefs.invisible;
   if (menu_sess->server->connected)
   {
      if (prefs.invisible)
         sprintf (tbuf, "MODE %s +i\r\n", menu_sess->server->nick);
      else
         sprintf (tbuf, "MODE %s -i\r\n", menu_sess->server->nick);
      tcp_send (menu_sess->server, tbuf);
   }
}

static void
menu_help (GtkWidget * wid, gpointer sess)
{
   cmd_help (menu_sess, 0, 0, 0);
}

static void
menu_savedefault (GtkWidget * wid, gpointer sess)
{   
   if (save_config ())
      gtkutil_simpledialog ("Settings saved.");
}

static void
menu_themes (GtkWidget * wid, gpointer sess)
{
  menu_themehandler ();
}

#ifdef USE_PERL

static void
menu_loadperl_callback (struct session *sess, void *data2, char *file)
{
   if (file)
   {
      char *buf = malloc (strlen (file) + 7);

      sprintf (buf, "/LOAD %s", file);
      free (file);
      handle_command (buf, sess, FALSE, FALSE);
      free (buf);
   }
}

static void
menu_loadperl (void)
{
   gtkutil_file_req ("Select a Perl script to load", menu_loadperl_callback,
                     menu_sess, 0, FALSE);
}

static void
menu_unloadall (void)
{
   cmd_unloadall (menu_sess, 0, 0, 0);
}

static void
menu_perllist (void)
{
   handle_command ("/scpinfo", menu_sess, FALSE, FALSE);
}

#else

#define menu_perllist 0
#define menu_unloadall 0
#define menu_loadperl 0

#endif


#ifdef USE_PLUGIN

static void
menu_loadplugin_callback (struct session *sess, void *data2, char *file)
{
   if (file)
   {
      char *buf = malloc (strlen (file) + 10);

      sprintf (buf, "/LOADDLL %s", file);
      free (file);
      handle_command (buf, sess, FALSE, FALSE);
      free (buf);
   }
}

static void
menu_loadplugin (void)
{
   gtkutil_file_req ("Select a Plugin to load", menu_loadplugin_callback,
                 menu_sess, 0, FALSE);
}

static void
menu_pluginlist (void)
{
   module_glist (menu_sess);
}

static void
menu_unloadallplugins (void)
{
   module_unload (0, menu_sess);
}

#else

#define menu_unloadallplugins 0
#define menu_pluginlist 0
#define menu_loadplugin 0

#endif

#define usercommands_help  "User Commands - Special codes:\n\n"\
                           "%c  =  current channel\n"\
                           "%v  =  x-chat version ("VERSION")\n"\
                           "%n  =  your nick\n"\
                           "%1  =  word 1\n"\
                           "%2  =  word 2\n"\
                           "&1  =  word 1 to the end of line\n"\
                           "&2  =  word 2 to the end of line\n\n"\
                           "eg:\n"\
                           "/cmd john hello\n\n"\
                           "%2 would be \042john\042\n"\
                           "&2 would be \042john hello\042."\

#define ulpopup_help       "Userlist Popup - Special codes:\n\n"\
                           "%n  =  your nick\n"\
                           "%c  =  current channel\n"\
                           "%s  =  selected nick\n"\
                           "%h  =  selected nick's hostname\n"

#define ulbutton_help       "Userlist Popup - Special codes:\n\n"\
                           "%n  =  your nick\n"\
                           "%c  =  current channel\n"\
                           "%s  =  selected nick\n"\
                           "%h  =  selected nick's hostname\n"\
                           "%a  =  all selected nicks\n"

#define ctcp_help          "CTCP Replies - Special codes:\n\n"\
                           "%s  =  nick who sent the ctcp\n"\
                           "%d  =  data (the whole ctcp)\n"\
                           "%t  =  current time/date\n"

#define url_help           "URL Handlers - Special codes:\n\n"\
                           "%s  =  the URL string\n\n"\
                           "Putting a ! infront of the command\n"\
                           "indicates it should be sent to a\n"\
                           "shell instead of X-Chat"

static void
menu_usercommands (void)
{
   editlist_gui_open (command_list, "X-Chat: User Defined Commands", "commands", "commands.conf", usercommands_help);
}

static void
menu_ulpopup (void)
{
   editlist_gui_open (popup_list, "X-Chat: Userlist Popup menu", "popup", "popup.conf", ulpopup_help);
}

static void
menu_rpopup (void)
{
   editlist_gui_open (replace_list, "X-Chat: Replace menu", "replace", "replace.conf", 0);
}

static void
menu_urlhandlers (void)
{
   editlist_gui_open (urlhandler_list, "X-Chat: URL Handlers", "urlhandlers", "urlhandlers.conf", url_help);
}

static void
menu_evtpopup (void)
{
   pevent_dialog_show ();
}

static void
menu_keypopup (void)
{
   key_dialog_show ();
}

static void
menu_ulbuttons (void)
{
   editlist_gui_open (button_list, "X-Chat: Userlist buttons", "buttons", "buttons.conf", ulbutton_help);
}

static void
menu_ctcpguiopen (void)
{
   editlist_gui_open (ctcp_list, "X-Chat: CTCP Replies", "ctcpreply", "ctcpreply.conf", ctcp_help);
}

static void
menu_reload (void)
{
   char *buf = malloc (strlen (default_file()) + 12);
   load_config ();
   sprintf (buf, "%s reloaded.", default_file());
   fe_message (buf, FALSE);
   free (buf);
}

static void
menu_docs ()
{
   goto_url (0, "http://xchat.org/docs.html");
}

static void
menu_webpage ()
{
   goto_url (0, "http://xchat.org");
}

static struct mymenu mymenu[] =
{
   {M_NEWMENU, N_ ("X-Chat"), 0, 0, 1},
   {M_MENU, N_ ("New Server Tab.."), (menucallback)menu_newserver_tab, 0, 1},
   {M_MENU, N_ ("New Server Window.."), (menucallback)menu_newserver_window, 0, 1},
   {M_MENU, N_ ("New Channel Tab.."), (menucallback)menu_newchannel_tab, 0, 1},
   {M_MENU, N_ ("New Channel Window.."), (menucallback)menu_newchannel_window, 0, 1},
   {M_SEP, 0, 0, 0, 0},
   {M_MENU, N_ ("Close"), (menucallback)menu_close, 0, 1},
   {M_SEP, 0, 0, 0, 0},
   {M_MENU, N_ ("Quit"), (menucallback)xchat_cleanup, 0, 1},  /* 10 */

   {M_NEWMENU, N_ ("Windows"), 0, 0, 1},
   {M_MENU, N_ ("DCC Send Window.."), (menucallback)fe_dcc_open_send_win, 0, 1},
   {M_MENU, N_ ("DCC Receive Window.."), (menucallback)fe_dcc_open_recv_win, 0, 1},
   {M_MENU, N_ ("DCC Chat Window.."), (menucallback)fe_dcc_open_chat_win, 0, 1},
   {M_SEP, 0, 0, 0, 0},
   {M_MENU, N_ ("Flush Buffer"), (menucallback)menu_flushbuffer, 0, 1},
   {M_MENU, N_ ("Save Buffer.."), (menucallback)menu_savebuffer, 0, 1},  /* 23 */

   {M_NEWMENU, N_ ("User Modes"), 0, 0, 1},
   {M_MENUTOG, N_ ("Invisible"), (menucallback) menu_invisible, 1, 1},
   {M_MENUTOG, N_ ("Receive Wallops"), (menucallback) menu_wallops, 1, 1},
   {M_MENUTOG, N_ ("Receive Server Notices"), (menucallback) menu_servernotice, 1, 1},
   {M_SEP, 0, 0, 0, 0},
   {M_MENUTOG, N_ ("Marked Away"), (menucallback) menu_away, 0, 1},
   {M_SEP, 0, 0, 0},
   {M_MENUTOG, N_ ("Auto ReJoin on Kick"), (menucallback) menu_autorejoin, 0, 1},
   {M_MENUTOG, N_ ("Auto ReConnect to Server"), (menucallback) menu_autoreconnect, 0, 1},
   {M_MENUTOG, N_ ("Never-give-up ReConnect"), (menucallback) menu_autoreconnectonfail, 0, 1},
   {M_SEP, 0, 0, 0},
   {M_MENUTOG, N_ ("Auto Open Dialog Windows"), (menucallback) menu_autodialog, 0, 1},  /* 35 */
   {M_MENUTOG, N_ ("Auto Accept DCC Chat"), (menucallback) menu_autodccchat, 0, 1},
   {M_MENUTOG, N_ ("Auto Accept DCC Send"), (menucallback) menu_autodccsend, 0, 1},

   {M_NEWMENU, N_ ("Settings"), 0, 0, 1},  /* 38 */
   {M_MENU, N_ ("Setup.."), (menucallback) menu_settings, 0, 1},
   {M_MENU, N_ ("User Commands.."), (menucallback) menu_usercommands, 0, 1},
   {M_MENU, N_ ("CTCP Replies.."), (menucallback) menu_ctcpguiopen, 0, 1},
   {M_MENU, N_ ("Userlist Buttons.."), (menucallback) menu_ulbuttons, 0, 1},
   {M_MENU, N_ ("Userlist Popup.."), (menucallback) menu_ulpopup, 0, 1},
   {M_MENU, N_ ("Replace.."), (menucallback) menu_rpopup, 0, 1},
   {M_MENU, N_ ("URL Handlers.."), (menucallback) menu_urlhandlers, 0, 1},
   {M_MENU, N_ ("Edit Event Texts.."), (menucallback) menu_evtpopup, 0, 1},
   {M_MENU, N_ ("Edit Key Bindings.."), (menucallback) menu_keypopup, 0, 1},
   {M_MENU, N_ ("Select theme.."), (menucallback) menu_themes, 0, 1},
   {M_SEP, 0, 0, 0, 0},
   {M_MENU, N_ ("Reload Settings"), (menucallback) menu_reload, 0, 1},
   {M_SEP, 0, 0, 0, 0},
   {M_MENU, N_ ("Save Settings now"), (menucallback) menu_savedefault, 0, 1},
   {M_MENUTOG, N_ ("Save Settings on exit"), (menucallback) menu_saveexit, 1, 1},
   {M_NEWMENU, N_ ("Scripts & Plugins"), 0, 0, 1}, /* 54 */
#ifdef USE_PERL
   {M_MENU, N_ ("Load Perl Script.."), (menucallback) menu_loadperl, 0, 1},
   {M_MENU, N_ ("Unload All Scripts"), (menucallback) menu_unloadall, 0, 1},
   {M_MENU, N_ ("Perl List"), (menucallback) menu_perllist, 0, 1},
#else
   {M_MENU, N_ ("Load Perl Script.."), 0, 0, 0},
   {M_MENU, N_ ("Unload All Scripts"), 0, 0, 0},
   {M_MENU, N_ ("Perl List"), 0, 0, 0},
#endif
   {M_SEP, 0, 0, 0, 0},
#ifdef USE_PLUGIN
   {M_MENU, N_ ("Load Plugin (*.so).."), (menucallback) menu_loadplugin, 0, 1},
   {M_MENU, N_ ("Unload All Plugins"), (menucallback) menu_unloadallplugins, 0, 1},
   {M_MENU, N_ ("Plugin List"), (menucallback) menu_pluginlist, 0, 1},
#else
   {M_MENU, N_ ("Load Plugin (*.so).."), 0, 0, 0},
   {M_MENU, N_ ("Unload All Plugins"), 0, 0, 0},
   {M_MENU, N_ ("Plugin List"), 0, 0, 0},
#endif
   {M_NEWMENURIGHT, N_ ("Help"), 0, 0, 1},
   {M_MENU, N_ ("Help.."), (menucallback) menu_help, 0, 1},
   {M_SEP, 0, 0, 0, 0},
   {M_MENU, N_ ("X-Chat Homepage.."), (menucallback) menu_webpage, 0, 1},
   {M_MENU, N_ ("Online Docs.."), (menucallback) menu_docs, 0, 1},
   {M_END, 0, 0, 0, 0}, 
};


GtkWidget *
createmenus (struct session *sess)
{
   int i = 0;
   GtkWidget *item;
   GtkWidget *menu = 0;
   GtkWidget *menu_item = 0;
   GtkWidget *menu_bar = gtk_menu_bar_new ();
  /* GtkWidget *usermenu = 0; */

   if (!menu_sess)
      menu_sess = sess;

   mymenu[25].state = prefs.invisible;
   mymenu[26].state = prefs.wallops;
   mymenu[27].state = prefs.servernotice;
   mymenu[31].state = prefs.autorejoin;
   mymenu[32].state = prefs.autoreconnect;
   mymenu[33].state = prefs.autoreconnectonfail;
   mymenu[35].state = prefs.autodialog;
   mymenu[36].state = prefs.autodccchat;
   mymenu[37].state = prefs.autodccsend;
   mymenu[53].state = prefs.autosave;

   while (1)
   {
      switch (mymenu[i].type)
      {
      case M_NEWMENURIGHT:
      case M_NEWMENU:
         if (menu)
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu);
         menu = gtk_menu_new ();
         /* not sure about this though*/
         /* if (mymenu[i].callback == (void *)-1)
            usermenu = menu;*/
         menu_item = gtk_menu_item_new_with_label (mymenu[i].text);
         if (mymenu[i].type == M_NEWMENURIGHT)
            gtk_menu_item_right_justify ((GtkMenuItem *) menu_item);
         gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), menu_item);
         gtk_widget_show (menu_item);
         break;
      case M_MENU:
         item = gtk_menu_item_new_with_label (mymenu[i].text);
         if (mymenu[i].callback)
            gtk_signal_connect_object (GTK_OBJECT (item), "activate",
                                       GTK_SIGNAL_FUNC (mymenu[i].callback), (gpointer) sess);
         gtk_menu_append (GTK_MENU (menu), item);
         gtk_widget_show (item);
         gtk_widget_set_sensitive (item, mymenu[i].activate);
         break;
      case M_MENUTOG:
         item = gtk_check_menu_item_new_with_label (mymenu[i].text);
         gtk_check_menu_item_set_state (GTK_CHECK_MENU_ITEM (item), mymenu[i].state);
         if (mymenu[i].callback)
            gtk_signal_connect_object (GTK_OBJECT (item), "toggled",
                                       GTK_SIGNAL_FUNC (mymenu[i].callback), (gpointer) sess);
         gtk_menu_append (GTK_MENU (menu), item);
         gtk_widget_show (item);
         gtk_widget_set_sensitive (item, mymenu[i].activate);
         break;
      case M_SEP:
         item = gtk_menu_item_new ();
         gtk_widget_set_sensitive (item, FALSE);
         gtk_menu_append (GTK_MENU (menu), item);
         gtk_widget_show (item);
         break;
      case M_END:
         if (menu)
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu);
         return (menu_bar);
      }
      i++;
   }
}