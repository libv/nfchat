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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "xchat.h"
#include "signals.h"
#include "util.h"
#include "fe.h"
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "popup.h"


int handle_command (char *cmd, session_t *sess, int history, int);
void handle_multiline (session_t *sess, char *cmd, int history, int);
void check_special_chars (char *cmd);

extern GSList *sess_list;
extern struct xchatprefs prefs;
extern session_t *current_tab;

extern void auto_reconnect (int send_quit, int err);
extern void do_dns (session_t *sess, char *tbuf, char *nick, char *host);
extern int tcp_send (char *buf);
extern void PrintText (session_t *sess, char *text);
extern void connect_server (session_t *sess, char *server, int port, int quiet);
extern void channel_action (char *tbuf, char *chan, char *from, char *text, int fromme);
extern void user_new_nick (char *outbuf, char *nick, char *newnick, int quiet);
extern void channel_msg (char *outbuf, char *chan, char *from, char *text, char fromme);
extern void disconnect_server (session_t *sess, int sendquit, int err);

#ifdef MEMORY_DEBUG
extern int current_mem_usage;
#endif

void
notj_msg (session_t *sess)
{
   PrintText (sess, "No channel joined. Try /join #<channel>\n");
}

void
notc_msg (session_t *sess)
{
   PrintText (sess, "Not connected. Try /server <host> [<port>]\n");
}

void
process_data_init (unsigned char *buf, char *cmd, char *word[], char *word_eol[])
{
   int wordcount = 2;
   int space = FALSE;
   int quote = FALSE;
   int j = 0;

   word[1] = cmd;
   word_eol[1] = buf;

   while (1)
   {
      switch (*cmd)
      {
      case 0:
       jump:
         buf[j] = 0;
         for (j = wordcount; j < 32; j++)
         {
            word[j] = "\000\000";
            word_eol[j] = "\000\000";
         }
         return;
      case '\042':
         if (quote)
            quote = FALSE;
         else
            quote = TRUE;
         break;
      case ' ':
         if (!quote)
         {
            if (!space)
            {
               buf[j] = 0;
               j++;

               word[wordcount] = &buf[j];
               word_eol[wordcount] = cmd + 1;
               wordcount++;

               if (wordcount == 31)
                  goto jump;

               space = TRUE;
            }
            break;
         }
      default:
         space = FALSE;
         buf[j] = *cmd;
         j++;
      }
      cmd++;
   }
}

static int cmd_debug (session_t *sess, char *tbuf, char *word[], char *word_eol[]);

