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
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "../common/xchat.h"
#include "fe-gtk.h"
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

GtkWidget *gtkutil_simpledialog (char *msg);
extern void path_part (char *file, char *path);
extern GtkWidget *maingui_new_tab (char *title, char *name, void *close_callback, void *userdata);
extern struct xchatprefs prefs;
extern GtkWidget *main_window;

void
gtkutil_destroy (GtkWidget * igad, GtkWidget * dgad)
{
   gtk_widget_destroy (dgad);
}

GtkWidget *
gtkutil_simpledialog (char *msg)
{
   GtkWidget *label, *button, *dialog;

   dialog = gtk_dialog_new ();
   gtk_window_set_title (GTK_WINDOW (dialog), "Message");
   gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
   gtk_container_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 10);

   label = gtk_label_new (msg);
   gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, TRUE, TRUE, 10);
   gtk_widget_show (label);

   button = gtk_button_new_with_label ("Ok");
   GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
   gtk_signal_connect (GTK_OBJECT (button), "clicked",
                       GTK_SIGNAL_FUNC (gtkutil_destroy), dialog);
   gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->action_area), button, TRUE, TRUE, 10);
   gtk_widget_grab_default (button);
   gtk_widget_show (button);

   gtk_widget_show (dialog);

   return dialog;
}

GtkWidget *
gtkutil_clist_new (int columns, char *titles[],
          GtkWidget * box, int policy,
                   void *select_callback, gpointer select_userdata,
              void *unselect_callback,
           gpointer unselect_userdata,
                   int selection_mode)
{
   GtkWidget *clist, *win;

   win = gtk_scrolled_window_new (0, 0);
   gtk_container_add (GTK_CONTAINER (box), win);
   gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (win),
                 GTK_POLICY_AUTOMATIC,
                              policy);
   gtk_widget_show (win);

   if (titles)
      clist = gtk_clist_new_with_titles (columns, titles);
   else
      clist = gtk_clist_new (columns);
   if (selection_mode)
      gtk_clist_set_selection_mode (GTK_CLIST (clist), selection_mode);
   
   gtk_clist_column_titles_passive (GTK_CLIST (clist));
   gtk_container_add (GTK_CONTAINER (win), clist);

   if (select_callback)
   {
      gtk_signal_connect (GTK_OBJECT (clist), "select_row",
                          GTK_SIGNAL_FUNC (select_callback), select_userdata);
   }

   if (unselect_callback)
   {
      gtk_signal_connect (GTK_OBJECT (clist), "unselect_row",
                          GTK_SIGNAL_FUNC (unselect_callback), unselect_userdata);
   }
   gtk_widget_show (clist);

   return clist;
}

int
gtkutil_clist_selection (GtkWidget * clist)
{
   if (GTK_CLIST (clist)->selection)
      return (int) GTK_CLIST (clist)->selection->data;
   else
      return -1;
}

void
gtkutil_null_this_var (GtkWidget * unused, GtkWidget ** dialog)
{
   *dialog = 0;
}