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
#include "util.h"

extern int handle_command (char *cmd, void *sess, int history, int nocommand);
extern int tcp_send (char *buf);
extern void channel_action (char *tbuf, char *chan, char *from, char *text, int fromme);

extern GSList *ctcp_list;

static void
ctcp_reply (char *tbuf, char *nick, char *word[], char *word_eol[], char *conf)
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
         handle_command (tbuf, session, 0, 0);
         return;
      default:
         tbuf[j] = conf[i];
         j++;
      }
      i++;
   }
}

static int
ctcp_check (char *tbuf, char *nick, char *word[], char *word_eol[], char *ctcp)
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
         ctcp_reply (tbuf, nick, word, word_eol, pop->cmd);
         ret = 1;
      }
      list = list->next;
   }
   return ret;
}

void
handle_ctcp (char *outbuf, char *to, char *nick, char *msg, char *word[], char *word_eol[])
{
   char *po;
   session_t *chansess;

   if (!strncasecmp (msg, "VERSION", 7))
   {
      sprintf (outbuf, "NOTICE %s :\001VERSION NF-Chat "VERSION" %s: http://www.netforce.be\001\r\n", nick);
      tcp_send (outbuf);
   }

   if (!ctcp_check (outbuf, nick, word, word_eol, word[4] + 2))
     if (!strncasecmp (msg, "ACTION", 6))
       {
         po = strchr (msg + 7, '\001');
         if (po)
	   po[0] = 0;
         channel_action (outbuf, to, nick, msg + 7, FALSE);
         return;
       }
   

   po = strchr (msg, '\001');
   if (po)
      po[0] = 0;
   if (!is_channel (to))
   {
      EMIT_SIGNAL (XP_TE_CTCPGEN, server->session, msg, nick, NULL, NULL, 0);
   } else
   {
      if (session)
      {
         EMIT_SIGNAL (XP_TE_CTCPGENC, session, msg, nick, to, NULL, 0);
         return;
      }
      EMIT_SIGNAL (XP_TE_CTCPGENC, server->session, msg, nick, to, NULL, 0);
   }
}
