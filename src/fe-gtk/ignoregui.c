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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../common/xchat.h"
#include "../common/ignore.h"
#include "../common/plugin.h"
#include "../common/cfgfiles.h"
#include "fe-gtk.h"
#include "gtkutil.h"

extern GSList *ignore_list;

extern int ignored_ctcp;
extern int ignored_priv;
extern int ignored_chan;
extern int ignored_noti;
extern int ignored_invi;
extern int ignored_total;


static GtkWidget *ignorewin = 0;
static GtkWidget *ignorelist;
static GtkWidget *entry_mask;
static GtkWidget *entry_ctcp;
static GtkWidget *entry_private;
static GtkWidget *entry_channel;
static GtkWidget *entry_notice;
static GtkWidget *entry_invi;
static GtkWidget *entry_unignore;   /* these are toggles, not really entrys */

static GtkWidget *num_ctcp;
static GtkWidget *num_priv;
static GtkWidget *num_chan;
static GtkWidget *num_noti;
static GtkWidget *num_invi;

static void
ignore_save_clist_tomem (void)
{
   struct ignore *ignore;
   char *tmp;
   int i = 0;
   GSList *list;

   list = ignore_list;
   while (list)
   {
      ignore = (struct ignore *) list->data;
      ignore_list = g_slist_remove (ignore_list, ignore);
      free (ignore->mask);
      free (ignore);
      list = ignore_list;
   }

   while (1)
   {
      if (!gtk_clist_get_text (GTK_CLIST (ignorelist), i, 0, &tmp))
      {
         break;
      }
      ignore = malloc (sizeof (struct ignore));
      memset (ignore, 0, sizeof (struct ignore));
      ignore->mask = strdup (tmp);
      gtk_clist_get_text (GTK_CLIST (ignorelist), i, 1, &tmp);
      if (!strcmp (tmp, "Yes"))
         ignore->ctcp = 1;
      gtk_clist_get_text (GTK_CLIST (ignorelist), i, 2, &tmp);
      if (!strcmp (tmp, "Yes"))
         ignore->priv = 1;
      gtk_clist_get_text (GTK_CLIST (ignorelist), i, 3, &tmp);
      if (!strcmp (tmp, "Yes"))
         ignore->chan = 1;
      gtk_clist_get_text (GTK_CLIST (ignorelist), i, 4, &tmp);
      if (!strcmp (tmp, "Yes"))
         ignore->noti = 1;
      gtk_clist_get_text (GTK_CLIST (ignorelist), i, 5, &tmp);
      if (!strcmp (tmp, "Yes"))
         ignore->invi = 1;
      gtk_clist_get_text (GTK_CLIST (ignorelist), i, 6, &tmp);
      if (!strcmp (tmp, "Yes"))
         ignore->unignore = 1;
      ignore_list = g_slist_append (ignore_list, ignore);
      i++;
   }
}

static void
ignore_add_clist_entry (struct ignore *ignore)
{
   char *nnew[7];

   nnew[0] = ignore->mask;
   if (ignore->ctcp)
      nnew[1] = "Yes";
   else
      nnew[1] = "No";
   if (ignore->priv)
      nnew[2] = "Yes";
   else
      nnew[2] = "No";
   if (ignore->chan)
      nnew[3] = "Yes";
   else
      nnew[3] = "No";
   if (ignore->noti)
      nnew[4] = "Yes";
   else
      nnew[4] = "No";
   if (ignore->invi)
      nnew[5] = "Yes";
   else
      nnew[5] = "No";
   if (ignore->unignore)
      nnew[6] = "Yes";
   else
      nnew[6] = "No";

   gtk_clist_append (GTK_CLIST (ignorelist), nnew);
}

static void
ignore_sort_clicked (void)
{
   gtk_clist_sort (GTK_CLIST (ignorelist));
}

static void
ignore_delete_entry_clicked (GtkWidget * wid, struct session *sess)
{
   int row;

   row = gtkutil_clist_selection (ignorelist);
   if (row != -1)
   {
      gtk_clist_unselect_all (GTK_CLIST (ignorelist));
      gtk_clist_remove ((GtkCList *) ignorelist, row);
   }
}

