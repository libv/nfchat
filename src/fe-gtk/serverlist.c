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
#include <pwd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "../common/xchat.h"
#include "../common/cfgfiles.h"
#include "../common/fe.h"
#include "fe-gtk.h"
#include "serverlist.h"
#include "gtkutil.h"
#include "themes.h"

extern struct xchatprefs prefs;

extern void connect_server (struct session *sess, char *server, int port, int quiet);
extern struct session *new_session (struct server *serv, char *from);
extern struct server *new_server (void);
extern GdkPixmap *theme_pixmap (GtkWidget *window, GdkBitmap **mask, GtkWidget *style_widget, int theme);

static GtkWidget *slwin = 0;
static GtkWidget *sleditwin = 0;
static GtkWidget *entry_server;
static GtkWidget *entry_channel;
static GtkWidget *quickview;
static GtkWidget *entry_port;
static GtkWidget *entry_comment;
static GtkWidget *entry_password;
static GtkWidget *entry_autoconnect;
static GtkWidget *nick1gad;
static GtkWidget *nick2gad;
static GtkWidget *nick3gad;
static GtkWidget *realnamegad;
static GtkWidget *usernamegad;
static GtkWidget *sltree;
static GtkWidget *button_connnew;
static GtkWidget *button_conn;
static GtkWidget *button_chan_new;

static GdkPixmap *pixmap_group;
static GdkBitmap *mask_group;
static GdkPixmap *pixmap_group_expanded;
static GdkBitmap *mask_group_expanded;
static GdkPixmap *pixmap_server;
static GdkBitmap *mask_server;

static GSList *server_malloc_list = 0;
static GSList *server_parents = 0;
static GtkCTreeNode *editnode = 0;

static struct slentry *editentry = 0;
static int previous_depth = 0;
static int closing_list = 0;
static gboolean channelbox = 0;
static gboolean first_autoconnect = 0;
static gboolean new_entry = 0;

/**************************
 * Public functions:      *
 * -----------------      *
 * open_server_list       *
 * serverlist_autoconnect *
 **************************/

/* 
 *  is_group: 
 *  Returns TRUE or FALSE if a TreeNode is a group or not
 */
static int
is_group (GtkCTreeNode * selected)
{
   struct slentry *conn;

   if (!selected)
      return 0;
   conn = gtk_ctree_node_get_row_data (GTK_CTREE (sltree), selected);
   if (!strcmp (conn->server, "SUB"))
      return 1;
   else
      return 0;
}

/*
 * add_server_entry:
 * takes a struct slentry as argument and parses + inserts it into the tree (and mem)
 */
static void
add_server_entry (struct slentry *serv)
{
   gchar *text[] = {""};
   struct slentry *new = 0;
   GtkCTreeNode *node = 0;
   GtkCTreeNode *parent = 0;
   GdkColor autoconnect_color = {0, 0x0000, 0x8000, 0x0000};

   if (g_slist_last (server_parents))
      parent = ((g_slist_last (server_parents))->data);

   if (!strcmp ("SUB", serv->server))
   {
      new = malloc (sizeof (struct slentry));
      text[0] = new->comment;
      strcpy (new->comment, serv->comment);
      strcpy (new->server, serv->server);
      strcpy (new->channel, "");
      strcpy (new->password, "");
      new->autoconnect = FALSE;
      new->port = 0;
      if (serv->port)
         new->port = 1;
      node = gtk_ctree_insert_node (GTK_CTREE (sltree), parent,
                                    NULL, text, 4, pixmap_group, mask_group,
                                    pixmap_group_expanded, mask_group_expanded, FALSE, FALSE);

      if (new->port)
         gtk_ctree_expand (GTK_CTREE (sltree), node);
      else
         gtk_ctree_collapse (GTK_CTREE (sltree), node);
      gtk_ctree_node_set_row_data (GTK_CTREE (sltree), node, new);
      server_parents = g_slist_append (server_parents, node);
      server_malloc_list = g_slist_prepend (server_malloc_list, new);
   } 
   else if (!strcmp ("ENDSUB", serv->server) && g_slist_last (server_parents))
     server_parents = g_slist_remove (server_parents,
				      ((g_slist_last (server_parents))->data));
   else
     {
      new = malloc (sizeof (struct slentry));
      new->port = serv->port;
      new->autoconnect = serv->autoconnect;
      strcpy (new->comment, serv->comment);
      strcpy (new->channel, serv->channel);
      strcpy (new->server, serv->server);
      strcpy (new->password, serv->password);
      text[0] = new->comment;
      node = gtk_ctree_insert_node (GTK_CTREE (sltree), parent,
                                    NULL, text, 4, pixmap_server, mask_server,
                                    pixmap_server, mask_server, TRUE, FALSE);
      gtk_ctree_node_set_row_data (GTK_CTREE (sltree), node, new);
      server_malloc_list = g_slist_prepend (server_malloc_list, new);
      if (new->autoconnect)
	gtk_ctree_node_set_foreground (GTK_CTREE (sltree), node, &autoconnect_color);
   }
}

/* 
 *  add_defaults:
 *  called if ~/.xchat/serverlist.conf isn't found
 */
