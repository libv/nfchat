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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "xchat.h"
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "cfgfiles.h"
#include "signals.h"
#include "fe.h"
#include "util.h"
#include "themes-common.h"

extern GSList *sess_list;
extern struct xchatprefs prefs;

extern unsigned char *strip_color (unsigned char *text);
extern void my_system (char *cmd);
extern void check_special_chars (char *);
extern int child_handler (int pid);
extern int load_themeconfig (int themefile);
extern int save_themeconfig (int themefile);

extern char **environ;

int pevt_build_string (char *input, char **output, int *max_arg);

int
timecat (char *buf)
{
   time_t timval = time (0);
   char *tim = ctime (&timval) + 10;
   tim[0] = '[';
   tim[9] = ']';
   tim[10] = ' ';
   tim[11] = 0;
   strcat (buf, tim);
   return 11;
}

void
PrintText (struct session *sess, unsigned char *text)
{
   if (!sess)
   {
      if (!sess_list)
         return;
      sess = (struct session *) sess_list->data;
   }

   if (sess->logfd != -1 && prefs.logging)
   {
      time_t timval = time (0);
      char *tim = ctime (&timval) + 11;
      char *temp = strip_color (text);
      tim[9] = 0;
      if (prefs.timestamp)
         write (sess->logfd, tim, 9);/*strlen (tim));*/
      write (sess->logfd, temp, strlen (temp));
      free (temp);
   }
   fe_print_text (sess, text);
}

void
end_logging (int fd)
{
   char obuf[512];
   time_t currenttime;

   currenttime = time (NULL);
   write (fd, obuf, snprintf (obuf, 510, "**** ENDING LOGGING AT %s\n", ctime (&currenttime)));
   close (fd);
}

static int
parse_log_filename (char *in, char *out, int olen, char *serv, char *chan,
		    char *netw)
{
   int i, len = strlen (in), j = 0, flag = 0;
   char d = 0;
   int t; /* Used in APPENDSTRING below */

#define APPENDCHAR(y) if (j == olen) goto overflow; out[j++] = y;
#define APPENDSTRING(y) t = strlen (y); if ( (j + t) >= olen) goto overflow;\
memcpy (out + j, y, t); j += t;

   for (i = 0; i < len; i++) {
      d = in[i];
      if (!flag) {
	 if (d == '%') {
	    flag = 1;
	    continue;
	 }
	 APPENDCHAR (d);
	 continue;
      }
      /* flag == 1 */
      flag = 0;
      switch (d) {
      case '%':
	 APPENDCHAR ('%');
	 continue;
      case 's':
	 APPENDSTRING (serv);
	 continue;
      case 'c':
	 APPENDSTRING (chan);
	 continue;
      case 'n':
	 APPENDSTRING (netw);
	 continue;
      case 't':
      {
         char       buffer[256];
         struct tm* tm_ptr;
         time_t     my_time;
         my_time = time(0);
         tm_ptr = localtime(&my_time);
         sprintf(buffer, "%4d%02d%02d", 1900+tm_ptr->tm_year,
1+tm_ptr->tm_mon, tm_ptr->tm_mday);
         APPENDSTRING (buffer);
       }
    continue;
      default:
	 goto bad_esc;
      }
      g_assert_not_reached ();
   }
   APPENDCHAR ('\0');
   return 0;

 overflow:
   fe_message ("Overflow in buffer while parsing log name", FALSE);
   return 1;
 bad_esc:
   fe_message ("Bad escape sequence while parsing log name", FALSE);
   return 1;
   
#undef APPENDCHAR
#undef APPENDSTRING
}  

int
open_log_file (char *servname, char *channame)
{
   char buf[512], fn[256];
   int fd;
   time_t currenttime;

   if (!strcasecmp (channame, servname))
   {
      channame = strdup ("server");
   } else
   {
      channame = strdup (channame);
      tolowerStr (channame);
   }

   snprintf (buf, 510, "%s/xchatlogs", get_xdir ());
   if (access (buf, F_OK) != 0)
      mkdir (buf, S_IRUSR | S_IWUSR | S_IXUSR);

   if (parse_log_filename (prefs.logmask, fn, sizeof (fn), servname,
			   channame, "")) {
      /* Abort and cleanup --AGL */
      free (channame);
      return -1;
   }
   
   snprintf (buf, 510, "%s/xchatlogs/%s", get_xdir (), fn);
   free (channame);
   fd = open (buf, O_CREAT | O_APPEND | O_WRONLY, 0x180);
   if (fd < 0)
      return -1;
   currenttime = time (NULL);
   write (fd, buf, snprintf (buf, 510, "**** BEGIN LOGGING AT %s\n", ctime (&currenttime)));

   return fd;
}

void
setup_logging (struct session *sess)
{
   if (sess->logfd != -1)
      end_logging (sess->logfd);
   sess->logfd = open_log_file (sess->server->servername, sess->channel);
}
/* Print Events stuff here --AGL */

/* Consider the following a NOTES file:

   The main upshot of this is:
   * Plugins can intercept text events and do what they like
   * The default text engine can be config'ed

   By default it should appear *exactly* the same (I'm working hard not to change the default style) but if you go into Settings->Edit Event Texts you can change the text's. The format is thus:

   The normal %Cx (color) and %B (bold) etc work

   $x is replaced with the data in var x (e.g. $1 is often the nick)

   $axxx is replace with a single byte of value xxx (in base 10)

   AGL (990507)
 */

/* These lists are thus:
   pntevts_text[] are the strings the user sees (WITH %x etc)
   pntevts[] are the data strings with \000 etc
   evtnames[] are the names of the events
 */

/* To add a new event:

   Think up a name (like JOIN)
   Make up a pevt_name_help struct (with a NULL at the end)
   add a struct to the end of te with,
   {"Name", help_struct, "Default text", num_of_args, NULL}
   Open up signal.h
   add one to NUM_XP
   add a #define XP_TE_NAME with a value of +1 on the last XP_
 */

/* As of 990528 this code is in BETA --AGL */

