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
#include <time.h>
#include "../common/xchat.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#include "../common/notify.h"
#include "../common/cfgfiles.h"
#include "../common/plugin.h"
#include "../common/util.h"

extern void update_all_of (char *name);
extern GSList *serv_list;
extern GdkColor colors[];

extern GSList *notify_list;
static GtkWidget *notify_window = 0;
static GtkWidget *notify_guilist;

void notify_adduser (char *name);
int notify_deluser (char *name);


static int
notify_closegui (void)
{
   notify_window = 0;
   return 0;
}

void
notify_gui_update (void)
{
   struct notify *notify;
   struct notify_per_server *servnot;
   GSList *list = notify_list;
   GSList *slist;
   gchar *nnew[4];
   int row, crow, online, servcount;
   time_t lastseen;

   if (!notify_window)
      return;

   row = gtkutil_clist_selection (notify_guilist);

   gtk_clist_clear ((GtkCList *) notify_guilist);
   while (list)
   {
      notify = (struct notify *) list->data;
      nnew[0] = notify->name;

      online = FALSE;
      lastseen = 0;
      /* First see if they're online on any servers */
      slist = notify->server_list;
      while (slist)
      {
         servnot = (struct notify_per_server *) slist->data;
         if (servnot->ison)
         {
            online = TRUE;
            if (servnot->laston > lastseen)
               lastseen = servnot->lastseen;
         }
         slist = slist->next;
      }

      if (!online)              /* Offline on all servers */
      {
         nnew[1] = "Offline";
         nnew[2] = "";
         if (!lastseen)
            nnew[3] = "Never";
         else
            nnew[3] = ctime (&lastseen);
         crow = gtk_clist_append ((GtkCList *) notify_guilist, nnew);
         gtk_clist_set_foreground (GTK_CLIST (notify_guilist),
                    crow, &colors[4]);
      } else
      {                         /* Online - add one line per server */
         servcount = 0;
         slist = notify->server_list;
         while (slist)
         {
            servnot = (struct notify_per_server *) slist->data;
            if (servnot->ison)
            {
               if (servcount == 0)
                  nnew[0] = notify->name;
               else
                  nnew[0] = "";
               nnew[1] = "Online";
               nnew[2] = servnot->server->servername;
               nnew[3] = ctime (&servnot->laston);
               crow = gtk_clist_append ((GtkCList *) notify_guilist, nnew);
               gtk_clist_set_foreground (GTK_CLIST (notify_guilist),
                    crow, &colors[3]);
               servcount++;
            }
            slist = slist->next;
         }
      }
      list = list->next;
   }
   if (row != -1)
      gtk_clist_select_row ((GtkCList *) notify_guilist, row, 0);
}

static void
notify_remove_clicked (GtkWidget *igad)
{
   int row;
   char *name;

   row = gtkutil_clist_selection (notify_guilist);
   if (row != -1)
   {
      gtk_clist_get_text (GTK_CLIST (notify_guilist), row, 0, &name);
      notify_deluser (name);
   }
}

static void
notify_add_clicked (GtkWidget *igad)
{
   gtkutil_simpledialog ("Use the /NOTIFY command to add people to your notify list.");
}

void
notify_opengui (void)
{
   GtkWidget *vbox, *bbox, *Remove, *Add;
   static gchar *titles[] =
   {"User", "Status", "Server", "Last Seen"};

   if (notify_window)
   {
      gdk_window_show (notify_window->window);
      return;
   }
   notify_window = gtkutil_window_new ("X-Chat: Notify List", "notify", 400, 120,
                                       notify_closegui, 0, TRUE);

   vbox = gtk_vbox_new (FALSE, 0);
   gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
   gtk_container_add (GTK_CONTAINER (notify_window), vbox);
   gtk_widget_show (vbox);

   notify_guilist = gtkutil_clist_new (4, titles, vbox, GTK_POLICY_ALWAYS,
                                 0, 0,
          0, 0, GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width (GTK_CLIST (notify_guilist), 0, 100);
   gtk_clist_set_column_width (GTK_CLIST (notify_guilist), 1, 60);
   gtk_clist_set_column_width (GTK_CLIST (notify_guilist), 2, 100);
   gtk_clist_set_auto_sort (GTK_CLIST (notify_guilist), FALSE);

   bbox = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
   gtk_widget_show (bbox);

   Add = gtkutil_button (notify_window, 0, "Add", notify_add_clicked, 0, bbox);
   Remove = gtkutil_button (notify_window, 0, "Remove", notify_remove_clicked, 0, bbox);

   notify_gui_update ();

   gtk_widget_show (notify_window);
}