static void
add_defaults (void)
{
   struct slentry serv;
   int i = 0;

   serv.password[0] = 0;
   while (dserv[i].server != 0)
   {
      strcpy (serv.channel, dserv[i].channel);
      strcpy (serv.server, dserv[i].server);
      strcpy (serv.comment, dserv[i].comment);
      serv.port = dserv[i].port;
      serv.autoconnect = dserv[i].autoconnect;
      add_server_entry (&serv);
      i++;
   }
}

/*
 * read_next_server_entry:
 * reads lines from ~/.xchat/serverlist.conf until it has filled a server entry
 */
static char *
read_next_server_entry (char *my_cfg, struct slentry *serv)
{
   if (my_cfg)
      my_cfg = cfg_get_str (my_cfg, "servername ", serv->server);
   if (my_cfg)
   {
      my_cfg = cfg_get_str (my_cfg, "port ", serv->channel);
      serv->port = atoi (serv->channel);
   }
   if (my_cfg)
   {
      my_cfg = cfg_get_str (my_cfg, "autoconnect ", serv->channel);
      serv->autoconnect = atoi (serv->channel);
   }
   if (my_cfg)
      my_cfg = cfg_get_str (my_cfg, "channel ", serv->channel);
   if (my_cfg)
      my_cfg = cfg_get_str (my_cfg, "password ", serv->password);
   if (my_cfg)
      my_cfg = cfg_get_str (my_cfg, "comment ", serv->comment);

   return my_cfg;
}

/*
 * load_serverentrys:
 * opens the serverlist.conf file and parses it
 */
static void
load_serverentrys (void)
{
   struct slentry serv;
   struct stat st;
   int fh;
   char *cfg, *my_cfg;
   char file[256];
   char buf[10];

   snprintf (file, sizeof file, "%s/serverlist.conf", get_xdir ());
   fh = open (file, O_RDONLY);
   if (fh != -1)
   {
      fstat (fh, &st);
      if (st.st_size == 0)
      {
         add_defaults ();
      } else
      {
         cfg = malloc (st.st_size);
         read (fh, cfg, st.st_size);
         my_cfg = cfg;
         my_cfg = cfg_get_str (my_cfg, "connecttab ", &buf[0]); /* legacy */
         if (!my_cfg)
            my_cfg = cfg;
         my_cfg = cfg_get_str (my_cfg, "channelbox ", &buf[0]);
         channelbox = atoi (buf);
         while ((my_cfg = read_next_server_entry (my_cfg, &serv)))
         {
            add_server_entry (&serv);
         }
         free (cfg);
         while (g_slist_last (server_parents))
            server_parents = g_slist_remove (server_parents,
                                             ((g_slist_last (server_parents))->data));
      }
      close (fh);
   } else
   {
      add_defaults ();
   }

   gtk_clist_select_row (GTK_CLIST (sltree), 1, 0);
   GTK_CLIST (sltree) -> focus_row = 1;
}

/*
 * write_serverentry:
 * takes a serverentry and a filehandle and writes the entry to the file
 */
static void
write_serverentry (struct slentry *entry, int fh)
{
   char buf[1024];

   snprintf (buf, sizeof buf,
             "servername = %s\n"
             "port = %d\n"
             "autoconnect = %d\n"
             "channel = %s\n"
             "password = %s\n"
             "comment = %s\n\n",
             entry->server, entry->port, entry->autoconnect,
             entry->channel, entry->password, entry->comment);
   write (fh, buf, strlen (buf));
}

/*
 * parse_serverentrys:
 * used by save_serverentrys to parse the servers currently in mem before writing
 */
static void
parse_serverentrys (GtkCTree * ctree, GtkCTreeNode * node, int *fh)
{
   struct slentry *temporary = gtk_ctree_node_get_row_data (GTK_CTREE (sltree), node);
   struct slentry *buf = malloc (sizeof (struct slentry));
   GtkCTreeNode *tempus = node;
   int diff = 0, depth = 0;

   while ((GTK_CTREE_ROW(tempus))->parent)
   {
      depth++;
      tempus = GTK_CTREE_NODE((GTK_CTREE_ROW(tempus))->parent);
   }

   diff = depth - previous_depth;
   previous_depth = depth;
   temporary = gtk_ctree_node_get_row_data (GTK_CTREE (sltree), node);

   while (diff < 0)
   {
      buf->port = 0;
      buf->autoconnect = 0;
      *(buf->channel) = 0;
      *(buf->password) = 0;
      *(buf->comment) = 0;
      strcpy (buf->server, "ENDSUB");
      write_serverentry (buf, *fh);
      diff++;
   }

   strcpy (buf->channel, temporary->channel);
   strcpy (buf->server, temporary->server);
   strcpy (buf->password, temporary->password);
   strcpy (buf->comment, temporary->comment);
   buf->port = temporary->port;
   buf->autoconnect = temporary->autoconnect;
   write_serverentry (buf, *fh);

   if (!strcmp (buf->server, "SUB") && (((GTK_CTREE_ROW(node))->children)) == 0)
     {
       /* If we got a group without any children we need an ENDSUB at least */
       buf->port = 0;
       buf->autoconnect = 0;
       *(buf->channel) = 0;
       *(buf->password) = 0;
       *(buf->comment) = 0;
       strcpy (buf->server, "ENDSUB");
       write_serverentry (buf, *fh);
     }
   free (buf);
}

