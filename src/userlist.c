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
#include "xchat.h"
#include "fe-gtk.h"
#include "util.h"
#include "userlist.h"
#include <gdk_imlib.h>

#include "op.xpm"
#include "voice.xpm"

extern GdkColor colors[]; 
GdkPixmap *op_pixmap, *voice_pixmap;
GdkBitmap *op_mask_bmp, *voice_mask_bmp;

static GdkPixmap *
create_pixmap_from_data (GtkWidget *window, GdkBitmap **mask, GtkWidget *style_widget, char **data)
{
  GdkPixmap *pixmap = 0;

  gdk_imlib_data_to_pixmap (data, &pixmap, mask);

  return(pixmap);
}

void
init_userlist_xpm (void)
{
  op_pixmap = create_pixmap_from_data (session->gui->window, &op_mask_bmp, session->gui->window, op_xpm);
  voice_pixmap = create_pixmap_from_data (session->gui->window, &voice_mask_bmp, session->gui->window, voice_xpm);
}

static void
voice_myself (void)
{
   if (session->gui->op_xpm)
      gtk_widget_destroy (session->gui->op_xpm);
   session->gui->op_xpm = gtk_pixmap_new (voice_pixmap, voice_mask_bmp);
   gtk_box_pack_start (GTK_BOX (session->gui->op_box), session->gui->op_xpm, 0, 0, 0);
   gtk_widget_show (session->gui->op_xpm);
}

static void
op_myself (void)
{
   if (session->gui->op_xpm)
      gtk_widget_destroy (session->gui->op_xpm);
   session->gui->op_xpm = gtk_pixmap_new (op_pixmap, op_mask_bmp);
   gtk_box_pack_start (GTK_BOX (session->gui->op_box), session->gui->op_xpm, 0, 0, 0);
   gtk_widget_show (session->gui->op_xpm);
}

static void
fe_userlist_numbers (void)
{
   char tbuf[42];
   sprintf (tbuf, "%d ops, %d total", session->ops, session->total);
   gtk_label_set_text (GTK_LABEL (session->gui->namelistinfo), tbuf);
}

static void
fe_userlist_remove (struct user *user)
{
   gint row = gtk_clist_find_row_from_data (GTK_CLIST (session->gui->namelistgad), (gpointer) user);
   GtkAdjustment *adj;
   gfloat val, end;

   adj = gtk_clist_get_vadjustment (GTK_CLIST (session->gui->namelistgad));
   val = adj->value;

   gtk_clist_remove (GTK_CLIST (session->gui->namelistgad), row);

   end = adj->upper - adj->lower - adj->page_size;
   if (val > end)
      val = end;
   gtk_adjustment_set_value (adj, val);
}

static int
fe_userlist_insert (struct user *newuser, int row)
{
   char *name = newuser->nick;
   GtkAdjustment *adj;
   gfloat val;

   adj = gtk_clist_get_vadjustment (GTK_CLIST (session->gui->namelistgad));
   val = adj->value;

   if (row == -1)
      row = gtk_clist_append (GTK_CLIST (session->gui->namelistgad), &name);
   else
      gtk_clist_insert (GTK_CLIST (session->gui->namelistgad), row, &name);
   gtk_clist_set_row_data (GTK_CLIST (session->gui->namelistgad), row, (gpointer) newuser);
   /*  gtk_clist_set_selectable (GTK_CLIST (session->gui->namelistgad), row, FALSE); */
   if (!strcmp (newuser->nick, server->nick))
   {
      if (newuser->op)
         op_myself ();
      else if (newuser->voice)
         voice_myself ();
      if (!newuser->voice && !newuser->op)
      {
         if (session->gui->op_xpm)
         {
            gtk_widget_destroy (session->gui->op_xpm);
            session->gui->op_xpm = 0;
         }
      }
   }
   if (newuser->op)
      gtk_clist_set_pixtext ((GtkCList *) session->gui->namelistgad, row, 0, newuser->nick, 3, op_pixmap, op_mask_bmp);
   else if (newuser->voice)
         gtk_clist_set_pixtext ((GtkCList *) session->gui->namelistgad, row, 0, newuser->nick, 3, voice_pixmap, voice_mask_bmp);

   gtk_adjustment_set_value (adj, val);

   return row;
}