static void
ignore_new_entry_clicked (GtkWidget * wid, struct session *sess)
{
   int i, row;
   gchar *nnew[7];
   /*
      Serverlist copies an existing entry, but not much point to do so here
    */
   nnew[0] = "new!new@new.com";
   nnew[1] = "No";
   nnew[2] = "No";
   nnew[3] = "No";
   nnew[4] = "No";
   nnew[5] = "No";
   nnew[6] = "No";
   row = gtkutil_clist_selection (ignorelist);
   i = gtk_clist_insert (GTK_CLIST (ignorelist), row + 1, nnew);
   gtk_clist_select_row (GTK_CLIST (ignorelist), i, 0);
   gtk_clist_moveto (GTK_CLIST (ignorelist), i, 0, 0.5, 0);
}

static void
ignore_check_state_toggled (GtkWidget * wid, int row, int col)
{
   char *text;
   int state;

   gtk_clist_get_text (GTK_CLIST (ignorelist), row, col, &text);
   if (!strcmp (text, "Yes"))
      state = 1;
   else
      state = 0;

   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wid), state);
}

static void
ignore_row_unselected (GtkWidget * clist, int row, int column,
                GdkEventButton * even)
{
   gtk_entry_set_text (GTK_ENTRY (entry_mask), "");
}

static void
ignore_row_selected (GtkWidget *clist, int row, int column,
                     GdkEventButton *even, gpointer sess)
{
   char *mask;
   row = gtkutil_clist_selection (ignorelist);
   if (row != -1)
   {
      gtk_clist_get_text (GTK_CLIST (ignorelist), row, 0, &mask);
      gtk_entry_set_text (GTK_ENTRY (entry_mask), mask);
      ignore_check_state_toggled (entry_ctcp, row, 1);
      ignore_check_state_toggled (entry_private, row, 2);
      ignore_check_state_toggled (entry_channel, row, 3);
      ignore_check_state_toggled (entry_notice, row, 4);
      ignore_check_state_toggled (entry_invi, row, 5);
      ignore_check_state_toggled (entry_unignore, row, 6);
   } else
      ignore_row_unselected (0, 0, 0, 0);
}

static void
ignore_handle_mask (GtkWidget *igad)
{
   int row;
   char *mask;

   row = gtkutil_clist_selection (ignorelist);
   if (row != -1)
   {
      mask = gtk_entry_get_text ((GtkEntry *) igad);
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 0, mask);
   }
}

static void
ignore_ctcp_toggled (GtkWidget *igad, gpointer serv)
{
   int row;
   row = gtkutil_clist_selection (ignorelist);
   if (GTK_TOGGLE_BUTTON (igad)->active)
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 1, "Yes");
   else
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 1, "No");
}

static void
ignore_private_toggled (GtkWidget *igad, gpointer serv)
{
   int row;
   row = gtkutil_clist_selection (ignorelist);
   if (GTK_TOGGLE_BUTTON (igad)->active)
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 2, "Yes");
   else
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 2, "No");
}

static void
ignore_channel_toggled (GtkWidget *igad, gpointer serv)
{
   int row;
   row = gtkutil_clist_selection (ignorelist);
   if (GTK_TOGGLE_BUTTON (igad)->active)
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 3, "Yes");
   else
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 3, "No");
}

static void
ignore_notice_toggled (GtkWidget* igad, gpointer serv)
{
   int row;
   row = gtkutil_clist_selection (ignorelist);
   if (GTK_TOGGLE_BUTTON (igad)->active)
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 4, "Yes");
   else
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 4, "No");
}

static void
ignore_invi_toggled (GtkWidget *igad, gpointer serv)
{
   int row;
   row = gtkutil_clist_selection (ignorelist);
   if (GTK_TOGGLE_BUTTON (igad)->active)
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 5, "Yes");
   else
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 5, "No");
}

static void
ignore_unignore_toggled (GtkWidget *igad, gpointer serv)
{
   int row;
   row = gtkutil_clist_selection (ignorelist);
   if (GTK_TOGGLE_BUTTON (igad)->active)
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 6, "Yes");
   else
      gtk_clist_set_text ((GtkCList *) ignorelist, row, 6, "No");
}

