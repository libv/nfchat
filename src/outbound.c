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
#include "userlist.h"
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

extern struct xchatprefs prefs;

extern void auto_reconnect (int send_quit, int err);
extern int tcp_send (char *buf);
extern void connect_server (char *server, int port, int quiet);
extern void channel_action (char *tbuf, char *from, char *text, int fromme);
extern void user_new_nick (char *outbuf, char *nick, char *newnick, int quiet);
extern void channel_msg (char *outbuf, char *from, char *text, char fromme);
extern void disconnect_server (int sendquit, int err);
extern void PrintText (char *text);

#ifdef MEMORY_DEBUG
extern int current_mem_usage;
#endif

static void
notj_msg (void)
{
   PrintText ("No channel joined.\n");
}

void
notc_msg (void)
{
  PrintText ("Not connected to a server.\n");
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

static int cmd_help (char *tbuf, char *word[], char *word_eol[]);
static int cmd_me (char *tbuf, char *word[], char *word_eol[]);
static int cmd_msg (char *tbuf, char *word[], char *word_eol[]);
static int cmd_nick (char *tbuf, char *word[], char *word_eol[]);

commands_t **cmds;

static commands_t * 
add_command (char *name, char needserver, char needchannel, char *help)
{
  commands_t *cmd;
 
  cmd = malloc(sizeof(commands_t));

  if (name)
      strcpy(cmd->name = malloc(strlen(name) + 1), name);
  else
    cmd->name = NULL;

  cmd->needserver = needserver;
  cmd->needchannel = needchannel;

  if (help)
      strcpy(cmd->help = malloc(strlen(help) + 1), help);
  else
    cmd->help = NULL;

  return (cmd);
}

void
init_commands (void)
{
  if (prefs.allow_nick)
    cmds = malloc(5*sizeof(commands_t *));
  else
    cmds = malloc(4*sizeof(commands_t *));

  cmds[0] = add_command ("HELP", 0, 0, 0);
  cmds[0]->callback = cmd_help;
  cmds[1] = add_command ("ME", 1, 1, "/ME <action>, sends the action to the current channel (actions are written in the 3rd person, like /me jumps)\n");
  cmds[1]->callback = cmd_me;
  cmds[2] = add_command ("MSG", 0, 0, "/MSG <nick> <message>, sends a private message\n");
  cmds[2]->callback = cmd_msg;

  if (prefs.allow_nick)
    {
      cmds[3] = add_command ("NICK", 0, 0, "/NICK <nickname>, sets your nick\n");
      cmds[3]->callback = cmd_nick;
      cmds[4] = add_command ( 0, 0, 0, 0);
    }
  else
    cmds[3] = add_command ( 0, 0, 0, 0);
}

static void
help (char *helpcmd, int quiet)
{
   int i = 0;
   while (1)
   {
      if (!cmds[i]->name)
         break;
      if (!strcasecmp (helpcmd, cmds[i]->name))
      {
         if (cmds[i]->help)
         {
            PrintText ("Usage:");
            PrintText (cmds[i]->help);
            return;
         } else
         {
            if (!quiet)
	      PrintText ("\nNo help available on that command.\n");
            return;
         }
      }
      i++;
   }
   if (!quiet)
     PrintText ("No such command.\n");
}

#define find_word_to_end(a, b) word_eol[b]
#define find_word(a, b) word[b]

int
cmd_help (char *tbuf, char *word[], char *word_eol[])
{
  int i = 0, longfmt = 0;
  char *helpcmd = "";
  
  if (tbuf)
    helpcmd = find_word (pdibuf, 2);
  if (*helpcmd && strcmp (helpcmd, "-l") == 0)
    longfmt = 1;
  
  if (*helpcmd && !longfmt)
    {
      help (helpcmd, FALSE);
    } else
      {
	char *buf = malloc (4096);
	int t = 1, j;
	strcpy (buf, "\nCommands Available:\n\n  ");
	if (longfmt)
	  {
	    while (1)
	      {
		if (!cmds[i]->name)
		  break;
		if (!cmds[i]->help)
		  snprintf (buf, 4096, "   \0034%s\003 :\n", cmds[i]->name);
		else
		  snprintf (buf, 4096, "   \0034%s\003 : %s", cmds[i]->name,
			    cmds[i]->help);
		PrintText (buf);
		i++;
	      }
	  } else
	    {
	      while (1)
		{
		  if (!cmds[i]->name)
		    break;
		  strcat (buf, cmds[i]->name);
		  t++;
		  if (t == 6)
		    {
		      t = 1;
		      strcat (buf, "\n  ");
		    } else
		      for (j = 0; j < (10 - strlen (cmds[i]->name)); j++)
			strcat (buf, " ");
		  i++;
		}
	    }
	strcat (buf, "\n\nType /HELP <command> for more information, or /HELP -l\n\n");

      PrintText (buf);
      free (buf);
      }
  return TRUE;
}

int
cmd_me (char *tbuf, char *word[], char *word_eol[])
{
   char *act = find_word_to_end (cmd, 2);
   if (*act)
   {
      channel_action (tbuf, server->nick, act, TRUE);
      sprintf (tbuf, "\001ACTION %s\001\r", act);
      sprintf (tbuf, "PRIVMSG %s :\001ACTION %s\001\r\n", session->channel, act);
      tcp_send (tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_msg (char *tbuf, char *word[], char *word_eol[])
{
  char *nick = find_word (pdibuf, 2);
  char *msg = find_word_to_end (cmd, 3);
  
  if (*nick)
    {
      if (*msg)
	{
	  if (strcmp (nick, ".") == 0)
	    {                      /* /msg the last nick /msg'ed */
	      if (session->lastnick[0])
		nick = session->lastnick;
	    } else
	      strcpy (session->lastnick, nick);  /* prime the last nick memory */
	  
	  if (*nick == '=')
	    {
	      nick++;
	      return TRUE;
	    } else
	      {
		if (!server->connected)
		  {
		    notc_msg ();
		    return TRUE;
		  }
		sprintf (tbuf, "PRIVMSG %s :%s\r\n", nick, msg);
		tcp_send (tbuf);
	      }
	  fire_signal (XP_TE_MSGSEND, nick, msg, NULL, NULL, 0);

	  return TRUE;
	}
    }
  return FALSE;
}

int
cmd_nick (char *tbuf, char *word[], char *word_eol[])
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
check_nick_completion (char *cmd, char *tbuf)
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

            list = session->userlist;
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
handle_command (char *cmd)
{
   int user_cmd = FALSE, i;
   unsigned char pdibuf[2048];
   unsigned char tbuf[4096];
   char *word[32];
   char *word_eol[32];

   if (!session || !*cmd)
      return TRUE;

   if (cmd[0] == '/')
   {
      cmd++;

      process_data_init (pdibuf, cmd, word, word_eol);

      if (fire_signal (XP_USERCOMMAND, pdibuf, word, word_eol, NULL, 0) == 1)
         return TRUE;

      if (user_cmd)
         return TRUE;

      check_special_chars (cmd);

      i = 0;
      while (1)
      {
         if (!cmds[i]->name)
            break;
         if (!strcasecmp (pdibuf, cmds[i]->name))
	   {
	     if (cmds[i]->needserver && !server->connected)
	       {
		 notc_msg ();
		 return TRUE;
	       }
	     if (cmds[i]->needchannel && session->channel[0] == 0)
	       {
		 notj_msg ();
		 return TRUE;
	       }
	     switch (cmds[i]->callback (tbuf, word, word_eol))
	       {
	       case FALSE:
		 help (cmds[i]->name, TRUE);
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
	  notc_msg ();
	  return FALSE;
	}
      PrintText ("Unknown Command. Try /help\n");
      return TRUE;
   } else
   {
      check_special_chars (cmd);

      if (session->channel[0])
      {
         char newcmd[4096];
         if (prefs.nickcompletion)
            check_nick_completion (cmd, newcmd);
         else
            newcmd[0] = 0;
         if (!newcmd[0])
	   {
	     if (server->connected)
               {
		 channel_msg (tbuf, server->nick, cmd, TRUE);
		 sprintf (tbuf, "PRIVMSG %s :%s\r\n", session->channel, cmd);
               } else
		 {
		   notc_msg ();
		   return TRUE;
		 }
	   } else
	     {
	       if (server->connected)
		 {
		   channel_msg (tbuf, server->nick, newcmd, TRUE);
		   sprintf (tbuf, "PRIVMSG %s :%s\r\n", session->channel, newcmd);
		 } else
		   {
		     notc_msg ();
		     return TRUE;
		   }
	     }
         tcp_send (tbuf);
      } else
	notj_msg ();
   }
   return TRUE;
}

void
handle_multiline (char *cmd)
{
  char *cr;

  cr = strchr (cmd, '\n');
  if (cr)
    while (1)
      {
	if (cr)
	  *cr = 0;
	if (!handle_command (cmd))
	  return;
	if (!cr)
	  break;
	cmd = cr + 1;
	if (*cmd == 0)
	  break;
	cr = strchr (cmd, '\n');
      }
  else if (!handle_command (cmd))
    return;
}