/* Internals:

   On startup ~/.xchat/printevents.conf is loaded if it doesn't exist the
   defaults are loaded. Any missing events are filled from defaults.
   Each event is parsed by pevt_build_string and a binary output is produced
   which looks like:

   (byte) value: 0 = {
   (int) numbers of bytes
   (char []) that number of byte to be memcpy'ed into the buffer
   }
   1 =
   (byte) number of varable to insert
   2 = end of buffer

   Each XP_TE_* signal is hard coded to call textsignal_handler which calls
   display_event which decodes the data

   This means that this system *should be faster* than snprintf because
   it always 'knows' that format of the string (basically is preparses much
   of the work)

   --AGL
 */

char **pntevts_text;
char **pntevts;

static char *pevt_join_help[] =
{
   "The nick of the joining person",
   "The channel being joined",
   "The host of the person",
   NULL
};

static char *pevt_chanaction_help[] =
{
 "Nickname (which may contain color)",
   "The action",
   NULL
};

static char *pevt_chanmsg_help[] =
{
 "Nickname (which may contain color)",
   "The text",
   NULL
};

static char *pevt_privmsg_help[] =
{
   "Nickname",
   "The message",
   NULL
};

static char *pevt_changenick_help[] =
{
   "Old nickname",
   "New nickname",
   NULL
};

static char *pevt_newtopic_help[] =
{
"Nick of person who changed the topic",
   "Topic",
   "Channel",
   NULL
};

static char *pevt_topic_help[] =
{
   "Channel",
   "Topic",
   NULL
};

static char *pevt_kick_help[] =
{
   "The nickname of the kicker",
   "The person being kicked",
   "The channel",
   "The reason",
   NULL
};

static char *pevt_part_help[] =
{
   "The nick of the person leaving",
   "The host of the person",
   "The channel",
   NULL
};

static char *pevt_chandate_help[] =
{
   "The channel",
   "The time",
   NULL
};

static char *pevt_topicdate_help[] =
{
   "The channel",
   "The creator",
   "The time",
   NULL
};

static char *pevt_quit_help[] =
{
   "Nick",
   "Reason",
   NULL
};

static char *pevt_pingrep_help[] =
{
   "Who it's from",
 "The time in x.x format (see below)",
   NULL
};

static char *pevt_notice_help[] =
{
   "Who it's from",
   "The message",
   NULL
};

static char *pevt_ujoin_help[] =
{
   "The nick of the joining person",
   "The channel being joined",
   "The host of the person",
   NULL
};

static char *pevt_uchanmsg_help[] =
{
 "Nickname (which may contain color)",
   "The text",
   NULL
};

static char *pevt_dprivmsg_help[] =
{
   "Nickname",
   "The message",
   NULL
};

static char *pevt_uchangenick_help[] =
{
   "Old nickname",
   "New nickname",
   NULL
};

static char *pevt_ukick_help[] =
{
   "The nickname of the kicker",
   "The person being kicked",
   "The channel",
   "The reason",
   NULL
};

static char *pevt_partreason_help[] =
{
   "The nick of the person leaving",
   "The host of the person",
   "The channel",
   "The reason",
   NULL
};

static char *pevt_ctcpsnd_help[] =
{
   "The sound",
   "The nick of the person",
   NULL
};

static char *pevt_ctcpgen_help[] =
{
   "The CTCP event",
   "The nick of the person",
   NULL
};

static char *pevt_ctcpgenc_help[] =
{
   "The CTCP event",
   "The nick of the person",
   "The Channel it's going to",
   NULL
};

static char *pevt_chansetkey_help[] =
{
   "The nick of the person who set the key",
   "The key",
   NULL
};

static char *pevt_chansetlimit_help[] =
{
   "The nick of the person who set the limit",
   "The limit",
   NULL
};

static char *pevt_chanop_help[] =
{
   "The nick of the person who has been op'ed",
   "The nick of the person of did the op'ing",
   NULL
};

static char *pevt_chanvoice_help[] =
{
   "The nick of the person who has been voice'ed",
   "The nick of the person of did the voice'ing",
   NULL
};

static char *pevt_chanban_help[] =
{
   "The nick of the person who did the banning",
   "The ban mask",
   NULL
};

static char *pevt_chanrmkey_help[] =
{
   "The nick who removed the key",
   NULL
};

static char *pevt_chanrmlimit_help[] =
{
   "The nick who removed the limit",
   NULL
};

static char *pevt_chandeop_help[] =
{
   "The nick of the person of did the deop'ing",
   "The nick of the person who has been deop'ed",
   NULL
};

static char *pevt_chandevoice_help[] =
{
   "The nick of the person of did the devoice'ing",
   "The nick of the person who has been devoice'ed",
   NULL
};

static char *pevt_chanunban_help[] =
{
   "The nick of the person of did the unban'ing",
   "The ban mask",
   NULL
};

static char *pevt_chanexempt_help[] =
{
   "The nick of the person who did the exempt",
   "The exempt mask",
   NULL
};
  
static char *pevt_chanrmexempt_help[] =
{
   "The nick of the person removed the exempt",
   "The exempt mask",
   NULL
};

static char *pevt_chaninvite_help[] =
{ 
   "The nick of the person who did the invite",
   "The invite mask",
   NULL
};
   
static char *pevt_chanrminvite_help[] =
{ 
   "The nick of the person removed the invite",
   "The invite mask",
   NULL
};

static char *pevt_chanmodegen_help[] =
{
   "The nick of the person setting the mode",
   "The mode's sign (+/-)",
   "The mode letter",
   "The channel it's being set on",
   NULL
};

static char *pevt_whois1_help[] =
{
   "Nickname",
   "Username",
   "Host",
   "Full name",
   NULL
};

static char *pevt_whois2_help[] =
{
   "Nickname",
   "Channel Membership/\"is an IRC operator\"",
   NULL
};

static char *pevt_whois3_help[] =
{
   "Nickname",
   "Server Information",
   NULL
};

static char *pevt_whois4_help[] =
{
   "Nickname",
   "Idle time",
   NULL
};

