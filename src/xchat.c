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

server_t *server = NULL;
session_t *session = NULL;
GSList *away_list = 0;

int xchat_is_quitting = 0;

struct xchatprefs prefs;

extern int fe_args (int argc, char *argv[]);
extern void fe_init (void);
extern void fe_main (void);
extern void fe_exit (void);
extern int fe_timeout_add (int interval, void *callback);
extern void fe_new_window (void);
extern void fe_input_remove (int tag);
extern void fe_session_callback (void);

extern void process_line (void);
extern void signal_setup (void);
extern int close_socket (void);
extern void connect_server (char *server, int port, int quiet);
extern void disconnect_server (int sendquit, int err);
extern void load_text_events (void);
void init_commands (void);

/* anything above SEND_MAX bytes in 1 second is
   queued & sent QUEUE_TIMEOUT milliseconds later */

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

      if (!fire_signal (XP_IF_SEND, buf, (void *)len, NULL, NULL, 0))
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
         fe_timeout_add (QUEUE_TIMEOUT, tcp_send_queue);
      server->outbound_queue = g_slist_append (server->outbound_queue, buf);
      return 1;
   }

   if (!fire_signal (XP_IF_SEND, buf, (void *)len, NULL, NULL, 0))
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
  if (!server->connected && !server->connecting)
      connect_server (server->hostname, server->port, FALSE);
  return 0;         /* returning 0 should remove timeout handler */
}

void 
reconnect (void)
{
  if (prefs.channel)
    strcpy (session->willjoinchannel, prefs.channel);
  
  if (prefs.recon_delay)
    fe_timeout_add (prefs.recon_delay * 1000, timeout_auto_reconnect);
  else
    connect_server (server->hostname, server->port, FALSE);
}

void
auto_reconnect (int send_quit, int err)
{
  disconnect_server (send_quit, err);

  
  reconnect ();
}

void
read_data (int blah, int sok)
{
   int err, i, len;
   char lbuf[2050];

   while (1)
   {
     if (!fire_signal (XP_IF_RECV, (void *)sok, &lbuf, (void *)((sizeof lbuf) - 2), NULL, 0))
	 len = recv (sok, lbuf, sizeof lbuf - 2, 0);
     else
       {
	 fprintf(stderr, "Error: read_data: unable to write to socket\n");
	 exit (70);
       }
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
	   disconnect_server (FALSE, err);
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

void
init_session (void)
{
  session = malloc (sizeof (session_t));
  memset (session, 0, sizeof (session_t));
  
  fe_new_window ();
}

static void
init_server (void)
{
   server = malloc (sizeof (server_t));
   memset (server, 0, sizeof (server_t));

   server->sok = -1;
   server->iotag = -1;
   strcpy (server->nick, prefs.nick1);
}

static void
kill_server_callback (void)
{
  if (server->connected)
    {
      if (server->iotag != -1)
	fe_input_remove (server->iotag);
      if (fe_timeout_add (5000, close_socket) == -1)
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

void
kill_session_callback (void)
{
  char tbuf[256];

  xchat_is_quitting = TRUE;

  if (!session->quitreason)
    session->quitreason = prefs.quitreason;
  
  fe_session_callback ();

  if (server->connected && !server->sent_quit)
    {
      flush_server_queue ();
      snprintf (tbuf, sizeof tbuf, "QUIT :%s\r\n", session->quitreason);
      tcp_send (tbuf);
      server->sent_quit = TRUE;
    }
  
  fe_exit ();

  free (session);
  
  kill_server_callback ();
}

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

  init_commands ();
  init_server ();
  init_session ();

  if (!prefs.server || !prefs.channel)
    {
      fprintf(stderr, "Check your options!\n");
      exit;
    }

  if (prefs.serverpass)
   strcpy (server->password, prefs.serverpass);

  if (prefs.channel)
    strcpy (session->willjoinchannel, prefs.channel);

  connect_server(prefs.server, prefs.port, FALSE);
  
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