/*
 * save_serverentrys:
 * function called elsewhere whenever we want to dump contents in mem to disk
 */
static void
save_serverentrys (void)
{
   int fh;
   char file[512];
   char buf[256];

   check_prefs_dir ();

   snprintf (file, sizeof file, "%s/serverlist.conf", get_xdir ());
   fh = open (file, O_TRUNC | O_WRONLY | O_CREAT, 0600);
   if (fh != -1)
   {
      previous_depth = 0;
      snprintf (buf, sizeof buf,
                "connecttab = 1\n\n"         /* legacy */
                "channelbox = %d\n\n",
                channelbox);
      write (fh, buf, strlen (buf));
      gtk_ctree_pre_recursive (GTK_CTREE (sltree), NULL,
                               GTK_CTREE_FUNC (parse_serverentrys), &fh);
      close (fh);
   }
}

/*
 * reload_servers:
 * little cute function to dump mem to disk, free mem and reload from disk
 */
static void
reload_servers (void)
{
   if (sleditwin)
   {
      gtk_widget_destroy (sleditwin);
      sleditwin = 0;
   }
   if (sltree)
   {
      closing_list = 1;
      gtk_clist_freeze (GTK_CLIST (sltree));
      save_serverentrys ();
      while (gtk_ctree_node_nth (GTK_CTREE (sltree), 0))
         gtk_ctree_remove_node (GTK_CTREE (sltree),
                                gtk_ctree_node_nth (GTK_CTREE (sltree), 0));
      while (g_slist_last (server_malloc_list))
      {
         free ((g_slist_last (server_malloc_list))->data);
         server_malloc_list = g_slist_remove (server_malloc_list,
                                              ((g_slist_last (server_malloc_list))->data));
      }
      load_serverentrys ();
      gtk_clist_thaw (GTK_CLIST (sltree));
      closing_list = 0;
   }
}

/*
 * delete_server_clicked:
 * called when the "delete" button is clicked in server list GUI
 */
static void
delete_server_clicked (GtkWidget * wid, struct session *sess)
{
   GtkCTreeNode *selected = 0;

   selected = GTK_CTREE_NODE (g_list_nth (GTK_CLIST (sltree)->row_list,
      GTK_CLIST (sltree)->focus_row));
   if (selected)
   {
      gtk_ctree_unselect (GTK_CTREE (sltree), selected);
      gtk_ctree_remove_node (GTK_CTREE (sltree), selected);
      reload_servers ();
   }
}

/*
 * close_edit_entry:
 * called when we want to destroy the edit entry win
 */
static void
close_edit_entry (void)
{
   if (sleditwin)
   {
      gtk_widget_destroy (GTK_WIDGET (sleditwin));
      sleditwin = 0;
   }
   if (new_entry)
     gtk_ctree_remove_node (GTK_CTREE (sltree), editnode);
   if (sltree)
     reload_servers ();
   sleditwin = 0;
   editentry = 0;
   editnode = 0;
   new_entry = 0;
}

/*
 * done_edit_entry:
 * called when were done editing an entry
 */
static void
done_edit_entry (void)
{
   if (!sleditwin)
      return;
   if (!editentry)
      return;
   if (!editnode)
      return;
   if (!is_group (editnode))
   {
      strcpy (editentry->server, gtk_entry_get_text (GTK_ENTRY (entry_server)));
      strcpy (editentry->channel, gtk_entry_get_text (GTK_ENTRY (entry_channel)));
      strcpy (editentry->password, gtk_entry_get_text (GTK_ENTRY (entry_password)));
      editentry->port = atoi (gtk_entry_get_text (GTK_ENTRY (entry_port)));
      if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (entry_autoconnect)))
         editentry->autoconnect = FALSE;
      else
         editentry->autoconnect = TRUE;
   }
   strcpy (editentry->comment, gtk_entry_get_text (GTK_ENTRY (entry_comment)));
   new_entry = 0;
   close_edit_entry ();
}

/*
 * edit_entry_clicked:
 * called when the users clicks on the "edit" button in the serverlist GUI
 */
