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
#include <string.h>
#include <stdlib.h>
#include "../common/xchat.h"
#include "../common/fe.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#include "xtext.h"
#ifdef USE_IMLIB
#include <gdk_imlib.h>
#endif

static int autoconnect = 0;

GdkFont *font_normal;
GdkFont *dialog_font_normal;

extern GtkStyle *redtab_style;
extern GtkStyle *bluetab_style;
extern GtkStyle *inputgad_style;
GtkStyle *channelwin_style;
GtkStyle *dialogwin_style;

extern char *xdir;
extern struct session *current_tab;
extern struct xchatprefs prefs;
extern GSList *sess_list;

extern int is_session (session *sess);
extern char *get_cpu_str (int color);
extern void userlist_hide (GtkWidget * igad, struct session *sess);
extern void PrintTextRaw (GtkWidget * textwidget, unsigned char *text);
extern void notify_gui_update (void);
extern void update_all_of (char *name);
extern void my_gtk_entry_set_text (GtkWidget * wid, char *text, struct session *sess);
extern void palette_load (void);
extern void key_init (void);
extern void palette_save (void);
extern void create_window (struct session *);
extern GtkStyle *my_widget_get_style (char *bg_pic);
extern void PrintText (struct session *, char *);
extern struct session *new_session (struct server *serv, char *from);
extern void init_userlist_xpm (struct session *sess);
extern struct session *find_session_from_waitchannel (char *target, struct server *serv);
extern void tree_blue_style (struct session *);
extern void tree_red_style (struct session *);

GdkFont *my_font_load (char *fontname);


int
fe_args (int argc, char *argv[])
{
   if (argc > 1)
   {
      if (!strcasecmp (argv[1], "-v") || !strcasecmp (argv[1], "--version"))
      {
         puts ("X-Chat " VERSION "");
         return 0;
      }
#ifdef ENABLE_NLS
      bindtextdomain (PACKAGE, LOCALEDIR);
      textdomain (PACKAGE);
#endif
      if (!strcasecmp (argv[1], "-h") || !strcasecmp (argv[1], "--help"))
      {
         puts ("X-Chat " VERSION " Options:\n\n"
               "   --connect      -c        : auto connect\n"
               "   --cfgdir <dir> -d        : use a different config dir\n"
               "   --version      -v        : show version information\n"
            );
         return 0;
      }
   }

   if (argc > 1)
   {
      if (!strcasecmp (argv[1], "-c") || !strcasecmp (argv[1], "--connect"))
         autoconnect = 1;

      if (argc > 2)
      {
         if (!strcasecmp (argv[1], "-d") || !strcasecmp (argv[1], "--cfgdir"))
         {
            xdir = strdup (argv[2]);
            if (xdir[strlen (xdir) - 1] == '/')
               xdir[strlen (xdir) - 1] = 0;
         }
      }
   }

   gtk_set_locale ();

   gtk_init (&argc, &argv);

#ifdef USE_IMLIB
   gdk_imlib_init ();
#endif

   return 1;
}

void
fe_init (void)
{
   font_normal = my_font_load (prefs.font_normal);
   dialog_font_normal = my_font_load (prefs.dialog_font_normal);

   palette_load ();
   key_init ();

   channelwin_style = my_widget_get_style (prefs.background);
   dialogwin_style = my_widget_get_style (prefs.background_dialog);
   inputgad_style = my_widget_get_style ("");
}

static int done_intro = 0;

