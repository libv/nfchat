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
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "xchat.h"
#include "util.h"
#include "notify.h"
#include "cfgfiles.h"
#include "plugin.h"
#include "fe.h"

extern int tcp_send_len (struct server *serv, char *buf, int len);
extern int tcp_send (struct server *serv, char *buf);
extern void PrintText (struct session *sess, unsigned char *text);
extern int waitline (int sok, char *buf, int bufsize);
extern GSList *serv_list;

GSList *notify_list = 0;

void notify_adduser (char *name);
int notify_deluser (char *name);


struct notify_per_server *
notify_find_server_entry (struct notify *notify,
                  struct server *serv)
{
   GSList *list = notify->server_list;
   struct notify_per_server *servnot;

   while (list)
   {
      servnot = (struct notify_per_server *) list->data;
      if (servnot->server == serv)
         return servnot;
      list = list->next;
   }
   servnot = malloc (sizeof (struct notify_per_server));
   if (servnot)
   {
      memset (servnot, 0, sizeof (struct notify_per_server));
      servnot->server = serv;
      notify->server_list = g_slist_prepend (notify->server_list, servnot);
   }
   return servnot;
}

void
notify_save (void)
{
   int fh;
   char buf[256];

   snprintf (buf, sizeof buf, "%s/notify.conf", get_xdir ());

   fh = open (buf, O_TRUNC | O_WRONLY | O_CREAT, 0600);
   if (fh != -1)
   {
      struct notify *notify;
      GSList *list = notify_list;
      while (list)
      {
         notify = (struct notify *) list->data;
         write (fh, notify->name, strlen (notify->name));
         write (fh, "\n", 1);
         list = list->next;
      }
      close (fh);
   }
}

void
notify_load (void)
{
   int fh;
   char buf[256];

   snprintf (buf, sizeof buf, "%s/notify.conf", get_xdir ());

   fh = open (buf, O_RDONLY);
   if (fh != -1)
   {
      while (waitline (fh, buf, sizeof buf) != -1)
      {
         if (*buf != '#')
            notify_adduser (buf);
      }
      close (fh);
   }
}

/* called when receiving a ISON 303 */

void
notify_markonline (struct session *sess, char *outbuf, char *word[])
{
   struct notify *notify;
   struct server *serv = sess->server;
   struct notify_per_server *servnot;
   GSList *list = notify_list;
   int i, seen;

   if (!sess->server)
   {
      /*fprintf (stderr, "*** something fucked up, no sess->server\n");*/
      return;
   }
   while (list)
   {
      notify = (struct notify *) list->data;
      servnot = notify_find_server_entry (notify, serv);
      if (!servnot)
      {
         list = list->next;
         continue;
      }
      i = 4;
      seen = FALSE;
      while (*word[i])
      {
         if (!strcasecmp (notify->name, word[i]))
         {
            seen = TRUE;
            servnot->lastseen = time (0);
            if (!servnot->ison)
            {
               servnot->ison = TRUE;
               servnot->laston = time (0);
               EMIT_SIGNAL (XP_TE_NOTIFYONLINE, sess, notify->name,
                            serv->servername, NULL, NULL, 0);
              /* fe_notify_update (notify->name); */
            }
            break;
         }
         i++;
         /* FIXME: word[] is only a 32 element array, limits notify list to
            about 27 people */
         if (i > 27)
         {
            fprintf (stderr, "*** XCHAT WARNING: notify list too large.\n");
            break;
         }
      }
      if (!seen && servnot->ison)
      {
         servnot->ison = FALSE;
         servnot->lastoff = time (0);
         EMIT_SIGNAL (XP_TE_NOTIFYOFFLINE, sess, notify->name,
                      serv->servername, NULL, NULL, 0);
        /* fe_notify_update (notify->name); */
      }
      list = list->next;
   }
  /* fe_notify_update (0); */
}

