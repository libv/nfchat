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
#include <unistd.h>
#include "xchat.h"
#include "fe.h"
#include "util.h"
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include "cfgfiles.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "signals.h"


GSList *ctcp_list = 0;
GSList *sess_list = 0;
GSList *serv_list = 0;
/* server_t server = NULL */
GSList *away_list = 0;

int xchat_is_quitting = 0;

extern GSList *ctcp_list;

struct session *current_tab;
struct session *menu_sess = 0;
struct xchatprefs prefs;

void xchat_cleanup (void);

/* inbound.c */

extern void process_line (struct session *sess, server_t *server, char *buf);

/* plugin.c */

extern void signal_setup ();

/* server.c */

extern int close_socket (int sok);
extern void connecting_fin (struct session *);
extern void connect_server (struct session *sess, char *server, int port, int quiet);
extern void disconnect_server (struct session *sess, int sendquit, int err);

/* userlist.c */

extern struct user *find_name (struct session *sess, char *name);

/* editlist.c */

extern void list_loadconf (char *, GSList **, char *);

/* text.c */

extern unsigned char *strip_color (unsigned char *text);
extern void load_text_events ();

void auto_reconnect (server_t *serv, int send_quit, int err);

/* anything above SEND_MAX bytes in 1 second is
   queued and sent QUEUE_TIMEOUT milliseconds later */

#define SEND_MAX 256
#define QUEUE_TIMEOUT 2500

/* this whole throttling system is lame, can anyone suggest
   a better system? */

static int
tcp_send_queue (server_t *serv)
{
   char *buf;
   int len;
   GSList *list = serv->outbound_queue;
   time_t now = time (0);

   while (list)
   {
      buf = (char *) list->data;
      len = strlen (buf);

      if (serv->last_send != now)
      {
         serv->last_send = now;
         serv->bytes_sent = 0;
      } else
      {
         serv->bytes_sent += len;
      }

      if (serv->bytes_sent > SEND_MAX)
         return 1;              /* don't remove the timeout handler */

      if (!EMIT_SIGNAL (XP_IF_SEND, (void *)serv->sok, buf, (void *)len, NULL, NULL, 0))
		  send (serv->sok, buf, len, 0);

      serv->outbound_queue = g_slist_remove (serv->outbound_queue, buf);
      free (buf);

      list = serv->outbound_queue;
   }
   return 0;                    /* remove the timeout handler */
}

int
tcp_send_len (server_t *serv, char *buf, int len)
{

   time_t now = time (0);

   if (serv->last_send != now)
   {
      serv->last_send = now;
      serv->bytes_sent = 0;
   } else
   {
      serv->bytes_sent += len;
   }

   if (serv->bytes_sent > SEND_MAX || serv->outbound_queue)
   {
      buf = strdup (buf);
      if (!serv->outbound_queue)
         fe_timeout_add (QUEUE_TIMEOUT, tcp_send_queue, serv);
      serv->outbound_queue = g_slist_append (serv->outbound_queue, buf);
      return 1;
   }

   if (!EMIT_SIGNAL (XP_IF_SEND, (void *)serv->sok, buf, (void *)len, NULL, NULL, 0))
	   return send (serv->sok, buf, len, 0);
   return 1;
}

int
tcp_send (server_t *serv, char *buf)
{
   return tcp_send_len (serv, buf, strlen (buf));
}

static int
is_server (server_t *serv)
{
   GSList *list = serv_list;
   while (list)
   {
      if (list->data == serv)
         return 1;
      list = list->next;
   }
   return 0;
}

int
is_session (session *sess)
{
   GSList *list = sess_list;
   while (list)
   {
      if (list->data == sess)
         return 1;
      list = list->next;
   }
   return 0;
}

struct session *
find_session_from_channel (char *chan, server_t *serv)
{
   struct session *sess;
   GSList *list = sess_list;
   while (list)
   {
      sess = (struct session *) list->data;
      if (!strcasecmp (chan, sess->channel))
      {
         if (!serv)
            return sess;
         if (serv == sess->server)
            return sess;
      }
      list = list->next;
   }
   return 0;
}

struct session *
find_session_from_nick (char *nick, server_t *serv)
{
   struct session *sess;
   GSList *list = sess_list;

   if (serv->front_session)
   {
      if (find_name (serv->front_session, nick))
         return serv->front_session;
   }
   sess = find_session_from_channel (nick, serv);
   if (sess)
      return sess;

   while (list)
   {
      sess = (struct session *) list->data;
      if (sess->server == serv)
      {
         if (find_name (sess, nick))
            return sess;
      }
      list = list->next;
   }
   return 0;
}

