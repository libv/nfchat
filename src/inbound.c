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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "xchat.h"
#include "util.h"
#include "signals.h"
#include "fe.h"

/* xchat.c */

extern int tcp_send (char *buf);
struct away_msg *find_away_message (char *nick);
void save_away_message (char *nick, char *msg);

/* text.c */

extern unsigned char *strip_color (unsigned char *text);
extern void PrintText (session_t *sess, char *text);

/* userlist.c */

extern void change_nick (session_t *sess, char *oldnick, char *newnick);

/* outbound.c */

extern int handle_command (char *cmd, session_t *sess, int history, int nocommand);
extern void process_data_init (unsigned char *buf, char *cmd, char *word[], char *word_eol[]);

/* ctcp.c */

extern void handle_ctcp (char *outbuf, char *to, char *nick, char *msg, char *word[], char *word_eol[]);

extern struct xchatprefs prefs;
extern session_t *current_tab;

#define find_word_to_end(a, b) word_eol[b]
#define find_word(a, b) word[b]

/* black n white(0/1) are bad colors for nicks, and we'll use color 2 for us */
/* also light/dark gray (14/15) */
/* 5,7,8 are all shades of yellow which happen to look dman near the same */

void
clear_channel ()
{
   session->channel[0] = 0;
   fe_clear_channel ();
   fe_set_title ();
}

static void
private_msg (char *tbuf, char *from, char *ip, char *text)
{
  if (EMIT_SIGNAL (XP_PRIVMSG, server, from, ip, text, NULL, 0) == 1)
    return;
  EMIT_SIGNAL (XP_TE_PRIVMSG, session, from, text, NULL, NULL, 0);
}

void
channel_action (char *tbuf, char *chan, char *from, char *text, int fromme)
{
   if (!session)
      fprintf (stderr, "*** NFCHAT WARNING: sess = 0x0 in channel_action()\n");

   if (EMIT_SIGNAL (XP_CHANACTION, session, chan, from, text, NULL, fromme) == 1)
      return;

   if (fromme)
     strcpy (tbuf, from);
   else
     strcpy (tbuf, from);
   
   EMIT_SIGNAL (XP_TE_CHANACTION, session, tbuf, text, NULL, NULL, 0);
}

static int
SearchNick (char *text, char *nicks)
{
   char S[64];
   char *n;
   char *p;
   char *t;
   size_t ns;

   if (nicks == NULL)
      return 0;
   strncpy (S, nicks, 63);
   n = strtok (S, ",");
   while (n != NULL)
   {
      t = text;
      ns = strlen(n);
      while ((p = nocasestrstr (t, n)))
      {
	 if ((p == text || !isalnum(*(p-1))) && !isalnum(*(p+ns)))
	     return 1;

	 t = p + 1;
      }

      n = strtok (NULL, ",");
   }
   return 0;
}

void
channel_msg (char *outbuf, char *chan, char *from, char *text, char fromme)
{
   struct user *user;
   char *real_outbuf = outbuf;
   int highlight = FALSE;

   if (!session)
      return;

   user = find_name (session, from);
   if (user) user->lasttalk = time (0);

   if (EMIT_SIGNAL (XP_CHANMSG, server, chan, from, text, NULL, fromme) == 1)
      return;

   if (fromme)
     strcpy (outbuf, from);
   else
   {

      if (SearchNick (text, server->nick) || SearchNick (text, prefs.bluestring))
      {
         if (EMIT_SIGNAL (XP_HIGHLIGHT, session, chan, from, text, NULL, fromme) == 1)
            return;
         highlight = TRUE;

         if (session != current_tab && session->is_tab)
         {
            session->nick_said = TRUE;
            fe_set_hilight ();
         }
      }
      if (highlight)
	sprintf (outbuf, "\002\003%d%s\002\017", prefs.bt_color, from);
      else
	strcpy (outbuf, from);
   }

   if (fromme)
     EMIT_SIGNAL (XP_TE_UCHANMSG, session, real_outbuf, text, NULL, NULL, 0);
   else 
     EMIT_SIGNAL (XP_TE_CHANMSG, session, real_outbuf, text, NULL, NULL, 0);
}

