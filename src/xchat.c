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
server_t *server = NULL;
session_t *session = NULL;
GSList *away_list = 0;

int xchat_is_quitting = 0;

extern GSList *ctcp_list;

session_t *current_tab;
session_t *menu_sess = 0;
struct xchatprefs prefs;

void xchat_cleanup (void);

/* inbound.c */

extern void process_line ();

/* plugin.c */

extern void signal_setup ();

/* server.c */

extern int close_socket (int sok);
extern void connecting_fin (session_t *sess);
extern void connect_server (session_t *sess, char *server, int port, int quiet);
extern void disconnect_server (session_t *sess, int sendquit, int err);

/* userlist.c */

extern struct user *find_name (session_t *sess, char *name);

/* editlist.c */

extern void list_loadconf (char *, GSList **, char *);

/* text.c */

extern unsigned char *strip_color (unsigned char *text);
extern void load_text_events ();

void auto_reconnect (int send_quit, int err);

/* anything above SEND_MAX bytes in 1 second is
   queued and sent QUEUE_TIMEOUT milliseconds later */

#define SEND_MAX 256
#define QUEUE_TIMEOUT 2500

/* this whole throttling system is lame, can anyone suggest
   a better system? */

static int
tcp_send_queue (void)
{
   char *buf;
   int len;
   GSList *list = server->outbound_queue;
   time_t now = time (0);

   while (list)
   {
      buf = (char *) list->data;
      len = strlen (buf);

      if (server->last_send != now)
      {
         server->last_send = now;
         server->bytes_sent = 0;
      } else
      {
         server->bytes_sent += len;
      }

      if (server->bytes_sent > SEND_MAX)
         return 1;              /* don't remove the timeout handler */

      if (!EMIT_SIGNAL (XP_IF_SEND, (void *)server->sok, buf, (void *)len, NULL, NULL, 0))
		  send (server->sok, buf, len, 0);

      server->outbound_queue = g_slist_remove (server->outbound_queue, buf);
      free (buf);

      list = server->outbound_queue;
   }
   return 0;                    /* remove the timeout handler */
}

static int
tcp_send_len (char *buf, int len)
{

   time_t now = time (0);

   if (server->last_send != now)
   {
      server->last_send = now;
      server->bytes_sent = 0;
   } else
   {
      server->bytes_sent += len;
   }

   if (server->bytes_sent > SEND_MAX || server->outbound_queue)
   {
      buf = strdup (buf);
      if (!server->outbound_queue)
         fe_timeout_add (QUEUE_TIMEOUT, tcp_send_queue, server);
      server->outbound_queue = g_slist_append (server->outbound_queue, buf);
      return 1;
   }

   if (!EMIT_SIGNAL (XP_IF_SEND, (void *)server->sok, buf, (void *)len, NULL, NULL, 0))
	   return send (server->sok, buf, len, 0);
   return 1;
}

int
tcp_send (char *buf)
{
   return tcp_send_len (buf, strlen (buf));
}

static int
timeout_auto_reconnect (void)
{
  /*if (is_server (serv)) make sure it hasnt been closed during the delay */
  if (!server->connected && !server->connecting && server->session)
      connect_server (server->session, server->hostname, server->port, FALSE);
  return 0;         /* returning 0 should remove the timeout handler */
}

void
auto_reconnect (int send_quit, int err)
{
  if (server->session == NULL)
    return;
  
  disconnect_server (server->session, send_quit, err);
  
  if (prefs.recon_delay)
    fe_timeout_add (prefs.recon_delay * 1000, timeout_auto_reconnect, server);
  else
      connect_server (server->session, server->hostname, server->port, FALSE);
}

void
read_data (void *blah, int sok)
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
	     err = 0;
         if (prefs.autoreconnect)
	   auto_reconnect (FALSE, err);
         else
	   disconnect_server (server->session, FALSE, err);
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
		   server->linebuf[server->pos] = 0;
		   process_line ();
		   server->pos = 0;
		   break;
		   
		 default:
		   server->linebuf[server->pos] = lbuf[i];
		   server->pos++;
		   if (server->pos == 2047)
		     {
		       fprintf (stderr, "*** NF-Chat: Buffer overflow - shit server!\n");
		       server->pos = 2046;
		     }
		 }
	       i++;
	     }
	 }
   }
}