struct session *
find_session_from_waitchannel (char *chan, server_t *serv)
{
   struct session *sess;
   GSList *list = sess_list;
   while (list)
   {
      sess = (struct session *) list->data;
      if (sess->server == serv && sess->channel[0] == 0)
         if (!strcasecmp (chan, sess->waitchannel))
            return sess;
      list = list->next;
   }
   return 0;
}

static int
timeout_auto_reconnect (server_t *serv)
{
   if (is_server (serv))   /* make sure it hasnt been closed during the delay */
   {
      if (!serv->connected && !serv->connecting && serv->front_session)
      {
         connect_server (serv->front_session, serv->hostname, serv->port, FALSE);
      }
   }
   return 0;         /* returning 0 should remove the timeout handler */
}

void
auto_reconnect (server_t *serv, int send_quit, int err)
{
   struct session *s;
   GSList *list;

   if (serv->front_session == NULL)
      return;

   list = sess_list;
   while (list)                 /* make sure auto rejoin can work */
   {
      s = (struct session *) list->data;
      if (is_channel (s->channel))
      {
         strcpy (s->waitchannel, s->channel);
         strcpy (s->willjoinchannel, s->channel);
      }
      list = list->next;
   }
   disconnect_server (serv->front_session, send_quit, err);

   if (prefs.recon_delay)
      fe_timeout_add (prefs.recon_delay * 1000, timeout_auto_reconnect, serv);
   else
      connect_server (serv->front_session, serv->hostname, serv->port, FALSE);
}

void
read_data (server_t *serv, int sok)
{
   int err, i, len;
   char lbuf[2050];

   while (1)
   {
	  if (!EMIT_SIGNAL (XP_IF_RECV, &len, (void *)sok, &lbuf, (void *)((sizeof lbuf) - 2), NULL, 0))
		  len = recv (sok, lbuf, sizeof lbuf - 2, 0);
      if (len < 1)
      {
         if (len < 0)
         {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
               return;
            err = errno;
         } else
         {
            err = 0;
         }
         if (prefs.autoreconnect)
            auto_reconnect (serv, FALSE, err);
         else
            disconnect_server (serv->front_session, FALSE, err);
         return;
      } else
      {
         i = 0;

         lbuf[len] = 0;

         while (i < len)
         {
            switch (lbuf[i])
            {
            case '\r':
               break;

            case '\n':
               serv->linebuf[serv->pos] = 0;
	       process_line (serv->front_session, serv, serv->linebuf);
               serv->pos = 0;
               break;

            default:
               serv->linebuf[serv->pos] = lbuf[i];
               serv->pos++;
               if (serv->pos == 2047)
               {
                  fprintf (stderr, "*** X-Chat: Buffer overflow - shit server!\n");
                  serv->pos = 2046;
               }
            }
            i++;
         }
      }
   }
}

void
flush_server_queue (server_t *serv)
{
   GSList *list = serv->outbound_queue;
   void *data;

   while (list)
   {
      data = list->data;
      serv->outbound_queue = g_slist_remove (serv->outbound_queue, data);
      free (data);
      list = serv->outbound_queue;
   }
   serv->last_send = 0;
   serv->bytes_sent = 0;
}

struct session *
new_session (server_t *serv)
{
   struct session *sess;

   sess = malloc (sizeof (struct session));
   memset (sess, 0, sizeof (struct session));

   sess->server = serv;
   
   sess_list = g_slist_prepend (sess_list, sess);

   fe_new_window (sess);
   
   return sess;
}

server_t *
new_server (void)
{
   server_t *serv;

   serv = malloc (sizeof (server_t));
   memset (serv, 0, sizeof (server_t));

   serv->sok = -1;
   serv->iotag = -1;
   strcpy (serv->nick, prefs.nick1);
   serv_list = g_slist_prepend (serv_list, serv);
   
   if (prefs.use_server_tab)
     {
       serv->front_session = new_session (serv);
       serv->front_session->is_server = TRUE;
     }
   return serv;
}

static void
kill_server_callback (server_t *serv)
{
   if (serv->connected)
   {
      if (serv->iotag != -1)
         fe_input_remove (serv->iotag);
      if (fe_timeout_add (5000, close_socket, (void *)serv->sok) == -1)
         close (serv->sok);
   }
   if (serv->connecting)
   {
      kill (serv->childpid, SIGKILL);
      waitpid (serv->childpid, NULL, 0/*WNOHANG*/);
      if (serv->iotag != -1)
         fe_input_remove (serv->iotag);
      close (serv->childread);
      close (serv->childwrite);
      close (serv->sok);
   }
   
   serv_list = g_slist_remove (serv_list, serv);

   flush_server_queue (serv);

   free (serv);
}

