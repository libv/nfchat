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
#include "../common/xchat.h"
#include "fe-gtk.h"
#include "../common/util.h"
#include "../common/userlist.h"
#include "gtkutil.h"
#ifdef USE_IMLIB
#include <gdk_imlib.h>
#endif

#include "../pixmaps/op.xpm"
#include "../pixmaps/voice.xpm"

extern GdkColor colors[]; 
extern struct xchatprefs prefs;

GdkPixmap *op_pixmap, *voice_pixmap;
GdkBitmap *op_mask_bmp, *voice_mask_bmp;

/*
 * create_pixmap_from_data [STATIC]
 *
 * uses IMLIB or Gtk functions to render images
 *
 */
static GdkPixmap *
create_pixmap_from_data (GtkWidget *window, GdkBitmap **mask, GtkWidget *style_widget, char **data)
{
  GdkPixmap *pixmap = 0;
#ifndef USE_IMLIB
  GtkStyle *style;
  if (!style_widget)
    style = gtk_widget_get_default_style ();
  else
    style = gtk_widget_get_style (style_widget);
  pixmap = gdk_pixmap_create_from_xpm_d (window->window, mask,
                                         &style->bg[GTK_STATE_NORMAL], data);
#else
  gdk_imlib_data_to_pixmap (data, &pixmap, mask);
#endif 
  return(pixmap);
}

void
init_userlist_xpm (struct session *sess)
{
  op_pixmap = create_pixmap_from_data (sess->gui->window, &op_mask_bmp, 
			     sess->gui->window, op_xpm);
  voice_pixmap = create_pixmap_from_data (sess->gui->window, &voice_mask_bmp, 
				sess->gui->window, voice_xpm);
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