void
flush_server_queue (void)
{
   GSList *list = server->outbound_queue;
   void *data;

   while (list)
   {
      data = list->data;
      server->outbound_queue = g_slist_remove (server->outbound_queue, data);
      free (data);
      list = server->outbound_queue;
   }
   server->last_send = 0;
   server->bytes_sent = 0;
}

session_t *
new_session (void)
{
  session_t *sess;
  
  if (session)
    {
      fprintf(stderr, "What? Dont call new_session twice!\n");
      return (NULL);
    }

  sess = malloc (sizeof (session_t));
  memset (sess, 0, sizeof (session_t));
  
  fe_new_window ();
  
  return sess;
}

static void
init_server (void)
{
   server = malloc (sizeof (server_t));
   memset (server, 0, sizeof (server_t));

   server->sok = -1;
   server->iotag = -1;
   strcpy (server->nick, prefs.nick1);
   
   if (prefs.use_server_tab)
     {
       server->session = new_session ();
       server->session->is_server = TRUE;
     }
}

static void
kill_server_callback (void)
{
   if (server->connected)
   {
      if (server->iotag != -1)
         fe_input_remove (server->iotag);
      if (fe_timeout_add (5000, close_socket, (void *)server->sok) == -1)
         close (server->sok);
   }
   if (server->connecting)
   {
      kill (server->childpid, SIGKILL);
      waitpid (server->childpid, NULL, 0/*WNOHANG*/);
      if (server->iotag != -1)
         fe_input_remove (server->iotag);
      close (server->childread);
      close (server->childwrite);
      close (server->sok);
   }
   flush_server_queue ();
   free (server);
}

static void
send_quit_or_part (void)
{
   int willquit = TRUE;
   char tbuf[256];
     
   if (xchat_is_quitting)
      willquit = TRUE;

   if (server->connected)
   {
      if (willquit)
      {
         if (!server->sent_quit)
         {
            flush_server_queue ();
            snprintf (tbuf, sizeof tbuf, "QUIT :%s\r\n", killsess->quitreason);
            tcp_send (tbuf);
            server->sent_quit = TRUE;
         }
      } else
      {
         if (!killsess->is_server && killsess->channel[0])
         {
            snprintf (tbuf, sizeof tbuf, "PART %s\r\n", killsess->channel);
            tcp_send (tbuf); 
         }
      }
   }
}

void
kill_session_callback (void)
{
   session_t *sess;
   GSList *list;

   if (!session->quitreason)
      session->quitreason = prefs.quitreason;

   if (current_tab == session)
      current_tab = NULL;

   fe_session_callback ();

   send_quit_or_part ();

   history_free (&session->history);
  
   free (session);

   xchat_cleanup ();

   kill_server_callback ();
}

struct away_msg *
find_away_message (char *nick)
{
   struct away_msg *away;
   GSList *list = away_list;
   while (list)
   {
      away = (struct away_msg *) list->data;
      if (!strcasecmp (nick, away->nick))
         return away;
      list = list->next;
   }
   return 0;
}

void
save_away_message (char *nick, char *msg)
{
   struct away_msg *away = find_away_message (nick);

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
  struct sigaction act;

  act.sa_handler = SIG_IGN;
  act.sa_flags = 0;
  sigemptyset (&act.sa_mask);
  sigaction (SIGPIPE, &act, NULL);
  
  signal_setup ();
  load_text_events ();
  list_loadconf ("ctcpreply.conf", &ctcp_list, defaultconf_ctcp);
  
  init_server ();
  if (prefs.use_server_tab)
    session = server->session;
  else
    session = new_session ();
}

void
xchat_cleanup (void)
{
   xchat_is_quitting = TRUE;
   
   free (session);
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