static int
close_ignore_list ()
{
   ignore_save_clist_tomem ();
   ignore_save ();
   gtk_widget_destroy (ignorewin);
   return 0;
}

static int
close_ignore_gui_callback ()
{
   ignorewin = 0;
   return 0;
}

static GtkWidget *
ignore_stats_entry (GtkWidget * box, char *label, int value)
{
   GtkWidget *wid;
   char buf[16];

   sprintf (buf, "%d", value);
   gtkutil_label_new (label, box);
   wid = gtkutil_entry_new (16, box, 0, 0);
   gtk_widget_set_usize (wid, 30, 0);
   gtk_entry_set_editable (GTK_ENTRY (wid), FALSE);
   gtk_entry_set_text (GTK_ENTRY (wid), buf);

   return wid;
}

void
ignore_gui_open ()
{
   static gchar *titles[] =
   {"Mask", "CTCP", "Private", "Chan", "Notice", "Invite", "Unignore"};
   GtkWidget *vbox, *box, *wid, *stat_box,
            *frame;
   GSList *temp = ignore_list;

   if (ignorewin)
   {
      gdk_window_show (ignorewin->window);
      return;
   }
   ignorewin = gtkutil_window_new ("X-Chat: Ignore list", "ignorelist",
                                   0, 0, close_ignore_gui_callback, 0, TRUE);
   if (GTK_IS_WINDOW(ignorewin))
      gtk_window_set_policy (GTK_WINDOW (ignorewin), TRUE, TRUE, FALSE);

   vbox = gtk_vbox_new (0, 2);
   gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
   gtk_container_add (GTK_CONTAINER (ignorewin), vbox);
   gtk_widget_show (vbox);

   ignorelist = gtkutil_clist_new (7, titles, vbox, GTK_POLICY_ALWAYS,
               ignore_row_selected, 0,
                                   ignore_row_unselected, 0, GTK_SELECTION_BROWSE);

   gtk_widget_set_usize (ignorelist, 500, 150);

   gtk_clist_set_column_width (GTK_CLIST (ignorelist), 0, 196);
   gtk_clist_set_column_width (GTK_CLIST (ignorelist), 1, 40);
   gtk_clist_set_column_width (GTK_CLIST (ignorelist), 2, 40);
   gtk_clist_set_column_width (GTK_CLIST (ignorelist), 3, 40);
   gtk_clist_set_column_width (GTK_CLIST (ignorelist), 4, 40);
   gtk_clist_set_column_width (GTK_CLIST (ignorelist), 5, 40);

   box = gtk_hbox_new (0, 0);
   gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, TRUE, 5);
   gtk_widget_show (box);

   gtkutil_label_new ("Ignore Mask:", box);
   entry_mask = gtkutil_entry_new (99, box, ignore_handle_mask, 0);

   box = gtk_hbox_new (0, 0);
   gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, TRUE, 5);
   gtk_widget_show (box);

   entry_ctcp = gtk_check_button_new_with_label ("CTCP");
   gtk_signal_connect (GTK_OBJECT (entry_ctcp), "toggled",
                       GTK_SIGNAL_FUNC (ignore_ctcp_toggled), 0);
   gtk_container_add (GTK_CONTAINER (box), entry_ctcp);
   gtk_widget_show (entry_ctcp);

   entry_private = gtk_check_button_new_with_label ("Private");
   gtk_signal_connect (GTK_OBJECT (entry_private), "toggled",
                       GTK_SIGNAL_FUNC (ignore_private_toggled), 0);
   gtk_container_add (GTK_CONTAINER (box), entry_private);
   gtk_widget_show (entry_private);

   entry_channel = gtk_check_button_new_with_label ("Channel");
   gtk_signal_connect (GTK_OBJECT (entry_channel), "toggled",
                       GTK_SIGNAL_FUNC (ignore_channel_toggled), 0);
   gtk_container_add (GTK_CONTAINER (box), entry_channel);
   gtk_widget_show (entry_channel);

   entry_notice = gtk_check_button_new_with_label ("Notice");
   gtk_signal_connect (GTK_OBJECT (entry_notice), "toggled",
                       GTK_SIGNAL_FUNC (ignore_notice_toggled), 0);
   gtk_container_add (GTK_CONTAINER (box), entry_notice);
   gtk_widget_show (entry_notice);

   entry_invi = gtk_check_button_new_with_label ("Invite");
   gtk_signal_connect (GTK_OBJECT (entry_invi), "toggled",
                       GTK_SIGNAL_FUNC (ignore_invi_toggled), 0);
   gtk_container_add (GTK_CONTAINER (box), entry_invi);
   gtk_widget_show (entry_invi);

   entry_unignore = gtk_check_button_new_with_label ("Unignore");
   gtk_signal_connect (GTK_OBJECT (entry_unignore), "toggled",
                       GTK_SIGNAL_FUNC (ignore_unignore_toggled), 0);
   gtk_container_add (GTK_CONTAINER (box), entry_unignore);
   gtk_widget_show (entry_unignore);

   wid = gtk_hseparator_new ();
   gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 8);
   gtk_widget_show (wid);

   frame = gtk_frame_new ("Ignore Stats:");
   gtk_widget_show (frame);

   stat_box = gtk_hbox_new (0, 2);
   gtk_container_set_border_width (GTK_CONTAINER (stat_box), 6);
   gtk_container_add (GTK_CONTAINER (frame), stat_box);
   gtk_widget_show (stat_box);

   num_ctcp = ignore_stats_entry (stat_box, "CTCP:", ignored_ctcp);
   num_priv = ignore_stats_entry (stat_box, "Private:", ignored_priv);
   num_chan = ignore_stats_entry (stat_box, "Channel:", ignored_chan);
   num_noti = ignore_stats_entry (stat_box, "Notice:", ignored_noti);
   num_invi = ignore_stats_entry (stat_box, "Invite:", ignored_invi);

   gtk_container_add (GTK_CONTAINER (vbox), frame);

   box = gtk_hbox_new (0, 2);
   gtk_container_set_border_width (GTK_CONTAINER (box), 2);
   gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, TRUE, 2);
   gtk_widget_show (box);

   gtkutil_button (ignorewin, GNOME_STOCK_BUTTON_OK, "OK",
           close_ignore_list, 0, box);
   gtkutil_button (ignorewin, GNOME_STOCK_PIXMAP_NEW, "New",
    ignore_new_entry_clicked, 0, box);
   gtkutil_button (ignorewin, GNOME_STOCK_PIXMAP_CLOSE, "Delete",
                   ignore_delete_entry_clicked, 0, box);
   gtkutil_button (ignorewin, GNOME_STOCK_PIXMAP_SPELLCHECK, "Sort",
         ignore_sort_clicked, 0, box);
   gtkutil_button (ignorewin, GNOME_STOCK_BUTTON_CANCEL, "Cancel",
     gtkutil_destroy, ignorewin, box);

   while (temp)
   {
      ignore_add_clist_entry ((struct ignore *) temp->data);
      temp = temp->next;
   }
   gtk_widget_show (ignorewin);
}

void
fe_ignore_update (int level)
{
   /* some ignores have changed via /ignore, we should update
      the gui now */
   /* level 1 = the list only. */
   /* level 2 = the numbers only. */
   /* for now, ignore level 1, since the ignore GUI isn't realtime,
      only saved when you click OK */
   char buf[16];

   if (level == 2 && ignorewin)
   {
      sprintf (buf, "%d", ignored_ctcp);
      gtk_entry_set_text (GTK_ENTRY (num_ctcp), buf);

      sprintf (buf, "%d", ignored_noti);
      gtk_entry_set_text (GTK_ENTRY (num_noti), buf);

      sprintf (buf, "%d", ignored_chan);
      gtk_entry_set_text (GTK_ENTRY (num_chan), buf);

      sprintf (buf, "%d", ignored_invi);
      gtk_entry_set_text (GTK_ENTRY (num_invi), buf);

      sprintf (buf, "%d", ignored_priv);
      gtk_entry_set_text (GTK_ENTRY (num_priv), buf);
   }
}
