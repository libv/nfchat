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
#include <string.h>
#include <unistd.h>
#include "../common/xchat.h"
#include "../common/util.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#include <fcntl.h>
#include <time.h>


extern unsigned char *strip_color (unsigned char *text);
extern int tcp_send_len (struct server *serv, char *buf, int len);
extern int tcp_send (struct server *serv, char *buf);


void
fe_add_chan_list (struct server *serv, char *chan, char *users, char *topic)
{
   gchar *nnew[3];
   char *wild;

   if (atoi (users) < serv->gui->chanlist_minusers)
      return;

   wild = gtk_entry_get_text (GTK_ENTRY (serv->gui->chanlist_wild));
   if (wild && wild[0])
   {
      if (!match (wild, chan))
         return;
   }
   nnew[0] = chan;
   nnew[1] = users;
   nnew[2] = strip_color (topic);

   gtk_clist_append (GTK_CLIST (serv->gui->chanlist_list), nnew);

   free (nnew[2]);
}

static void
chanlist_join (GtkWidget * wid, struct server *serv)
{
   int row;
   char *chan;
   char tbuf[256];

   row = gtkutil_clist_selection (serv->gui->chanlist_list);
   if (row != -1)
   {
      gtk_clist_get_text (GTK_CLIST (serv->gui->chanlist_list), row, 0, &chan);
      if (serv->connected && (strcmp (chan, "*") != 0))
      {
         snprintf (tbuf, sizeof tbuf, "JOIN %s\r\n", chan);
         tcp_send (serv, tbuf);
      } else
         gdk_beep ();
   }
}

static void
chanlist_filereq_done (struct server *serv, void *data2, char *file)
{
   time_t t = time (0);
   int i = 0;
   int fh;
   char *chan, *users, *topic;
   char buf[1024];

   if (!file)
      return;

   fh = open (file, O_TRUNC | O_WRONLY | O_CREAT, 0600);
   free (file);

   if (fh == -1)
      return;

   snprintf (buf, sizeof buf, "X-Chat Channel List: %s - %s\n", serv->servername, ctime (&t));
   write (fh, buf, strlen (buf));

   while (1)
   {
      if (!gtk_clist_get_text (GTK_CLIST (serv->gui->chanlist_list), i, 0, &chan))
         break;
      gtk_clist_get_text (GTK_CLIST (serv->gui->chanlist_list), i, 1, &users);
      gtk_clist_get_text (GTK_CLIST (serv->gui->chanlist_list), i, 2, &topic);
      i++;
      snprintf (buf, sizeof buf, "%-16s %-5s%s\n", chan, users, topic);
      write (fh, buf, strlen (buf));
   }

   close (fh);
}

static void
chanlist_save (GtkWidget * wid, struct server *serv)
{
   char *temp;

   if (!gtk_clist_get_text (GTK_CLIST (serv->gui->chanlist_list), 0, 0, &temp))
   {
      gtkutil_simpledialog ("I can't save an empty list!");
      return;
   }
   gtkutil_file_req ("Select an output filename", chanlist_filereq_done,
                     serv, 0, TRUE);
}

static void
chanlist_refresh (GtkWidget * wid, struct server *serv)
{
   if (serv->connected)
   {
      gtk_clist_clear (GTK_CLIST (serv->gui->chanlist_list));
      gtk_widget_set_sensitive (serv->gui->chanlist_refresh, FALSE);
      tcp_send_len (serv, "LIST\r\n", 6);
   } else
      gtkutil_simpledialog ("Not connected.");
}

static void
chanlist_minusers (GtkWidget * wid, struct server *serv)
{
   serv->gui->chanlist_minusers = atoi (gtk_entry_get_text (GTK_ENTRY (wid)));
}

static void
chanlist_row_selected (GtkWidget * clist, gint row, gint column,
                       GdkEventButton * even, struct server *serv)
{
   if (even && even->type == GDK_2BUTTON_PRESS)
   {
      chanlist_join (0, (struct server *) serv);
   }
}