void
user_new_nick (char *outbuf, char *nick, char *newnick, int quiet)
{
   int me;

   if (*newnick == ':')
      newnick++;
   if (!strcasecmp (nick, server->nick))
   {
      me = TRUE;
      strcpy (server->nick, newnick);
   } else
      me = FALSE;

   if (EMIT_SIGNAL (XP_CHANGENICK, server, nick, newnick, NULL, NULL, me) == 1)
      return;

   if (me || find_name (session, nick))
     {
       if (!quiet)
	 {
	   if (me)
	     EMIT_SIGNAL (XP_TE_UCHANGENICK, session, nick, newnick, NULL, NULL, 0);
	   else
	     EMIT_SIGNAL (XP_TE_CHANGENICK, session, nick, newnick, NULL, NULL, 0);
	 }
       change_nick (session, nick, newnick);
     }
   if (!strcasecmp (session->channel, nick))
     {
       strncpy (session->channel, newnick, 200);
       fe_set_channel ();
       fe_set_title ();
     }
   if (me)
     fe_set_nick (newnick);
}

static void
you_joined (char *outbuf, char *chan, char *nick, char *ip)
{
  strncpy (session->channel, chan, 200);
  fe_set_channel ();
  fe_set_title ();
  session->waitchannel[0] = 0;
  session->end_of_names = FALSE;
  sprintf (outbuf, "MODE %s\r\n", chan);
  tcp_send (outbuf);
  clear_user_list (session);
  EMIT_SIGNAL (XP_TE_UJOIN, session, nick, chan, ip, NULL, 0);
}

static void
you_kicked (char *tbuf, char *chan, char *kicker, char *reason)
{
  EMIT_SIGNAL (XP_TE_UKICK, session, server->nick, chan, kicker, reason, 0);
  clear_channel (session);
  if (prefs.autorejoin)
    {
      if (session->channelkey[0] == '\0')
	sprintf (tbuf, "JOIN %s\r\n", chan);
      else
	sprintf (tbuf, "JOIN %s %s\r\n", chan, session->channelkey);
      tcp_send (tbuf);
      strcpy (session->waitchannel, chan);
    }
}

static void
you_parted (char *tbuf, char *chan, char *ip, char *reason)
{
  if (*reason)
    EMIT_SIGNAL (XP_TE_UPARTREASON, session, server->nick, ip, chan, reason, 0);
  else
    EMIT_SIGNAL (XP_TE_UPART, session, server->nick, ip, chan, NULL, 0);
  clear_channel (sess);
}


static void
names_list (char *tbuf, char *chan, char *names)
{
   char name[64];
   int pos = 0;

   if (session->channel != chan)
   {
      EMIT_SIGNAL (XP_TE_USERSONCHAN, session, chan, names, NULL,
                   NULL, 0);
      return;
   }
   EMIT_SIGNAL (XP_TE_USERSONCHAN, session, chan, names, NULL, NULL, 0);

   if (session->end_of_names)
   {
      session->end_of_names = FALSE;
      clear_user_list (session);
   }

   while (1)
   {
      switch (*names)
      {
      case 0:
         name[pos] = 0;
         if (pos != 0)
            add_name (session, name, 0);
         return;
      case ' ':
         name[pos] = 0;
         pos = 0;
         add_name (session, name, 0);
         break;
      default:
         name[pos] = *names;
         if (pos < 61)
            pos++;
      }
      names++;
   }
}

static void
topic (char *tbuf, char *buf)
{
   char *po, *new_topic;

   po = strchr (buf, ' ');
   if (po)
   {
      po[0] = 0;
      new_topic = strip_color (po + 2);
      fe_set_topic (new_topic);
      free (new_topic);
      EMIT_SIGNAL (XP_TE_TOPIC, session, buf, po + 2, NULL, NULL, 0);
   }
}