static void
edit_entry_clicked (GtkWidget * wid, GtkCTreeNode * temp)
{
   char portbuffer[16];
   GtkWidget *vbox, *hbox, *box1, *box2,
            *frame, *wid1;

   if (sleditwin)
   {
      close_edit_entry ();
      return;
   }
   if (temp)
      editnode = temp;
   else
      editnode = GTK_CTREE_NODE (g_list_nth (GTK_CLIST (sltree)->row_list,
      GTK_CLIST (sltree)->focus_row));
   if (!editnode)
      return;

   sleditwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title (GTK_WINDOW (sleditwin), "X-Chat: Edit entry");
   gtk_signal_connect (GTK_OBJECT (sleditwin), "delete_event",
                       GTK_SIGNAL_FUNC (close_edit_entry), 0);
   editentry = (struct slentry *) gtk_ctree_node_get_row_data (GTK_CTREE (sltree),
                            editnode);
   vbox = gtk_vbox_new (FALSE, 0);
   gtk_container_add (GTK_CONTAINER (sleditwin), vbox);
   gtk_container_border_width (GTK_CONTAINER (sleditwin), 5);
   gtk_widget_show (vbox);

   hbox = gtk_hbox_new (TRUE, 0);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
   gtk_widget_show (hbox);
   if (is_group (editnode))
      wid1 = gtk_pixmap_new (pixmap_group, mask_group);
   else
      wid1 = gtk_pixmap_new (pixmap_server, mask_server);
   gtk_box_pack_start (GTK_BOX (hbox), wid1, FALSE, FALSE, 0);
   gtk_widget_show (wid1);

   frame = gtk_frame_new ("Edit entry:");
   gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
   if (!is_group (editnode))
      gtk_widget_set_usize (frame, 400, 150);
   else
      gtk_widget_set_usize (frame, 400, 70);
   gtk_widget_show (frame);

   hbox = gtk_hbox_new (FALSE, 0);
   gtk_container_add (GTK_CONTAINER (frame), hbox);
   gtk_widget_show (hbox);

   box1 = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox), box1, FALSE, FALSE, 0);
   gtk_widget_show (box1);

   box2 = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox), box2, TRUE, TRUE, 0);
   gtk_widget_show (box2);

   gtkutil_label_new ("Name:", box1);
   entry_comment = gtkutil_entry_new (99, box2, NULL, NULL);
   gtk_entry_set_text (GTK_ENTRY (entry_comment), editentry->comment);

   if (!is_group (editnode))
   {
      gtkutil_label_new ("Server:", box1);
      entry_server = gtkutil_entry_new (131, box2, NULL, NULL);
      gtk_entry_set_text (GTK_ENTRY (entry_server), editentry->server);

      sprintf (portbuffer, "%d", editentry->port);
      gtkutil_label_new ("Port:", box1);
      entry_port = gtkutil_entry_new (5, box2, NULL, NULL);
      gtk_entry_set_text (GTK_ENTRY (entry_port), portbuffer);

      gtkutil_label_new ("Password:", box1);
      entry_password = gtkutil_entry_new (85, box2, NULL, NULL);
      gtk_entry_set_text (GTK_ENTRY (entry_password), editentry->password);

      gtkutil_label_new ("Channels:", box1);
      entry_channel = gtkutil_entry_new (201, box2, NULL, NULL);
      gtk_entry_set_text (GTK_ENTRY (entry_channel), editentry->channel);
      gtk_widget_show (entry_channel);

      box1 = gtk_hbox_new (FALSE, 0);
      entry_autoconnect = gtk_check_button_new_with_label ("Autoconnect");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (entry_autoconnect),
            (editentry->autoconnect));
      gtk_box_pack_start (GTK_BOX (box1), entry_autoconnect, TRUE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), box1, TRUE, TRUE, 0);
      gtk_widget_show (entry_autoconnect);
      gtk_widget_show (box1);
   }
   box1 = gtk_hbox_new (TRUE, 2);

   gtkutil_button (slwin, 0, "Ok", done_edit_entry, 0, box1);
   gtkutil_button (slwin, 0, "Cancel", close_edit_entry, 0, box1);

   gtk_box_pack_start (GTK_BOX (vbox), box1, TRUE, TRUE, 0);
   gtk_widget_show (box1);
   gtk_widget_show (sleditwin);
}

/*
 * new_server_clicked:
 * called when the user clicks the "New Server" button
 */
static void
new_server_clicked (GtkWidget * wid, struct session *sess)
{
   struct slentry *new = 0;
   GtkCTreeNode *node;
   GtkCTreeNode *sibling;
   GtkCTreeNode *parent = 0;
   gchar *text[] =
   {""};

   parent = GTK_CTREE_NODE (g_list_nth (GTK_CLIST (sltree)->row_list,
					GTK_CLIST (sltree)->focus_row));

   new = malloc (sizeof (struct slentry));
   new->port = 6667;
   new->autoconnect = 0;
   strcpy (new->comment, "New Server");
   strcpy (new->channel, "");
   strcpy (new->server, "");
   strcpy (new->password, "");
   text[0] = new->comment;

   if (!parent)
     node = gtk_ctree_insert_node (GTK_CTREE (sltree), NULL,
				   NULL, text, 4, pixmap_server, mask_server,
				   pixmap_server, mask_server, FALSE, FALSE);
   else if (is_group (parent))
     node = gtk_ctree_insert_node (GTK_CTREE (sltree), parent,
				   NULL, text, 4, pixmap_server, mask_server,
				   pixmap_server, mask_server, FALSE, FALSE);
   else
     {
       sibling = parent;
       parent = ((GTK_CTREE_ROW(parent))->parent);
       node = gtk_ctree_insert_node (GTK_CTREE (sltree), parent,
                                    sibling, text, 4, pixmap_server, mask_server,
                                    pixmap_server, mask_server, FALSE, FALSE);
     }
   
   gtk_ctree_node_set_row_data (GTK_CTREE (sltree), node, new);
   server_malloc_list = g_slist_prepend (server_malloc_list, new);
   if (parent)
      gtk_ctree_expand_recursive (GTK_CTREE (sltree), parent);
   gtk_ctree_select (GTK_CTREE (sltree), node);
   new_entry = TRUE;
   edit_entry_clicked (NULL, node);
}

