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
#include "xchat.h"
#include "fe-gtk.h"
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

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