static char *pevt_whois4t_help[] =
{
   "Nickname",
   "Idle time",
   "Signon time",
   NULL
};

static char *pevt_whois5_help[] =
{
   "Nickname",
   "Away reason",
   NULL
};

static char *pevt_whois6_help[] =
{
   "Nickname",
   NULL
};

static char *pevt_generic_channel_help[] =
{
   "Channel Name",
   NULL
};

static char *pevt_generic_none_help[] =
{
   NULL
};

static char *pevt_servertext_help[] =
{
   "Text",
   NULL
};

static char *pevt_invited_help[] =
{
   "Channel Name",
   "Nick of person who invited you",
   NULL
};

static char *pevt_usersonchan_help[] =
{
   "Channel Name",
   "Users",
   NULL
};

static char *pevt_nickclash_help[] =
{
   "Nickname in use",
   "Nick being tried",
   NULL
};

static char *pevt_connfail_help[] =
{
   "Error String",
   NULL
};

static char *pevt_connect_help[] =
{
   "Host",
   "IP",
   "Port",
   NULL
};

static char *pevt_sconnect_help[] =
{
   "PID",
   NULL
};

static char *pevt_wintype_help[] =
{
   "Type of window",
   NULL
};

static char *pevt_chanmodes_help[] =
{
   "Channel name",
   "Modes string",
   NULL
};

static char *pevt_rawmodes_help[] =
{
   "Nickname",
   "Modes string",
   NULL
};

static char *pevt_kill_help[] =
{
   "Nickname",
   "Reason",
   NULL
};

static char *pevt_serverlookup_help[] =
{
   "Servername",
   NULL
};

static char *pevt_servererror_help[] =
{
   "Text",
   NULL
};

/*static char *pevt_servergenmessage_help[] =
{
   "Text",
   NULL
};*/

static char *pevt_foundip_help[] =
{
   "IP",
   NULL
};

static char *pevt_ctcpsend_help[] =
{
   "Receiver",
   "Message",
   NULL
};

static char *pevt_resolvinguser_help[] =
{
   "Nickname",
   "Hostname",
   NULL
};

static char *pevt_newmail_help[] =
{
   "Number of messages",
   "Bytes in mailbox",
   NULL
};

struct text_event
{
   char *name;
   char **help;
   char *def;
   int num_args;
   char *sound;
};

/* *BBIIGG* struct ahead!! --AGL */

struct text_event te[] =
{
   /* Padding for all the non-text signals */
/*000*/   {NULL, NULL, NULL, 0, NULL},
/*001*/   {NULL, NULL, NULL, 0, NULL},
/*002*/   {NULL, NULL, NULL, 0, NULL},
/*003*/   {NULL, NULL, NULL, 0, NULL},
/*004*/   {NULL, NULL, NULL, 0, NULL},
/*005*/   {NULL, NULL, NULL, 0, NULL},
/*006*/   {NULL, NULL, NULL, 0, NULL},
/*007*/   {NULL, NULL, NULL, 0, NULL},
/*008*/   {NULL, NULL, NULL, 0, NULL},
/*009*/   {NULL, NULL, NULL, 0, NULL},
/*010*/   {NULL, NULL, NULL, 0, NULL},
/*011*/   {NULL, NULL, NULL, 0, NULL},
/*012*/   {NULL, NULL, NULL, 0, NULL},
/*013*/   {NULL, NULL, NULL, 0, NULL},
/*014*/   {NULL, NULL, NULL, 0, NULL},
/*015*/   {NULL, NULL, NULL, 0, NULL},
/*016*/   {NULL, NULL, NULL, 0, NULL},
/*017*/   {NULL, NULL, NULL, 0, NULL},
/*018*/   {NULL, NULL, NULL, 0, NULL},
/*019*/   {NULL, NULL, NULL, 0, NULL},
/*020*/   {NULL, NULL, NULL, 0, NULL},