static void
new_topic (char *tbuf, char *nick, char *chan, char *topic)
{
   session_t *sess = session;
   if (sess)
   {
      fe_set_topic (topic);
      EMIT_SIGNAL (XP_TE_NEWTOPIC, sess, nick, topic, chan, NULL, 0);
   }
}

static void
user_joined (char *outbuf, char *chan, char *user, char *ip)
{
   if (EMIT_SIGNAL (XP_JOIN, server, chan, user, ip, NULL, 0) == 1)
      return;

   EMIT_SIGNAL (XP_TE_JOIN, session, user, chan, ip, NULL, 0);
   add_name (session, user, ip);
}

static void
user_kicked (char *outbuf, char *chan, char *user, char *kicker, char *reason)
{
  EMIT_SIGNAL (XP_TE_KICK, session, kicker, user, chan, reason, 0);
  sub_name (session, user);
}

static void
user_parted (char *chan, char *user, char *ip, char *reason)
{
  if (*reason)
    EMIT_SIGNAL (XP_TE_PARTREASON, session, user, ip, chan, reason, 0);
  else
    EMIT_SIGNAL (XP_TE_PART, session, user, ip, chan, NULL, 0);
  sub_name (session, user);
}

static void
channel_date (session_t *sess, char *tbuf, char *chan, char *timestr)
{
   long n = atol (timestr);
   char *p, *tim = ctime (&n);
   p = strchr (tim, '\n');
   if (p)
      *p = 0;
   EMIT_SIGNAL (XP_TE_CHANDATE, sess, chan, tim, NULL, NULL, 0);
}

static void
topic_nametime (char *tbuf, char *chan, char *nick, char *date)
{
   long n = atol (date);
   char *tim = ctime (&n);
   char *p;

   p = strchr (tim, '\n');
   if (p)
      *p = 0;

   EMIT_SIGNAL (XP_TE_TOPICDATE, session, chan, nick, tim, NULL, 0);
}

void
set_server_name (char *name)
{
  if (name[0] == 0)
    name = server->hostname;
  
  strcpy (server->servername, name);
  
  fe_set_title ();
  
  if (session->is_server)
    {
      strcpy (session->channel, name);
      fe_set_channel ();
    }
}

static void
user_quit (char *outbuf, char *nick, char *reason)
{
  if (sub_name (session, nick))
    EMIT_SIGNAL (XP_TE_QUIT, session, nick, reason, NULL, NULL, 0);
}

static void
got_ping_reply (char *outbuf, char *timestring, char *from)
{
   struct timeval timev;
   unsigned long tim, nowtim, dif;

   gettimeofday (&timev, 0);
   sscanf (timestring, "%lu", &tim);
   nowtim = (timev.tv_sec - 50000) * 1000000 + timev.tv_usec;
   dif = nowtim - tim;

   if (atol (timestring) == 0)
      EMIT_SIGNAL (XP_TE_PINGREP, session, from, "?", NULL, NULL, 0);
   else
   {
      sprintf (outbuf, "%ld.%ld", dif / 1000000, (dif / 100000) % 10);
      EMIT_SIGNAL (XP_TE_PINGREP, session, from, outbuf, NULL, NULL, 0);
   }
}

static void
notice (char *outbuf, char *to, char *nick, char *msg, char *ip)
{
   char *po;

   if (msg[0] == 1)
   {
      msg++;
      if (!strncmp (msg, "PING", 4))
      {
         got_ping_reply (outbuf, msg + 5, nick);
         return;
      }
   }
   po = strchr (msg, '\001');
   if (po)
      po[0] = 0;
   EMIT_SIGNAL (XP_TE_NOTICE, session, nick, msg, NULL, NULL, 0);
}

static void
handle_away (char *outbuf, char *nick, char *msg)
{
   struct away_msg *away = find_away_message (nick);
 
   if (away && !strcmp (msg, away->message))  /* Seen the msg before? */
      save_away_message (nick, msg);
  
   EMIT_SIGNAL (XP_TE_WHOIS5, session, nick, msg, NULL, NULL, 0);
}