static void
fe_userlist_move (struct user *user, int new_row)
{
   gint old_row;

   old_row = gtk_clist_find_row_from_data (GTK_CLIST (session->gui->namelistgad), (gpointer) user);
   gtk_clist_remove (GTK_CLIST (session->gui->namelistgad), old_row);
   new_row = fe_userlist_insert (user, new_row);
}

static int
nick_cmp_az_ops (struct user *user1, struct user *user2)
{
   if (user1->op && !user2->op)
      return -1;
   if (!user1->op && user2->op)
      return +1;

   if (user1->voice && !user2->voice)
      return -1;
   if (!user1->voice && user2->voice)
      return +1;

   return strcasecmp (user1->nick, user2->nick);
}

void
clear_user_list (void)
{
   struct user *user;
   GSList *list = session->userlist;

   gtk_clist_clear (GTK_CLIST (session->gui->namelistgad));
   while (list)
   {
      user = (struct user *)list->data;
      free (user);
      list = g_slist_remove (list, user);
   }
   session->userlist = 0;
   session->ops = 0;
   session->total = 0;
}

struct user *
find_name (char *name)
{
   struct user *user;
   GSList *list;

   list = session->userlist;
   while (list)
   {
      user = (struct user *)list -> data;
      if (!strcasecmp (user->nick, name))
         return user;
      list = list->next;
   }
   return FALSE;
}

static int
userlist_insertname_sorted (struct user *newuser)
{
   int row = 0;
   struct user *user;
   GSList *list = session->userlist;

   while (list)
   {
      user = (struct user *)list -> data;
      if (nick_cmp_az_ops (newuser, user) < 1)
      {
         session->userlist = g_slist_insert (session->userlist, newuser, row);
         return row;
      }
      row++;
      list = list -> next;
   }
   session->userlist = g_slist_append (session->userlist, newuser);
   return -1;
}

static void
update_entry (struct user *user)
{
   int row;

   session->userlist = g_slist_remove (session->userlist, user);
   row = userlist_insertname_sorted (user);

   fe_userlist_move (user, row);
   fe_userlist_numbers ();
}

void
ul_op_name (char *name)
{
   struct user *user = find_name (name);
   if (user && !user->op)
   {
      session->ops++;
      user->op = TRUE;
      update_entry (user);
   }
}

void
deop_name (char *name)
{
   struct user *user = find_name (name);
   if (user && user->op)
   {
      session->ops--;
      user->op = FALSE;
      update_entry (user);
   }
}

void
voice_name (char *name)
{
   struct user *user = find_name (name);
   if (user)
   {
      user->voice = TRUE;
      update_entry (user);
   }
}

void
devoice_name (char *name)
{
   struct user *user = find_name (name);
   if (user)
   {
      user->voice = FALSE;
      update_entry (user);
   }
}

void
change_nick (char *oldname, char *newname)
{
   struct user *user = find_name (oldname);
   if (user)
   {
      strcpy (user->nick, newname);
      update_entry (user);
   }
}

int
sub_name (char *name)
{
   struct user *user;

   user = find_name (name);
   if (!user)
      return FALSE;

   if (user->op)
      session->ops--;
   session->total--;
   fe_userlist_numbers ();
   fe_userlist_remove (user);

   free (user);
   session->userlist = g_slist_remove (session->userlist, user);

   return TRUE;
}

int
add_name (char *name)
{
   struct user *user;
   int row;

   user = malloc (sizeof (struct user));
   memset (user, 0, sizeof (struct user));

   /* filter out UnrealIRCD stuff */
   while (*name == '~' || *name == '*' || *name == '%' || *name == '.')
      name++;

   if (*name == '@')
   {
      name++;
      user->op = TRUE;
      session->ops++;
   }
   session->total++;
   if (*name == '+')
   {
      name++;
      user->voice = TRUE;
   }

   strcpy (user->nick, name);
   row = userlist_insertname_sorted (user);

   fe_userlist_insert (user, row);
   fe_userlist_numbers ();

   return row;
}