   /* Now we get down to business */

/*021*/   {"Join", pevt_join_help, "-%C10-%C11>%O$t%B$1%B %C14(%C10$3%C14)%C has joined $2", 3, NULL},
/*022*/   {"Channel Action", pevt_chanaction_help, "%C13*%O$t$1 $2%O", 2, NULL},
/*023*/   {"Channel Message", pevt_chanmsg_help, "%C2<%O$1%C2>%O$t$2%O", 2, NULL},
/*024*/   {"Private Message", pevt_privmsg_help, "%C12*%C13$1%C12*$t%O$2%O", 2, NULL},
/*025*/   {"Change Nick", pevt_changenick_help, "-%C10-%C11-%O$t$1 is now known as $2", 2, NULL},
/*026*/   {"New Topic", pevt_newtopic_help, "-%C10-%C11-%O$t$1 has changed the topic to: $2%O", 3, NULL},
/*027*/   {"Topic", pevt_topic_help, "-%C10-%C11-%O$tTopic for %C11$1%C is %C11$2%O", 2, NULL},
/*028*/   {"Kick", pevt_kick_help, "<%C10-%C11-%O$t$1 has kicked $2 from $3 ($4%O)", 4, NULL},
/*029*/   {"Part", pevt_part_help, "<%C10-%C11-%O$t$1 %C14(%O$2%C14)%C has left $3", 3, NULL},
/*030*/   {"Channel Creation", pevt_chandate_help, "-%C10-%C11-%O$tChannel $1 created on $2", 2, NULL},
/*031*/   {"Topic Creation", pevt_topicdate_help, "-%C10-%C11-%O$tTopic for %C11$1%C set by %C11$2%C at %C11$3%O", 3, NULL},
/*032*/   {"Quit", pevt_quit_help, "<%C10-%C11-%O$t$1 has quit %C14(%O$2%O%C14)%O", 2, NULL},
/*033*/   {"Ping Reply", pevt_pingrep_help, "-%C10-%C11-%O$tPing reply from $1 : $2 second(s)", 2, NULL},
/*034*/   {"Notice", pevt_notice_help, "%C12-%C13$1%C12-%O$t$2%O", 2, NULL},
/*035*/   {"You Joining", pevt_ujoin_help, "-%C10-%C11>%O$t%B$1%B %C14(%C10$3%C14)%C has joined $2", 3, NULL},
/*036*/   {"Your Message", pevt_uchanmsg_help, "%C6<%O$1%C6>%O$t$2%O", 2, NULL},
/*037*/   {"Private Message to Dialog", pevt_dprivmsg_help, "%C2<%O$1%C2>%O$t$2%O", 2, NULL},
/*038*/   {"Your Nick Changing", pevt_uchangenick_help, "-%C10-%C11-%O$tYou are now known as $2", 2, NULL},
/*039*/   {"You're Kicked", pevt_ukick_help, "-%C10-%C11-%O$tYou have been kicked from $2 by $3 ($4%O)", 4, NULL},
/*040*/   {"You Parting", pevt_part_help, "-%C10-%C11-%O$tYou have left channel $3", 3, NULL},
/*041*/   {"CTCP Sound", pevt_ctcpsnd_help, "-%C10-%C11-%O$tReceived a CTCP Sound $1 from $2", 2, NULL},
/*042*/   {"CTCP Generic", pevt_ctcpgen_help, "-%C10-%C11-%O$tReceived a CTCP $1 from $2", 2, NULL},
/*043*/   {"CTCP Generic to Channel", pevt_ctcpgenc_help, "-%C10-%C11-%O$tReceived a CTCP $1 from $2 (to $3)", 3, NULL},
/*044*/   {"Channel Set Key", pevt_chansetkey_help, "-%C10-%C11-%O$t$1 sets channel keyword to $2", 2, NULL},
/*045*/   {"Channel Set Limit", pevt_chansetlimit_help, "-%C10-%C11-%O$t$1 sets channel limit to $2", 2, NULL},
/*046*/   {"Channel Operator", pevt_chanop_help, "-%C10-%C11-%O$t$1 gives channel operator status to $2", 2, NULL},
/*047*/   {"Channel Voice", pevt_chanvoice_help, "-%C10-%C11-%O$t$1 gives voice to $2", 2, NULL},
/*048*/   {"Channel Ban", pevt_chanban_help, "-%C10-%C11-%O$t$1 sets ban on $2", 2, NULL},
/*049*/   {"Channel Remove Keyword", pevt_chanrmkey_help, "-%C10-%C11-%O$t$1 removes channel keyword", 1, NULL},
/*050*/   {"Channel Remove Limit", pevt_chanrmlimit_help, "-%C10-%C11-%O$t$1 removes user limit", 1, NULL},
/*051*/   {"Channel DeOp", pevt_chandeop_help, "-%C10-%C11-%O$t$1 removes channel operator status from $2", 2, NULL},
/*052*/   {"Channel DeVoice", pevt_chandevoice_help, "-%C10-%C11-%O$t$1 removes voice from $2", 2, NULL},
/*053*/   {"Channel UnBan", pevt_chanunban_help, "-%C10-%C11-%O$t$1 removes ban on $2", 2, NULL},
/*054*/   {"Channel Exempt", pevt_chanexempt_help, "-%C10-%C11-%O$t$1 sets exempt on $2", 2, NULL},
/*055*/   {"Channel Remove Exempt", pevt_chanrmexempt_help, "-%C10-%C11-%O$t$1 removes exempt on $2", 2, NULL},
/*056*/   {"Channel INVITE", pevt_chaninvite_help, "-%C10-%C11-%O$t$1 sets invite on $2", 2, NULL},
/*057*/   {"Channel Remove INVITE", pevt_chanrminvite_help, "-%C10-%C11-%O$t$1 removes invite on $2", 2, NULL},
/*058*/   {"Channel Mode Generic", pevt_chanmodegen_help, "-%C10-%C11-%O$t$1 sets mode $2$3 $4", 4, NULL},
/*059*/   {"WhoIs Name Line", pevt_whois1_help, "-%C10-%C11-%O$t%C12[%O$1%C12] %C14(%O$2@$3%C14) %O: $4%O", 4, NULL},
/*060*/   {"WhoIs Channel/Oper Line", pevt_whois2_help, "-%C10-%C11-%O$t%C12[%O$1%C12]%C $2", 2, NULL},
/*061*/   {"WhoIs Server Line", pevt_whois3_help, "-%C10-%C11-%O$t%C12[%O$1%C12]%O $2", 2, NULL},
/*062*/   {"WhoIs Idle Line", pevt_whois4_help, "-%C10-%C11-%O$t%C12[%O$1%C12]%O idle %C11$2%O", 2, NULL},
/*063*/   {"WhoIs Idle Line with Signon", pevt_whois4t_help, "-%C10-%C11-%O$t%C12[%O$1%C12]%O idle %C11$2%O, signon: %C11$3%O", 3, NULL},
/*064*/   {"WhoIs Away Line", pevt_whois5_help, "-%C10-%C11-%O$t%C12[%O$1%C12] %Cis away %C14(%O$2%O%C14)", 2, NULL},
/*065*/   {"WhoIs End", pevt_whois6_help, "-%C10-%C11-%O$t%C12[%O$1%C12] %CEnd of WHOIS list.", 1, NULL},
/*066*/   {"User Limit", pevt_generic_channel_help, "-%C10-%C11-%O$tCannot join%C11 %B$1 %O(User limit reached).", 1, NULL},
/*067*/   {"Banned", pevt_generic_channel_help, "-%C10-%C11-%O$tCannot join%C11 %B$1 %O(You are banned).", 1, NULL},
/*068*/   {"Invite", pevt_generic_channel_help, "-%C10-%C11-%O$tCannot join%C11 %B$1 %O(Channel is invite only).", 1, NULL},
/*069*/   {"Keyword", pevt_generic_channel_help, "-%C10-%C11-%O$tCannot join%C11 %B$1 %O(Requires keyword).", 1, NULL},
/*070*/   {"MOTD Skipped", pevt_generic_none_help, "-%C10-%C11-%O$tMOTD Skipped.", 0, NULL},
/*071*/   {"Server Text", pevt_servertext_help, "-%C10-%C11-%O$t$1%O", 1, NULL},
/*072*/   {"Invited", pevt_invited_help, "-%C10-%C11-%O$tYou have been invited to %C11$1%C by %C11$2", 2, NULL},
/*073*/   {"Users On Channel", pevt_usersonchan_help, "-%C10-%C11-%O$t%C11Users on $1:%C $2", 2, NULL},
/*074*/   {"Nick Clash", pevt_nickclash_help, "-%C10-%C11-%O$t$1 already in use. Retrying with $2..", 2, NULL},
/*075*/   {"Nick Failed", pevt_generic_none_help, "-%C10-%C11-%O$tNickname already in use. Use /NICK to try another.", 0, NULL},
/*076*/   {"Unknown Host", pevt_generic_none_help, "-%C10-%C11-%O$tUnknown host. Maybe you misspelled it?", 0, NULL},
/*077*/   {"Connection Failed", pevt_connfail_help, "-%C10-%C11-%O$tConnection failed. Error: $1", 1, NULL},
/*078*/   {"Connecting", pevt_connect_help, "-%C10-%C11-%O$tConnecting to %C11$1 %C14(%C11$2%C14)%C port %C11$3%C..", 3, NULL},
/*079*/   {"Connected", pevt_generic_none_help, "-%C10-%C11-%O$tConnected. Now logging in..", 0, NULL},
/*080*/   {"Stop Connection", pevt_sconnect_help, "-%C10-%C11-%O$tStopped previous connection attempt (pid=$1)", 1, NULL},
/*081*/   {"Disconnected", pevt_generic_none_help, "-%C10-%C11-%O$tDisconnected ($1).", 1, NULL},
/*082*/   {NULL, NULL, NULL, 0, NULL},
/*083*/   {NULL, NULL, NULL, 0, NULL},
/* {"Delete Notify", pevt_generic_nick_help, "-%C10-%C11-%O$t$1 deleted from notify list.", 1, NULL}, */
/*084*/   {NULL, NULL, NULL, 0, NULL},
/*  {"Add Notify", pevt_generic_nick_help, "-%C10-%C11-%O$t$1 added to notify list.", 1, NULL}, */
/*085*/   {"Window Type Error", pevt_wintype_help, "-%C10-%C11-%O$tYou can't do that in a $1 window.", 1, NULL},
/*086*/   {"Channel Modes", pevt_chanmodes_help, "-%C10-%C11-%O$tChannel $1 modes: $2", 2, NULL},
/*087*/   {"Raw Modes", pevt_rawmodes_help, "-%C10-%C11-%O$t$1 sets modes%B %C14[%O$2%B%C14]%O", 2, NULL},
/*088*/   {"Killed", pevt_kill_help, "-%C10-%C11-%O$tYou have been killed by $1 ($2%O)", 2, NULL},
/*089*/   {NULL, NULL, NULL, 0, NULL},
/*090*/   {NULL, NULL, NULL, 0, NULL},
/*091*/   {NULL, NULL, NULL, 0, NULL},
/*092*/   {NULL, NULL, NULL, 0, NULL},
/*093*/   {NULL, NULL, NULL, 0, NULL},
/*094*/   {NULL, NULL, NULL, 0, NULL},
/*095*/   {NULL, NULL, NULL, 0, NULL},
/*096*/   {NULL, NULL, NULL, 0, NULL},
/*097*/   {NULL, NULL, NULL, 0, NULL},
/*098*/   {NULL, NULL, NULL, 0, NULL},
/*099*/   {NULL, NULL, NULL, 0, NULL},
/*100*/   {NULL, NULL, NULL, 0, NULL},
/*101*/   {NULL, NULL, NULL, 0, NULL},
/*102*/   {NULL, NULL, NULL, 0, NULL},
/*103*/   {NULL, NULL, NULL, 0, NULL},
/*104*/   {NULL, NULL, NULL, 0, NULL},
/*105*/   {NULL, NULL, NULL, 0, NULL},
/*106*/   {NULL, NULL, NULL, 0, NULL},
/*107*/   {NULL, NULL, NULL, 0, NULL},
/*108*/   {NULL, NULL, NULL, 0, NULL},
/*109*/   {NULL, NULL, NULL, 0, NULL},
  /* {"Notify Online", pevt_generic_nick_help, "-%C10-%C11-%O$tNotify: $1 is online ($2).", 2, NULL}, */
/*110*/   {NULL, NULL, NULL, 0, NULL},
 /*{"Notify Number", pevt_notifynumber_help, "-%C10-%C11-%O$t$1 users in notify list.", 1, NULL},*/
/*111*/   {NULL, NULL, NULL, 0, NULL},
  /* {"Notify Empty", pevt_generic_none_help, "-%C10-%C11-%O$tNotify list is empty.", 0, NULL},*/
/*112*/   {"No Running Process", pevt_generic_none_help, "-%C10-%C11-%O$tNo process is currently running", 0, NULL},
/*113*/   {"Process Already Running", pevt_generic_none_help, "-%C10-%C11-%O$tA process is already running", 0, NULL},
/*114*/   {"Server Lookup", pevt_serverlookup_help, "-%C10-%C11-%O$tLooking up %C11$1%C..", 1, NULL},
/*115*/   {"Server Connected", pevt_generic_none_help, "-%C10-%C11-%O$tConnected.", 0, NULL},
/*116*/   {"Server Error", pevt_servererror_help, "-%C10-%C11-%O$t$1%O", 1, NULL},
/*117*/   {"Server Generic Message", pevt_servererror_help, "-%C10-%C11-%O$t$1%O", 1, NULL},
/*118*/   {"Found IP", pevt_foundip_help, "-%C10-%C11-%O$tFound your IP: [$1]", 1, NULL},
/*119*/   {NULL, NULL, NULL, 0, NULL},
/*120*/   {"CTCP Send", pevt_ctcpsend_help, "%C3>%O$1%C3<%O$tCTCP $2%O", 2, NULL},
/*121*/   {"Message Send", pevt_ctcpsend_help, "%C3>%O$1%C3<%O$t$2%O", 2, NULL},
/*122*/   {"Notice Send", pevt_ctcpsend_help, "%C3>%O$1%C3<%O$t$2%O", 2, NULL},
/*123*/   {"Receive Wallops", pevt_dprivmsg_help, "%C12-%C13$1/Wallops%C12-%O$t$2%O", 2, NULL},
/*124*/   {NULL, NULL, NULL, 0, NULL},  /* XP_HIGHLIGHT */
/*125*/   {"Ignore Header", pevt_generic_none_help, "%C08,02 Hostmask             PRIV NOTI CHAN CTCP INVI UNIG %O", 0, NULL},
/*126*/   {NULL, NULL, NULL, 0, NULL},
/*127*/   {"Ignore Footer", pevt_generic_none_help, "%C08,02                                                    %O", 0, NULL},
/*128*/   {NULL, NULL, NULL, 0, NULL},
/*129*/   {NULL, NULL, NULL, 0, NULL},
/*130*/   {"Resolving User", pevt_resolvinguser_help, "-%C10-%C11-%O$tLooking up IP number for%C11 $1%O..", 2, NULL},
/*131*/   {"Ignorelist Empty", pevt_generic_none_help, "  Ignore list is empty.", 0, NULL},
/*132*/   {NULL, NULL, NULL, 0, NULL},
/*133*/   {NULL, NULL, NULL, 0, NULL},
  /* {"Notify Offline", pevt_generic_nick_help, "-%C10-%C11-%O$tNotify: $1 is offline ($2).", 2, NULL}, */
/*134*/   {NULL, NULL, NULL, 0, NULL}, /* XP_TE_MALFORMED_FROM */
/*135*/   {NULL, NULL, NULL, 0, NULL}, /* XP_TE_MALFORMED_PACKET */
/*136*/   {"Part with Reason", pevt_partreason_help, "<%C10-%C11-%O$t$1 %C14(%O$2%C14)%C has left $3 %C14(%O$4%C14)%O", 4, NULL},
/*137*/   {"You Part with Reason", pevt_partreason_help, "-%C10-%C11-%O$tYou have left channel $3 %C14(%O$4%C14)%O", 4, NULL},
/*138*/   {"New Mail", pevt_newmail_help, "-%C3-%C9-%O$tYou have new mail ($1 messages, $2 bytes total).", 2, NULL},
/*139*/   {"Motd", pevt_servertext_help, "-%C10-%C11-%O$t$1%O", 1, NULL},

/*140*/   {NULL, NULL, NULL, 0, NULL},	/* XP_IF_SEND */
/*141*/   {NULL, NULL, NULL, 0, NULL},	/* XP_IF_RECV */
};