static void
init_sess (void)
{
   char buf[512];
   time_t tim = time (0);
   struct session *sess = sess_list->data;

   if (done_intro)
      return;
   done_intro = 1;

   init_userlist_xpm (sess_list->data);

   PrintText (sess,
"\n"
"\00314\002          ######\002\n"
"\00314\002    ##### #####  ######## #### #### ######### ##########\002\n"
"\00314\002     #### ####   ######## #### #### ######### ##########\002\n"
"\00314\002      #######    ####     ######### #### ####    ####\002\n"
"\00314\002      #######    ####     ######### #########    ####\002\n"
"\00314\002     #### ####   ######## #### #### #### ####    ####\002\n"
"\00314\002    ##### #####  ######## #### #### #### ####    ####\002\n"
"\00314\002   ######\002\n"
"\n");

   snprintf (buf, sizeof buf,
"   \00313xc\0032! \00310"VERSION"\017 on %s \017GTK\00310 %d.%d.%d\n"
"\n"
"   \00310Launched\0032: \017%s\n",
      get_cpu_str (1), gtk_major_version, gtk_minor_version, gtk_micro_version,
      ctime (&tim));
   PrintText (sess, buf);

   strcpy (buf, "   \00310Features\0032: \017");
#ifdef USE_IMLIB
   strcat (buf, "Imlib ");
#endif
#ifdef USE_PERL
   strcat (buf, "Perl ");
#endif
#ifdef USE_PLUGIN
   strcat (buf, "Plugin ");
#endif
#ifdef ENABLE_NLS
   strcat (buf, "NLS ");
#endif
#ifdef SOCKS
   strcat (buf, "Socks5\n\n");
#else
   strcat (buf, "\n\n");
#endif
   PrintText (sess, buf);
   while (gtk_events_pending ())
      gtk_main_iteration ();
}

void
fe_main (void)
{
   gtk_main ();
}

void
fe_cleanup (void)
{
   palette_save ();
}

void
fe_exit (void)
{
   gtk_main_quit ();
}

int
fe_timeout_add (int interval, void *callback, void *userdata)
{
   return gtk_timeout_add (interval, (GtkFunction) callback, userdata);
}

void
fe_timeout_remove (int tag)
{
   gtk_timeout_remove (tag);
}

void
fe_new_window (struct session *sess)
{
   sess->gui = malloc (sizeof (struct session_gui));
   memset (sess->gui, 0, sizeof (struct session_gui));
   create_window (sess);
   init_sess ();
}

void
fe_message (char *msg, int wait)
{
   GtkWidget *dialog;

   dialog = gtkutil_simpledialog (msg);
   if (wait)
   {
      gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
      gtkutil_null_this_var, &dialog);
      while (dialog)
         gtk_main_iteration ();
   }
}

void
fe_input_remove (int tag)
{
   gdk_input_remove (tag);
}

int
fe_input_add (int sok, int read, int write, int ex, void *func, void *data)
{
   int type = 0;

   if (read)
      type |= GDK_INPUT_READ;
   if (write)
      type |= GDK_INPUT_WRITE;
   if (ex)
      type |= GDK_INPUT_EXCEPTION;

   return gdk_input_add (sok, type, func, data);
}

GdkFont *
my_font_load (char *fontname)
{
   GdkFont *font;
   char temp[256];

   if (!*fontname)
      fontname = "fixed";
   if (prefs.use_fontset)
      font = gdk_fontset_load (fontname);
   else
      font = gdk_font_load (fontname);
   if (!font)
   {
      snprintf (temp, 255, "Cannot open font:\n\n%s", fontname);
      gtkutil_simpledialog (temp);
      font = gdk_font_load ("fixed");
      if (!font)
      {
         g_error ("gdk_font_load failed");
         gtk_exit (0);
      }
   }
   return font;
}

static struct session *
find_unused_session (struct server *serv)
{
   struct session *sess;
   GSList *list = sess_list;
   while (list)
   {
      sess = (struct session *) list->data;
      if (sess->channel[0] == 0 && sess->server == serv)
         return sess;
      list = list->next;
   }
   return 0;
}

struct session *
fe_new_window_popup (char *target, struct server *serv)
{
   struct session *sess = find_session_from_waitchannel (target, serv);
   if (!sess)
   {
      sess = find_unused_session (serv);
      if (!sess)
         sess = new_session (serv, 0);
   }
   if (sess)
   {
      strncpy (sess->channel, target, 200);
      fe_set_channel (sess);
      fe_set_title (sess);
   }
   return sess;
}