/*
 * new_group_clicked:
 * same as new_server_clicked but applies to group instead
 */
static void
new_group_clicked (GtkWidget * wid, struct session *sess)
{
   struct slentry *new = 0;
   GtkCTreeNode *node;
   GtkCTreeNode *sibling;
   GtkCTreeNode *parent = 0;
   gchar *text[] =
   {""};

   sibling = GTK_CTREE_NODE (g_list_nth (GTK_CLIST (sltree)->row_list,
      GTK_CLIST (sltree)->focus_row));

   new = malloc (sizeof (struct slentry));
   new->port = 0;
   new->autoconnect = 0;
   strcpy (new->comment, "New Group");
   strcpy (new->channel, "");
   strcpy (new->server, "SUB");
   strcpy (new->password, "");
   text[0] = new->comment;

   if (!sibling)
      node = gtk_ctree_insert_node (GTK_CTREE (sltree), NULL,
                                    NULL, text, 4, pixmap_group, mask_group,
                                    pixmap_group_expanded, mask_group_expanded, FALSE, FALSE);
   else
   {
     parent = ((GTK_CTREE_ROW(sibling))->parent);
     node = gtk_ctree_insert_node (GTK_CTREE (sltree), parent,
				   sibling, text, 4, pixmap_group, mask_group,
				   pixmap_group_expanded, mask_group_expanded, FALSE, FALSE);
   }

   gtk_ctree_node_set_row_data (GTK_CTREE (sltree), node, new);
   server_malloc_list = g_slist_prepend (server_malloc_list, new);
   if (parent)
      gtk_ctree_expand_recursive (GTK_CTREE (sltree), parent);
   gtk_ctree_select (GTK_CTREE (sltree), node);
   new_entry = TRUE;
   edit_entry_clicked (NULL, node);
}

/* 
 * no_servlist:
 * called when the state of the servlist toggle button is changed 
 */
static void
no_servlist (GtkWidget * igad, gpointer serv)
{
   if (GTK_TOGGLE_BUTTON (igad)->active)
      prefs.skipserverlist = TRUE;
   else
      prefs.skipserverlist = FALSE;
}


/* 
 * skip_motd:
 * called when the state of the skip motd toggle button is changed 
 */
static void
skip_motd (GtkWidget * igad, gpointer serv)
{
   if (GTK_TOGGLE_BUTTON (igad)->active)
      prefs.skipmotd = TRUE;
   else
      prefs.skipmotd = FALSE;
}

/* 
 * channelbox_toggled:
 * called when the state of the channelbox toggle button is changed 
 */
static void
channelbox_toggled (GtkWidget * igad, gpointer serv)
{
   if (GTK_TOGGLE_BUTTON (igad)->active)
   {
      gtk_widget_show (quickview);
      channelbox = TRUE;
   } else
   {
      gtk_widget_hide (slwin);
      gtk_widget_hide (quickview);
      gtk_widget_show (slwin);
      channelbox = FALSE;
   }
}

/*
 * close_server_list:
 * self explainatory?
 */
static int
close_server_list (GtkWidget * win, int destroy)
{
   strcpy (prefs.nick1, gtk_entry_get_text ((GtkEntry *) nick1gad));
   strcpy (prefs.nick2, gtk_entry_get_text ((GtkEntry *) nick2gad));
   strcpy (prefs.nick3, gtk_entry_get_text ((GtkEntry *) nick3gad));
   strcpy (prefs.username, gtk_entry_get_text ((GtkEntry *) usernamegad));
   strcpy (prefs.realname, gtk_entry_get_text ((GtkEntry *) realnamegad));
   save_serverentrys ();
   if (destroy && slwin)
   {
      gtk_widget_destroy (slwin);
      if (sleditwin)
         gtk_widget_destroy (sleditwin);
      slwin = 0;
      sleditwin = 0;
      while (g_slist_last (server_malloc_list))
      {
         free ((g_slist_last (server_malloc_list))->data);
         server_malloc_list = g_slist_remove (server_malloc_list,
                                              ((g_slist_last (server_malloc_list))->data));
      }
      sltree = 0;
   }
   return 0;
}

/*
 * connect_auto:
 * called when the autoconnect parser finds an entry
 */
static void
connect_auto (struct slentry *conn, struct session *sess)
{
   int port;
   char channel[202];
   char server[132];
   char password[86];

   if (!(strlen (conn->server)))
      return;
   strncpy (server, conn->server, 131);
   strncpy (password, conn->password, 85);
   strncpy (channel, conn->channel, 201);
   port = conn->port;
   if (!sess)
   {
      struct server *serv = new_server ();
      if (prefs.use_server_tab)
         sess = serv->front_session;
      else
         sess = new_session (serv, 0);
   }
   strcpy (sess->willjoinchannel, channel);
   strcpy (sess->server->password, password);

   connect_server (sess, server, port, FALSE);
}