int
text_event (int i)
{
   if (te[i].name == NULL)
      return 0;
   else
      return 1;
}

void
pevent_load_defaults ()
{
   int i, len;

   for (i = 0; i < NUM_XP; i++)
   {
      if (!text_event (i))
         continue;
      len = strlen (te[i].def);
      len++;
      if (pntevts_text[i])
         free (pntevts_text[i]);
      pntevts_text[i] = malloc (len);
      memcpy (pntevts_text[i], te[i].def, len);
   }
}

void
pevent_make_pntevts ()
{
   int i, m, len;
   char out[1024];

   for (i = 0; i < NUM_XP; i++)
   {
      if (!text_event (i))
         continue;
      if (pntevts[i] != NULL)
         free (pntevts[i]);
      if (pevt_build_string (pntevts_text[i], &(pntevts[i]), &m) != 0)
      {
         snprintf (out, sizeof (out), "Error parsing event %s.\nLoading default", te[i].name);
         fe_message (out, FALSE);
         free (pntevts_text[i]);
         len = strlen (te[i].def) + 1;
         pntevts_text[i] = malloc (len);
         memcpy (pntevts_text[i], te[i].def, len);
         if (pevt_build_string (pntevts_text[i], &(pntevts[i]), &m) != 0)
         {
            fprintf (stderr, "XChat CRITICAL *** default event text failed to build!\n");
            abort ();
         }
      }
      check_special_chars (pntevts[i]);
   }
}