int
notify_checklist (void)
{
   char *outbuf = malloc (512);
   struct server *serv;
   struct notify *notify;
   GSList *list = notify_list;
   int i = 0;

   strcpy (outbuf, "ISON ");
   while (list)
   {
      i++;
      notify = (struct notify *) list->data;
      strcat (outbuf, notify->name);
      strcat (outbuf, " ");
      if (strlen (outbuf) > 460)
      {
         /* LAME: we can't send more than 512 bytes to the server, but     *
          * if we split it in two packets, our offline detection wouldn't  *
          work                                                           */
         fprintf (stderr, "*** XCHAT WARNING: notify list too large.\n");
         break;
      }
      list = list->next;
   }
   if (i)
   {
      GSList *list = serv_list;
      strcat (outbuf, "\r\n");
      while (list)
      {
         serv = (struct server *) list->data;
         if (serv->connected && serv->end_of_motd)
            tcp_send (serv, outbuf);
         list = list->next;
      }
   }
   free (outbuf);
   return 1;
}

void
notify_showlist (struct session *sess)
{
   char outbuf[256];
   struct notify *notify;
   GSList *list = notify_list;
   struct notify_per_server *servnot;
   int i = 0;

   PrintText (sess, "\00308,02 \002-- Notify List --------------- \003\n");
   while (list)
   {
      i++;
      notify = (struct notify *) list->data;
      servnot = notify_find_server_entry (notify, sess->server);
      if (servnot && servnot->ison)
         sprintf (outbuf, "  %-20s online\n", notify->name);
      else
         sprintf (outbuf, "  %-20s offline\n", notify->name);
      PrintText (sess, outbuf);
      list = list->next;
   }
   if (i)
   {
      sprintf (outbuf, "%d", i);
      EMIT_SIGNAL (XP_TE_NOTIFYNUMBER, sess, outbuf, NULL, NULL, NULL, 0);
   } else
      EMIT_SIGNAL (XP_TE_NOTIFYEMPTY, sess, NULL, NULL, NULL, NULL, 0);
}

int
notify_deluser (char *name)
{
   struct notify *notify;
   struct notify_per_server *servnot;
   GSList *list = notify_list;

   while (list)
   {
      notify = (struct notify *) list->data;
      if (!strcasecmp (notify->name, name))
      {
       /*  fe_notify_update (notify->name); */
         /* Remove the records for each server */
         while (notify->server_list)
         {
            servnot = (struct notify_per_server *) notify->server_list->data;
            notify->server_list = g_slist_remove (notify->server_list, servnot);
            free (servnot);
         }
         notify_list = g_slist_remove (notify_list, notify);
         free (notify->name);
         free (notify);
        /* fe_notify_update (0); */
         return 1;
      }
      list = list->next;
   }
   return 0;
}

void
notify_adduser (char *name)
{
   struct notify *notify = malloc (sizeof (struct notify));
   if (notify)
   {
      memset (notify, 0, sizeof (struct notify));
      notify->name = strdup (name);
      notify->server_list = 0;
      notify_list = g_slist_prepend (notify_list, notify);
      notify_checklist ();
     /* fe_notify_update (notify->name); */
     /* fe_notify_update (0); */
   }
}

int
notify_isnotify (struct session *sess, char *name)
{
   struct notify *notify;
   struct notify_per_server *servnot;
   GSList *list = notify_list;

   while (list)
   {
      notify = (struct notify *) list->data;
      if (!strcasecmp (notify->name, name))
      {
         servnot = notify_find_server_entry (notify, sess->server);
         if (servnot && servnot->ison)
            return TRUE;
      }
      list = list->next;
   }

   return FALSE;
}

void 
notify_cleanup ()
{
   GSList *list = notify_list;
   GSList *nslist, *srvlist;
   struct notify *notify;
   struct notify_per_server *servnot;
   struct server *serv;
   int valid;

   while (list)
   {
      /* Traverse the list of notify structures */
      notify = (struct notify *) list->data;
      nslist = notify->server_list;
      while (nslist)
      {
         /* Look at each per-server structure */
         servnot = (struct notify_per_server *) nslist->data;

         /* Check the server is valid */
         valid = FALSE;
         srvlist = serv_list;
         while (srvlist)
         {
            serv = (struct server *) srvlist->data;
            if (servnot->server == serv)
            {
               valid = serv->connected;  /* Only valid if server is too */
               break;
            }
            srvlist = srvlist->next;
         }
         if (!valid)
         {
            notify->server_list = g_slist_remove (notify->server_list, servnot);
            nslist = notify->server_list;
         } else
         {
            nslist = nslist->next;
         }
      }
      list = list->next;
   }
  /* fe_notify_update (0); */
}