void
fe_set_topic (struct session *sess, char *topic)
{
   gtk_entry_set_text (GTK_ENTRY (sess->gui->topicgad), topic);
}

void
fe_set_hilight (struct session *sess)
{
   gtk_widget_set_style (sess->gui->changad, bluetab_style);
   if (prefs.treeview)
      tree_blue_style (sess);
}

void
fe_update_channel_key (struct session *sess)
{
   my_gtk_entry_set_text (sess->gui->key_entry, sess->channelkey, sess);
}

void
fe_update_channel_limit (struct session *sess)
{
   char tmp[16];
   sprintf (tmp, "%d", sess->limit);
   my_gtk_entry_set_text (sess->gui->limit_entry, tmp, sess);
}

void
fe_text_clear (struct session *sess)
{
   gtk_xtext_remove_lines ((GtkXText *)sess->gui->textgad, -1, TRUE);
}

void
fe_close_window (struct session *sess)
{
   gtk_widget_destroy (sess->gui->window);
}

static int
updatedate_bar (struct session *sess)
{
   static int type = 0;
   static float pos = 0;

   if (!is_session (sess))
      return 0;

   pos += 0.05;
   if (pos >= 0.99)
   {
      if (type == 0)
      {
         type = 1;
         gtk_progress_bar_set_orientation ((GtkProgressBar *) sess->gui->bar, GTK_PROGRESS_RIGHT_TO_LEFT);
      } else
      {
         type = 0;
         gtk_progress_bar_set_orientation ((GtkProgressBar *) sess->gui->bar, GTK_PROGRESS_LEFT_TO_RIGHT);
      }
      pos = 0.05;
   }
   gtk_progress_bar_update ((GtkProgressBar *) sess->gui->bar, pos);
   return 1;
}

void
fe_progressbar_start (struct session *sess)
{
   if (sess->gui->op_box)
   {
      sess->gui->bar = gtk_progress_bar_new ();
      gtk_box_pack_start (GTK_BOX (sess->gui->op_box), sess->gui->bar, 0, 0, 0);
      gtk_widget_show (sess->gui->bar);
      sess->server->bartag = gtk_timeout_add (50, (GtkFunction) updatedate_bar, sess);
   }
}

void
fe_progressbar_end (struct session *sess)
{
   struct server *serv;
   GSList *list = sess_list;

   if (sess)
   {
      serv = sess->server;
      while (list)       /* check all windows that use this server and  *
                          * remove the connecting graph, if it has one. */
      {
         sess = (struct session *) list->data;
         if (sess->server == serv && sess->gui->bar)
         {
            if (GTK_IS_WIDGET (sess->gui->bar))
               gtk_widget_destroy (sess->gui->bar);
            sess->gui->bar = 0;
            gtk_timeout_remove (sess->server->bartag);
         }
         list = list->next;
      }
   }
}

void
fe_print_text (struct session *sess, char *text)
{
   PrintTextRaw (sess->gui->textgad, text);

   if (!sess->new_data && !sess->nick_said && sess != current_tab)
   {
      if (prefs.treeview)
	 tree_red_style (sess);
   }

   if (!sess->new_data && sess != current_tab &&
     sess->is_tab && !sess->nick_said)
   {
      sess->new_data = TRUE;
      gtk_widget_set_style (sess->gui->changad, redtab_style);
   }
}

void
fe_beep (void)
{
   gdk_beep ();
}

int
fe_is_confmode (struct session *sess)
{
   if (GTK_TOGGLE_BUTTON (sess->gui->confbutton)->active)
      return 1;
   return 0;
}

void
fe_userlist_hide (struct session *sess)
{
   userlist_hide (0, sess);
}

char *
fe_buffer_get (session *sess)
{
   return gtk_xtext_get_chars (GTK_XTEXT (sess->gui->textgad));
}