/* Loading happens at 2 levels:
   1) File is read into blocks
   2) Pe block is parsed and loaded

   --AGL */

/* Better hope you pass good args.. --AGL */

void
pevent_trigger_load (int *i_penum, char **i_text, char **i_snd)
{
   int penum = *i_penum, len;
   char *text = *i_text, *snd = *i_snd;
   
   if (penum == -1)
      goto cleanup_exit;
   if (text == NULL)
      goto cleanup_exit;

   len = strlen (text) + 1;
   if (pntevts_text[penum])
      free (pntevts_text[penum]);
   pntevts_text[penum] = malloc (len);
   memcpy (pntevts_text[penum], text, len);
   if (te[penum].sound)
	 free (te[penum].sound);
   if (snd) {
      len = strlen (snd) + 1;
      te[penum].sound = malloc (len);
      memcpy (te[penum].sound, snd, len);
   } else
      te[penum].sound = NULL;

 cleanup_exit:
   if (text)
      free (text);
   if (snd)
      free (snd);
   *i_text = NULL;
   *i_snd = NULL;
   *i_penum = 0;
}

static int
pevent_find (char *name, int *i_i)
{
   int i = *i_i, j;
   
   j = i + 1;
   while (1) {
      if (j == NUM_XP)
	 j = 0;
      if (j == i)
	 return -1;
      if (!text_event (j)) {
	 j++;
	 continue;
      }
      if (strcmp (te[j].name, name) == 0) {
	 *i_i = j;
	 return j;
      }
      j++;
   }
}