/*
 * autoconnect_parser:
 * checks each entry to see if it should be autoconnected to or not
 */
static void
autoconnect_parser (GtkCTree * ctree, GtkCTreeNode * node, struct session *sess)
{
   struct slentry *conn;

   if (is_group (node))
      return;

   conn = gtk_ctree_node_get_row_data (GTK_CTREE (sltree), node);
   if (!conn->autoconnect)
      return;

   if (first_autoconnect)
   {
      connect_auto (conn, sess);
      first_autoconnect = 0;
   } else
      connect_auto (conn, NULL);
}

int
serverlist_autoconnect_after (struct session *sess)
{
   gtk_ctree_pre_recursive (GTK_CTREE (sltree), NULL,
                            GTK_CTREE_FUNC (autoconnect_parser), sess);
   close_server_list (0, TRUE);
   return 0;                    /* returning 0 removes this idle event */
}

/*
 * serverlist_autoconnect: [PUBLIC]
 * called from xchat.c if the user has selected that he/she wants autoconnect
 */
void
serverlist_autoconnect (struct session *sess)
{
   first_autoconnect = 1;
   gtk_idle_add ((GtkFunction) serverlist_autoconnect_after, sess);
}

/*
 * connectnew_clicked:
 * called when the user clicks the connect new button
 */

static void
connectnew_clicked (GtkWidget * wid, struct session *sess)
{
   GtkCTreeNode *selected;
   struct slentry *conn;
   struct server *serv;
   char channel[202];
   char server[132];
   char password[86];
   int port;

   selected = GTK_CTREE_NODE (g_list_nth (GTK_CLIST (sltree)->row_list,
      GTK_CLIST (sltree)->focus_row));
   if (!selected)
      return;

   conn = gtk_ctree_node_get_row_data (GTK_CTREE (sltree), selected);
   if (!(strlen (conn->server)))
      return;
   strncpy (server, conn->server, 131);
   strncpy (password, conn->password, 85);
   strncpy (channel, conn->channel, 201);
   port = conn->port;
   serv = new_server ();
   if (serv)
   {
      close_server_list (0, TRUE);
      if (prefs.use_server_tab)
         sess = serv->front_session;
      else
         sess = new_session (serv, 0);
      if (sess)
      {
         strcpy (sess->server->password, password);
         strcpy (sess->willjoinchannel, channel);
         strcpy (sess->server->nick, prefs.nick1);
         connect_server (sess, server, port, FALSE);
      }
   }
}

/*
 * connect_clicked:
 * called when the user clicks the connect button
 */

static void
connect_clicked (GtkWidget * wid, struct session *sess)
{
   GtkCTreeNode *selected;
   struct slentry *conn;
   char server[132];
   int port;

   /* clueless user filter! */
   if (!strcmp ("root", gtk_entry_get_text ((GtkEntry *) nick1gad)))
   {
      gtkutil_simpledialog ("Cannot use \"root\" as a nickname,\n"
         "please change that first.");
      return;
   }
   selected = GTK_CTREE_NODE (g_list_nth (GTK_CLIST (sltree)->row_list,
      GTK_CLIST (sltree)->focus_row));
   if (!selected)
      return;

   conn = gtk_ctree_node_get_row_data (GTK_CTREE (sltree), selected);
   if (!(strlen (conn->server)))
      return;
   strncpy (server, conn->server, 131);
   strncpy (sess->server->password, conn->password, 85);
   strncpy (sess->willjoinchannel, conn->channel, 201);
   port = conn->port;

   close_server_list (0, TRUE);

   if (strcmp (sess->server->nick, prefs.nick1))
   {
      fe_set_nick (sess->server, prefs.nick1);
      strcpy (sess->server->nick, prefs.nick1);
   }
   connect_server (sess, server, port, FALSE);
}

/*
 * row_selected:
 * called when the user clicks on a row, checks if connect button should be clickable
 */
static void
row_selected (GtkCTree * tree, gint row, gint col, GdkEventButton * even, gpointer sess)
{
   GtkCTreeNode *selected = 0;
   struct slentry *conn;

   selected = gtk_ctree_node_nth (GTK_CTREE (sltree), row);
   if (!is_group (selected))
   {
      conn = gtk_ctree_node_get_row_data (GTK_CTREE (sltree), selected);
      gtk_entry_set_text (GTK_ENTRY (quickview), conn->channel);
      gtk_widget_set_sensitive (button_conn, TRUE);
      gtk_widget_set_sensitive (button_connnew, TRUE);
      if (even && even->type == GDK_2BUTTON_PRESS)
         connect_clicked (0, (struct session *) sess);
   }
}

/*
 * row_unselected:
 * makes sure you can't click the connect button if no row is selected
 */
static void
row_unselected (GtkCTree * tree, GtkCTreeNode * row, gint col)
{
   gtk_widget_set_sensitive (button_connnew, FALSE);
   gtk_widget_set_sensitive (button_conn, FALSE);
   gtk_entry_set_text (GTK_ENTRY (quickview), "");
}

/*
 * group_collapse:
 * called when a group is collapsed and sets port = 0 on that group data to make sure
 * that xchat "remembers" it's state
 */
