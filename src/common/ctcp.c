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
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "xchat.h"
#include "cfgfiles.h"
#include "util.h"
#include "popup.h"
#include "signals.h"
#include "../common/util.h"


extern void my_system (char *cmd);
extern int handle_command (char *cmd, void *sess, int history, int nocommand);
extern int tcp_send (struct server *serv, char *buf);
extern void channel_action (struct session *sess, char *tbuf, char *chan, char *from, char *text, int fromme);
extern struct session *find_session_from_channel (char *chan, struct server *serv);


extern GSList *ctcp_list;
extern struct xchatprefs prefs;


static void
ctcp_reply (void *sess, char *tbuf, char *nick, char *word[], char *word_eol[], char *conf)
{
   int i = 0, j = 0;

   while (1)
   {
      switch (conf[i])
      {
      case '%':
         i++;
         switch (conf[i])
         {
         case '\n':
         case 0:
            goto jump;
         case 'd':
            tbuf[j] = 0;
            strcat (tbuf, word_eol[5]);
            j = strlen (tbuf);
            break;
         case 's':
            tbuf[j] = 0;
            strcat (tbuf, nick);
            j = strlen (tbuf);
            break;
         case 't':
            {
               char timestr[64];
               long tt = time (0);
               char *p, *t = ctime (&tt);
               strcpy (timestr, t);
               p = strchr (timestr, '\n');
               if (p)
                  *p = 0;
               tbuf[j] = 0;
               strcat (tbuf, timestr);
               j = strlen (tbuf);
            }
            break;
         }
         break;
      case '\n':
      case 0:
       jump:
         tbuf[j] = 0;
         handle_command (tbuf, sess, 0, 0);
         return;
      default:
         tbuf[j] = conf[i];
         j++;
      }
      i++;
   }
}

static int
ctcp_check (void *sess, char *tbuf, char *nick, char *word[], char *word_eol[], char *ctcp)
{
   int ret = 0;
   char *po;
   struct popup *pop;
   GSList *list = ctcp_list;

   po = strchr (ctcp, '\001');
   if (po)
      *po = 0;

   po = strchr (word_eol[5], '\001');
   if (po)
      *po = 0;

   while (list)
   {
      pop = (struct popup *) list->data;
      if (!strcasecmp (ctcp, pop->name))
      {
         ctcp_reply (sess, tbuf, nick, word, word_eol, pop->cmd);
         ret = 1;
      }
      list = list->next;
   }
   return ret;
}

void
handle_ctcp (struct session *sess, char *outbuf, char *to, char *nick, char *msg, char *word[], char *word_eol[])
{
   char *po;
   session *chansess;

   if (!strncasecmp (msg, "VERSION", 7) && !prefs.hidever)
   {
      sprintf (outbuf,
               "NOTICE %s :\001VERSION xc! "VERSION" %s: http://xchat.org\001\r\n",
               nick, get_cpu_str (0));
      tcp_send (sess->server, outbuf);
   }

   if (!ctcp_check ((void *) sess, outbuf, nick, word, word_eol, word[4] + 2))
   {
      if (!strncasecmp (msg, "ACTION", 6))
      {
         po = strchr (msg + 7, '\001');
         if (po)
            po[0] = 0;
         channel_action (sess, outbuf, to, nick, msg + 7, FALSE);
         return;
      }
      if (!strncasecmp (msg, "SOUND", 5))
      {
         po = strchr (word[5], '\001');
         if (po)
            po[0] = 0;
         EMIT_SIGNAL (XP_TE_CTCPSND, sess->server->front_session, word[5], nick,
                      NULL, NULL, 0);
         sprintf (outbuf, "%s/%s", prefs.sounddir, word[5]);
         if (access (outbuf, R_OK) == 0)
         {
            sprintf (outbuf, "%s %s/%s", prefs.soundcmd, prefs.sounddir, word[5]);
            my_system (outbuf);
         }
         return;
      }
   }

   po = strchr (msg, '\001');
   if (po)
      po[0] = 0;
   if (!is_channel (to))
   {
      EMIT_SIGNAL (XP_TE_CTCPGEN, sess->server->front_session, msg, nick, NULL, NULL, 0);
   } else
   {
      chansess = find_session_from_channel (to, sess->server);
      if (chansess)
      {
         EMIT_SIGNAL (XP_TE_CTCPGENC, chansess, msg, nick, to, NULL, 0);
         return;
      }
      EMIT_SIGNAL (XP_TE_CTCPGENC, sess->server->front_session, msg, nick, to, NULL, 0);
   }
}