static void
channel_mode (char *outbuf, char *chan, char *nick, char sign, char mode, char *extra, int quiet)
{
  char tbuf[2][2];
  
  switch (sign)
    {
    case '+':
      switch (mode)
	{
	case 'k':
	  if (EMIT_SIGNAL (XP_CHANSETKEY, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  strncpy (session->channelkey, extra, 60);
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANSETKEY, session, nick, extra, NULL, NULL, 0);
	  return;
	case 'l':
	  if (EMIT_SIGNAL (XP_CHANSETLIMIT, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  session->limit = atoi (extra);
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANSETLIMIT, session, nick, extra, NULL, NULL, 0);
	  return;
	case 'o':
	  if (EMIT_SIGNAL (XP_CHANOP, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  ul_op_name (session, extra);
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANOP, session, nick, extra, NULL, NULL, 0);
	  return;
	case 'v':
	  if (EMIT_SIGNAL (XP_CHANVOICE, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  voice_name (session, extra);
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANVOICE, session, nick, extra, NULL, NULL, 0);
	  return;
	case 'b':
	  if (EMIT_SIGNAL (XP_CHANBAN, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANBAN, session, nick, extra, NULL, NULL, 0);
	  return;
	case 'e':
	  if (EMIT_SIGNAL (XP_CHANEXEMPT, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANEXEMPT, session, nick, extra, NULL, NULL, 0);
	  return;   
	case 'I':
	  if (EMIT_SIGNAL (XP_CHANINVITE, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANINVITE, session, nick, extra, NULL, NULL, 0);
	  return;
	}
      break;
    case '-':
      switch (mode)
	{
	case 'k':
	  if (EMIT_SIGNAL (XP_CHANRMKEY, session, chan, nick, NULL, NULL, 0) == 1)
	    return;
	  session->channelkey[0] = 0;
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANRMKEY, session, nick, NULL, NULL, NULL, 0);
	  return;
	case 'l':
	  if (EMIT_SIGNAL (XP_CHANRMLIMIT, session, chan, nick, NULL, NULL, 0) == 1)
	    return;
	  session->limit = 0;
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANRMLIMIT, session, nick, NULL, NULL, NULL, 0);
	  return;
	case 'o':
	  if (EMIT_SIGNAL (XP_CHANDEOP, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  deop_name (session, extra);
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANDEOP, session, nick, extra, NULL, NULL, 0);
	  return;
	case 'v':
	  if (EMIT_SIGNAL (XP_CHANDEVOICE, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  devoice_name (session, extra);
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANDEVOICE, session, nick, extra, NULL,
			 NULL, 0);
	  return;
	case 'b':
	  if (EMIT_SIGNAL (XP_CHANUNBAN, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANUNBAN, session, nick, extra, NULL, NULL, 0);
	  return;
	case 'e':
	  if (EMIT_SIGNAL (XP_CHANRMEXEMPT, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANRMEXEMPT, session, nick, extra, NULL, NULL, 0);   
	  return;
	case 'I':
	  if (EMIT_SIGNAL (XP_CHANRMINVITE, session, chan, nick, extra, NULL, 0) == 1)
	    return;
	  if (!quiet)
	    EMIT_SIGNAL (XP_TE_CHANRMINVITE, session, nick, extra, NULL, NULL, 0);
	  return;
	}
      break;
    }
  
  if (!quiet)
    {
      tbuf[0][0] = sign;
      tbuf[0][1] = 0;
      tbuf[1][0] = mode;
      tbuf[1][1] = 0;
      if (extra && *extra)
	EMIT_SIGNAL (XP_TE_CHANMODEGEN, session, nick, tbuf[0], tbuf[1], extra, 0);
      else
	EMIT_SIGNAL (XP_TE_CHANMODEGEN, session, nick, tbuf[0], tbuf[1], extra, 0);
    }
}

static void
channel_modes (char *outbuf, char *word[], char *nick, int displacement)
{
  char *chan = find_word (pdibuf, 3 + displacement);
  if (*chan)
    {
      char *modes = find_word (pdibuf, 4 + displacement);
      if (*modes)
	{
	  int i = 5 + displacement;
	  char sign;
	  if (*modes == ':')
            modes++;            /* not a channel mode, eg +i */
	  sign = modes[0];
	  modes++;
	  
	  while (1)
	    {
	      if (sign == '+')
		{
		stupidcode:
		  switch (modes[0])
		    {
		    case '-':
		      modes++;
		      goto stupidcode;
		    case 'h':
		    case 'o':
		    case 'v':
		    case 'k':
		    case 'l':
		    case 'b':
		    case 'e':
		    case 'I':
		      channel_mode (outbuf, chan, nick, sign, modes[0], word[i], displacement);
		      i++;
		      break;
		    default:
		      channel_mode (outbuf, chan, nick, sign, modes[0], "", displacement);
		    }
		} else
		  {
               switch (modes[0])
               {
               case 'h':
               case 'o':
               case 'v':
               case 'k':
               case 'b':
               case 'e':
               case 'I':
		 channel_mode (outbuf, chan, nick, sign, modes[0], word[i], displacement);
                  i++;
                  break;
               default:
		 channel_mode (outbuf, chan, nick, sign, modes[0], "", displacement);
               }
		  }
	      modes++;
	      if (modes[0] == 0 || modes[0] == ' ')
		return;
	      if (modes[0] == '-' || modes[0] == '+')
		{
		  sign = modes[0];
		  modes++;
		  if (modes[0] == 0 || modes[0] == ' ')
		    return;
		}
	    }
	}
    }
}

static void
check_willjoin_channels (char *tbuf)
{
  if (session->willjoinchannel[0] != 0)
    {
      strcpy (session->waitchannel, session->willjoinchannel);
      session->willjoinchannel[0] = 0;
      if (session->channelkey[0] == '\0')
	sprintf (tbuf, "JOIN %s\r\n", session->waitchannel);
      else
	sprintf (tbuf, "JOIN %s %s\r\n", session->waitchannel,
		 session->channelkey);
      tcp_send (tbuf);
    }
}

static void
next_nick (char *outbuf, char *nick)
{
  server->nickcount++;
  
  switch (server->nickcount)
    {
    case 2:
      sprintf (outbuf, "NICK %s\r\n", prefs.nick2);
      tcp_send (outbuf);
      EMIT_SIGNAL (XP_TE_NICKCLASH, session, nick, prefs.nick2, NULL, NULL, 0);
      break;
    case 3:
      sprintf (outbuf, "NICK %s\r\n", prefs.nick3);
      tcp_send (outbuf);
      EMIT_SIGNAL (XP_TE_NICKCLASH, session, nick, prefs.nick3, NULL, NULL, 0);
      break;
    default:
      EMIT_SIGNAL (XP_TE_NICKFAIL, session, NULL, NULL, NULL, NULL, 0);
    }
}

static void
set_default_modes (char *outbuf)
{
  if (!prefs.invisible)
    return;
  sprintf (outbuf, "MODE %s +i\r\n", server->nick);
  tcp_send (outbuf);  
}

/* process_line() */

void
process_line (void)
{
  char pdibuf[4096];
  char outbuf[4096];
  char *word[32];
  char *word_eol[32], *buf;
  int n;
  
  buf = server->linebuf;
  process_data_init (pdibuf, buf + 1, word, word_eol);
  
  if (EMIT_SIGNAL (XP_INBOUND, session, server, buf, NULL, NULL, 0) == 1)
    return;
  
  if (*buf != ':')
    {
      if (!strncmp (buf, "NOTICE ", 7))
	buf += 7;
      if (!strncmp (buf, "PING ", 5))
	{
	  sprintf (outbuf, "PONG %s\r\n", buf + 5);
	  tcp_send (outbuf);
	  return;
	}
      if (!strncmp (buf, "ERROR", 5))
	{
	  EMIT_SIGNAL (XP_TE_SERVERERROR, session, buf + 7, NULL,
		       NULL, NULL, 0);
	  return;
	}
      EMIT_SIGNAL (XP_TE_SERVERGENMESSAGE, session, buf, NULL, NULL, NULL, 0);
    } else
      {
	buf++;
	
	n = atoi (find_word (pdibuf, 2));
	if (n)
	  {
	    char *text = find_word_to_end (buf, 3);
	    if (*text)
	      {
		if (!strncasecmp (server->nick, text, strlen (server->nick)))
		  text += strlen (server->nick) + 1;
		if (*text == ':')
		  text++;
		switch (n)
		  {
		  case 1:
		    user_new_nick (outbuf, server->nick, word[3], TRUE);
		    set_server_name (pdibuf);
		    goto def;
		  case 301:
		    handle_away (outbuf, find_word (pdibuf, 4), find_word_to_end (buf, 5) + 1);
		    break;
		  case 303:
		    break;
		  case 312:
		    EMIT_SIGNAL (XP_TE_WHOIS3, session, word[4], word_eol[5], NULL, NULL, 0);
		    break;
		  case 311:
		    server->inside_whois = 1;
		    /* FALL THROUGH */
		  case 314:
		    EMIT_SIGNAL (XP_TE_WHOIS1, session, find_word (pdibuf, 4), find_word (pdibuf, 5), find_word (pdibuf, 6), find_word_to_end (buf, 8) + 1, 0);
		    break;
		  case 317:
		    {
		      long n = atol (find_word (pdibuf, 6));
		      long idle = atol (find_word (pdibuf, 5));
		      char *po, *tim = ctime (&n);
		      sprintf (outbuf, "%02ld:%02ld:%02ld", idle / 3600, (idle / 60) % 60, idle % 60);
		      if (n == 0)
			EMIT_SIGNAL (XP_TE_WHOIS4, session, find_word (pdibuf, 4), outbuf, NULL, NULL, 0);
		      else
			EMIT_SIGNAL (XP_TE_WHOIS4T, session, find_word (pdibuf, 4), outbuf, tim, NULL, 0);
		    }
		    break;
		  case 318:
		    server->inside_whois = 0;
		    EMIT_SIGNAL (XP_TE_WHOIS6, session, word[4], NULL, NULL, NULL, 0);
		    break;
		  case 313:
		  case 319:
		    EMIT_SIGNAL (XP_TE_WHOIS2, session, word[4], word_eol[5] + 1, NULL, NULL, 0);
		    break;
		  case 321:
		    break;
		  case 322:
		    sprintf (outbuf, "%-16.16s %-7d %s\017\n", find_word (pdibuf, 4), atoi (find_word (pdibuf, 5)), find_word_to_end (buf, 6) + 1);
		    PrintText (session, outbuf);
		    break;
		  case 323:
		    break;
		  case 324:
		    EMIT_SIGNAL (XP_TE_CHANMODES, session, word[4], word_eol[5], NULL, NULL, 0);
		    channel_modes (outbuf, word, word[3], 1);
		    break;
		  case 329:
		    channel_date (session, outbuf, word[4], word[5]);
		    break;
		  case 332:
		    topic (outbuf, text);
		    break;
		  case 333:
		    topic_nametime (outbuf, find_word (pdibuf, 4), find_word (pdibuf, 5), find_word (pdibuf, 6));
		    break;
		  case 352:          /* WHO */
		    if (!server->skip_next_who)
		      {
			sprintf (outbuf, "%s@%s", word[5], word[6]);
			if (!userlist_add_hostname (session, word[8], outbuf, word_eol[11], word[7]) && !who_sess->doing_who)
			  goto def;
		      } else
			if (!server->doing_who)
			  goto def;
		    break;
		  case 315:          /* END OF WHO */
		    if (server->skip_next_who)
		      server->skip_next_who = FALSE;
		    else
		      {
			if (session->doing_who)
			  session->doing_who = FALSE;
			else
			  goto def;
		      }
		    break;
		  case 353:          /* NAMES */
		    char *names, *chan;
		    
		    chan = word[5];
		    names = word_eol[6];
		    if (*names == ':')
		      names++;
		    names_list (outbuf, chan, names);
		    break;
		  case 366:
		    session->end_of_names = TRUE;
		    break;
		  case 376:
		  case 422:          /* end of motd */
		    server->end_of_motd = TRUE;
		    set_default_modes (outbuf);
		    check_willjoin_channels (outbuf);
		    goto def;
		  case 433:
		    if (server->end_of_motd)
		      goto def;
		    else
		      next_nick (outbuf, word[4]);
		    break;
		  case 437:
		    if (!is_channel (word[4]))
		      next_nick (outbuf, word[4]);
		    else
		      goto def;
		    break;
		  case 471:
		    EMIT_SIGNAL (XP_TE_USERLIMIT, session, word[4], NULL, NULL, NULL, 0);
		    break;
		  case 473:
		    EMIT_SIGNAL (XP_TE_INVITE, session, word[4], NULL, NULL, NULL, 0);
		    break;
		  case 474:
		    EMIT_SIGNAL (XP_TE_BANNED, session, word[4], NULL, NULL, NULL, 0);
		    break;
		  case 475:
		    EMIT_SIGNAL (XP_TE_KEYWORD, session, word[4], NULL, NULL, NULL, 0);
		    break;
		    
		  default:
		  def:
		    if (prefs.skipmotd && !server->motd_skipped)
		      {
			if (n == 375 || n == 372)
			  return;
			if (n == 376)
			  {
			    server->motd_skipped = TRUE;
			    EMIT_SIGNAL (XP_TE_MOTDSKIP, session, NULL, NULL, NULL, NULL, 0);
			    return;
			  }
		      }
		    if (n == 375 || n == 372 || n == 376 || n == 422)
		      {
			EMIT_SIGNAL (XP_TE_MOTD, session, text, NULL, NULL, NULL, 0);
			return;
		      }
		    
		    if (is_channel (text))
		      {
			char *chan = find_word (pdibuf, 3);
			if (!strncasecmp (server->nick, chan, strlen (server->nick)))
			  chan += strlen (server->nick) + 1;
			if (*chan == ':')
			  chan++;
		      }
		    EMIT_SIGNAL (XP_TE_SERVTEXT, server->session, text, NULL, NULL, NULL, 0);
		  }
	      }
	  } else
	    {
	      char t;
	      char nick[64], ip[128];
	      char *po2, *po = strchr (buf, '!');
	      
	      if (po)
		{
		  po2 = strchr (buf, ' ');
		  if ((unsigned long) po2 < (unsigned long) po)
		    po = 0;
		}
	      if (!po)               /* SERVER Message */
		{
		  strcpy (ip, pdibuf);
		  strcpy (nick, pdibuf);
		  goto j2;
		}
	      if (po)
		{
		  t = *po;
		  *po = 0;
		  strcpy (nick, buf);
		  *po = t;
		  po2 = strchr (po, ' ');
		  if (po2)
		    {
		      char *cmd;
		      t = *po2;
		      *po2 = 0;
		      strcpy (ip, po + 1);
		      *po2 = t;
		    j2:
		      cmd = find_word (pdibuf, 2);
		      
		      if (!strcmp ("INVITE", cmd))
			{
			  EMIT_SIGNAL (XP_TE_INVITED, session, word[4] + 1, nick, NULL, NULL, 0);
			  return;
			}
		      if (!strcmp ("JOIN", cmd))
			{
			  char *chan2 = find_word (buf, 4);
			  char *chan = find_word (buf, 3);
			  
			  if (*chan2)
			    return;  /* fuck up */
			  
			  if (*chan == ':')
			    chan++;
			  if (!strcasecmp (nick, server->nick))
			    you_joined (outbuf, chan, nick, ip);
			  else 
			    user_joined (outbuf, chan, nick, ip);
			  return;
			}
		      if (!strcmp ("MODE", cmd))
			{
			  channel_modes (outbuf, word, nick, 0);
			  return;
			}
		      if (!strcmp ("NICK", cmd))
			{
			  user_new_nick (outbuf, nick, find_word_to_end (buf, 3), FALSE);
			  return;
			}
		      if (!strcmp ("NOTICE", cmd))
			{
			  char *to = find_word (pdibuf, 3);
			  if (*to)
			    {
			      char *msg = find_word_to_end (buf, 4) + 1;
			      if (strcmp (nick, server->servername) == 0 || strchr (nick, '.'))
				EMIT_SIGNAL (XP_TE_SERVERGENMESSAGE, session, msg, NULL, NULL, NULL, 0);
			      else
				notice (outbuf, to, nick, msg, ip);
			      return;
			    }
			}
		      if (!strcmp ("PART", cmd))
			{
			  char *chan = cmd + 5;
			  char *reason = word_eol[4];
			  
			  if (*chan == ':')
			    chan++;
			  if (*reason == ':')
			    reason++;
			  if (!strcmp (nick, server->nick))
			    you_parted (outbuf, chan, ip, reason);
			  else
			    user_parted (chan, nick, ip, reason);
			  return;
			}
		      if (!strcmp ("PRIVMSG", cmd))
			{
			  char *to = find_word (pdibuf, 3);
			  if (*to)
			    {
			      char *msg = find_word_to_end (buf, 4) + 1;
			      if (msg[0] == 1 && msg[strlen (msg) - 1] == 1)  /* ctcp */
				handle_ctcp (outbuf, to, nick, msg + 1, word, word_eol);
			      else
				{
				  if (is_channel (to))
				    channel_msg (outbuf, to, nick, msg, FALSE);
				  else
				    private_msg (outbuf, nick, ip, msg);
				}
			      return;
			    }
			}
		      if (!strcmp ("PONG", cmd))
			{
			  got_ping_reply (outbuf, find_word (pdibuf, 4) + 1, find_word (pdibuf, 3));
			  return;
			}
		      if (!strcmp ("QUIT", cmd))
			{
			  user_quit (outbuf, nick, find_word_to_end (buf, 3) + 1);
			  return;
			}
		      if (!strcmp ("TOPIC", cmd))
			{
			  new_topic (outbuf, nick, find_word (pdibuf, 3), find_word_to_end (buf, 4) + 1);
			  return;
			}
		      if (!strcmp ("KICK", cmd))
			{
			  char *kicked = find_word (pdibuf, 4);
			  if (*kicked)
			    {
			      if (!strcmp (kicked, server->nick))
				you_kicked (outbuf, find_word (pdibuf, 3), nick, find_word_to_end (buf, 5) + 1);
			      else
				user_kicked (outbuf, find_word (pdibuf, 3), kicked, nick, find_word_to_end (buf, 5) + 1);
			      return;
			    }
			}
		      if (!strcmp ("KILL", cmd))
			{
			  EMIT_SIGNAL (XP_TE_KILL, session, nick, word_eol[5], NULL,NULL, 0);
			  return;
			}
		      if (!strcmp ("WALLOPS", cmd))
			{
			  char *msg = word_eol[3];
			  if (*msg == ':')
			    msg++;
			  EMIT_SIGNAL (XP_TE_WALLOPS, session, nick, msg, NULL, NULL, 0);
			  return;
			}
		      sprintf (outbuf, "(%s/%s) %s\n", nick, ip, find_word_to_end (buf, 2));
		      PrintText (session, outbuf);
		      return;
		    }
		}
	      EMIT_SIGNAL (XP_TE_SERVERGENMESSAGE, session, buf, NULL, NULL, NULL, 0);
	    }
      }
}