static void
exec_notify_kill (session *sess)
{
   struct nbexec *re;

   if (sess->running_exec != NULL)
   {
      re = sess->running_exec;
      sess->running_exec = NULL;
      kill (re->childpid, SIGKILL);
      fe_input_remove (re->iotag);
      close (re->myfd);
      close (re->childfd);
      free (re);
   }
}

static void
send_quit_or_part (session *killsess)
{
   int willquit = TRUE;
   char tbuf[256];
   GSList *list;
   session *sess;
   server_t *killserv = killsess->server;

   /* check if this is the last session using this server */
   list = sess_list;
   while (list)
   {
      sess = (session *) list->data;
      if (sess->server == killserv && sess != killsess)
      {
         willquit = FALSE;
         list = 0;
      } else
         list = list->next;
   }

   if (xchat_is_quitting)
      willquit = TRUE;

   if (killserv->connected)
   {
      if (willquit)
      {
         if (!killserv->sent_quit)
         {
            flush_server_queue (killserv);
            snprintf (tbuf, sizeof tbuf, "QUIT :%s\r\n", killsess->quitreason);
            tcp_send (killserv, tbuf);
            killserv->sent_quit = TRUE;
         }
      } else
      {
         if (!killsess->is_server && killsess->channel[0])
         {
            snprintf (tbuf, sizeof tbuf, "PART %s\r\n", killsess->channel);
            tcp_send (killserv, tbuf);
         }
      }
   }
}

void
kill_session_callback (session *killsess)
{
   server_t *killserv = killsess->server;
   session *sess;
   GSList *list;

   if (!killsess->quitreason)
      killsess->quitreason = prefs.quitreason;

   if (current_tab == killsess)
      current_tab = NULL;

   if (killserv->front_session == killsess)
   {
      /* front_session is closed, find a valid replacement */
      killserv->front_session = NULL;
      list = sess_list;
      while (list)
      {
         sess = (session *)list->data;
         if (sess != killsess && sess->server == killserv)
         {
            killserv->front_session = sess;
            break;
         }
         list = list->next;
      }
   }

   sess_list = g_slist_remove (sess_list, killsess);

   fe_session_callback (killsess);

   exec_notify_kill (killsess);

   send_quit_or_part (killsess);

   history_free (&killsess->history);
  
   free (killsess);

   if (!sess_list)
      xchat_cleanup ();       /* sess_list is empty, quit! */

   list = sess_list;
   while (list)
   {
      sess = (session *) list->data;
      if (sess->server == killserv)
         return;              /* this server is still being used! */
      list = list->next;
   }

   kill_server_callback (killserv);
}

static void
free_sessions (void)
{
   struct session *sess;
   GSList *list = sess_list;

   while (list)
   {
      sess = (struct session *) list->data;
      /*send_quit_or_part (sess);*/
      fe_close_window (sess);
      list = sess_list;
   }
}

struct away_msg *
find_away_message (server_t *serv, char *nick)
{
   struct away_msg *away;
   GSList *list = away_list;
   while (list)
   {
      away = (struct away_msg *) list->data;
      if (away->server == serv && !strcasecmp (nick, away->nick))
         return away;
      list = list->next;
   }
   return 0;
}

void
save_away_message (server_t *serv, char *nick, char *msg)
{
   struct away_msg *away = find_away_message (serv, nick);

   if (away)                    /* Change message for known user */
   {
      if (away->message)
         free (away->message);
      away->message = strdup (msg);
   } else
      /* Create brand new entry */
   {
      away = malloc (sizeof (struct away_msg));
      if (away)
      {
         away->server = serv;
         strcpy (away->nick, nick);
         away->message = strdup (msg);
         away_list = g_slist_prepend (away_list, away);
      }
   }
}

#define defaultconf_ctcp  "NAME TIME\nCMD /nctcp %s TIME %t\n\n"\
                          "NAME PING\nCMD /nctcp %s PING %d\n\n"

static void
xchat_init (void)
{
   struct session *sess;
   server_t *serv;
   struct sigaction act;

   act.sa_handler = SIG_IGN;
   act.sa_flags = 0;
   sigemptyset (&act.sa_mask);
   sigaction (SIGPIPE, &act, NULL);

   signal_setup ();
   load_text_events ();
   list_loadconf ("ctcpreply.conf", &ctcp_list, defaultconf_ctcp);
   
   serv = new_server ();
   if (prefs.use_server_tab)
      sess = serv->front_session;
   else
     sess = new_session (serv);
}

void
xchat_cleanup (void)
{
   xchat_is_quitting = TRUE;
   
   free_sessions ();
   fe_exit ();
}

int
main (int argc, char *argv[])
{
#ifdef SOCKS
   SOCKSinit (argv[0]);
#endif
   if (!fe_args (argc, argv))
      return 0;

   load_config ();

   fe_init ();

   xchat_init ();

   fe_main ();


   return 0;
}

