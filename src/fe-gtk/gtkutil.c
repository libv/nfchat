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

/* gtkutil.c, just some gtk wrappers */

typedef void (*filereqcallback) (void *, void *, char *file);
GtkWidget *gtkutil_simpledialog (char *msg);
extern void path_part (char *file, char *path);
extern GtkWidget *maingui_new_tab (char *title, char *name, void *close_callback, void *userdata);


struct file_req
{
   GtkWidget *dialog;
   void *userdata;
   void *userdata2;
   filereqcallback callback;
   int write;
};

char last_dir[256] = "";

void
gtkutil_file_req_cancel (GtkWidget * wid, struct file_req *freq)
{
   gtk_widget_destroy (freq->dialog);
   freq->callback (freq->userdata, freq->userdata2, 0);
   free (freq);
}

void
gtkutil_file_req_done (GtkWidget * wid, struct file_req *freq)
{
   struct stat st;
   int axs = FALSE;
   char *file, *f;

   f = gtk_file_selection_get_filename (GTK_FILE_SELECTION (freq->dialog));
   file = malloc (strlen (f) + 2);
   strcpy (file, f);
   path_part (f, last_dir);

   if (stat (file, &st) != -1)
   {
      if (S_ISDIR (st.st_mode))
      {
         strcat (file, "/");
         gtk_file_selection_set_filename (GTK_FILE_SELECTION (freq->dialog), file);
         free (file);
         return;
      }
   }
   if (freq->write)
   {
      if (access (last_dir, W_OK) == 0)
         axs = TRUE;
   } else
   {
      if (stat (file, &st) != -1)
      {
         if (!S_ISDIR (st.st_mode))
            axs = TRUE;
      }
   }

   if (axs)
   {
      freq->callback (freq->userdata, freq->userdata2, file);
   } else
   {
      free (file);
      if (freq->write)
         gtkutil_simpledialog ("Cannot write to that file.");
      else
         gtkutil_simpledialog ("Cannot read that file.");
   }

   gtk_widget_destroy (freq->dialog);
   free (freq);
}

void
gtkutil_file_req (char *title, void *callback, void *userdata, void *userdata2, int write)
{
   struct file_req *freq;
   GtkWidget *dialog;

   dialog = gtk_file_selection_new (title);
   if (last_dir[0])
      gtk_file_selection_set_filename (GTK_FILE_SELECTION (dialog), last_dir);

   freq = malloc (sizeof (struct file_req));
   freq->dialog = dialog;
   freq->write = write;
   freq->callback = callback;
   freq->userdata = userdata;
   freq->userdata2 = userdata2;

   gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (dialog)->cancel_button),
                       "clicked", (GtkSignalFunc) gtkutil_file_req_cancel, (gpointer) freq);
   gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (dialog)->ok_button),
                       "clicked", (GtkSignalFunc) gtkutil_file_req_done, (gpointer) freq);
   gtk_widget_show (dialog);
}


extern struct xchatprefs prefs;
extern GtkWidget *main_window;

GtkWidget *
gtkutil_window_new (char *title, char *wmclass, int x, int y, void *callback, gpointer userdata, int allowtab)
{
   GtkWidget *win;

   if (prefs.windows_as_tabs && main_window && allowtab)
      return maingui_new_tab (title, wmclass, callback, userdata);

   win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_wmclass (GTK_WINDOW (win), wmclass, "X-Chat");
   gtk_widget_set_usize (win, x, y);
   gtk_window_set_title (GTK_WINDOW (win), title);
   gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_MOUSE);
   gtk_signal_connect (GTK_OBJECT (win), "destroy",
                       GTK_SIGNAL_FUNC (callback), userdata);
   return win;
}

GtkWidget *
gtkutil_dialog_new (char *title, char *wmclass, void *callback, gpointer userdata)
{
   GtkWidget *dialog;

   dialog = gtk_dialog_new ();
   gtk_window_set_wmclass (GTK_WINDOW (dialog), wmclass, "X-Chat");
   gtk_window_set_title (GTK_WINDOW (dialog), title);
   gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
   gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
                       GTK_SIGNAL_FUNC (callback), userdata);

   return dialog;
}

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
gtkutil_button (GtkWidget * win, char *stock, char *labeltext,
                void *callback, gpointer userdata, GtkWidget * box)
{
   GtkWidget *button, *label, *hbox;

   hbox = gtk_hbox_new (0, 0);
   gtk_widget_show (hbox);

   button = gtk_button_new ();
   gtk_signal_connect (GTK_OBJECT (button), "clicked",
                       GTK_SIGNAL_FUNC (callback), userdata);
   gtk_widget_show (button);

   if (labeltext)
   {
      label = gtk_label_new (labeltext);
      gtk_container_add (GTK_CONTAINER (hbox), label);
      gtk_widget_show (label);
   }
   gtk_container_add (GTK_CONTAINER (button), hbox);
   if (box)
      gtk_container_add (GTK_CONTAINER (box), button);

   return button;
}

void
gtkutil_label_new (char *text, GtkWidget * box)
{
   GtkWidget *label = gtk_label_new (text);
   gtk_container_add (GTK_CONTAINER (box), label);
   gtk_widget_show (label);
}

GtkWidget *
gtkutil_entry_new (int max, GtkWidget * box, void *callback, gpointer userdata)
{
   GtkWidget *entry = gtk_entry_new_with_max_length (max);
   gtk_container_add (GTK_CONTAINER (box), entry);
   if (callback)
      gtk_signal_connect (GTK_OBJECT (entry), "changed",
                          GTK_SIGNAL_FUNC (callback), userdata);
   gtk_widget_show (entry);
   return entry;
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

void
gtkutil_set_icon (GdkWindow *window, GdkPixmap *pixmap, GdkBitmap *mask)
{
   Atom icon_atom;
   glong data[2];

   data[0] = ((GdkPixmapPrivate *)pixmap)->xwindow;
   data[1] = ((GdkPixmapPrivate *)mask)->xwindow;
   icon_atom = gdk_atom_intern ("KWM_WIN_ICON", FALSE);
   gdk_property_change (window, icon_atom, icon_atom,
                        32, GDK_PROP_MODE_REPLACE,
                        (guchar *)data, 2);
}

