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
#include <unistd.h>
#include "xchat.h"
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "signals.h"
#include "fe.h"

extern GSList *sess_list;
extern struct xchatprefs prefs;

extern void auto_reconnect (struct server *serv, int send_quit, int err);
extern void clear_channel (struct session *sess);
extern void set_server_name (struct server *serv, char *name);
extern void flush_server_queue (struct server *serv);
extern int tcp_send_len (struct server *serv, char *buf, int len);
extern int tcp_send (struct server *serv, char *buf);
extern void PrintText (struct session *sess, unsigned char *text);
extern void read_data (struct server *serv, gint sok);
extern char *errorstring (int err);
extern int waitline (int sok, char *buf, int bufsize);
extern void notc_msg (struct session *sess);

static void
server_cleanup (server *serv)
{
   if (serv->iotag != -1)
   {
      fe_input_remove (serv->iotag);
      serv->iotag = -1;
   }
   close (serv->childwrite);
   close (serv->childread);
   waitpid (serv->childpid, NULL, 0);
   fe_progressbar_end (serv->front_session);
   serv->connecting = 0;
}

static void
connected_signal (server *serv, int sok)
{
   session *sess = serv->front_session;
   char tbuf[128];
   char outbuf[512];
   char host[100];
   char ip[100];

   waitline (serv->childread, tbuf, sizeof tbuf);

   switch (tbuf[0])
   {
   case '1':                   /* unknown host */
      server_cleanup (serv);
      close (sok);
      EMIT_SIGNAL (XP_TE_UKNHOST, sess, NULL, NULL, NULL, NULL, 0);
      break;
   case '2':                   /* connection failed */
      waitline (serv->childread, tbuf, sizeof tbuf);
      server_cleanup (serv);
      close (sok);
      EMIT_SIGNAL (XP_TE_CONNFAIL, sess, errorstring (atoi (tbuf)), NULL, NULL,
                   NULL, 0);
      if (prefs.autoreconnectonfail)
         auto_reconnect (serv, FALSE, -1);
      break;
   case '3':                   /* gethostbyname finished */
      waitline (serv->childread, host, sizeof host);
      waitline (serv->childread, ip, sizeof ip);
      waitline (serv->childread, outbuf, sizeof outbuf);
      EMIT_SIGNAL (XP_TE_CONNECT, sess, host, ip, outbuf, NULL, 0);
      strcpy (serv->hostname, host);
      break;
   case '4':                   /* success */
      server_cleanup (serv);
      serv->connected = TRUE;
      serv->iotag = fe_input_add (serv->sok, 1, 0, 1, read_data, serv);
      if (!serv->no_login)
      {
         EMIT_SIGNAL (XP_TE_CONNECTED, sess, NULL, NULL, NULL, NULL, 0);
         if (serv->password[0])
         {
            sprintf (outbuf, "PASS %s\r\n", serv->password);
            tcp_send (serv, outbuf);
         }
         snprintf (outbuf, 511, "NICK %s\r\nUSER %s 0 0 :%s\r\n", serv->nick,
                  prefs.username, prefs.realname);
         tcp_send (serv, outbuf);
      } else
      {
         EMIT_SIGNAL (XP_TE_SERVERCONNECTED, sess, NULL, NULL, NULL, NULL, 0);
      }
      fcntl (serv->sok, F_SETFL, O_NONBLOCK);
      set_server_name (serv, serv->servername);
      break;
   case '5':                   /* prefs ip discovered */
      waitline (serv->childread, tbuf, sizeof tbuf);
      prefs.local_ip = inet_addr (tbuf);
      break;
   /*case '6':*/                  /* bind() returned -1 */
      /*waitline (serv->childread, tbuf, sizeof tbuf);
      sprintf (outbuf, "bind() failed, errno=%s\nCheck your IP Settings!", errorstring (atoi (tbuf)));
      PrintText (sess, outbuf);
      break;*/
   case '7':                  /* gethostbyname (prefs.hostname) failed */
      sprintf (outbuf, "Cannot resolv hostname %s\nCheck your IP Settings!", prefs.hostname);
      PrintText (sess, outbuf);
      break;
   }
}

static int
check_connecting (struct session *sess)
{
   char tbuf[256];

   if (sess->server->connecting)
   {
      kill (sess->server->childpid, SIGKILL);
      sprintf (tbuf, "%d", sess->server->childpid);
      EMIT_SIGNAL (XP_TE_SCONNECT, sess, tbuf, NULL, NULL, NULL, 0);
      server_cleanup (sess->server);
      close (sess->server->sok);
      return TRUE;
   }
   return FALSE;
}

int
close_socket (int sok)
{
   close (sok);
   return 0;
}