static void
group_collapse (GtkCTree * tree, GtkCTreeNode * node)
{
   struct slentry *temporary;
   if (!closing_list)
   {
      temporary = gtk_ctree_node_get_row_data (GTK_CTREE (sltree), GTK_CTREE_NODE (node));
      if (temporary && (!strcmp (temporary->server, "SUB")))
         temporary->port = 0;
   }
}

/*
 * group_expand:
 * same as above, just port = 1 instead
 */
static void
group_expand (GtkCTree * tree, GtkCTreeNode * node)
{
   struct slentry *temporary;
   if (!closing_list)
   {
      temporary = gtk_ctree_node_get_row_data (GTK_CTREE (sltree), GTK_CTREE_NODE (node));
      if (temporary && (!strcmp (temporary->server, "SUB")))
         temporary->port = 1;
   }
}

/*
 * open_server_list [PUBLIC]
 * 
 * called by xchat.c to open the server list GUI
 *
 */
void
open_server_list (GtkWidget * widd, struct session *sess)
{
   GtkWidget *scroll;
   GtkWidget *vbox;
   GtkWidget *hbox;
   GtkWidget *wid1, *wid2, *wid3, *wid4;
   GdkPixmap *pixmap_serverlist_logo;
   GdkBitmap *mask_serverlist_logo;

   if (slwin)
   {
      gdk_window_show (slwin->window);
      return;
   }
   slwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title (GTK_WINDOW (slwin), "X-Chat: Server List");
   gtk_signal_connect (GTK_OBJECT (slwin), "delete_event",
                       GTK_SIGNAL_FUNC (close_server_list), 0);
   gtk_window_set_policy (GTK_WINDOW (slwin), TRUE, TRUE, FALSE);
   gtk_window_set_wmclass (GTK_WINDOW (slwin), "servlist", "X-Chat");
   gtk_widget_realize (slwin);

   pixmap_group = theme_pixmap (slwin, &mask_group, 0, THEME_GROUP);
   pixmap_group_expanded = theme_pixmap (slwin, &mask_group_expanded, 0, THEME_GROUP_EXPANDED);
   pixmap_server = theme_pixmap (slwin, &mask_server, 0, THEME_SERVER);

   gtk_container_border_width (GTK_CONTAINER (slwin), 5);
   vbox = gtk_vbox_new (FALSE, 0);
   gtk_container_add (GTK_CONTAINER (slwin), vbox);

   /* NICKNAME ETC. */
   /* boxes */
   wid1 = gtk_frame_new ("User info:");

   wid3 = gtk_hbox_new (0, 0);
   gtk_widget_set_usize (wid3, 602, 0);
   gtk_container_add (GTK_CONTAINER (wid1), wid3);
   gtk_widget_show (wid3);

   wid2 = gtk_vbox_new (0, 0);
   gtk_container_set_border_width (GTK_CONTAINER (wid2), 4);
   gtk_container_add (GTK_CONTAINER (wid3), wid2);
   gtk_box_pack_start (GTK_BOX (vbox), wid1, FALSE, FALSE, 0);
   gtk_widget_show (wid1);
   gtk_widget_show (wid2);
   /* hbox */
   hbox = gtk_hbox_new (0, 2);
   gtk_box_pack_start (GTK_BOX (wid2), hbox, FALSE, TRUE, 3);
   gtk_widget_show (hbox);
   /* nickboxes */
   gtkutil_label_new ("Nicknames:", hbox);
   nick1gad = gtkutil_entry_new (63, hbox, 0, 0);
   gtk_entry_set_text (GTK_ENTRY (nick1gad), prefs.nick1);
   nick2gad = gtkutil_entry_new (63, hbox, 0, 0);
   gtk_entry_set_text (GTK_ENTRY (nick2gad), prefs.nick2);
   nick3gad = gtkutil_entry_new (63, hbox, 0, 0);
   gtk_entry_set_text (GTK_ENTRY (nick3gad), prefs.nick3);
   hbox = gtk_hbox_new (0, 2);
   gtk_box_pack_start (GTK_BOX (wid2), hbox, FALSE, TRUE, 3);
   gtk_widget_show (hbox);
   /* nameboxes */
   gtkutil_label_new ("Realname:", hbox);
   realnamegad = gtkutil_entry_new (63, hbox, 0, 0);
   gtk_entry_set_text (GTK_ENTRY (realnamegad), prefs.realname);
   gtkutil_label_new ("Username:", hbox);
   usernamegad = gtkutil_entry_new (63, hbox, 0, 0);
   gtk_entry_set_text (GTK_ENTRY (usernamegad), prefs.username);

   /* X-CHAT ICON */
   pixmap_serverlist_logo = theme_pixmap (slwin, &mask_serverlist_logo, 0, THEME_SERVERLIST_LOGO);
   wid4 = gtk_pixmap_new (pixmap_serverlist_logo, mask_serverlist_logo);
   gtk_box_pack_start (GTK_BOX (wid3), wid4, FALSE, TRUE, 4);
   gtk_widget_show (wid4);

   /* THE ACTUAL SERVERLIST */
   scroll = gtk_scrolled_window_new (0, 0);
   gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
				   GTK_POLICY_NEVER,
				   GTK_POLICY_AUTOMATIC);
   gtk_widget_set_usize (GTK_WIDGET (scroll), 450, 250);
   gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
   gtk_widget_show (scroll);
   sltree = gtk_ctree_new (1, 0);
   gtk_clist_freeze (GTK_CLIST (sltree));
   gtk_ctree_set_line_style (GTK_CTREE (sltree), GTK_CTREE_LINES_DOTTED);
   gtk_clist_set_reorderable (GTK_CLIST (sltree), TRUE);
   gtk_clist_set_selection_mode (GTK_CLIST (sltree), GTK_SELECTION_BROWSE);
   gtk_ctree_set_expander_style (GTK_CTREE (sltree), GTK_CTREE_EXPANDER_CIRCULAR);
   gtk_widget_show (sltree);
   gtk_signal_connect (GTK_OBJECT (GTK_CLIST (sltree)), "select_row",
                       GTK_SIGNAL_FUNC (row_selected), sess);
   gtk_signal_connect (GTK_OBJECT (sltree), "tree_unselect_row",
                       GTK_SIGNAL_FUNC (row_unselected), 0);
   gtk_container_add (GTK_CONTAINER (scroll), sltree);
   gtk_clist_thaw (GTK_CLIST (sltree));
   wid1 = gtk_hseparator_new ();
   gtk_box_pack_start (GTK_BOX (vbox), wid1, FALSE, FALSE, 8);
   gtk_widget_show (wid1);


   /* START BUTTON ROWS */
   hbox = gtk_hbox_new (FALSE, 8);
   wid2 = gtk_vbox_new (FALSE, 7);
   gtk_box_pack_start (GTK_BOX (hbox), wid2, TRUE, TRUE, 0);
   gtk_widget_show (wid2);
   /* quickview, shows which channels we're gonna join */
   quickview = gtk_entry_new ();
   gtk_entry_set_editable (GTK_ENTRY (quickview), FALSE);
   gtk_box_pack_start (GTK_BOX (wid2), quickview, TRUE, TRUE, 0);
   if (channelbox)
      gtk_widget_show (quickview);

   /* hbox1 */
   wid1 = gtk_hbox_new (FALSE, 2);
   gtk_box_pack_end (GTK_BOX (wid2), wid1, FALSE, FALSE, 0);

   /* FIRST "NORMAL" BUTTON */
   button_conn =
      gtkutil_button (slwin, 0, "Connect", connect_clicked, sess, wid1);

   button_connnew =
      gtkutil_button (slwin, 0, "Connect New", connectnew_clicked, sess, wid1);

   gtkutil_button (slwin, 0, "New Server", new_server_clicked, 0, wid1);
   gtkutil_button (slwin, 0, "New Group", new_group_clicked, 0, wid1);

   /* 2 MORE "NORMAL" BUTTONS */
   gtkutil_button (slwin, 0, "Delete", delete_server_clicked, 0, wid1);
   gtkutil_button (slwin, 0, "Edit", edit_entry_clicked, 0, wid1);

   gtk_widget_set_sensitive (button_conn, FALSE);
   gtk_widget_set_sensitive (button_connnew, FALSE);
   gtk_widget_show (wid1);

   wid1 = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (wid2), wid1, FALSE, FALSE, 0);

   wid3 = gtk_check_button_new_with_label ("Skip MOTD");
   gtk_signal_connect (GTK_OBJECT (wid3), "toggled",
      GTK_SIGNAL_FUNC (skip_motd), 0);
   gtk_box_pack_start (GTK_BOX (wid1), wid3, TRUE, FALSE, 0);
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wid3),
                    (prefs.skipmotd));
   gtk_widget_show (wid3);

   button_chan_new = gtk_check_button_new_with_label ("Show channels");
   gtk_box_pack_start (GTK_BOX (wid1), button_chan_new, TRUE, FALSE, 0);
   gtk_widget_show (button_chan_new);
   gtk_signal_connect (GTK_OBJECT (button_chan_new), "toggled",
                       GTK_SIGNAL_FUNC (channelbox_toggled), 0);

   wid3 = gtk_check_button_new_with_label ("No ServerList on startup");
   gtk_signal_connect (GTK_OBJECT (wid3), "toggled",
                       GTK_SIGNAL_FUNC (no_servlist), 0);
   gtk_box_pack_start (GTK_BOX (wid1), wid3, TRUE, FALSE, 0);
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wid3), prefs.skipserverlist);
   gtk_widget_show (wid3);

   gtk_widget_show (wid1);

   /* END OF BOXES */
   load_serverentrys ();
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_chan_new),
                        (channelbox));
   gtk_signal_connect (GTK_OBJECT (sltree), "tree_expand",
   GTK_SIGNAL_FUNC (group_expand), 0);
   gtk_signal_connect (GTK_OBJECT (sltree), "tree_collapse",
                       GTK_SIGNAL_FUNC (group_collapse), 0);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   gtk_widget_show (hbox);
   gtk_widget_show (vbox);

   gtk_widget_show (slwin);
}