static int
chanlist_closegui (GtkWidget * wid, struct server *serv)
{
   serv->gui->chanlist_window = 0;
   return 0;
}

void
chanlist_opengui (struct server *serv)
{
   static gchar *titles[] =
   {"Channel", "Users", "Topic"};
   GtkWidget *vbox, *hbox, *wid;
   char tbuf[256];

   if (serv->gui->chanlist_window)
   {
      gdk_window_show (serv->gui->chanlist_window->window);
      return;
   }
   snprintf (tbuf, sizeof tbuf, "X-Chat: Channel List (%s)", serv->servername);

   serv->gui->chanlist_window = gtkutil_window_new (tbuf, "chanlist", 450, 200,
             chanlist_closegui, serv, TRUE);

   vbox = gtk_vbox_new (FALSE, 2);
   gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
   gtk_container_add (GTK_CONTAINER (serv->gui->chanlist_window), vbox);
   gtk_widget_show (vbox);

   hbox = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
   gtk_widget_show (hbox);

   wid = gtk_label_new ("Minimum Users:");
   gtk_widget_set_usize (wid, 120, 0);
   gtk_box_pack_start (GTK_BOX (hbox), wid, 0, 0, 0);
   gtk_widget_show (wid);

   if (!serv->gui->chanlist_minusers)
      serv->gui->chanlist_minusers = 3;
   wid = gtk_entry_new_with_max_length (6);
   gtk_widget_set_usize (wid, 40, 0);
   sprintf (tbuf, "%d", serv->gui->chanlist_minusers);
   gtk_entry_set_text (GTK_ENTRY (wid), tbuf);
   gtk_signal_connect (GTK_OBJECT (wid), "changed",
                       GTK_SIGNAL_FUNC (chanlist_minusers), serv);
   gtk_box_pack_start (GTK_BOX (hbox), wid, 0, 0, 0);
   gtk_widget_show (wid);

   wid = gtk_label_new ("Wildcard Match:");
   gtk_widget_set_usize (wid, 125, 0);
   gtk_box_pack_start (GTK_BOX (hbox), wid, 0, 0, 0);
   gtk_widget_show (wid);

   wid = gtk_entry_new_with_max_length (32);
   gtk_widget_set_usize (wid, 155, 0);
   gtk_entry_set_text (GTK_ENTRY (wid), "*");
   gtk_box_pack_start (GTK_BOX (hbox), wid, 0, 0, 0);
   gtk_widget_show (wid);
   serv->gui->chanlist_wild = wid;

   serv->gui->chanlist_list = gtkutil_clist_new (3, titles, vbox, GTK_POLICY_ALWAYS,
                chanlist_row_selected,
                      (gpointer) serv,
          0, 0, GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width (GTK_CLIST (serv->gui->chanlist_list), 0, 90);
   gtk_clist_set_column_width (GTK_CLIST (serv->gui->chanlist_list), 1, 45);
   gtk_clist_set_column_width (GTK_CLIST (serv->gui->chanlist_list), 2, 165);
   gtk_clist_set_auto_sort (GTK_CLIST (serv->gui->chanlist_list), 1);

   hbox = gtk_hbox_new (0, 1);
   gtk_box_pack_end (GTK_BOX (vbox), hbox, 0, 0, 0);
   gtk_widget_show (hbox);

   serv->gui->chanlist_refresh =
      gtkutil_button (serv->gui->chanlist_window, GNOME_STOCK_PIXMAP_REFRESH,
                      "Refresh the list", chanlist_refresh, serv, hbox);
   gtkutil_button (serv->gui->chanlist_window, GNOME_STOCK_PIXMAP_SAVE,
                   "Save the list", chanlist_save, serv, hbox);
   gtkutil_button (serv->gui->chanlist_window, GNOME_STOCK_PIXMAP_JUMP_TO,
                   "Join Channel", chanlist_join, serv, hbox);

   gtk_widget_show (serv->gui->chanlist_window);
}