int
pevent_load (char *filename)
{
   /* AGL, I've changed this file and pevent_save, could you please take a look at
    *      the changes and possibly modify them to suit you
    *      //David H
    */
   char *buf, *ibuf;
   int fd, i = 0, pnt = 0;
   struct stat st;
   char *text = NULL, *snd = NULL;
   int penum = 0;
   char *ofs;

   buf = malloc (1000);

   if (filename == NULL)
      /* No name given, use load_themeconfig to find file */
      fd = load_themeconfig (THEME_CONF_EVENTS);
   else {
      strncpy (buf, filename, 1000);
      fd = open (buf, O_RDONLY);
   }

   free (buf);
   if (fd < 0)
      return 1;
   if (fstat (fd, &st) != 0)
      return 1;
   ibuf = malloc (st.st_size);
   read (fd, ibuf, st.st_size);
   close (fd);

   while (buf_get_line (ibuf, &buf, &pnt, st.st_size)) {
      if (buf[0] == '#')
	 continue;
      if (strlen (buf) == 0)
	 continue;

      ofs = strchr (buf, '=');
      if (!ofs)
	 continue;
      *ofs = 0;
      ofs++;
      if (*ofs == 0)
	 continue;

      if (strcmp (buf, "event_name") == 0) {
	 if (penum)
	    pevent_trigger_load (&penum, &text, &snd);
	 penum = pevent_find (ofs, &i);
	 continue;
      } else if (strcmp (buf, "event_text") == 0) {
	 if (text)
	    free (text);
	 text = strdup (ofs);
	 continue;
      } else if (strcmp (buf, "event_sound") == 0) {
	 if (snd)
	    free (snd);
	 snd = strdup (ofs);
	 continue;
      }

      continue;
   }

   pevent_trigger_load (&penum, &text, &snd);
   free (ibuf);
   return 0;
}      
	 
      
void
pevent_check_all_loaded ()
{
   int i, len;

   for (i = 0; i < NUM_XP; i++)
   {
      if (!text_event (i))
         continue;
      if (pntevts_text[i] == NULL)
      {

printf("%s\n", te[i].name);

         /*snprintf(out, sizeof(out), "The data for event %s failed to load. Reverting to defaults.\nThis may be because a new version of XChat is loading an old config file.\n\nCheck all print event texts are correct", evtnames[i]);
	   gtkutil_simpledialog(out);*/
         len = strlen (te[i].def) + 1;
         pntevts_text[i] = malloc (len);
         memcpy (pntevts_text[i], te[i].def, len);
      }
   }
}

void
load_text_events ()
{
   /* I don't free these as the only time when we don't need them
      is once XChat has exit(2)'ed, so why bother ?? --AGL */

   pntevts_text = malloc (sizeof (char *) * (NUM_XP));
   memset (pntevts_text, 0, sizeof (char *) * (NUM_XP));
   pntevts = malloc (sizeof (char *) * (NUM_XP));
   memset (pntevts, 0, sizeof (char *) * (NUM_XP));

   if (pevent_load (NULL))
      pevent_load_defaults ();
   pevent_check_all_loaded ();
   pevent_make_pntevts ();
}

void
display_event (char *i, struct session *sess, int numargs, char **args)
{
   int len, oi, ii, *ar;
   char o[4096], d, a, done_all = FALSE;

   oi = ii = len = d = a = 0;

   if (i == NULL)
      return;

   while (done_all == FALSE)
   {
      d = i[ii++];
      switch (d)
      {
      case 0:
         memcpy (&len, &(i[ii]), sizeof (int));
         ii += sizeof (int);
         if (oi + len > sizeof (o))
         {
            printf ("Overflow in display_event (%s)\n", i);
            return;
         }
         memcpy (&(o[oi]), &(i[ii]), len);
         oi += len;
         ii += len;
         break;
      case 1:
         a = i[ii++];
         if (a > numargs)
         {
            fprintf (stderr, "XChat DEBUG: display_event: arg > numargs (%d %d %s)\n", a, numargs, i);
	     break;
         }
         ar = (int *) args[(int) a];
         if (ar == NULL)
         {
            printf ("Error args[a] == NULL in display_event\n");
            abort ();
         }
         len = strlen ((char *) ar);
         memcpy (&o[oi], ar, len);
         oi += len;
         break;
      case 2:
         o[oi++] = '\n';
         o[oi++] = 0;
         done_all = TRUE;
         continue;
      case 3:
	 if (sess->is_dialog) {
	    if (prefs.dialog_indent_nicks)
	       o[oi++] = '\t';
	    else
	       o[oi++] = ' ';
	 } else {
	    if (prefs.indent_nicks)
	       o[oi++] = '\t';
	    else
	       o[oi++] = ' ';
	 }
	 break;
      }
   }
   o[oi] = 0;
   if (*o == '\n')
      return;
   PrintText (sess, o);
}

