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
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include "../common/xchat.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#include "../common/cfgfiles.h"
#include "../common/popup.h"


extern GSList *url_list;
extern GSList *urlhandler_list;

extern char *nocasestrstr (char *, char *);
extern void my_system (char *cmd);
extern void menu_urlmenu (struct session *sess, GdkEventButton * event, char *url);

static GtkWidget *urlgrabberwindow = 0;
static GtkWidget *urlgrabberlist;
void url_addurlgui (char *urltext);


static int
url_closegui (void)
{
   urlgrabberwindow = 0;
   return 0;
}

static void
url_button_clear (void)
{
   while (url_list)
   {
      free (url_list->data);
      url_list = g_slist_remove (url_list, url_list->data);
   }
   gtk_clist_clear (GTK_CLIST (urlgrabberlist));
}

void
url_autosave (void)
{
   FILE *fd;
   GSList *list;
   char *buf;

   buf = malloc (1000);
   snprintf (buf, 1000, "%s/url.save", get_xdir ());

   fd = fopen (buf, "a");
   free (buf);
   if (fd == NULL)
      return;

   list = url_list;

   while (list)
   {
      fprintf (fd, "%s\n", (char *) list->data);
      list = list->next;
   }

   fclose (fd);
}

static void
url_save_callback (GtkWidget * b, GtkWidget * d)
{
   FILE *fd;
   GSList *list;
   char *f;

   f = gtk_file_selection_get_filename (GTK_FILE_SELECTION (d));

   if (f)
   {
      fd = fopen (f, "w");
      if (fd == NULL)
         return;
      list = url_list;
      while (list)
      {
         fprintf (fd, "%s\n", (char *) list->data);
         list = list->next;
      }
      fclose (fd);
   }
   gtk_widget_destroy (d);
}

static void
url_save_cancel (GtkWidget * b, GtkWidget * d)
{
   gtk_widget_destroy (d);
}

static void
url_button_save (void)
{
   GtkWidget *d;

   d = gtk_file_selection_new ("Select a file to save to");

   gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (d)->cancel_button),
                       "clicked", (GtkSignalFunc) url_save_cancel, (gpointer) d);
   gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (d)->ok_button),
                       "clicked", (GtkSignalFunc) url_save_callback, (gpointer) d);
   gtk_widget_show (d);
}

static void
url_clicklist (GtkWidget *widget, GdkEventButton *event)
{
   int row, col;
   char *text;

   if (event->button == 3)
   {
      if (gtk_clist_get_selection_info (GTK_CLIST (widget), event->x, event->y, &row, &col) < 0)
         return;
      gtk_clist_unselect_all (GTK_CLIST (widget));
      gtk_clist_select_row (GTK_CLIST (widget), row, 0);
      if (gtk_clist_get_text (GTK_CLIST (widget), row, 0, &text))
      {
         if (text && text[0])
         {
            menu_urlmenu (0, event, text);
         }
      }
   }
}

void
url_opengui ()
{
   GtkWidget *vbox, *hbox;
   GSList *list;

   if (urlgrabberwindow)
   {
      gdk_window_show (urlgrabberwindow->window);
      return;
   }
   urlgrabberwindow = gtkutil_window_new ("X-Chat: URL Grabber", "urlgrabber", 350, 100,
                                          url_closegui, 0, TRUE);

   vbox = gtk_vbox_new (FALSE, 1);
   gtk_container_add (GTK_CONTAINER (urlgrabberwindow), vbox);
   gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
   gtk_widget_show (vbox);

   urlgrabberlist = gtkutil_clist_new (1, 0, vbox, GTK_POLICY_AUTOMATIC,
                                 0, 0,
          0, 0, GTK_SELECTION_BROWSE);
   gtk_signal_connect (GTK_OBJECT (urlgrabberlist), "button_press_event",
                       GTK_SIGNAL_FUNC (url_clicklist), 0);
   gtk_widget_set_usize (urlgrabberlist, 350, 0);
   gtk_clist_set_column_width (GTK_CLIST (urlgrabberlist), 0, 100);

   hbox = gtk_hbox_new (FALSE, 1);
   gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 2);
   gtk_widget_show (hbox);

   gtkutil_button (urlgrabberwindow, 0, "Clear", url_button_clear, 0, hbox);
   gtkutil_button (urlgrabberwindow, 0, "Save", url_button_save, 0, hbox);

   gtk_widget_show (urlgrabberwindow);

   list = url_list;
   while (list)
   {
      url_addurlgui ((char *) list->data);
      list = list->next;
   }
}

void
url_addurlgui (char *urltext)
{
   if (urlgrabberwindow)
      gtk_clist_prepend ((GtkCList *) urlgrabberlist, &urltext);
}

static int
url_findurl (char *urltext)
{
   GSList *list = url_list;
   while (list)
   {
      if (!strcasecmp (urltext, (char *) list->data))
         return 1;
      list = list->next;
   }
   return 0;
}

static void
url_addurl (char *urltext)
{
   char *data = strdup (urltext);
   if (!data)
      return;

   if (data[strlen (data) - 1] == '.')  /* chop trailing dot */
      data[strlen (data) - 1] = 0;

   if (url_findurl (data))
      return;

   url_list = g_slist_prepend (url_list, data);
   url_addurlgui (data);
}

void
fe_checkurl (char *buf)
{
   char t, *po, *urltext = nocasestrstr (buf, "http:");
   if (!urltext)
      urltext = nocasestrstr (buf, "www.");
   if (!urltext)
      urltext = nocasestrstr (buf, "ftp.");
   if (!urltext)
      urltext = nocasestrstr (buf, "ftp:");
   if (!urltext)
      urltext = nocasestrstr (buf, "irc://");
   if (!urltext)
      urltext = nocasestrstr (buf, "irc.");
   if (urltext)
   {
      po = strchr (urltext, ' ');
      if (po)
      {
         t = *po;
         *po = 0;
         url_addurl (urltext);
         *po = t;
      } else
         url_addurl (urltext);
   }
}