int cmd_help (session_t *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_join (session_t *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_me (session_t *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_msg (session_t *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_nick (session_t *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_part (session_t *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_quit (session_t *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_reconnect (session_t *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_server (session_t *sess, char *tbuf, char *word[], char *word_eol[]);

struct commands cmds[] =
{
   {"DEBUG", cmd_debug, 0, 0, 0},
   {"HELP", cmd_help, 0, 0, 0},
   {"JOIN", cmd_join, 1, 0, "/JOIN <channel>, joins the channel\n"},
   {"ME", cmd_me, 1, 1, "/ME <action>, sends the action to the current channel (actions are written in the 3rd person, like /me jumps)\n"},
   {"MSG", cmd_msg, 0, 0, "/MSG <nick> <message>, sends a private message\n"},
   {"NICK", cmd_nick, 0, 0, "/NICK <nickname>, sets your nick\n"},
   {"PART", cmd_part, 1, 1, "/PART [<channel>] [<reason>], leaves the channel, by default the current one\n"},
   {"QUIT", cmd_quit, 0, 0, "/QUIT [<reason>], disconnects from the current server\n"},
   {"RECONNECT", cmd_reconnect, 0, 0, "/RECONNECT, reconnects to the current server\n"},
   {"SERVER", cmd_server, 0, 0, "/SERVER <host> [<port>] [<password>], connects to a server, the default port is 6667\n"},
   {0, 0, 0, 0, 0}
};

static void
help (session_t *sess, char *helpcmd, int quiet)
{
   int i = 0;
   while (1)
   {
      if (!cmds[i].name)
         break;
      if (!strcasecmp (helpcmd, cmds[i].name))
      {
         if (cmds[i].help)
         {
            PrintText (sess, "Usage:");
            PrintText (sess, cmds[i].help);
            return;
         } else
         {
            if (!quiet)
               PrintText (sess, "\nNo help available on that command.\n");
            return;
         }
      }
      i++;
   }
   if (!quiet)
      PrintText (sess, "No such command.\n");
}

#define find_word_to_end(a, b) word_eol[b]
#define find_word(a, b) word[b]

int
cmd_debug (session_t *sess, char *tbuf, char *word[], char *word_eol[])
{
   session_t *s;
   server_t *v;
   GSList *list = sess_list;

   PrintText (sess, "Session   Channel    WaitChan  WillChan  Server\n");
   while (list)
   {
      s = (session_t *) list->data;
      sprintf (tbuf, "0x%lx %-10.10s %-10.10s %-10.10s 0x%lx\n",
               (unsigned long) s, s->channel, s->waitchannel, s->willjoinchannel,
           (unsigned long) server);
      PrintText (sess, tbuf);
      list = list->next;
   }


   PrintText (sess, "Server    Sock  Name\n");

   v = (server_t *) server;
   sprintf (tbuf, "0x%lx %-5ld %s\n",
	    (unsigned long) v, (unsigned long) v->sok, v->servername);
   PrintText (sess, tbuf);

   sprintf (tbuf,
            "\nsession: %lx\n"
            "current_tab: %lx\n\n",
            (unsigned long) server->session,
         (unsigned long) current_tab);
   PrintText (sess, tbuf);
#ifdef MEMORY_DEBUG
   sprintf (tbuf, "current mem: %d\n\n", current_mem_usage);
   PrintText (sess, tbuf);
#endif /* MEMORY_DEBUG */

   return TRUE;
}

int
cmd_quit (session_t *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (*word_eol[2])
      sess->quitreason = word_eol[2];
   else
      sess->quitreason = prefs.quitreason;
   disconnect_server (sess, TRUE, -1);
   return 2;
}

int
cmd_help (session_t *sess, char *tbuf, char *word[], char *word_eol[])
{
   int i = 0, longfmt = 0;
   char *helpcmd = "";

   if (tbuf)
      helpcmd = find_word (pdibuf, 2);
   if (*helpcmd && strcmp (helpcmd, "-l") == 0)
      longfmt = 1;

   if (*helpcmd && !longfmt)
   {
      help (sess, helpcmd, FALSE);
   } else
   {
      char *buf = malloc (4096);
      int t = 1, j;
      strcpy (buf, "\nCommands Available:\n\n  ");
      if (longfmt)
      {
         while (1)
         {
            if (!cmds[i].name)
               break;
            if (!cmds[i].help)
               snprintf (buf, 4096, "   \0034%s\003 :\n", cmds[i].name);
            else
               snprintf (buf, 4096, "   \0034%s\003 : %s", cmds[i].name,
                         cmds[i].help);
            PrintText (sess, buf);
            i++;
         }
      } else
      {
         while (1)
         {
            if (!cmds[i].name)
               break;
            strcat (buf, cmds[i].name);
            t++;
            if (t == 6)
            {
               t = 1;
               strcat (buf, "\n  ");
            } else
               for (j = 0; j < (10 - strlen (cmds[i].name)); j++)
                  strcat (buf, " ");
            i++;
         }
      }
      strcat (buf, "\n\nType /HELP <command> for more information, or /HELP -l\n\n");

      PrintText (sess, buf);
      free (buf);
   }
   return TRUE;
}

int
cmd_join (session_t *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *chan = find_word (pdibuf, 2);
   if (*chan)
   {
      char *po, *pass = find_word (pdibuf, 3);
      if (*pass)
         sprintf (tbuf, "JOIN %s %s\r\n", chan, pass);
      else
         sprintf (tbuf, "JOIN %s\r\n", chan);
      tcp_send (tbuf);
      if (sess->channel[0] == 0)
      {
         po = strchr (chan, ',');
         if (po)
            *po = 0;
         strncpy (sess->waitchannel, chan, 200);
      }
      return TRUE;
   }
   return FALSE;
}

int
cmd_me (session_t *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *act = find_word_to_end (cmd, 2);
   if (*act)
   {
      channel_action (tbuf, sess->channel, server->nick, act, TRUE);
      sprintf (tbuf, "\001ACTION %s\001\r", act);
      sprintf (tbuf, "PRIVMSG %s :\001ACTION %s\001\r\n", sess->channel, act);
      tcp_send (tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_msg (session_t *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *nick = find_word (pdibuf, 2);
   char *msg = find_word_to_end (cmd, 3);

   if (*nick)
   {
      if (*msg)
      {
         if (strcmp (nick, ".") == 0)
         {                      /* /msg the last nick /msg'ed */
            if (sess->lastnick[0])
               nick = sess->lastnick;
         } else
            strcpy (sess->lastnick, nick);  /* prime the last nick memory */

         if (*nick == '=')
         {
            nick++;
            return TRUE;
         } else
         {
            if (!server->connected)
            {
               notc_msg (sess);
               return TRUE;
            }
            sprintf (tbuf, "PRIVMSG %s :%s\r\n", nick, msg);
            tcp_send (tbuf);
         }
         EMIT_SIGNAL (XP_TE_MSGSEND, sess, nick, msg, NULL, NULL, 0);

         return TRUE;
      }
   }
   return FALSE;
}

int
cmd_nick (session_t *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *nick = find_word (pdibuf, 2);
   if (*nick)
   {
      if (server->connected)
      {
         sprintf (tbuf, "NICK %s\r\n", nick);
         tcp_send (tbuf);
      } else
         user_new_nick (tbuf, server->nick, nick, TRUE);
      return TRUE;
   }
   return FALSE;
}

int
cmd_part (session_t *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *chan = find_word (pdibuf, 2);
   char *reason = word_eol[3];
   if (!*chan)
      chan = sess->channel;
   if ((*chan) && is_channel (chan))
   {
      sprintf (tbuf, "PART %s :%s\r\n", chan, reason);
      tcp_send (tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_reconnect (session_t *sess, char *tbuf, char *word[], char *word_eol[])
{
   int tmp = prefs.recon_delay;
  
   prefs.recon_delay = 0;

   auto_reconnect (TRUE, -1);
  
   prefs.recon_delay = tmp;

   return TRUE;
}

int
cmd_server (session_t *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *server_str = find_word (pdibuf, 2);
   char *port = find_word (pdibuf, 3);
   char *pass = find_word (pdibuf, 4);
   char *co;

   if (*server_str)
   {
      if (strncasecmp ("irc://", server_str, 6) == 0)
         server_str += 6;
      co = strchr (server_str, ':');
      if (co)
      {
         port = co + 1;
         *co = 0;
         pass = find_word (pdibuf, 3);
      }
      sess->willjoinchannel[0] = 0;
      if (pass)
         strcpy (server->password, pass);
      if (*port)
         connect_server (sess, server_str, atoi (port), FALSE);
      else
         connect_server (sess, server_str, 6667, FALSE);
      return TRUE;
   }
   return FALSE;
}

void
check_special_chars (char *cmd) /* check for %X */
{
   int occur = 0;
   int len = strlen (cmd);
   char *buf = malloc (len + 1);
   if (buf)
   {
      int i = 0, j = 0;
      while (cmd[j])
      {
         switch (cmd[j])
         {
         case '%':
            occur++;
            if (j + 3 < len && (isdigit (cmd[j + 1]) && isdigit (cmd[j + 2]) && isdigit (cmd[j + 3])))
            {
               char tbuf[4];
               tbuf[0] = cmd[j + 1];
               tbuf[1] = cmd[j + 2];
               tbuf[2] = cmd[j + 3];
               tbuf[3] = 0;
               buf[i] = atoi (tbuf);
               j += 3;
            } else
            {
               switch (cmd[j + 1])
               {
               case 'R':
                  buf[i] = '\026';
                  break;
               case 'U':
                  buf[i] = '\037';
                  break;
               case 'B':
                  buf[i] = '\002';
                  break;
               case 'C':
                  buf[i] = '\003';
                  break;
               case 'O':
                  buf[i] = '\017';
                  break;
               case 'T':
		 buf[i] = '\t';
		 break;
               case '%':
                  buf[i] = '%';
                  break;
               default:
                  buf[i] = '%';
                  j--;
                  break;
               }
               j++;
               break;
         default:
               buf[i] = cmd[j];
            }
         }
         j++;
         i++;
      }
      buf[i] = 0;
      if (occur)
         strcpy (cmd, buf);
      free (buf);
   }
}

#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

static void
check_nick_completion (session_t *sess, char *cmd, char *tbuf)
{
   long len;
   char *space = strchr (cmd, ' ');
   if (space && space != cmd)
   {
      if (space[-1] == ':' && space - 1 != cmd)
      {
         len = (long) space - (long) cmd - 1;
         if (len < 64)
         {
            struct user *user;
            struct user *best = NULL;
            int bestlen = INT_MAX, lenu;
            char nick[64];
            GSList *list;

            memcpy (nick, cmd, len);
            nick[len] = 0;

            list = sess->userlist;
            while (list)
            {
               user = (struct user *)list->data;
               if (!strncasecmp (user->nick, nick, len))
               {
                  lenu = strlen (user->nick);
                  if (lenu == len)
                  {
                     sprintf (tbuf, "%s:%s", user->nick, space);
                     return;
                  } else if (lenu < bestlen)
                  {
                     bestlen = lenu;
                     best = user;
                  }
               }
               list = list->next;
            }
            if (best)
            {
               sprintf (tbuf, "%s:%s", best->nick, space);
               return;
            }
         }
      }
   }
   tbuf[0] = 0;
}

int
handle_command (char *cmd, session_t *sess, int history, int nocommand)
{
   int user_cmd = FALSE, i;
   unsigned char pdibuf[2048];
   unsigned char tbuf[4096];
   char *word[32];
   char *word_eol[32];

   if (!sess || !*cmd)
      return TRUE;

   if (history)
      history_add (&sess->history, cmd);

   if (cmd[0] == '/' && !nocommand)
   {
      cmd++;

      process_data_init (pdibuf, cmd, word, word_eol);

      if (EMIT_SIGNAL (XP_USERCOMMAND, sess, pdibuf, word, word_eol, NULL, 0) == 1)
         return TRUE;

      if (user_cmd)
         return TRUE;

      check_special_chars (cmd);

      i = 0;
      while (1)
      {
         if (!cmds[i].name)
            break;
         if (!strcasecmp (pdibuf, cmds[i].name))
         {
            if (cmds[i].needserver && !server->connected)
            {
               notc_msg (sess);
               return TRUE;
            }
            if (cmds[i].needchannel && sess->channel[0] == 0)
            {
               notj_msg (sess);
               return TRUE;
            }
            switch (cmds[i].callback (sess, tbuf, word, word_eol))
            {
            case FALSE:
               help (sess, cmds[i].name, TRUE);
               break;
            case 2:
               return FALSE;
            }
            return TRUE;
         }
         i++;
      }
      if (!server->connected)
      {
         PrintText (sess, "Unknown Command. Try /help\n");
         return TRUE;
      }
      sprintf (tbuf, "%s\r\n", cmd);
      tcp_send (tbuf);

   } else
   {
      check_special_chars (cmd);

      if (sess->channel[0] && !sess->is_server)
      {
         char newcmd[4096];
         if (prefs.nickcompletion)
            check_nick_completion (sess, cmd, newcmd);
         else
            newcmd[0] = 0;
         if (!newcmd[0])
	   {
	     if (server->connected)
               {
		 channel_msg (tbuf, sess->channel, server->nick, cmd, TRUE);
		 sprintf (tbuf, "PRIVMSG %s :%s\r\n", sess->channel, cmd);
               } else
		 {
		   notc_msg (sess);
		   return TRUE;
		 }
	   } else
	     {
	       if (server->connected)
		 {
		   channel_msg (tbuf, sess->channel, server->nick, newcmd, TRUE);
		   sprintf (tbuf, "PRIVMSG %s :%s\r\n", sess->channel, newcmd);
		 } else
		   {
		     notc_msg (sess);
		     return TRUE;
		   }
	     }
         tcp_send (tbuf);
      } else
	notj_msg (sess);
   }
   return TRUE;
}

void
handle_multiline (session_t *sess, char *cmd, int history, int nocommand)
{
   char *cr;

   cr = strchr (cmd, '\n');
   if (cr)
   {
      while (1)
      {
         if (cr)
            *cr = 0;
         if (!handle_command (cmd, sess, history, nocommand))
            return;
         if (!cr)
            break;
         cmd = cr + 1;
         if (*cmd == 0)
            break;
         cr = strchr (cmd, '\n');
      }
   } else
   {
      if (!handle_command (cmd, sess, history, nocommand))
         return;
   }

}
