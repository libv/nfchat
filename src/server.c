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

extern void auto_reconnect (int send_quit, int err);
extern void clear_channel (session_t *sess);
extern void set_server_name (char *name);
extern void flush_server_queue (void);
extern int tcp_send (char *buf);
extern void PrintText (session_t *sess, unsigned char *text);
extern void read_data (gint sok);
extern char *errorstring (int err);
extern int waitline (int sok, char *buf, int bufsize);
extern void notc_msg (session_t *sess);

static void
server_cleanup (void)
{
   if (server->iotag != -1)
   {
      fe_input_remove (server->iotag);
      server->iotag = -1;
   }
   close (server->childwrite);
   close (server->childread);
   waitpid (server->childpid, NULL, 0);
   fe_progressbar_end (server->session);
   server->connecting = 0;
}

static void
connected_signal (int sok)
{
   session_t *sess = server->session;
   char tbuf[128];
   char outbuf[512];
   char host[100];
   char ip[100];

   waitline (server->childread, tbuf, sizeof tbuf);

   switch (tbuf[0])
   {
   case '1':                   /* unknown host */
      server_cleanup ();
      close (sok);
      EMIT_SIGNAL (XP_TE_UKNHOST, sess, NULL, NULL, NULL, NULL, 0);
      break;
   case '2':                   /* connection failed */
      waitline (server->childread, tbuf, sizeof tbuf);
      server_cleanup ();
      close (sok);
      EMIT_SIGNAL (XP_TE_CONNFAIL, sess, errorstring (atoi (tbuf)), NULL, NULL,
                   NULL, 0);
      if (prefs.autoreconnectonfail)
         auto_reconnect (FALSE, -1);
      break;
   case '3':                   /* gethostbyname finished */
      waitline (server->childread, host, sizeof host);
      waitline (server->childread, ip, sizeof ip);
      waitline (server->childread, outbuf, sizeof outbuf);
      EMIT_SIGNAL (XP_TE_CONNECT, sess, host, ip, outbuf, NULL, 0);
      strcpy (server->hostname, host);
      break;
   case '4':                   /* success */
     server_cleanup ();
     server->connected = TRUE;
    
     server->iotag = fe_input_add (server->sok, 1, 0, 1, read_data, server);
     if (!server->no_login)
       {
         EMIT_SIGNAL (XP_TE_CONNECTED, sess, NULL, NULL, NULL, NULL, 0);
         if (server->password[0])
	   {
	     sprintf (outbuf, "PASS %s\r\n", server->password);
	     tcp_send (outbuf);
	   }
         snprintf (outbuf, 511, "NICK %s\r\nUSER %s 0 0 :%s\r\n", server->nick,
		   prefs.username, prefs.realname);
         tcp_send (outbuf);
       }
     else
       EMIT_SIGNAL (XP_TE_SERVERCONNECTED, sess, NULL, NULL, NULL, NULL, 0);
      
     fcntl (server->sok, F_SETFL, O_NONBLOCK);
     set_server_name (server->servername);
     break;
   case '5':                   /* prefs ip discovered */
      waitline (server->childread, tbuf, sizeof tbuf);
      prefs.local_ip = inet_addr (tbuf);
      break;
   /*case '6':*/                  /* bind() returned -1 */
      /*waitline (serv->childread, tbuf, sizeof tbuf);
      sprintf (outbuf, "bind() failed, errno=%s\nCheck your IP Settings!", errorstring (atoi (tbuf)));
      PrintText (sess, outbuf);
      break;*/
   case '7':                  /* gethostbyname (prefs.hostname) failed */
      sprintf (outbuf, "Cannot resolve hostname %s\nCheck your IP Settings!", prefs.hostname);
      PrintText (sess, outbuf);
      break;
   }
}

static int
check_connecting (session_t *sess)
{
   char tbuf[256];

   if (server->connecting)
   {
      kill (server->childpid, SIGKILL);
      sprintf (tbuf, "%d", server->childpid);
      EMIT_SIGNAL (XP_TE_SCONNECT, sess, tbuf, NULL, NULL, NULL, 0);
      server_cleanup ();
      close (server->sok);
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
disconnect_server (session_t *sess, int sendquit, int err)
{
   session_t *orig_sess = sess;
   GSList *list;

   if (check_connecting (sess))
      return;

   if (!server->connected)
   {
      notc_msg (sess);
      return;
   }
   flush_server_queue ();

   if (server->iotag != -1)
   {
      fe_input_remove (server->iotag);
      server->iotag = -1;
   }

   list = sess_list;
   while (list)                 /* print "Disconnected" to each window using this server */
   {
      sess = (session_t *) list->data;
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
      tcp_send (tbuf);
   }

   /* close it in 5 seconds so the QUIT doesn't get lost. Technically      *
    * we should send a QUIT and then wait for the server to disconnect us, *
      but that would hold up the GUI                                       */
   if (fe_timeout_add (5000, close_socket, (void *)server->sok) == -1)
      close (server->sok);

   server->sok = -1;
   server->pos = 0;
   server->connected = FALSE;
   server->motd_skipped = FALSE;
   server->no_login = FALSE;
   server->servername[0] = 0;

   list = sess_list;
   while (list)
   {
      sess = (session_t *) list->data;

      if (sess->channel[0])
	{
	  if (!sess->is_server)
	    clear_channel (sess);
	} else
	  clear_channel (sess);

      list = list->next;
   }
}

void
connect_server (session_t *sess, char *server_str, int port, int no_login)
{
   int sok, sw, pid, read_des[2];

   if (!server_str[0])
      return;

   sess = server->session;

   if (server->connected)
      disconnect_server (sess, TRUE, -1);
   else
      check_connecting (sess);

   fe_progressbar_start (sess);

   EMIT_SIGNAL (XP_TE_SERVERLOOKUP, sess, server_str, NULL, NULL, NULL, 0);

   sok = socket (AF_INET, SOCK_STREAM, 0);
   if (sok == -1)
      return;

   sw = 1;
   setsockopt (sok, SOL_SOCKET, SO_REUSEADDR, (char *) &sw, sizeof (sw));
   sw = 1;
   setsockopt (sok, SOL_SOCKET, SO_KEEPALIVE, (char *) &sw, sizeof (sw));

   strcpy (server->servername, server_str);
   server->nickcount = 1;
   server->connecting = TRUE;
   server->sok = sok;
   server->port = port;
   server->end_of_motd = FALSE;
   server->no_login = no_login;
   flush_server_queue ();

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
         HostAddr = gethostbyname (server_str);
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
   server->childpid = pid;
   server->iotag = fe_input_add (read_des[0], 1, 0, 0,
                                       connected_signal, server);
   server->childread = read_des[0];
   server->childwrite = read_des[1];
}