void
disconnect_server (struct session *sess, int sendquit, int err)
{
   struct session *orig_sess = sess;
   struct server *serv = sess->server;
   GSList *list;

   if (check_connecting (sess))
      return;

   if (!serv->connected)
   {
      notc_msg (sess);
      return;
   }
   flush_server_queue (serv);

   if (serv->iotag != -1)
   {
      fe_input_remove (serv->iotag);
      serv->iotag = -1;
   }
   list = sess_list;
   while (list)                 /* print "Disconnected" to each window using this server */
   {
      sess = (struct session *) list->data;
      if (sess->server == serv)
         EMIT_SIGNAL (XP_TE_DISCON, sess, errorstring (err), NULL, NULL, NULL, 0);
      list = list->next;
   }

   sess = orig_sess;
   if (sendquit)
   {
      char tbuf[256];
      if (!sess->quitreason)
         sess->quitreason = prefs.quitreason;
      snprintf (tbuf, sizeof tbuf, "QUIT :%s\r\n", sess->quitreason);
      sess->quitreason = 0;
      tcp_send (serv, tbuf);
   }

   /* close it in 5 seconds so the QUIT doesn't get lost. Technically      *
    * we should send a QUIT and then wait for the server to disconnect us, *
      but that would hold up the GUI                                       */
   if (fe_timeout_add (5000, close_socket, (void *)serv->sok) == -1)
      close (serv->sok);

   serv->sok = -1;
   serv->pos = 0;
   serv->connected = FALSE;
   serv->motd_skipped = FALSE;
   serv->no_login = FALSE;
   serv->servername[0] = 0;

   list = sess_list;
   while (list)
   {
      sess = (struct session *) list->data;
      if (sess->server == serv)
      {
         if (sess->channel[0])
         {
            if (!sess->is_dialog && !sess->is_server)
            {
               clear_channel (sess);
            }
         } else
         {
            clear_channel (sess);
         }
      }
      list = list->next;
   }
}

void
connect_server (struct session *sess, char *server, int port, int no_login)
{
   int sok, sw, pid, read_des[2];

   if (!server[0])
      return;

   sess = sess->server->front_session;

   if (sess->server->connected)
      disconnect_server (sess, TRUE, -1);
   else
      check_connecting (sess);

   fe_progressbar_start (sess);

   EMIT_SIGNAL (XP_TE_SERVERLOOKUP, sess, server, NULL, NULL, NULL, 0);

   sok = socket (AF_INET, SOCK_STREAM, 0);
   if (sok == -1)
      return;

   sw = 1;
   setsockopt (sok, SOL_SOCKET, SO_REUSEADDR, (char *) &sw, sizeof (sw));
   sw = 1;
   setsockopt (sok, SOL_SOCKET, SO_KEEPALIVE, (char *) &sw, sizeof (sw));

   strcpy (sess->server->servername, server);
   sess->server->nickcount = 1;
   sess->server->connecting = TRUE;
   sess->server->sok = sok;
   sess->server->port = port;
   sess->server->end_of_motd = FALSE;
   sess->server->no_login = no_login;
   flush_server_queue (sess->server);

   if (pipe (read_des) < 0)
      return;
#ifdef __EMX__                   /* if os/2 */
   setmode (read_des[0], O_BINARY);
   setmode (read_des[1], O_BINARY);
#endif

   switch (pid = fork ())
   {
   case -1:
      return;

   case 0:
      {
         struct sockaddr_in DSAddr; /* for binding our local hostname. */
         struct sockaddr_in SAddr;  /* for server connect. */
         struct hostent *HostAddr;

         dup2 (read_des[1], 1);  /* make the pipe our stdout */
         setuid (getuid ());

         /* is a hostname set at all? */
         if (prefs.hostname[0])
         {
            HostAddr = gethostbyname (prefs.hostname);
            if (HostAddr)
            {
               memset (&DSAddr, 0, sizeof (DSAddr));
               memcpy ((void *) &DSAddr.sin_addr, HostAddr->h_addr, HostAddr->h_length);
               printf ("5\n%s\n", inet_ntoa (DSAddr.sin_addr));
               bind (sok, (struct sockaddr *) &DSAddr, sizeof (DSAddr));
            } else
            {
               printf ("7\n");
            }
            fflush (stdout);
         }

         /* resolv & connect to the IRC server here */
         HostAddr = gethostbyname (server);
         if (HostAddr)
         {
            printf ("3\n%s\n%s\n%d\n", HostAddr->h_name,
                    inet_ntoa (*((struct in_addr *) HostAddr->h_addr)),
                    port);
            fflush (stdout);
            memset (&SAddr, 0, sizeof (SAddr));
            SAddr.sin_port = htons (port);
            SAddr.sin_family = AF_INET;
            memcpy ((void *) &SAddr.sin_addr, HostAddr->h_addr, HostAddr->h_length);
            if (connect (sok, (struct sockaddr *) &SAddr, sizeof (SAddr)) < 0)
               printf ("2\n%d\n", errno);
            else
               printf ("4\n");
         } else
         {
            printf ("1\n");
         }
         fflush (stdout);
         _exit (0);
      }
   }
   sess->server->childpid = pid;
   sess->server->iotag = fe_input_add (read_des[0], 1, 0, 0,
                                       connected_signal, sess->server);
   sess->server->childread = read_des[0];
   sess->server->childwrite = read_des[1];
}
