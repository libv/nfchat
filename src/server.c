/*
 * NF-Chat: A cut down version of X-chat, cut down by _Death_ 
 * Largely based upon X-Chat 1.4.2 by Peter Zelezny. (www.xchat.org)
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

extern struct xchatprefs prefs;

extern void auto_reconnect (int send_quit, int err);
extern void clear_channel ();
extern void set_server_name (char *name);
extern void flush_server_queue (void);
extern int tcp_send (char *buf);
extern void read_data (gint sok);
extern void notc_msg (void);
extern void PrintText (char *text);
extern int fe_timeout_add (int interval, void *callback, void *userdata);
extern void fe_new_window (void);
extern int fe_input_add (int sok, int read, int write, int ex, void *func);
extern void fe_input_remove (int tag);
extern void fe_progressbar_start (void);
extern void fe_progressbar_end (void);

static int
waitline (int sok, char *buf, int bufsize)
{
   int i = 0;

   while (1)
   {
      if (read (sok, &buf[i], 1) < 1)
         return -1;
      if (buf[i] == '\n')
      {
         buf[i] = 0;
         return i;
      }
      i++;
      if (i == (bufsize - 1))
         return 0;
   }
}

static char *
errorstring (int err)
{
   static char tbuf[16];
   switch (err)
   {
   case -1:
      return "";
   case 0:
      return "Remote host closed socket";
   case ECONNREFUSED:
      return "Connection refused";
   case ENETUNREACH:
   case EHOSTUNREACH:
      return "No route to host";
   case ETIMEDOUT:
      return "Connection timed out";
   case EADDRNOTAVAIL:
      return "Cannot assign that address";
   case ECONNRESET:
      return "Connection reset by peer";
   }
   sprintf (tbuf, "%d", err);
   return tbuf;
}

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
   fe_progressbar_end ();
   server->connecting = 0;
}

static void
connected_signal (int sok)
{
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
      fire_signal (XP_TE_UKNHOST, NULL, NULL, NULL, NULL, 0);
      break;
    case '2':                   /* connection failed */
      waitline (server->childread, tbuf, sizeof tbuf);
      server_cleanup ();
      close (sok);
      fire_signal (XP_TE_CONNFAIL, errorstring (atoi (tbuf)), NULL, NULL, NULL, 0);
      if (prefs.autoreconnectonfail)
	auto_reconnect (FALSE, -1);
      break;
    case '3':                   /* gethostbyname finished */
      waitline (server->childread, host, sizeof host);
      waitline (server->childread, ip, sizeof ip);
      waitline (server->childread, outbuf, sizeof outbuf);
      fire_signal (XP_TE_CONNECT, host, ip, outbuf, NULL, 0);
      strcpy (server->hostname, host);
      break;
    case '4':                   /* success */
      server_cleanup ();
      server->connected = TRUE;
      
      server->iotag = fe_input_add (server->sok, 1, 0, 1, read_data);
      if (!server->no_login)
	{
	  fire_signal (XP_TE_CONNECTED, NULL, NULL, NULL, NULL, 0);
	  if (server->password[0])
	    {
	      sprintf (outbuf, "PASS %s\r\n", server->password);
	      tcp_send (outbuf);
	    }
	  snprintf (outbuf, 511, "NICK %s\r\nUSER %s 0 0 :%s\r\n", server->nick, prefs.username,
prefs.realname);
	  tcp_send (outbuf);
	}
      else
	fire_signal (XP_TE_SERVERCONNECTED, NULL, NULL, NULL, NULL, 0);
      
      fcntl (server->sok, F_SETFL, O_NONBLOCK);
      set_server_name (server->servername);
      break;
    case '5':                   /* prefs ip discovered */
      waitline (server->childread, tbuf, sizeof tbuf);
      prefs.local_ip = inet_addr (tbuf);
      break;
      /*case '6': not needed aparently bind() returned -1 */
    case '7':                  /* gethostbyname (prefs.hostname) failed */
      sprintf (outbuf, "Cannot resolve hostname %s\nCheck your IP Settings!", prefs.hostname);
      PrintText (outbuf);
      break;
   }
}

static int
check_connecting (void)
{
   char tbuf[256];

   if (server->connecting)
   {
      kill (server->childpid, SIGKILL);
      sprintf (tbuf, "%d", server->childpid);
      fire_signal (XP_TE_SCONNECT, tbuf, NULL, NULL, NULL, 0);
      server_cleanup ();
      close (server->sok);
      return TRUE;
   }
   return FALSE;
}

int
close_socket (void)
{
   close (server->sok);
   return 0;
}

void
disconnect_server (int sendquit, int err)
{
   if (check_connecting ())
      return;

   if (!server->connected)
   {
      notc_msg ();
      return;
   }
   flush_server_queue ();

   if (server->iotag != -1)
   {
      fe_input_remove (server->iotag);
      server->iotag = -1;
   }
   fire_signal (XP_TE_DISCON, errorstring (err), NULL, NULL, NULL, 0);

   if (sendquit)
   {
      char tbuf[256];
      if (!session->quitreason)
         session->quitreason = prefs.quitreason;
      snprintf (tbuf, sizeof tbuf, "QUIT :%s\r\n", session->quitreason);
      session->quitreason = 0;
      tcp_send (tbuf);
   }

   /* close it in 5 seconds so the QUIT doesn't get lost. Technically      *
    * we should send a QUIT and then wait for the server to disconnect us, *
      but that would hold up the GUI                                       */
   if (fe_timeout_add (5000, close_socket, NULL) == -1)
      close (server->sok);

   server->sok = -1;
   server->pos = 0;
   server->connected = FALSE;
   server->motd_skipped = FALSE;
   server->no_login = FALSE;
   server->servername[0] = 0;

   clear_channel ();
}

void
connect_server (char *server_str, int port, int no_login)
{
   int sok, sw, pid, read_des[2];

   if (!server_str[0])
      return;

   if (server->connected)
      disconnect_server (TRUE, -1);
   else
      check_connecting ();

   if (!port)
     port = 6667;

   fe_progressbar_start ();

   fire_signal (XP_TE_SERVERLOOKUP, server_str, NULL, NULL, NULL, 0);

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
               printf ("7\n");
            fflush (stdout);
         }

         /* resolv & connect to the IRC server here */
         HostAddr = gethostbyname (server_str);
         if (HostAddr)
         {
            printf ("3\n%s\n%s\n%d\n", HostAddr->h_name, inet_ntoa (*((struct in_addr
*) HostAddr->h_addr)), port);
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
            printf ("1\n");
         fflush (stdout);
         _exit (0);
      }
   }
   server->childpid = pid;
   server->iotag = fe_input_add (read_des[0], 1, 0, 0, connected_signal);
   server->childread = read_des[0];
   server->childwrite = read_des[1];
}
