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
#include "themes.h"
#include "../common/xchat.h"
#include "fe-gtk.h"
#include "../common/util.h"
#include "../common/userlist.h"
#include "gtkutil.h"

extern GdkColor colors[];
extern struct xchatprefs prefs;
extern int notify_isnotify (struct session *sess, char *name);
extern void dcc_send (struct session *sess, char *tbuf, char *to, char *file);
extern GdkPixmap *theme_pixmap (GtkWidget *window, GdkBitmap **mask, GtkWidget *style_widget, int theme);

GdkPixmap *op_pixmap, *voice_pixmap;
GdkBitmap *op_mask_bmp, *voice_mask_bmp;


void
init_userlist_xpm (struct session *sess)
{
   op_pixmap = theme_pixmap (sess->gui->window, &op_mask_bmp, 
			     sess->gui->window, THEME_OP_ICON);
   voice_pixmap = theme_pixmap (sess->gui->window, &voice_mask_bmp, 
				sess->gui->window, THEME_VOICE_ICON);
}

void
voice_myself (struct session *sess)
{
   if (sess->gui->op_xpm)
      gtk_widget_destroy (sess->gui->op_xpm);
   sess->gui->op_xpm = gtk_pixmap_new (voice_pixmap, voice_mask_bmp);
   gtk_box_pack_start (GTK_BOX (sess->gui->op_box), sess->gui->op_xpm, 0, 0, 0);
   gtk_widget_show (sess->gui->op_xpm);
}

void
op_myself (struct session *sess)
{
   if (sess->gui->op_xpm)
      gtk_widget_destroy (sess->gui->op_xpm);
   sess->gui->op_xpm = gtk_pixmap_new (op_pixmap, op_mask_bmp);
   gtk_box_pack_start (GTK_BOX (sess->gui->op_box), sess->gui->op_xpm, 0, 0, 0);
   gtk_widget_show (sess->gui->op_xpm);
}

void
fe_userlist_numbers (struct session *sess)
{
   char tbuf[42];
   sprintf (tbuf, "%d ops, %d total", sess->ops, sess->total);
   gtk_label_set_text (GTK_LABEL (sess->gui->namelistinfo), tbuf);
}

void
fe_userlist_remove (struct session *sess, struct user *user)
{
   gint row = gtk_clist_find_row_from_data (GTK_CLIST (sess->gui->namelistgad),
                                            (gpointer) user);
   GtkAdjustment *adj;
   gfloat val, end;

   adj = gtk_clist_get_vadjustment (GTK_CLIST (sess->gui->namelistgad));
   val = adj->value;

   gtk_clist_remove (GTK_CLIST (sess->gui->namelistgad), row);

   end = adj->upper - adj->lower - adj->page_size;
   if (val > end)
      val = end;
   gtk_adjustment_set_value (adj, val);
}

int
fe_userlist_insert (struct session *sess, struct user *newuser, int row)
{
   char *name = newuser->nick;
   GtkAdjustment *adj;
   gfloat val;

   adj = gtk_clist_get_vadjustment (GTK_CLIST (sess->gui->namelistgad));
   val = adj->value;

   if (row == -1)
      row = gtk_clist_append (GTK_CLIST (sess->gui->namelistgad), &name);
   else
      gtk_clist_insert (GTK_CLIST (sess->gui->namelistgad), row, &name);
   gtk_clist_set_row_data (GTK_CLIST (sess->gui->namelistgad), row, (gpointer) newuser);

   if (!strcmp (newuser->nick, sess->server->nick))
   {
      if (newuser->op)
         op_myself (sess);
      else if (newuser->voice)
         voice_myself (sess);
      if (!newuser->voice && !newuser->op)
      {
         if (sess->gui->op_xpm)
         {
            gtk_widget_destroy (sess->gui->op_xpm);
            sess->gui->op_xpm = 0;
         }
      }
   }
   if (newuser->op)
   {
      gtk_clist_set_pixtext ((GtkCList *) sess->gui->namelistgad, row, 0,
                             newuser->nick, 3, op_pixmap, op_mask_bmp);
   } else if (newuser->voice)
   {
         gtk_clist_set_pixtext ((GtkCList *) sess->gui->namelistgad, row, 0,
                                newuser->nick, 3, voice_pixmap, voice_mask_bmp);
   }

   if (prefs.hilitenotify && notify_isnotify (sess, name))
   {
      gtk_clist_set_foreground ((GtkCList *) sess->gui->namelistgad, row,
             &colors[prefs.nu_color]);
   }

   gtk_adjustment_set_value (adj, val);

   return row;
}

void
fe_userlist_move (struct session *sess, struct user *user, int new_row)
{
   gint old_row;
   int sel = FALSE;

   old_row = gtk_clist_find_row_from_data (GTK_CLIST (sess->gui->namelistgad), (gpointer) user);

   if (old_row == gtkutil_clist_selection (sess->gui->namelistgad))
      sel = TRUE;

   gtk_clist_remove (GTK_CLIST (sess->gui->namelistgad), old_row);
   new_row = fe_userlist_insert (sess, user, new_row);

   if (sel)
      gtk_clist_select_row ((GtkCList *) sess->gui->namelistgad, new_row, 0);
}

void
fe_userlist_clear (struct session *sess)
{
   gtk_clist_clear (GTK_CLIST (sess->gui->namelistgad));
}

#ifdef USE_GNOME

void
userlist_dnd_drop (GtkWidget * widget, GdkDragContext * context,
                   gint x, gint y,
    GtkSelectionData * selection_data,
             guint info, guint32 time,
                 struct session *sess)
{
   struct user *user;
   char tbuf[256];
   char *file;
   int row, col;
   GList *list;

   if (gtk_clist_get_selection_info (GTK_CLIST (widget), x, y, &row, &col) < 0)
      return;

   user = gtk_clist_get_row_data (GTK_CLIST (widget), row);
   if (!user)
      return;
   list = gnome_uri_list_extract_filenames (selection_data->data);
   while (list)
   {
      file = (char *) (list->data);
      dcc_send (sess, tbuf, user->nick, file);
      list = list->next;
   }
   gnome_uri_list_free_strings (list);
}

int
userlist_dnd_motion (GtkWidget * widget, GdkDragContext * context, gint x, gint y, guint ttime)
{
   int row, col;

   if (gtk_clist_get_selection_info (GTK_CLIST (widget), x, y, &row, &col) != -1)
   {
      gtk_clist_select_row (GTK_CLIST (widget), row, col);
   }
   return 1;
}

int
userlist_dnd_leave (GtkWidget * widget, GdkDragContext * context, guint ttime)
{
   gtk_clist_unselect_all (GTK_CLIST (widget));
   return 1;
}

#endif