int
pevt_build_string (char *input, char **output, int *max_arg)
{
   struct pevt_stage1 *s = NULL, *base = NULL,
              *last = NULL, *next;
   int clen;
   char o[4096], d, *obuf, *i;
   int oi, ii, ti, max = -1, len, x;

   len = strlen (input);
   i = malloc (len + 1);
   memcpy (i, input, len + 1);
   check_special_chars (i);

   len = strlen (i);

   clen = oi = ii = ti = 0;

   for (;;)
   {
      if (ii == len)
         break;
      d = i[ii++];
      if (d != '$')
      {
         o[oi++] = d;
         continue;
      }
      if (i[ii] == '$')
      {
         o[oi++] = '$';
         continue;
      }
      if (oi > 0)
      {
         s = (struct pevt_stage1 *) malloc (sizeof (struct pevt_stage1));
         if (base == NULL)
            base = s;
         if (last != NULL)
            last->next = s;
         last = s;
         s->next = NULL;
         s->data = malloc (oi + sizeof (int) + 1);
         s->len = oi + sizeof (int) + 1;
         clen += oi + sizeof (int) + 1;
         s->data[0] = 0;
         memcpy (&(s->data[1]), &oi, sizeof (int));
         memcpy (&(s->data[1 + sizeof (int)]), o, oi);
         oi = 0;
      }
      if (ii == len)
      {
         fe_message ("String ends with a $", FALSE);
         return 1;
      }
      d = i[ii++];
      if (d == 'a')
      {                         /* Hex value */
         x = 0;
         if (ii == len)
            goto a_len_error;
         d = i[ii++];
         d -= '0';
         x = d * 100;
         if (ii == len)
            goto a_len_error;
         d = i[ii++];
         d -= '0';
         x += d * 10;
         if (ii == len)
            goto a_len_error;
         d = i[ii++];
         d -= '0';
         x += d;
         if (x > 255)
            goto a_range_error;
         o[oi++] = x;
         continue;

       a_len_error:
         fe_message ("String ends in $a", FALSE);
         return 1;
       a_range_error:
         fe_message ("$a value is greater then 255", FALSE);
         return 1;
      }
      if (d == 't') {
	 /* Tab - if tabnicks is set then write '\t' else ' ' */
	 /*s = g_new (struct pevt_stage1, 1);*/
    s = (struct pevt_stage1 *) malloc (sizeof (struct pevt_stage1));
	 if (base == NULL)
	   base = s;
	 if (last != NULL)
	   last->next = s;
	 last = s;
	 s->next = NULL;
	 s->data = malloc (1);
	 s->len = 1;
	 clen += 1;
	 s->data[0] = 3;
	 
	 continue;
      }
      if (d < '0' || d > '9')
      {
         snprintf (o, sizeof (o), "Error, invalid argument $%c\n", d);
         fe_message (o, FALSE);
         return 1;
      }
      d -= '0';
      if (max < d)
         max = d;
      s = (struct pevt_stage1 *) malloc (sizeof (struct pevt_stage1));
      if (base == NULL)
         base = s;
      if (last != NULL)
         last->next = s;
      last = s;
      s->next = NULL;
      s->data = malloc (2);
      s->len = 2;
      clen += 2;
      s->data[0] = 1;
      s->data[1] = d - 1;
   }
   if (oi > 0)
   {
      s = (struct pevt_stage1 *) malloc (sizeof (struct pevt_stage1));
      if (base == NULL)
         base = s;
      if (last != NULL)
         last->next = s;
      last = s;
      s->next = NULL;
      s->data = malloc (oi + sizeof (int) + 1);
      s->len = oi + sizeof (int) + 1;
      clen += oi + sizeof (int) + 1;
      s->data[0] = 0;
      memcpy (&(s->data[1]), &oi, sizeof (int));
      memcpy (&(s->data[1 + sizeof (int)]), o, oi);
      oi = 0;
   }
   s = (struct pevt_stage1 *) malloc (sizeof (struct pevt_stage1));
   if (base == NULL)
      base = s;
   if (last != NULL)
      last->next = s;
   last = s;
   s->next = NULL;
   s->data = malloc (1);
   s->len = 1;
   clen += 1;
   s->data[0] = 2;

   oi = 0;
   s = base;
   obuf = malloc (clen);
   while (s)
   {
      next = s->next;
      memcpy (&obuf[oi], s->data, s->len);
      oi += s->len;
      free (s->data);
      free (s);
      s = next;
   }

   free (i);

   if (max_arg)
      *max_arg = max;
   if (output)
      *output = obuf;

   return 0;
}

static void
play_wave (char *file)
{
   char *buf = malloc (512);

   snprintf (buf, 512, "%s/%s", prefs.sounddir, file);
   if (access (buf, R_OK) == 0)
   {
      snprintf (buf, 512, "%s %s/%s", prefs.soundcmd, prefs.sounddir, file);
      my_system (buf);
   }
   free (buf);
}

int
textsignal_handler (struct session *sess, void *b, void *c,
             void *d, void *e, char f)
{
   /* This handler *MUST* always be the last in the chain
      because it doesn't call the next handler
    */

   char *args[8];
   int numargs, i;

   if (!sess)
      return 0;

   if (!text_event (current_signal))
   {
      printf ("error, textsignal_handler passed non TE signal (%d)\n", current_signal);
      abort ();
   }
   numargs = te[current_signal].num_args;
   i = 0;

   if (te[current_signal].sound)
      play_wave (te[current_signal].sound);

   args[0] = b;
   args[1] = c;
   args[2] = d;
   args[3] = e;

   display_event (pntevts[(int) current_signal], sess, numargs, args);
   return 0;
}

void
printevent_setup ()
{
   int evt = 0;
   struct xp_signal *sig;

   sig = (struct xp_signal *) malloc (sizeof (struct xp_signal));

   sig->signal = -1;
   sig->naddr = NULL;
   sig->callback = XP_CALLBACK (textsignal_handler);
   
   for (evt = 0; evt < NUM_XP; evt++)
   {
      if (!text_event (evt))
         continue;
      g_assert (!sigroots[evt]);
      sigroots[evt] = (struct xp_signal *)g_slist_prepend ((void *)sigroots[evt], sig);
   }
}

void
pevent_dialog_save (char *fn)
{
   int fd, i;
   char buf[1024];
   
   if (!fn)
      /* If no fn was given, use save_themeconfig to determine where to save */
      fd = save_themeconfig (THEME_CONF_EVENTS);
   else {
      snprintf (buf, 510, "%s", fn);
      fd = open (buf, O_CREAT | O_TRUNC | O_WRONLY, 0x180);
   }
   if (fd < 0)
   {
     /*
       fe_message ("Error opening config file\n", FALSE); 
       If we get here when X-Chat is closing the fe-message causes a nice & hard crash
       so we have to use perror which doesn't rely on GTK
     */

      perror ("Error opening config file\n");
      return;
   }

   for (i = 0; i < NUM_XP; i++) {
      if (!text_event (i))
	 continue;
      write (fd, buf, snprintf (buf, sizeof (buf),
				"event_name=%s\n", te[i].name));
      write (fd, buf, snprintf (buf, sizeof (buf),
				"event_text=%s\n", pntevts_text[i]));
      if (te[i].sound && te[i].sound[0])
	 write (fd, buf, snprintf (buf, sizeof (buf),
		"event_sound=%s\n", te[i].sound));
      write (fd, "\n", 1);
   }

   close (fd);
}








