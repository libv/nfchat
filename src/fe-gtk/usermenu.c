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
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <unistd.h>
#include "../common/xchat.h"
#include "fe-gtk.h"
#include "../common/popup.h"


extern void menu_quick_item (char *cmd, char *label, GtkWidget * menu, int flags, gpointer userdata);

extern GSList *usermenu_list;
extern GSList *sess_list;
extern session *menu_sess;


void
usermenu_create (GtkWidget *menu)
{
   GSList *list = usermenu_list;
   struct popup *pop;

   while (list)
   {
      pop = (struct popup *) list->data;

      if (!strncasecmp (pop->name, "SEP", 3))
         menu_quick_item (0, 0, menu, 1, "");
      else
         menu_quick_item (pop->cmd, pop->name, menu, 0, "");

      list = list->next;
   }
}

static void
usermenu_destroy (GtkWidget *menu)
{
   GList *items = ((GtkMenuShell*)menu)->children;
   GList *next;

   items = items->next->next; /* don't destroy the 1st 2 items */

   while (items)
   {
      next = items -> next;
      gtk_widget_destroy (items->data);
      items = next;
   }
}

void
usermenu_update (void)
{
   int done_main = FALSE;
   GSList *list = sess_list;
   session *sess;

   while (list)
   {
      sess = list -> data;
      if (sess->is_tab)
      {
         if (!done_main)
         {
            if (sess->gui->usermenu)
            {
               usermenu_destroy (sess->gui->usermenu);
               usermenu_create (sess->gui->usermenu);
            }
         } else
            done_main = TRUE;
      } else
      {
         usermenu_destroy (sess->gui->usermenu);
         usermenu_create (sess->gui->usermenu);
      }
      list = list -> next;
   }
}

