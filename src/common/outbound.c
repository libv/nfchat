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
#include "plugin.h"
#include "ignore.h"
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


int handle_command (char *cmd, struct session *sess, int history, int);
void handle_multiline (struct session *sess, char *cmd, int history, int);
void check_special_chars (char *cmd);

extern GSList *sess_list;
extern GSList *serv_list;
/* extern GSList *dcc_list; */
extern GSList *command_list;
extern GSList *button_list;
extern struct xchatprefs prefs;
extern struct session *current_tab;

extern int is_session (session *sess);
extern struct server *new_server (void);
extern int child_handler (int pid);
extern void auto_reconnect (struct server *serv, int send_quit, int err);
extern void do_dns (struct session *sess, char *tbuf, char *nick, char *host);
extern struct session *new_session (struct server *serv, char *from);
extern int tcp_send_len (struct server *serv, char *buf, int len);
extern int tcp_send (struct server *serv, char *buf);
extern struct session *tab_msg_session (char *target, struct server *serv);
extern struct session *find_session_from_channel (char *chan, struct server *serv);
extern int list_delentry (GSList ** list, char *name);
extern void list_addentry (GSList ** list, char *cmd, char *name);
extern struct session *find_dialog (struct server *serv, char *nick);
extern void PrintText (struct session *sess, char *text);
extern void connect_server (struct session *sess, char *server, int port, int quiet);
extern void channel_action (struct session *sess, char *tbuf, char *chan, char *from, char *text, int fromme);
extern void user_new_nick (struct server *serv, char *outbuf, char *nick, char *newnick, int quiet);
extern void channel_msg (struct server *serv, char *outbuf, char *chan, char *from, char *text, char fromme);
extern void disconnect_server (struct session *sess, int sendquit, int err);
extern void notify_showlist (struct session *sess);
extern void notify_adduser (char *name);
extern int notify_deluser (char *name);
#ifdef USE_PERL
extern int perl_load_file (char *script_name);
extern int perl_command (char *cmd, struct session *sess);
#endif
extern int module_command (char *cmd, struct session *sess, char *tbuf, char *word[], char *word_eol[]);
extern int module_load (char *name, struct session *sess);
extern int module_list (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
extern int module_unload (char *name, struct session *sess);

#ifdef MEMORY_DEBUG
extern int current_mem_usage;
#endif

/* trans */

unsigned char trans_serv2user[256];
unsigned char trans_user2serv[256];

void serv2user(unsigned char *s){
  for(;*s;++s)
    *s=trans_serv2user[*s];
}

void user2serv(unsigned char *s){
  for(;*s;++s)
    *s=trans_user2serv[*s];
}

int load_trans_table(char *full_path){ 
  int tf,i,st,val=0,t;
  char r;

  if((tf=open(full_path,O_RDONLY))!=-1){
  st=0;
  i=0;
  t=0;
  while(read(tf,&r,1)==1){
    switch(st){
    case 0:/*nothing yet...*/
      if(r=='0')
        st=1;
      break;
    case 1:
      if(r=='x') st=2;
      else st=0;
      break;
    case 2:
      if(r<='9' && r>='0') val=16*(r-'0');
      else if(r<='f' && r>='a') val=16*(r-'a'+10);
      else if(r<='F' && r>='A') val=16*(r-'A'+10);
      else {st=0;break;}
      st=3;
      break;
    case 3:
      if(r<='9' && r>='0') val+=r-'0';
      else if(r<='f' && r>='a') val+=r-'a'+10;
      else if(r<='F' && r>='A') val+=r-'A'+10;
      else {st=0;break;}
      st=0;
      if(t==0) trans_serv2user[i++]=val;
      else     trans_user2serv[i++]=val;
      if(i==256){
        if(t==1)
          {close(tf);return 1;}
        t=1;
        i=0;
      }
      break;
    default:/* impossible */
      close(tf);return 0;
    }
  }
  close(tf);
  }
  for(tf=0;tf<256;++tf){
    trans_user2serv[tf]=tf;
    trans_serv2user[tf]=tf;
  }
  return 0;
}
/* ~trans */

void
notj_msg (struct session *sess)
{
   PrintText (sess, "No channel joined. Try /join #<channel>\n");
}

void
notc_msg (struct session *sess)
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

static int cmd_addbutton (struct session *sess, char *tbuf, char *word[], char *word_eol[]); 
static int cmd_away (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_ban (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_clear (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_close (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_ctcp (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_country (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_cycle (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_debug (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_delbutton (struct session *sess, char *tbuf, char *word[], char *word_eol[]); 
static int cmd_deop (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_devoice (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_discon (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_dns (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_echo (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_exec (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_execk (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_execs (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_execc (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
#ifndef __EMX__
static int cmd_execw (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
#endif
static int cmd_gate (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
int cmd_help (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_ignore (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_invite (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_join (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_kick (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_kickban (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_lastlog (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_load (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
int cmd_loaddll (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_mdeop (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_me (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_mkick (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_mkickb (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_msg (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_names (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_nctcp (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_newserver (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_nick (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_notice (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_notify (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_op (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_part (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_ping (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_query (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_quit (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_quote (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_reconnect (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
int cmd_rmdll (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_say (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
int cmd_scpinfo (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
int cmd_set (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_settab (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_servchan (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_server (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_topic (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_unban (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_unignore (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
int cmd_unloadall (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_wallchop (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_wallchan (struct session *sess, char *tbuf, char *word[], char *word_eol[]);
static int cmd_voice (struct session *sess, char *tbuf, char *word[], char *word_eol[]);

struct commands cmds[] =
{
   {"ADDBUTTON", cmd_addbutton, 0, 0, "/ADDBUTTON <name> <action>, adds a button under the user-list\n"},
   {"AWAY", cmd_away, 1, 0, "/AWAY [<reason>], sets you away\n"},
   {"BAN", cmd_ban, 1, 1, "/BAN <mask> [<bantype>], bans everyone matching the mask from the current channel. If they are already on the channel this doesn't kick them (needs chanop)\n"},
   {"CLEAR", cmd_clear, 0, 0, "/CLEAR, Clears the current text window\n"},
   {"CLOSE", cmd_close, 0, 0, "/CLOSE, Closes the current window/tab\n"},
   {"CTCP", cmd_ctcp, 1, 0, "/CTCP <nick> <message>, send the CTCP message to nick, common messages are VERSION and USERINFO\n"},
   {"COUNTRY", cmd_country,0,0, "/COUNTRY <code>, finds a country code, eg: au = australia\n"},
   {"CYCLE", cmd_cycle, 1, 1, "/CYCLE, parts current channel and immediately rejoins\n"},
   {"DEBUG", cmd_debug, 0, 0, 0},
   {"DELBUTTON", cmd_delbutton, 0, 0, "/DELBUTTON <name>, deletes a button from under the user-list\n"},
   {"DEOP", cmd_deop, 1, 1, "/DEOP <nick>, removes chanop status from the nick on the current channel (needs chanop)\n"},
   {"DEVOICE", cmd_devoice, 1, 1, "/DEVOICE <nick>, removes voice status from the nick on the current channel (needs chanop)\n"},
   {"DISCON", cmd_discon, 0, 0, "/DISCON, Disconnects from server\n"},
   {"DNS", cmd_dns, 0, 0, "/DNS <nick|host|ip>, Finds a users IP number\n"},
   {"ECHO", cmd_echo, 0, 0, "/ECHO <text>, Prints text locally\n"},
   {"EXEC", cmd_exec, 0, 0, "/EXEC [-o] <command>, runs the command. If -o flag is used then output is sent to current channel, else is printed to current text box\n"},
   {"EXECKILL", cmd_execk, 0, 0, "/EXECKILL [-9], kills a running exec in the current session. If -9 is given the process is SIGKILL'ed\n"},
   {"EXECSTOP", cmd_execs, 0, 0, "/EXECSTOP, sends the process SIGSTOP\n"},
   {"EXECCONT", cmd_execc, 0, 0, "/EXECCONT, sends the process SIGCONT\n"},
#ifndef __EMX__
   {"EXECWRITE", cmd_execw, 0, 0, "/EXECWRITE, sends data to the processes stdin\n"},
#endif
   {"GATE", cmd_gate, 0, 0, "/GATE <host> [<port>], proxies through a host, port defaults to 23\n"},
   {"HELP", cmd_help, 0, 0, 0},
   {"IGNORE", cmd_ignore, 0, 0,
    "/IGNORE <mask> <types..> <options..>\n"
    "    mask - host mask to ignore, eg: *!*@*.aol.com\n"
    "    types - types of data to ignore, one or all of:\n"
    "            PRIV, CHAN, NOTI, CTCP, INVI, ALL\n"
    "    options - NOSAVE, QUIET\n"},
   {"INVITE", cmd_invite, 1, 0, "/INVITE <nick> [<channel>], invites someone to a channel, by default the current channel (needs chanop)\n"},
   {"JOIN", cmd_join, 1, 0, "/JOIN <channel>, joins the channel\n"},
   {"KICK", cmd_kick, 1, 1, "/KICK <nick>, kicks the nick from the current channel (needs chanop)\n"},
   {"KICKBAN", cmd_kickban, 1, 1, "/KICKBAN <nick>, bans then kicks the nick from the current channel (needs chanop)\n"},
   {"LASTLOG", cmd_lastlog, 0, 0, "/LASTLOG <string>, searches for a string in the buffer.\n"},
#ifdef USE_PLUGIN
   {"LISTDLL", module_list, 0, 0, "/LISTDLL, Lists all currenly loaded plugins\n"},
#endif
   {"LOAD", cmd_load, 0, 0, "/LOAD <file>, loads a Perl script\n"},
#ifdef USE_PLUGIN
   {"LOADDLL", cmd_loaddll, 0, 0, "/LOADDLL <file>, loads a plugin\n"},
#endif
   {"MDEOP", cmd_mdeop, 1, 1, "/MDEOP, Mass deop's all chanops in the current channel (needs chanop)\n"},
   {"MKICK", cmd_mkick, 1, 1, "/MKICK, Mass kicks everyone except you in the current channel (needs chanop)\n"},
   {"MKICKB", cmd_mkickb, 1, 1, "/MKICKB, Sets a ban of *@* and mass kicks everyone except you in the current channel (needs chanop)\n"},
   {"ME", cmd_me, 1, 1, "/ME <action>, sends the action to the current channel (actions are written in the 3rd person, like /me jumps)\n"},
   {"MSG", cmd_msg, 0, 0, "/MSG <nick> <message>, sends a private message\n"},
   {"NAMES", cmd_names, 1, 0, "/NAMES, Lists the nicks on the current channel\n"},
   {"NCTCP", cmd_nctcp, 1, 0, "/NCTCP <nick> <message>, Sends a CTCP notice\n"},
   {"NEWSERVER", cmd_newserver,0,0, "/NEWSERVER <hostname> [<port>]\n"},
   {"NICK", cmd_nick, 0, 0, "/NICK <nickname>, sets your nick\n"},
   {"NOTICE", cmd_notice, 1, 0, "/NOTICE <nick/channel> <message>, sends a notice. Notices are a type of message that should be auto reacted to\n"},
   {"NOTIFY", cmd_notify, 0, 0, "/NOTIFY [<nick>], lists your notify list or adds someone to it\n"},
   {"OP", cmd_op, 1, 1, "/OP <nick>, gives chanop status to the nick (needs chanop)\n"},
   {"PART", cmd_part, 1, 1, "/PART [<channel>] [<reason>], leaves the channel, by default the current one\n"},
   {"PING", cmd_ping, 1, 0, "/PING <nick | channel>, CTCP pings nick or channel\n"},
   {"QUERY", cmd_query, 0, 0, "/QUERY <nick>, opens up a new privmsg window to someone\n"},
   {"QUIT", cmd_quit, 0, 0, "/QUIT [<reason>], disconnects from the current server\n"},
   {"QUOTE", cmd_quote, 1, 0, "/QUOTE <text>, sends the text in raw form to the server\n"},
   {"RECONNECT", cmd_reconnect, 0, 0, "/RECONENCT, reconnects to the current server\n"},
#ifdef USE_PLUGIN
   {"RMDLL", cmd_rmdll, 0, 0, "/RMDLL <dll name>, unloads a plugin\n"},
#endif
   {"SAY", cmd_say, 0, 0, "/SAY <text>, sends the text to the object in the current window\n"},
#ifdef USE_PERL
   {"SCPINFO", cmd_scpinfo, 0, 0, "/SCPINFO, Lists some information about current Perl bindings\n"},
#endif
   {"SET", cmd_set, 0, 0, "/SET <variable> [<value>]\n"},
   {"SETTAB", cmd_settab, 0, 0, ""},
   {"SERVCHAN", cmd_servchan, 0, 0, "/SERVER <host> <host> <channel>, connects and joins a channel\n"},
   {"SERVER", cmd_server, 0, 0, "/SERVER <host> [<port>] [<password>], connects to a server, the default port is 6667\n"},
   {"TOPIC", cmd_topic, 1, 1, "/TOPIC [<topic>], sets the topic is one is given, else shows the current topic\n"},
   {"UNBAN", cmd_unban, 1, 1, "/UNBAN <mask>, removes a current ban on a mask\n"},
   {"UNIGNORE", cmd_unignore, 0, 0, "/UNIGNORE <mask> [QUIET]\n"},
#ifdef USE_PERL
   {"UNLOADALL", cmd_unloadall, 0, 0, "/UNLOADALL, Unloads all perl scripts\n"},
#endif
   {"WALLCHOP", cmd_wallchop, 1, 1, "/WALLCHOP <message>, sends the message to all chanops on the current channel\n"},
   {"WALLCHAN", cmd_wallchan, 1, 1, "/WALLCHAN <message>, writes the message to all channels\n"},
   {"VOICE", cmd_voice, 1, 1, "/VOICE <nick>, gives voice status to someone (needs chanop)\n"},
   {0, 0, 0, 0, 0}
};

static void
help (struct session *sess, char *helpcmd, int quiet)
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

static void
send_channel_modes (struct session *sess, char *tbuf,
                    char *word[],
                    int start, int end,
                 char sign, char mode)
{
   int left;
   int i = start;

   while (1)
   {
      left = end - i;
      switch (left)
      {
      case 0:
         return;
      case 1:
         sprintf (tbuf, "MODE %s %c%c %s\r\n", sess->channel, sign, mode, word[i]);
         break;
      case 2:
         sprintf (tbuf, "MODE %s %c%c%c %s %s\r\n", sess->channel, sign, mode, mode, word[i], word[i + 1]);
         i++;
         break;
      default:
         sprintf (tbuf, "MODE %s %c%c%c%c %s %s %s\r\n", sess->channel, sign, mode, mode, mode, word[i], word[i + 1], word[i + 2]);
         i += 2;
         break;
      }
      tcp_send (sess->server, tbuf);
      if (left < 3)
         return;
      i++;
   }
}

#define find_word_to_end(a, b) word_eol[b]
#define find_word(a, b) word[b]

int
cmd_addbutton (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (*word[2] && *word_eol[3])
   {
      list_addentry (&button_list, word_eol[3], word[2]);
      /* fe_buttons_update (sess); removed from maingui.c - this whole structure should go */
      return TRUE;
   }
   return FALSE;
} 

int
cmd_away (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *reason = find_word_to_end (cmd, 2);

   if (*reason)
   {
      sprintf (tbuf, "AWAY :%s\r\n", reason);
      tcp_send (sess->server, tbuf);
   } else
   {
      tcp_send_len (sess->server, "AWAY\r\n", 6);
   }
   if (prefs.show_away_message)
   {
      char *away_me;
      GSList *list = sess_list;

      if (*reason)
      {
         /* /me is away: + reason + \0 */
         away_me = malloc (13 + strlen (reason) + 1);
         sprintf (away_me, "/me is away: %s", reason);
      } else
      {
         away_me = malloc (13);
         strcpy (away_me, "/me is back");
      }

      while (list)
      {
         /* am I the right server and not a dialog box */
         if (((struct session *) list->data)->server == sess->server
             && !((struct session *) list->data)->is_dialog
             && !((struct session *) list->data)->is_server)
         {
            handle_command (away_me, (struct session *) list->data,
                        FALSE, FALSE);
         }
         list = list->next;
      }

      free (away_me);
   }
   return TRUE;
}

static void
ban (session *sess, char *tbuf, char *mask, char *bantypestr)
{
   int bantype;
   struct user *user;

   user = find_name (sess, mask);
   if (user && user->hostname)  /* it's a nickname, let's find a proper ban mask */
   {
      char *at, *dot, *mask;
      char username[64], fullhost[128],
           domain[128];

      mask = user->hostname;

      at = strchr (mask, '@');
      if (!at)
         return;        /* can't happen? */
      *at = 0;

      strcpy (username, mask);
      *at = '@';
      strcpy (fullhost, at + 1);

      dot = strchr (mask, '.');
      if (dot)
         strcpy (domain, dot);
      else
         strcpy (domain, fullhost);

      if (*bantypestr)
         bantype = atoi (bantypestr);
      else
         bantype = prefs.bantype;

      if (inet_addr (fullhost) != -1)  /* "fullhost" is really a IP number */
      {
         char *lastdot = strrchr (fullhost, '.');
         if (!lastdot)
            return;     /* can't happen? */

         *lastdot = 0;
         strcpy (domain, fullhost);
         *lastdot = '.';

         switch (bantype)
         {
         case 0:
            sprintf (tbuf, "MODE %s +b *!*@%s.*\r\n", sess->channel, domain);
            break;

         case 1:
            sprintf (tbuf, "MODE %s +b *!*@%s\r\n", sess->channel, fullhost);
            break;

         case 2:
            sprintf (tbuf, "MODE %s +b *!%s@%s.*\r\n", sess->channel, username, domain);
            break;

         case 3:
            sprintf (tbuf, "MODE %s +b *!%s@%s\r\n", sess->channel, username, fullhost);
            break;
         }
      } else
      {
         switch (bantype)
         {
         case 0:
            sprintf (tbuf, "MODE %s +b *!*@*%s\r\n", sess->channel, domain);
            break;

         case 1:
            sprintf (tbuf, "MODE %s +b *!*@%s\r\n", sess->channel, fullhost);
            break;

         case 2:
            sprintf (tbuf, "MODE %s +b *!%s@*%s\r\n", sess->channel, username, domain);
            break;

         case 3:
            sprintf (tbuf, "MODE %s +b *!%s@%s\r\n", sess->channel, username, fullhost);
            break;
         }
      }

   } else
   {
      sprintf (tbuf, "MODE %s +b %s\r\n", sess->channel, mask);
   }
   tcp_send (sess->server, tbuf);
}

int
cmd_ban (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *mask = find_word (pdibuf, 2);

   if (*mask)
   {
      ban (sess, tbuf, mask, word[3]);
   } else
   {
      sprintf (tbuf, "MODE %s +b\r\n", sess->channel);  /* banlist */
      tcp_send (sess->server, tbuf);
   }

   return TRUE;
}

int
cmd_clear (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   GSList *list = sess_list;
   char *reason = find_word_to_end (cmd, 2);

   if (strncasecmp(reason, "all", 3) == 0)
   {
      while (list)
      {
         if (!((session *)list->data)->is_shell)
            fe_text_clear (list->data);
         list = list->next;
      }
   } else
   {
      fe_text_clear (sess);
   }

   return TRUE;
}



int
cmd_close (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (*word_eol[2])
      sess->quitreason = word_eol[2];
   else
      sess->quitreason = prefs.quitreason;
   fe_close_window (sess);

   return TRUE;
}

int
cmd_ctcp (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *to = find_word (pdibuf, 2);
   if (*to)
   {
      char *msg = find_word_to_end (cmd, 3);
      if (*msg)
      {
         char *cmd = msg;

         while (1)
         {
            if (*cmd == ' ' || *cmd == 0)
               break;
            *cmd = toupper (*cmd);
            cmd++;
         }

         sprintf (tbuf, "PRIVMSG %s :\001%s\001\r\n", to, msg);
         tcp_send (sess->server, tbuf);

         EMIT_SIGNAL (XP_TE_CTCPSEND, sess, to, msg, NULL, NULL, 0);

         return TRUE;
      }
   }
   return FALSE;
}

int
cmd_country (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *code = word[2];
   if (*code)
   {
      sprintf (tbuf, "%s = %s\n", code, country (code));
      PrintText (sess, tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_cycle (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *key = sess->channelkey;
   char *chan = sess->channel;
   if (*chan && !sess->is_dialog && !sess->is_server)
   {
      sprintf (tbuf, "PART %s\r\nJOIN %s %s\r\n", chan, chan, key);
      tcp_send (sess->server, tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_debug (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   struct session *s;
   struct server *v;
   GSList *list = sess_list;

   PrintText (sess, "Session   Channel    WaitChan  WillChan  Server\n");
   while (list)
   {
      s = (struct session *) list->data;
      sprintf (tbuf, "0x%lx %-10.10s %-10.10s %-10.10s 0x%lx\n",
               (unsigned long) s, s->channel, s->waitchannel, s->willjoinchannel,
           (unsigned long) s->server);
      PrintText (sess, tbuf);
      list = list->next;
   }

   list = serv_list;
   PrintText (sess, "Server    Sock  Name\n");
   while (list)
   {
      v = (struct server *) list->data;
      sprintf (tbuf, "0x%lx %-5ld %s\n",
               (unsigned long) v, (unsigned long) v->sok, v->servername);
      PrintText (sess, tbuf);
      list = list->next;
   }

   sprintf (tbuf,
            "\nfront_session: %lx\n"
            "current_tab: %lx\n\n",
            (unsigned long) sess->server->front_session,
         (unsigned long) current_tab);
   PrintText (sess, tbuf);
#ifdef MEMORY_DEBUG
   sprintf (tbuf, "current mem: %d\n\n", current_mem_usage);
   PrintText (sess, tbuf);
#endif /* MEMORY_DEBUG */

   return TRUE;
}

int
cmd_delbutton (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (*word[2])
   {
      if (list_delentry (&button_list, word[2]))
         /* fe_buttons_update (sess); remove this whole structure please*/
      return TRUE;
   }
   return FALSE;
}

int
cmd_deop (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int i = 2;

   while (1)
   {
      if (!*word[i])
      {
         if (i == 2)
            return FALSE;
         send_channel_modes (sess, tbuf, word, 2, i, '-', 'o');
         return TRUE;
      }
      i++;
   }
}

int
cmd_mdeop (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   struct user *user;
   char **nicks = malloc (sizeof (char *) * sess->ops);
   int i = 0;
   GSList *list = sess->userlist;

   while (list)
   {
      user = (struct user *) list -> data;
      if (user->op && (strcmp (user->nick, sess->server->nick) != 0))
      {
         nicks[i] = user->nick;
         i++;
      }
      list = list -> next;
   }

   send_channel_modes (sess, tbuf, nicks, 0, i, '-', 'o');

   free (nicks);

   return TRUE;
}

int
cmd_mkick (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   struct user *user;
   char *reason = find_word_to_end (cmd, 2);
   GSList *list;

   list = sess->userlist;
   while (list)
   {
      user = (struct user *)list -> data;
      if (user->op)
      {
         if (strcmp (user->nick, sess->server->nick) != 0)
         {
            if (*reason)
               sprintf (tbuf, "KICK %s %s :%s\r\n", sess->channel, user->nick, reason);
            else
               sprintf (tbuf, "KICK %s %s\r\n", sess->channel, user->nick);
            tcp_send (sess->server, tbuf);
         }
      }
      list = list -> next;
   }

   list = sess->userlist;
   while (list)
   {
      user = (struct user *) list -> data;
      if (!user->op)
      {
         if (strcmp (user->nick, sess->server->nick) != 0)
         {
            if (*reason)
               sprintf (tbuf, "KICK %s %s :%s\r\n", sess->channel, user->nick, reason);
            else
               sprintf (tbuf, "KICK %s %s\r\n", sess->channel, user->nick);
            tcp_send (sess->server, tbuf);
         }
      }
      list = list -> next;
   }

   return TRUE;
}

int
cmd_mkickb (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   sprintf (tbuf, "MODE %s +b *@*\r\n", sess->channel);
   tcp_send (sess->server, tbuf);

   return cmd_mkick (sess, tbuf, word, word_eol);
}

int
cmd_devoice (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int i = 2;

   while (1)
   {
      if (!*word[i])
      {
         if (i == 2)
            return FALSE;
         send_channel_modes (sess, tbuf, word, 2, i, '-', 'v');
         return TRUE;
      }
      i++;
   }
}

int
cmd_discon (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   disconnect_server (sess, TRUE, -1);
   return TRUE;
}

int
cmd_dns (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *nick = word[2];
   struct user *user;

   if (*nick)
   {
      if (strchr (nick, '.') == NULL)
      {
         user = find_name (sess, nick);
         if (user && user->hostname)
         {
            do_dns (sess, tbuf, user->nick, user->hostname);
         } else
         {
            sprintf (tbuf, "WHO %s\r\n", nick);
            tcp_send (sess->server, tbuf);
            sess->server->doing_who = TRUE;
         }
      } else
      {
         sprintf (tbuf, "/exec %s %s", prefs.dnsprogram, nick);
         handle_command (tbuf, sess, 0, 0);
      }
      return TRUE;
   }
   return FALSE;
}

int
cmd_echo (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   PrintText (sess, word_eol[2]);
   return TRUE;
}

static void
exec_check_process (struct session *sess)
{
   int val;

   if (sess->running_exec == NULL)
      return;
   val = waitpid (sess->running_exec->childpid, NULL, WNOHANG);
   if (val == -1 || val > 0)
   {
      close (sess->running_exec->myfd);
      close (sess->running_exec->childfd);
      fe_input_remove (sess->running_exec->iotag);
      free (sess->running_exec);
      sess->running_exec = NULL;
   }
}

int
cmd_execs (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int r;

   exec_check_process (sess);
   if (sess->running_exec == NULL)
   {
      EMIT_SIGNAL (XP_TE_NOCHILD, sess, NULL, NULL, NULL, NULL, 0);
      return FALSE;
   }
   r = kill (sess->running_exec->childpid, SIGSTOP);
   if (r == -1)
      PrintText (sess, "Error in kill(2)\n");

   return TRUE;
}

int
cmd_execc (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int r;

   exec_check_process (sess);
   if (sess->running_exec == NULL)
   {
      EMIT_SIGNAL (XP_TE_NOCHILD, sess, NULL, NULL, NULL, NULL, 0);
      return FALSE;
   }
   r = kill (sess->running_exec->childpid, SIGCONT);
   if (r == -1)
      PrintText (sess, "Error in kill(2)\n");

   return TRUE;
}

int
cmd_execk (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int r;

   exec_check_process (sess);
   if (sess->running_exec == NULL)
   {
      EMIT_SIGNAL (XP_TE_NOCHILD, sess, NULL, NULL, NULL, NULL, 0);
      return FALSE;
   }
   if (strcmp (word[2], "-9") == 0)
      r = kill (sess->running_exec->childpid, SIGKILL);
   else
      r = kill (sess->running_exec->childpid, SIGTERM);
   if (r == -1)
      PrintText (sess, "Error in kill(2)\n");

   return TRUE;
}

/* OS/2 Can't have the /EXECW command because it uses pipe(2) not socketpair
   and thus it is simplex --AGL */
#ifndef __EMX__
int
cmd_execw (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   exec_check_process (sess);
   if (sess->running_exec == NULL)
   {
      EMIT_SIGNAL (XP_TE_NOCHILD, sess, NULL, NULL, NULL, NULL, 0);
      return FALSE;
   }
   /* Write it to both sockets to have a local echo */
   write (sess->running_exec->myfd, word_eol[2], strlen (word_eol[2]));
   write (sess->running_exec->childfd, word_eol[2], strlen (word_eol[2]));
   write (sess->running_exec->myfd, "\n", 1);
   write (sess->running_exec->childfd, "\n", 1);

   return TRUE;
}
#endif /* !__EMX__ */

int color_data[] = {
  1,
  4,
  3,
  5,
  2,
  6,
  10,
  0,
};

int color_data_bold[] = {
  1,
  13,
  9,
  8,
  12,
  13,
  11,
  0,
};

static void
insert_color (char *buf, int n)
{
  static int bold = FALSE;

  if (n == 1)
    {
      bold = TRUE;
      *buf = 0;
      return;
    }

  if (n > 39)
    {
      if (bold)
	sprintf (buf, "\003,%02d", color_data_bold[(n - 40) % 8]);
      else
	sprintf (buf, "\003,%02d", color_data[(n - 40) % 8]);
      return;
    }

  if (n > 29)
    {
      if (bold)
	sprintf (buf, "\003%02d", color_data_bold[(n - 30) % 8]);
      else
	sprintf (buf, "\003%02d", color_data[(n - 30) % 8]);
      return;
    }

  sprintf (buf, "\003%02d", n);
}

static int esc = 0;

static void
exec_handle_colors (char *buf)
{
  int i = 0, j = 0, n = 0, len;
  char *newbuf;
  char num[16];

  len = strlen (buf);
  newbuf = malloc (len + 1);

  while (i < len)
    {
      if (esc && isdigit (buf[i]))
	{
	  num[n] = buf[i];
	  n++;
	  if (n > 14)
	    n = 14;
	}
      else
	{
	  switch (buf[i])
	    {
	    case '\r':
	      break;
	    case 27:
	      esc = TRUE;
	      break;
	    case '[':
	      if (!esc)
		{
		  newbuf[j] = buf[i];
		  j++;
		}
	      break;
	    case ';':
	      if (esc)
		{
		  num[n] = 0;
		  n = atoi (num);
		  insert_color (&newbuf[j], n);
		  j = strlen (newbuf);
		  n = 0;
		}
	      else
		{
		  newbuf[j] = buf[i];
		  j++;
		}
	      break;
	    case 'm':
	      if (esc)
		{
		  num[n] = 0;
		  n = atoi (num);
		  insert_color (&newbuf[j], n);
		  j = strlen (newbuf);
		  n = 0;
		  esc = FALSE;
		}
	      else
		{
		  newbuf[j] = buf[i];
		  j++;
		}
	      break;
	    default:
	      newbuf[j] = buf[i];
	      j++;
	    }
	}
      i++;
    }
  newbuf[j] = 0;
  memcpy (buf, newbuf, j + 1);
  free (newbuf);
}

static void
exec_data (struct nbexec *s, gint sok)
{
   char buf[2050];
   int rd;

   rd = read (sok, buf, 2048);
   buf[rd] = 0;
   if (rd < 1 || !strcmp (buf, "child death\n")) /* WHAT A HACK!! */
   {
      /* The process has died */
      waitpid (s->childpid, NULL, 0);
      s->sess->running_exec = NULL;
      fe_input_remove (s->iotag);
      close (sok);
      free (s);
      return;
   }
   buf[rd] = 0;

   exec_handle_colors (buf);
   if (s->tochannel)
      handle_multiline (s->sess, buf, FALSE, TRUE);
   else
      PrintText (s->sess, buf);
   return;
}

int
cmd_exec (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int tochannel = FALSE;
   char *cmd = word_eol[2];
   int fds[2], pid = 0;
   struct nbexec *s;

   if (*cmd)
   {
      if (access ("/bin/sh", X_OK) != 0)
      {
         fe_message ("I need /bin/sh to run!\n", FALSE);
         return TRUE;
      }
      exec_check_process (sess);
      if (sess->running_exec != NULL)
      {
         EMIT_SIGNAL (XP_TE_ALREADYPROCESS, sess, NULL, NULL, NULL, NULL, 0);
         return TRUE;
      }
      if (!strcmp (word[2], "-o"))
      {
         if (!*word[3])
            return FALSE;
         else
         {
            cmd = word_eol[3];
            tochannel = TRUE;
         }
      }
#ifdef __EMX__                  /* if os/2 */
      if (pipe (fds) < 0)
      {
         printf ("Pipe create error");
         return FALSE;
      }
      setmode (fds[0], O_BINARY);
      setmode (fds[1], O_BINARY);
#else
      if (socketpair (PF_UNIX, SOCK_STREAM, 0, fds) == -1)
      {
         PrintText (sess, "socketpair(2) failed\n");
         perror ("socketpair");
         return FALSE;
      }
#endif
      s = (struct nbexec *) malloc (sizeof (struct nbexec));
      s->myfd = fds[0];
      s->childfd = fds[1];
      s->tochannel = tochannel;
      s->sess = sess;

      pid = fork ();
      if (pid == 0)
      {
         /* This is the child's context */
         close (0);
         close (1);
         close (2);
         /* Copy the child end of the pipe to stdout and stderr */
         dup2 (s->childfd, 1);
         dup2 (s->childfd, 2);
         /* Also copy it to stdin so we can write to it */
         dup2 (s->childfd, 0);
         /* Now we call /bin/sh to run our cmd ; made it more friendly -DC1 */
         /*execl ("/bin/sh", "sh", "-c", cmd, 0);*/
         system (cmd);
         /* cmd is in control of this child... if it dies child dies */
         fflush (stdin);
         write (s->childfd, "child death\n", 12);
         _exit (0);
      }
      if (pid == -1)
      {
         /* Parent context, fork() failed */

         PrintText (sess, "Error in fork(2)\n");
      } else
      {
         /* Parent path */
         s->childpid = pid;
         s->iotag = fe_input_add (s->myfd, 1, 0, 1, exec_data, s);
         sess->running_exec = s;
         return TRUE;
      }
   }
   return FALSE;
}

int
cmd_quit (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (*word_eol[2])
      sess->quitreason = word_eol[2];
   else
      sess->quitreason = prefs.quitreason;
   disconnect_server (sess, TRUE, -1);
   return 2;
}

int
cmd_gate (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *server = find_word (pdibuf, 2);
   if (*server)
   {
      char *port = find_word (pdibuf, 3);
      if (*port)
         connect_server (sess, server, atoi (port), TRUE);
      else
         connect_server (sess, server, 23, TRUE);
      return TRUE;
   }
   return FALSE;
}

int
cmd_help (struct session *sess, char *tbuf, char *word[], char *word_eol[])
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
      struct popup *pop;
      GSList *list = command_list;
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
      strcat (buf, "User defined commands:\n\n  ");
      t = 1;
      while (list)
      {
         pop = (struct popup *) list->data;
         strcat (buf, pop->name);
         t++;
         if (t == 6)
         {
            t = 1;
            strcat (buf, "\n  ");
         } else
            for (j = 0; j < (10 - strlen (pop->name)); j++)
               strcat (buf, " ");
         list = list->next;
      }
      strcat (buf, "\n");
      PrintText (sess, buf);
      free (buf);
   }
   return TRUE;
}

int
cmd_ignore (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int i;
   int priv = 0;
   int noti = 0;
   int chan = 0;
   int ctcp = 0;
   int invi = 0;
   int unignore = 0;
   int quiet = 0;
   int no_save = 0;

   if (!*word[2])
   {
      ignore_showlist (sess);
      return TRUE;
   }
   if (!*word[3])
      return FALSE;

   i = 3;
   while (1)
   {
      if (!*word[i])
      {
         if (!priv && !noti && !chan && !ctcp && !invi && !unignore)
            return FALSE;
         i = ignore_add (word[2], priv, noti, chan, ctcp, invi, unignore, no_save);
         if (!quiet)
         {
            if (i == 1)
               EMIT_SIGNAL (XP_TE_IGNOREADD, sess, word[2], NULL, NULL, NULL, 0);
            if (i == 2)         /* old ignore changed */
               EMIT_SIGNAL (XP_TE_IGNORECHANGE, sess, word[2], NULL, NULL, NULL, 0);
         }
         return TRUE;
      }
      if (!strcasecmp (word[i], "UNIGNORE"))
         unignore = 1;
      else if (!strcasecmp (word[i], "ALL"))
         priv = noti = chan = ctcp = invi = 1;
      else if (!strcasecmp (word[i], "PRIV"))
         priv = 1;
      else if (!strcasecmp (word[i], "NOTI"))
         noti = 1;
      else if (!strcasecmp (word[i], "CHAN"))
         chan = 1;
      else if (!strcasecmp (word[i], "CTCP"))
         ctcp = 1;
      else if (!strcasecmp (word[i], "INVI"))
         invi = 1;
      else if (!strcasecmp (word[i], "QUIET"))
         quiet = 1;
      else if (!strcasecmp (word[i], "NOSAVE"))
         no_save = 1;
      else
      {
         sprintf (tbuf, "Unknown arg '%s' ignored.", word[i]);
         PrintText (sess, tbuf);
      }
      i++;
   }
}

int
cmd_invite (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (!*word[2])
      return FALSE;
   if (*word[3])
      sprintf (tbuf, "INVITE %s %s\r\n", word[2], word[3]);
   else
      sprintf (tbuf, "INVITE %s %s\r\n", word[2], sess->channel);
   tcp_send (sess->server, tbuf);
   return TRUE;
}

int
cmd_join (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *chan = find_word (pdibuf, 2);
   if (*chan)
   {
      char *po, *pass = find_word (pdibuf, 3);
      if (*pass)
         sprintf (tbuf, "JOIN %s %s\r\n", chan, pass);
      else
         sprintf (tbuf, "JOIN %s\r\n", chan);
      tcp_send (sess->server, tbuf);
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
cmd_kick (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *nick = find_word (pdibuf, 2);
   char *reason = find_word_to_end (cmd, 3);
   if (*nick)
   {
      if (*reason)
         sprintf (tbuf, "KICK %s %s :%s\r\n", sess->channel, nick, reason);
      else
         sprintf (tbuf, "KICK %s %s\r\n", sess->channel, nick);
      tcp_send (sess->server, tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_kickban (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *nick = find_word (pdibuf, 2);
   char *reason = find_word_to_end (cmd, 3);

   if (*nick)
   {
      /* this will use prefs.bantype, no args allowed in /kickban */
      ban (sess, tbuf, nick, "");

      if (*reason)
         sprintf (tbuf, "KICK %s %s :%s\r\n", sess->channel, nick, reason);
      else
         sprintf (tbuf, "KICK %s %s\r\n", sess->channel, nick);
      tcp_send (sess->server, tbuf);

      return TRUE;
   }
   return FALSE;
}

static void
lastlog (session *sess, char *tbuf, char *search)
{
   session *lastlog_sess;
   char *buf, *end, *start;

   if (!is_session (sess))
      return;

   lastlog_sess = find_dialog (sess->server, "(lastlog)");
   if (!lastlog_sess)
   {
      lastlog_sess = new_session (sess->server, "(lastlog)");
      lastlog_sess->lastlog_sess = sess;
   }

   fe_text_clear (lastlog_sess);

   buf = start = fe_buffer_get (sess);
   if (buf)
   {
      while ((end = strchr (start, '\n')))
      {
         strncpy (tbuf, start, end - start);
   	   tbuf[end - start] = 0;
   	   if (nocasestrstr (tbuf, search))
   	   {
            strcat (tbuf, "\n");
            PrintText (lastlog_sess, tbuf);
         }
         start = ++end;
      }
      free (buf);
   } else
   {
      PrintText (lastlog_sess, "Search buffer is empty.\n");
   }
}

int
cmd_lastlog (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (*word_eol[2])
   {
      lastlog (sess, tbuf, word_eol[2]);
      return TRUE;
   }

   return FALSE;
}

int
cmd_load (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
#ifdef USE_PERL
   char *file;
   int i;

   file = expand_homedir (word[2]);
   i = perl_load_file (file);
   free (file);
   switch (i)
   {
   case 0:
      return TRUE;
   case 1:
      PrintText (sess, "Error compiling script\n");
      return FALSE;
   case 2:
      PrintText (sess, "Error Loading file\n");
      return FALSE;
   }
   return FALSE;
#else
   PrintText (sess, "Perl scripting not available in this compilation.\n");
   return TRUE;
#endif
}

#ifdef USE_PLUGIN
int
cmd_loaddll (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *file;
   int i;

   file = expand_homedir (word[2]);

   i = module_load (file, sess);

   free (file);

   if (i == 0)
      return TRUE;
   return FALSE;
}
#endif

int
cmd_me (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *act = find_word_to_end (cmd, 2);
   if (*act)
   {
      channel_action (sess, tbuf, sess->channel, sess->server->nick, act, TRUE);
      sprintf (tbuf, "\001ACTION %s\001\r", act);
      sprintf (tbuf, "PRIVMSG %s :\001ACTION %s\001\r\n", sess->channel, act);
      tcp_send (sess->server, tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_msg (struct session *sess, char *tbuf, char *word[], char *word_eol[])
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
            EMIT_SIGNAL (XP_TE_NODCC, sess, NULL, NULL, NULL, NULL, 0);
            return TRUE;
         } else
         {
            if (!sess->server->connected)
            {
               notc_msg (sess);
               return TRUE;
            }
            sprintf (tbuf, "PRIVMSG %s :%s\r\n", nick, msg);
            tcp_send (sess->server, tbuf);
         }
         EMIT_SIGNAL (XP_TE_MSGSEND, sess, nick, msg, NULL, NULL, 0);

         return TRUE;
      }
   }
   return FALSE;
}

int
cmd_names (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (*word[2])
      sprintf (tbuf, "NAMES %s\r\n", word[2]);
   else
      sprintf (tbuf, "NAMES %s\r\n", sess->channel);
   tcp_send (sess->server, tbuf);
   return TRUE;
}

int
cmd_nctcp (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (*word[2] && *word_eol[3])
   {
      sprintf (tbuf, "NOTICE %s :\001%s\001\r\n", word[2], word_eol[3]);
      tcp_send (sess->server, tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_newserver (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   struct server *serv;

   serv = new_server ();
   if (serv)
   {
      if (prefs.use_server_tab)
         sess = serv->front_session;
      else
         sess = new_session (serv, 0);
      if (sess)
         cmd_server (sess, tbuf, word, word_eol);
   }

   return TRUE;
}

int
cmd_nick (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *nick = find_word (pdibuf, 2);
   if (*nick)
   {
      if (sess->server->connected)
      {
         sprintf (tbuf, "NICK %s\r\n", nick);
         tcp_send (sess->server, tbuf);
      } else
         user_new_nick (sess->server, tbuf, sess->server->nick, nick, TRUE);
      return TRUE;
   }
   return FALSE;
}

int
cmd_notice (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (*word[2] && *word_eol[3])
   {
      sprintf (tbuf, "NOTICE %s :%s\r\n", word[2], word_eol[3]);
      tcp_send (sess->server, tbuf);
      EMIT_SIGNAL (XP_TE_NOTICESEND, sess, word[2], word_eol[3], NULL, NULL, 0);
      return TRUE;
   }
   return FALSE;
}

int
cmd_notify (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int i = 1;

   if (*word[2])
   {
      while (1)
      {
         i++;
         if (!*word[i])
            break;
         if (notify_deluser (word[i]))
         {
            EMIT_SIGNAL (XP_TE_DELNOTIFY, sess, word[i], NULL, NULL, NULL, 0);
            return TRUE;
         }
         notify_adduser (word[i]);
         EMIT_SIGNAL (XP_TE_ADDNOTIFY, sess, word[i], NULL, NULL, NULL, 0);
      }
   } else
      notify_showlist (sess);
   return TRUE;
}

int
cmd_op (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int i = 2;

   while (1)
   {
      if (!*word[i])
      {
         if (i == 2)
            return FALSE;
         send_channel_modes (sess, tbuf, word, 2, i, '+', 'o');
         return TRUE;
      }
      i++;
   }
}

int
cmd_part (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *chan = find_word (pdibuf, 2);
   char *reason = word_eol[3];
   if (!*chan)
      chan = sess->channel;
   if ((*chan) && is_channel (chan))
   {
      sprintf (tbuf, "PART %s :%s\r\n", chan, reason);
      tcp_send (sess->server, tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_ping (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   unsigned long tim;
   struct timeval timev;
   char *to = word[2];

   gettimeofday (&timev, 0);
   tim = (timev.tv_sec - 50000) * 1000000 + timev.tv_usec;

   if (*to)
      sprintf (tbuf, "PRIVMSG %s :\001PING %lu\001\r\n", to, tim);
   else
      sprintf (tbuf, "PING %lu :%s\r\n", tim, sess->server->servername);
   tcp_send (sess->server, tbuf);
   return TRUE;
}

int
cmd_query (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   /* char *nick = find_word (pdibuf, 2);
   if (*nick)
   {
      if (!find_dialog (sess->server, nick))
         new_session (sess->server, nick);
      return TRUE;
   } */
   return FALSE;
}

int
cmd_quote (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int len;
   char *raw = find_word_to_end (cmd, 2);
   if (*raw)
   {
      len = strlen (raw);
      if (len < 4094)
      {
         tcp_send_len (sess->server, tbuf, sprintf (tbuf, "%s\r\n", raw));
      } else
      {
         tcp_send_len (sess->server, raw, len);
         tcp_send_len (sess->server, "\r\n", 2);
      }
      return TRUE;
   }
   return FALSE;
}

int
cmd_reconnect (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int tmp = prefs.recon_delay;
   GSList *list;
   server *serv;

   prefs.recon_delay = 0;

   if (!strcasecmp (word[2], "ALL"))
   {
      list = serv_list;
      while (list)
      {
         serv = list->data;
         if (serv->connected)
            auto_reconnect (serv, TRUE, -1);
         list = list->next;
      }
   } else
   {
      auto_reconnect (sess->server, TRUE, -1);
   }

   prefs.recon_delay = tmp;

   return TRUE;
}

#ifdef USE_PLUGIN
int
cmd_rmdll (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *dllname = find_word (pdi_buf, 2);

   if (module_unload (dllname, sess) == 0)
      return TRUE;
   return FALSE;
}
#endif

int
cmd_say (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *speech = find_word_to_end (cmd, 2);
   if (*speech)
   {
      channel_msg (sess->server, tbuf, sess->channel, sess->server->nick, speech, TRUE);
      sprintf (tbuf, "PRIVMSG %s :%s\r\n", sess->channel, speech);
      tcp_send (sess->server, tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_settab (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   if (*word_eol[2])
   {
      strcpy (tbuf, sess->channel);
      strncpy (sess->channel, word_eol[2], 200);
      fe_set_channel (sess);
      strcpy (sess->channel, tbuf);
   }

   return TRUE;
}

int
cmd_server (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *server = find_word (pdibuf, 2);
   char *port = find_word (pdibuf, 3);
   char *pass = find_word (pdibuf, 4);
   char *co;

   if (*server)
   {
      if (strncasecmp ("irc://", server, 6) == 0)
         server += 6;
      co = strchr (server, ':');
      if (co)
      {
         port = co + 1;
         *co = 0;
         pass = find_word (pdibuf, 3);
      }
      sess->willjoinchannel[0] = 0;
      if (pass)
         strcpy (sess->server->password, pass);
      if (*port)
         connect_server (sess, server, atoi (port), FALSE);
      else
         connect_server (sess, server, 6667, FALSE);
      return TRUE;
   }
   return FALSE;
}

int
cmd_servchan (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *server = find_word (pdibuf, 2);
   char *port = find_word (pdibuf, 3);
   char *chan = find_word (pdibuf, 4);

   if (*server)
   {
      if (*port)
      {
         strcpy (sess->willjoinchannel, chan);
         connect_server (sess, server, atoi (port), FALSE);
         return TRUE;
      }
   }
   return FALSE;
}

int
cmd_topic (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *topic = find_word_to_end (cmd, 2);
   if (*topic)
      sprintf (tbuf, "TOPIC %s :%s\r\n", sess->channel, topic);
   else
      sprintf (tbuf, "TOPIC %s\r\n", sess->channel);
   tcp_send (sess->server, tbuf);
   return TRUE;
}

int
cmd_unban (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *mask = find_word (pdibuf, 2);
   if (*mask)
   {
      sprintf (tbuf, "MODE %s -b %s\r\n", sess->channel, mask);
      tcp_send (sess->server, tbuf);
      return TRUE;
   }
   return FALSE;
}

int
cmd_unignore (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   char *mask = find_word (pdibuf, 2);
   char *arg = find_word (pdibuf, 3);
   if (*mask)
   {
      if (ignore_del (mask, NULL))
      {
         if (strcasecmp (arg, "QUIET"))
            EMIT_SIGNAL (XP_TE_IGNOREREMOVE, sess, mask, NULL, NULL, NULL, 0);
      }
      return TRUE;
   }
   return FALSE;
}

int
cmd_wallchop (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int i = 0;
   struct user *user;
   GSList *list;

   if (*word_eol[2])
   {
      strcpy (tbuf, "NOTICE ");
      list = sess->userlist;
      while (list)
      {
         user = (struct user *)list -> data;
         if (user->op)
         {
            if (i)
               strcat (tbuf, ",");
            strcat (tbuf, user->nick);
            i++;
         }
         if ((i == 5 || !list->next) && i)
         {
            i = 0;
            sprintf (tbuf + strlen(tbuf), 
                     " :[@%s] %s\r\n", sess->channel, word_eol[2]);
            tcp_send (sess->server, tbuf);
            strcpy (tbuf, "NOTICE ");
         }
         list = list->next;
      }
      return TRUE;
   }
   return FALSE;
}

int
cmd_wallchan (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   struct session *s;
   GSList *list = sess_list;

   if (*word_eol[2])
   {
      while (list)
      {
         s = (struct session *) list->data;
         if (is_channel (s->channel))
         {
            channel_msg (s->server, tbuf, s->channel, s->server->nick, word_eol[2], TRUE);
            tcp_send_len (s->server, tbuf, sprintf (tbuf, "PRIVMSG %s :%s\r\n", s->channel, word_eol[2]));
         }
         list = list->next;
      }
      return TRUE;
   }
   return FALSE;
}

int
cmd_voice (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int i = 2;

   while (1)
   {
      if (!*word[i])
      {
         if (i == 2)
            return FALSE;
         send_channel_modes (sess, tbuf, word, 2, i, '+', 'v');
         return TRUE;
      }
      i++;
   }
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
                  if (prefs.indent_nicks)
                     buf[i] = '\t';
                  else
                     buf[i] = ' ';
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
check_nick_completion (struct session *sess, char *cmd, char *tbuf)
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

static void
user_command (struct session *sess, char *cmd, char *word[], char *word_eol[])
{
   unsigned char tbuf[4096];
   char num[2];
   int i = 0, j = 0, n;

   num[1] = 0;

   while (1)
   {
      switch (cmd[i])
      {
      case 0:
         tbuf[j] = 0;
         handle_command (tbuf, sess, FALSE, FALSE);
         return;
      case ';':
         tbuf[j] = 0;
         handle_command (tbuf, sess, FALSE, FALSE);
         j = 0;
         break;
      case '&':
      case '%':
         if (isdigit (cmd[i + 1]) && isdigit (cmd[i + 2]) && isdigit (cmd[i + 3]))
         {
            char ttbuf[4];
            ttbuf[0] = cmd[i + 1];
            ttbuf[1] = cmd[i + 2];
            ttbuf[2] = cmd[i + 3];
            ttbuf[3] = 0;
            tbuf[j] = atoi (ttbuf);
            i += 3;
            j++;
         } else
         {
            if (isdigit (cmd[i + 1]))
            {
               num[0] = cmd[i + 1];
               n = atoi (num);
               if (*word[n] == 0)
               {
                  PrintText (sess, "Bad arguments for user command.\n");
                  return;
               }
               if (cmd[i] == '%')
                  strcpy (&tbuf[j], word[n]);
               else
                  strcpy (&tbuf[j], word_eol[n]);
               j = strlen (tbuf);
               i++;
            } else
            {
               switch (cmd[i + 1])
               {
               case 'v':
                  strcpy (&tbuf[j], VERSION);
                  j = strlen (tbuf);
                  i++;
                  break;
               case 'n':
                  strcpy (&tbuf[j], sess->server->nick);
                  j = strlen (tbuf);
                  i++;
                  break;
               case 'c':
                  if (!sess->channel[0])
                  {
                     notj_msg (sess);
                     return;
                  } else
                  {
                     strcpy (&tbuf[j], sess->channel);
                     j = strlen (tbuf);
                     i++;
                  }
                  break;
               default:
                  tbuf[j] = cmd[i];
                  j++;
               }
            }
         }
         break;
      default:
         tbuf[j] = cmd[i];
         j++;
      }
      i++;
   }
}

int
handle_command (char *cmd, struct session *sess, int history, int nocommand)
{
   struct popup *pop;
   int user_cmd = FALSE, i;
   GSList *list = command_list;
   unsigned char pdibuf[2048];
   unsigned char tbuf[4096];
   char *word[32];
   char *word_eol[32];

   if (!sess)
      return TRUE;
   /*if (command_level > 10) {
      gtkutil_simpledialog("Too many levels of user commands, aborting");
      command_level = 0;
      return FALSE;
      }
      command_level++; */

   if (!*cmd)
      return TRUE;

   if (history)
      history_add (&sess->history, cmd);

#ifdef USE_PERL
   if (perl_command (cmd, sess))
      return TRUE;
#endif

   if (cmd[0] == '/' && !nocommand)
   {
      cmd++;

      process_data_init (pdibuf, cmd, word, word_eol);

      if (EMIT_SIGNAL (XP_USERCOMMAND, sess, pdibuf, word, word_eol, NULL, 0) == 1)
         return TRUE;

#ifdef USE_PLUGIN
      if (module_command (pdibuf, sess, tbuf, word, word_eol) == 0)
         return TRUE;
#endif

      while (list)
      {
         pop = (struct popup *) list->data;
         if (!strcasecmp (pop->name, pdibuf))
         {
            user_command (sess, pop->cmd, word, word_eol);
            user_cmd = TRUE;
         }
         list = list->next;
      }

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
            if (cmds[i].needserver && !sess->server->connected)
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
      if (!sess->server->connected)
      {
         PrintText (sess, "Unknown Command. Try /help\n");
         return TRUE;
      }
      sprintf (tbuf, "%s\r\n", cmd);
      tcp_send (sess->server, tbuf);

   } else
   {

      if (strcmp (sess->channel, "(lastlog)") == 0)
      {
         lastlog (sess->lastlog_sess, tbuf, cmd);
         return TRUE;
      }

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
           if (sess->is_dialog)
            {
              if (sess->server->connected)
              {
                 channel_msg (sess->server, tbuf, sess->channel, sess->server->nick, cmd, TRUE);
                 sprintf (tbuf, "PRIVMSG %s :%s\r\n", sess->channel, cmd);
              } else
              {
                 notc_msg (sess);
                 return TRUE;
              }
            } else
            { 
               if (sess->server->connected)
               {
                  channel_msg (sess->server, tbuf, sess->channel, sess->server->nick, cmd, TRUE);
                  sprintf (tbuf, "PRIVMSG %s :%s\r\n", sess->channel, cmd);
               } else
               {
                  notc_msg (sess);
                  return TRUE;
               }
            } 
         } else
         {
            if (sess->server->connected)
            {
               channel_msg (sess->server, tbuf, sess->channel, sess->server->nick, newcmd, TRUE);
               sprintf (tbuf, "PRIVMSG %s :%s\r\n", sess->channel, newcmd);
            } else
            {
               notc_msg (sess);
               return TRUE;
            }
         }
         tcp_send (sess->server, tbuf);
      } else
         notj_msg (sess);
   }
   return TRUE;
}

void
handle_multiline (struct session *sess, char *cmd, int history, int nocommand)
{
   char *cr;

   cr = strchr (cmd, '\n');
   if (cr)
   {
      while (1)
      {
         if (cr)
            *cr = 0;
         /*command_level = 0;*/
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
      /*command_level = 0;*/
      if (!handle_command (cmd, sess, history, nocommand))
         return;
   }

}
