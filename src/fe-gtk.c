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
#include <unistd.h>
#include "xchat.h"
#include "fe.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#include "xtext.h"
#include <gdk_imlib.h>

static int autoconnect = 0;

GdkFont *font_normal;

extern GdkColor colors[];
extern GtkStyle *inputgad_style;
GtkStyle *channelwin_style;

extern char *xdir;
extern struct xchatprefs prefs;

extern char *get_cpu_str (int color);
extern void PrintText (char *text);
extern void notify_gui_update (void);
extern void my_gtk_entry_set_text (GtkWidget * wid, char *text, session_t *sess);
extern void key_init (void);
extern void create_window (void);
extern void init_userlist_xpm (void);

GdkFont *my_font_load (char *fontname);
GtkStyle *my_widget_get_style (char *bg_pic);

int
fe_args (int argc, char *argv[])
{
   if (argc > 1)
   {
      if (!strcasecmp (argv[1], "-v") || !strcasecmp (argv[1], "--version"))
      {
         puts ("NF-Chat " VERSION "");
         return 0;
      }
#ifdef ENABLE_NLS
      bindtextdomain (PACKAGE, LOCALEDIR);
      textdomain (PACKAGE);
#endif
      if (!strcasecmp (argv[1], "-h") || !strcasecmp (argv[1], "--help"))
      {
         puts ("NF-Chat " VERSION " Options:\n\n"
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

   gtk_init (&argc, &argv);

   gdk_imlib_init ();
   return 1;
}

void
fe_init (void)
{
   font_normal = my_font_load (prefs.font_normal);
   key_init ();

   channelwin_style = my_widget_get_style (prefs.background);
   inputgad_style = my_widget_get_style ("");
}

void
fe_main (void)
{
   gtk_main ();
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
fe_new_window (void)
{
  char buf[512];

  session->gui = malloc (sizeof (struct session_gui));
  memset (session->gui, 0, sizeof (struct session_gui));
  create_window ();

  init_userlist_xpm ();
  
  snprintf (buf, sizeof buf, "Welcome to \002NF\002-Chat %s; An irc-client for dancings, clubs and discos.\n Written by \0032_Death_\003 for \0034NetForce\003 (\002www.netforce.be\002)\n\n\n", VERSION );
  
  PrintText (buf);
  
  while (gtk_events_pending ())
    gtk_main_iteration ();
}

void
fe_input_remove (int tag)
{
   gdk_input_remove (tag);
}

int
fe_input_add (int sok, int read, int write, int ex, void *func)
{
   int type = 0;

   if (read)
      type |= GDK_INPUT_READ;
   if (write)
      type |= GDK_INPUT_WRITE;
   if (ex)
      type |= GDK_INPUT_EXCEPTION;

   return gdk_input_add (sok, type, func, 0);
}

GdkFont *
my_font_load (char *fontname)
{
   GdkFont *font;
 
   if (!*fontname)
      fontname = "fixed";
   if (prefs.use_fontset)
      font = gdk_fontset_load (fontname);
   else
      font = gdk_font_load (fontname);
   if (!font)
   {
      fprintf (stderr, "Error: Cannot open font:\n\n%s", fontname);
      font = gdk_font_load ("fixed");
      if (!font)
      {
         g_error ("gdk_font_load failed");
         gtk_exit (0);
      }
   }
   return font;
}

GtkStyle *
my_widget_get_style (char *bg_pic)
{
   GtkStyle *style;
   GdkPixmap *pixmap;
   GdkImlibImage *img;

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
         img = gdk_imlib_load_image (bg_pic);
         if (img)
         {
            gdk_imlib_render (img, img->rgb_width, img->rgb_height);
            pixmap = gdk_imlib_move_image (img);
            gdk_imlib_destroy_image (img);
            style->bg_pixmap[GTK_STATE_NORMAL] = pixmap;
         }
      } else
         fprintf (stderr, "Error: Cannot access %s", bg_pic);
   }
   return style;
}

void
fe_set_topic (char *topic)
{
   gtk_entry_set_text (GTK_ENTRY (session->gui->topicgad), topic);
}

static int
updatedate_bar (void)
{
   static int type = 0;
   static float pos = 0;

   pos += 0.05;
   if (pos >= 0.99)
   {
      if (type == 0)
      {
         type = 1;
         gtk_progress_bar_set_orientation ((GtkProgressBar *) session->gui->bar, GTK_PROGRESS_RIGHT_TO_LEFT);
      } else
      {
         type = 0;
         gtk_progress_bar_set_orientation ((GtkProgressBar *) session->gui->bar, GTK_PROGRESS_LEFT_TO_RIGHT);
      }
      pos = 0.05;
   }
   gtk_progress_bar_update ((GtkProgressBar *) session->gui->bar, pos);
   return 1;
}

void
fe_progressbar_start (void)
{
   if (session->gui->op_box)
   {
      session->gui->bar = gtk_progress_bar_new ();
      gtk_box_pack_start (GTK_BOX (session->gui->op_box), session->gui->bar, 0, 0, 0);
      gtk_widget_show (session->gui->bar);
      server->bartag = gtk_timeout_add (50, (GtkFunction) updatedate_bar, session);
   }
}

void
fe_progressbar_end (void)
{
  if (session->gui->bar)
    {
      if (GTK_IS_WIDGET (session->gui->bar))
	gtk_widget_destroy (session->gui->bar);
      session->gui->bar = 0;
      gtk_timeout_remove (server->bartag);
    }
}

static void
PrintTextLine (char *text, int len)
{
  char *tab;
  int leftlen;
  if (len == -1)
    len = strlen (text);
  
  if (len == 0)
    len = 1;

  tab = strchr (text, '\t');
  if (tab && (unsigned long)tab < (unsigned long)(text + len))
    {
      leftlen = (unsigned long)tab - (unsigned long)text;
      gtk_xtext_append_indent (GTK_XTEXT (session->gui->textgad), text, leftlen, tab+1, len - (leftlen + 1));
    } else
      gtk_xtext_append_indent (GTK_XTEXT (session->gui->textgad), 0, 0, text, len);
}

void
PrintText (char *text)
{
   char *cr;

   cr = strchr (text, '\n');
   if (cr)
   {
      while (1)
      {
         PrintTextLine (text, (unsigned long)cr - (unsigned long)text);
         text = cr + 1;
         if (*text == 0)
            break;
         cr = strchr (text, '\n');
         if (!cr)
         {
            PrintTextLine (text, -1);
            break;
         }
      }
   } else
      PrintTextLine (text, -1);
}
