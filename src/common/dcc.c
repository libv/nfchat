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
 *
 * Wayne Conrad, 3 Apr 1999: Color-coded DCC file transfer status windows
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xchat.h"
#include "util.h"
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "dcc.h"
#include "plugin.h"
#include "fe.h"

/* trans */
extern void serv2user(unsigned char *s);
extern void user2serv(unsigned char *s);
/* ~trans */

static char *dcctypes[] =
{"SEND", "RECV", "CHAT", "CHAT"};

struct dccstat_info
{
   char *name;                  /* Display name */
   int color;                   /* Display color (index into colors[] ) */
};

static struct dccstat_info dccstat[] =
{
   {"Queued", 1 /*black */ },
   {"Active", 12 /*cyan */ },
   {"Failed", 4 /*red */ },
   {"Done", 3 /*green */ },
   {"Connect", 1 /*black */ },
   {"Aborted", 4 /*red */ },
};


extern GSList *sess_list;
extern GSList *serv_list;
extern GSList *dcc_list;

extern struct xchatprefs prefs;

extern int tcp_send_len (struct server *serv, char *buf, int len);
extern int tcp_send (struct server *serv, char *buf);
extern char *errorstring (int err);
extern void private_msg (struct server *serv, char *tbuf, char *from, char *ip, char *text);
extern void PrintText (struct session *sess, char *text);
extern void channel_action (struct session *sess, char *tbuf, char *chan, char *from, char *text, int fromme);
extern struct session *find_session_from_channel (char *chan, struct server *serv);

#ifdef USE_PERL
/* perl.c */

int perl_dcc_chat (struct session *sess, struct server *serv, char *buf);
#endif


static void
dcc_calc_cps (struct DCC *dcc)
{
   time_t sec;

   sec = time (0) - dcc->starttime;
   if (sec < 1) sec = 1;
   if (dcc->type == TYPE_SEND)
      dcc->cps = (dcc->ack - dcc->resumable) / sec;
   else
      dcc->cps = (dcc->pos - dcc->resumable) / sec;
}

/* this is called from xchat.c:xchat_misc_checks() every 2 seconds. */

void
dcc_check_timeouts (void)
{
   struct DCC *dcc;
   time_t tim = time (0);
   GSList *next, *list = dcc_list;

   while (list)
   {
      dcc = (struct DCC *) list->data;
      next = list->next;

      if (dcc->dccstat == STAT_ACTIVE)
      {
         dcc_calc_cps (dcc);

         switch (dcc->type)
         {
         case TYPE_SEND:
            fe_dcc_update_send (dcc);
            break;
         case TYPE_RECV:
            fe_dcc_update_recv (dcc);
            break;
         }
      }

      switch (dcc->dccstat)
      {
      case STAT_ACTIVE:
         if (dcc->type == TYPE_SEND || dcc->type == TYPE_RECV)
         {
            if (prefs.dccstalltimeout > 0)
            {
               if (tim - dcc->lasttime > prefs.dccstalltimeout)
               {
                  EMIT_SIGNAL (XP_TE_DCCSTALL, dcc->serv->front_session,
                               dcctypes[(int) dcc->type], file_part (dcc->file),
                               dcc->nick, NULL, 0);
                  dcc_close (dcc, 0, TRUE);
               }
            }
         }
         break;
      case STAT_QUEUED:
         if (dcc->type == TYPE_SEND || dcc->type == TYPE_CHATSEND)
         {
            if (tim - dcc->offertime > prefs.dcctimeout)
            {
               if (prefs.dcctimeout > 0)
               {
                  EMIT_SIGNAL (XP_TE_DCCTOUT, dcc->serv->front_session,
                               dcctypes[(int) dcc->type], file_part (dcc->file),
                               dcc->nick, NULL, 0);
                  dcc_close (dcc, 0, TRUE);
               }
            }
         }
         break;
      }
      list = next;
   }
}

static int
dcc_connect_sok (struct DCC *dcc)
{
   int sok;

   sok = socket (AF_INET, SOCK_STREAM, 0);
   if (sok == -1)
      return -1;

   dcc->SAddr.sin_port = htons (dcc->port);
   dcc->SAddr.sin_family = AF_INET;
   dcc->SAddr.sin_addr.s_addr = dcc->addr;

   fcntl (sok, F_SETFL, O_NONBLOCK);
   connect (sok, (struct sockaddr *) &dcc->SAddr, sizeof (struct sockaddr_in));

   return sok;
}

void
update_dcc_window (int type)
{
   switch (type)
   {
   case TYPE_SEND:
      fe_dcc_update_send_win ();
      break;
   case TYPE_RECV:
      fe_dcc_update_recv_win ();
      break;
   case TYPE_CHATRECV:
   case TYPE_CHATSEND:
      fe_dcc_update_chat_win ();
      break;
   }
}

void
dcc_close (struct DCC *dcc, int dccstat, int destroy)
{
   char type = dcc->type;
   if (dcc->sok != -1)
   {
      if (dcc->wiotag != -1)
         fe_input_remove (dcc->wiotag);
      fe_input_remove (dcc->iotag);
      close (dcc->sok);
      dcc->sok = -1;
      dcc->wiotag = -1;
   }
   if (dcc->fp != -1)
   {
      close (dcc->fp);
      dcc->fp = -1;
   }
   dcc->dccstat = dccstat;
   if (dcc->dccchat)
   {
      free (dcc->dccchat);
      dcc->dccchat = 0;
   }
   if (destroy)
   {
      dcc_list = g_slist_remove (dcc_list, dcc);
      if (dcc->file) free (dcc->file);
      if (dcc->destfile) free (dcc->destfile);
      free (dcc->nick);
      free (dcc);
      update_dcc_window (type);
      return;
   }
   switch (type)
   {
   case TYPE_SEND:
      fe_dcc_update_send (dcc);
      break;
   case TYPE_RECV:
      fe_dcc_update_recv (dcc);
      break;
   default:
      update_dcc_window (type);
   }
}

void
dcc_notify_kill (struct server *serv)
{
   struct server *replaceserv = 0;
   struct DCC *dcc;
   GSList *list = dcc_list;
   if (serv_list)
      replaceserv = (struct server *) serv_list->data;
   while (list)
   {
      dcc = (struct DCC *) list->data;
      if (dcc->serv == serv)
         dcc->serv = replaceserv;
      list = list->next;
   }
}

struct sockaddr_in *
dcc_write_chat (char *nick, char *text)
{
/* trans */
   unsigned char *tbuf;
#define TRANS_STAT_BUF_LEN 1024
   static unsigned char sbuf[TRANS_STAT_BUF_LEN];
/* ~trans */
   struct DCC *dcc;
   int len;

   dcc = find_dcc (nick, "", TYPE_CHATRECV);
   if (!dcc)
      dcc = find_dcc (nick, "", TYPE_CHATSEND);
   if (dcc && dcc->dccstat == STAT_ACTIVE)
   {
      len = strlen (text);
      dcc->size += len;
/* trans */
      if(prefs.use_trans){
        if(len>=TRANS_STAT_BUF_LEN)
          tbuf=malloc(len+1);
        else
          tbuf=sbuf;
        if(!tbuf){
          return 0;
        }
        strcpy(tbuf,text);
        user2serv(tbuf);
        send (dcc->sok, tbuf, len, 0);
        if(tbuf!=sbuf){
            free(tbuf);
        }
      }
      else
/* ~trans */
      send (dcc->sok, text, len, 0);
      send (dcc->sok, "\n", 1, 0);
      fe_dcc_update_chat_win ();
      return &dcc->SAddr;
   }
   return 0;
}

static void
dcc_read_chat (struct DCC *dcc, gint sok)
{
   int i, skip;
   long len;
   char tbuf[1226];
   char lbuf[1026];

   while (1)
   {
      len = recv (sok, lbuf, sizeof (lbuf) - 2, 0);
      if (len < 1)
      {
         if (len < 0)
         {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
               return;
         }
         sprintf (tbuf, "%d", dcc->port);
         EMIT_SIGNAL (XP_TE_DCCCHATF, dcc->serv->front_session, dcc->nick,
                      inet_ntoa (dcc->SAddr.sin_addr), tbuf, NULL, 0);
         dcc_close (dcc, STAT_FAILED, FALSE);
         return;
      }
      i = 0;
      lbuf[len] = 0;
      while (i < len)
      {
         switch (lbuf[i])
         {
         case '\r':
            break;
         case '\n':
            dcc->dccchat->linebuf[dcc->dccchat->pos] = 0;
#ifdef USE_PERL
            {
               char *host_n_nick_n_message;

               host_n_nick_n_message = malloc (
                                                 strlen (dcc->nick) + strlen (dcc->dccchat->linebuf) + 26
                  );
               sprintf (
                host_n_nick_n_message,
                      "%s %d %s: %s",
                          inet_ntoa (dcc->SAddr.sin_addr),
                          dcc->port,
                          dcc->nick,
                 dcc->dccchat->linebuf
                  );
               skip = perl_dcc_chat (find_session_from_channel (dcc->nick, dcc->serv), dcc->serv, host_n_nick_n_message);
               free (host_n_nick_n_message);
            }
#else
            skip = 0;
#endif
            if (!skip)
            {
               if (dcc->dccchat->linebuf[0] == 1 && !strncasecmp (dcc->dccchat->linebuf + 1, "ACTION", 6))
               {
                  session *sess;
                  char *po = strchr (dcc->dccchat->linebuf + 8, '\001');
                  if (po)
                     po[0] = 0;
                  sess = find_session_from_channel (dcc->nick, dcc->serv);
                  if (!sess)
                     sess = dcc->serv->front_session;
                  channel_action (sess, tbuf, dcc->serv->nick, dcc->nick, dcc->dccchat->linebuf + 8, FALSE);
               } else
               {
/* trans */
                 if(prefs.use_trans)
                    serv2user(dcc->dccchat->linebuf);
/* ~trans */
                  private_msg (dcc->serv, tbuf, dcc->nick, "", dcc->dccchat->linebuf);
               }
            }
            dcc->pos += dcc->dccchat->pos;
            dcc->dccchat->pos = 0;
            fe_dcc_update_chat_win ();
            break;
         default:
            dcc->dccchat->linebuf[dcc->dccchat->pos] = lbuf[i];
            if (dcc->dccchat->pos < 1022)
               dcc->dccchat->pos++;
         }
         i++;
      }
   }
}

static void
dcc_read (struct DCC *dcc, gint sok)
{
   guint32 pos;
   int n;
   char buf[4098];

   if (dcc->fp == -1)
   {
      if (dcc->resumable)
      {
         dcc->fp = open (dcc->destfile, O_WRONLY | O_APPEND | OFLAGS);
         dcc->pos = dcc->resumable;
         dcc->ack = dcc->resumable;
      } else
      {
         if (access (dcc->destfile, F_OK) == 0)
         {
            n = 0;
            do
            {
               n++;
               sprintf (buf, "%s.%d", dcc->destfile, n);
            }
            while (access (buf, F_OK) == 0);
            EMIT_SIGNAL (XP_TE_DCCRENAME, dcc->serv->front_session,
                         dcc->destfile, buf, NULL, NULL, 0);
            free (dcc->destfile);
            dcc->destfile = strdup (buf);
         }
         dcc->fp = open (dcc->destfile, OFLAGS | O_TRUNC | O_WRONLY | O_CREAT, prefs.dccpermissions);
      }
   }
   if (dcc->fp == -1)
   {
      EMIT_SIGNAL (XP_TE_DCCFILEERR, dcc->serv->front_session, dcc->destfile,
                 NULL, NULL, NULL, 0);
      dcc_close (dcc, STAT_FAILED, FALSE);
      return;
   }
   while (1)
   {
      n = recv (dcc->sok, buf, sizeof (buf) - 2, 0);
      if (n < 1)
      {
         if (n < 0)
         {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
               return;
         }
         EMIT_SIGNAL (XP_TE_DCCRECVERR, dcc->serv->front_session, dcc->file,
                      dcc->destfile, dcc->nick, NULL, 0);
         dcc_close (dcc, STAT_FAILED, FALSE);
         return;
      }

      if (n > 0)
      {
         write (dcc->fp, buf, n);
         dcc->pos += n;
         pos = htonl (dcc->pos - dcc->resumable);
         send (dcc->sok, (char *) &pos, 4, 0);

         dcc->lasttime = time (0);
         dcc->perc = (gfloat) ((dcc->pos * 100.00) / dcc->size);

         dcc_calc_cps (dcc);

         if (dcc->pos >= dcc->size)
         {
            dcc_close (dcc, STAT_DONE, FALSE);
            sprintf (buf, "%d", dcc->cps);

            EMIT_SIGNAL (XP_TE_DCCRECVCOMP, dcc->serv->front_session, dcc->file,
                         dcc->destfile, dcc->nick, buf, 0);
            return;
         }
      }
   }
}

static void
dcc_connect_finished (struct DCC *dcc, gint sok)
{
   int er;
   char host[128];

   fe_input_remove (dcc->iotag);

   if (connect (sok, (struct sockaddr *) &dcc->SAddr, sizeof (struct sockaddr_in)) < 0)
   {
      er = errno;
      if (er != EISCONN)
      {
         EMIT_SIGNAL (XP_TE_DCCCONFAIL, dcc->serv->front_session,
                      dcctypes[(int) dcc->type], dcc->nick, errorstring (er),
                      NULL, 0);
         dcc->dccstat = STAT_FAILED;
         update_dcc_window (dcc->type);
         return;
      }
   }
   fcntl (sok, F_SETFL, O_NONBLOCK);
   dcc->dccstat = STAT_ACTIVE;
   switch (dcc->type)
   {
   case TYPE_RECV:
      dcc->iotag = fe_input_add (dcc->sok, 1, 0, 1, dcc_read, dcc);
      break;

   case TYPE_CHATRECV:
      dcc->iotag = fe_input_add (dcc->sok, 1, 0, 1, dcc_read_chat, dcc);
      dcc->dccchat = malloc (sizeof (struct dcc_chat));
      dcc->dccchat->pos = 0;
      break;
   }
   update_dcc_window (dcc->type);
   dcc->starttime = time (0);
   dcc->lasttime = dcc->starttime;

   snprintf (host, sizeof host, "%s:%d", inet_ntoa (dcc->SAddr.sin_addr), dcc->port);
   EMIT_SIGNAL (XP_TE_DCCCON, dcc->serv->front_session, dcctypes[(int) dcc->type],
            dcc->nick, host, "to", 0);
}

void
dcc_connect (struct session *sess, struct DCC *dcc)
{
   if (dcc->dccstat == STAT_CONNECTING)
      return;
   dcc->dccstat = STAT_CONNECTING;
   dcc->sok = dcc_connect_sok (dcc);
   if (dcc->sok == -1)
   {
      dcc->dccstat = STAT_FAILED;
      update_dcc_window (dcc->type);
      return;
   }
   dcc->iotag = fe_input_add (dcc->sok, 0, 1, 1, dcc_connect_finished, dcc);
   fe_dcc_update_recv (dcc);
}

static void
dcc_send_data (struct DCC *dcc, gint sok)
{
   char *buf;
   long len, sent;

   if (prefs.dcc_blocksize < 1)        /* this is too little! */
      prefs.dcc_blocksize = 1024;

   if (prefs.dcc_blocksize > 102400)   /* this is too much! */
      prefs.dcc_blocksize = 102400;

   buf = malloc (prefs.dcc_blocksize | 32);  /* at least 32 bytes for the sprintf */
   if (!buf)
      return;

   if (dcc->pos < dcc->size)
   {
      lseek (dcc->fp, dcc->pos, SEEK_SET);
      len = read (dcc->fp, buf, prefs.dcc_blocksize);
      sent = send (sok, buf, len, 0);

      if (sent < 0 && errno != EWOULDBLOCK)
      {
         EMIT_SIGNAL (XP_TE_DCCSENDFAIL, dcc->serv->front_session,
                      file_part (dcc->file), dcc->nick, NULL, NULL, 0);
         dcc_close (dcc, STAT_FAILED, FALSE);
         free (buf);
         return;
      }
      if (sent > 0)
      {
         dcc->pos += sent;
         dcc->lasttime = time (0);
      }
      dcc->perc = (gfloat) ((dcc->pos * 100.00) / dcc->size);
      dcc_calc_cps (dcc);
   } else
   {
      /* it's all sent now, so remove the WRITE/SEND handler */
      if (dcc->wiotag != -1)
      {
         fe_input_remove (dcc->wiotag);
         dcc->wiotag = -1;
      }
   }

   if (dcc->pos >= dcc->size && (dcc->ack + dcc->resumable) >= dcc->size)
   {
      dcc->perc = 100.0;
      dcc_close (dcc, STAT_DONE, FALSE);
      sprintf (buf, "%d", dcc->cps);
      EMIT_SIGNAL (XP_TE_DCCSENDCOMP, dcc->serv->front_session,
                   file_part (dcc->file), dcc->nick, buf, NULL, 0);
   }
   free (buf);
}

static void
dcc_read_ack (struct DCC *dcc, gint sok)
{
   int len;
   guint32 ack;
   char buf[16];

   len = recv (sok, (char *) &ack, 4, MSG_PEEK);
   if (len < 1)
   {
      if (len < 0)
      {
         if (errno == EWOULDBLOCK)
            return;
      }
      EMIT_SIGNAL (XP_TE_DCCSENDFAIL, dcc->serv->front_session,
                   file_part (dcc->file), dcc->nick, NULL, NULL, 0);
      dcc_close (dcc, STAT_FAILED, FALSE);
      return;
   }
   if (len < 4)
      return;
   recv (sok, (char *) &ack, 4, 0);
   dcc->ack = ntohl (ack);

   if (!dcc->fastsend)
   {
      if ((dcc->ack + dcc->resumable) < dcc->pos)
         return;
      dcc_send_data (dcc, sok);
   } else
   {
      if (dcc->pos >= dcc->size && (dcc->ack + dcc->resumable) >= dcc->size)
      {
         dcc->perc = 100.0;
         dcc_close (dcc, STAT_DONE, FALSE);
         sprintf (buf, "%d", dcc->cps);
         EMIT_SIGNAL (XP_TE_DCCSENDCOMP, dcc->serv->front_session,
                   file_part (dcc->file), dcc->nick, buf, NULL, 0);
      }
   }
}

static void
dcc_accept (struct DCC *dcc, gint sokk)
{
   char host[128];
   struct sockaddr_in CAddr;
   int sok;
   size_t len;

   len = sizeof (CAddr);
   sok = accept (dcc->sok, (struct sockaddr *) &CAddr, &len);
   fe_input_remove (dcc->iotag);
   close (dcc->sok);
   if (sok < 0)
   {
      dcc_close (dcc, STAT_FAILED, FALSE);
      return;
   }
   fcntl (sok, F_SETFL, O_NONBLOCK);
   dcc->sok = sok;
   dcc->addr = ntohl (CAddr.sin_addr.s_addr);
   memcpy ((char *) &dcc->SAddr.sin_addr, (char *) &CAddr.sin_addr, sizeof (struct in_addr));
   dcc->dccstat = STAT_ACTIVE;
   dcc->lasttime = dcc->starttime = time (0);
   dcc->fastsend = prefs.fastdccsend;
   switch (dcc->type)
   {
   case TYPE_SEND:
      if (dcc->fastsend)
         dcc->wiotag = fe_input_add (sok, 0, 1, 0, dcc_send_data, dcc);
      dcc->iotag = fe_input_add (sok, 1, 0, 1, dcc_read_ack, dcc);
      dcc_send_data (dcc, dcc->sok);
      break;

   case TYPE_CHATSEND:
      dcc->iotag = fe_input_add (dcc->sok, 1, 0, 1, dcc_read_chat, dcc);
      dcc->dccchat = malloc (sizeof (struct dcc_chat));
      dcc->dccchat->pos = 0;
      break;
   }

   update_dcc_window (dcc->type);

   snprintf (host, sizeof host, "%s:%d", inet_ntoa (dcc->SAddr.sin_addr), dcc->port);
   EMIT_SIGNAL (XP_TE_DCCCON, dcc->serv->front_session, dcctypes[(int) dcc->type],
          dcc->nick, host, "from", 0);
}

static int
dcc_listen_init (struct DCC *dcc)
{
   size_t len;
   unsigned long my_addr;
   unsigned long old_addr;
   struct sockaddr_in SAddr;

   dcc->sok = socket (AF_INET, SOCK_STREAM, 0);
   if (dcc->sok == -1)
      return 0;

   memset (&SAddr, 0, sizeof (struct sockaddr_in));

   len = sizeof (SAddr);
   getsockname (dcc->serv->sok, (struct sockaddr *) &SAddr, &len);

   SAddr.sin_family = AF_INET;
   SAddr.sin_port = 0;

   if (prefs.dcc_ip == 0)
      my_addr = prefs.local_ip;
   else
      my_addr = prefs.dcc_ip;

   if (my_addr != 0)
   {
      /* bind to internet facing side */
      old_addr = SAddr.sin_addr.s_addr;
      SAddr.sin_addr.s_addr = my_addr;
      if (bind (dcc->sok, (struct sockaddr *) &SAddr, sizeof (SAddr)) == -1)
      {
         SAddr.sin_addr.s_addr = old_addr;
         bind (dcc->sok, (struct sockaddr *) &SAddr, sizeof (SAddr));
      }
   } else
   {
      bind (dcc->sok, (struct sockaddr *) &SAddr, sizeof (SAddr));
   }

   len = sizeof (SAddr);
   getsockname (dcc->sok, (struct sockaddr *) &SAddr, &len);

   memcpy ((char *) &dcc->SAddr, (char *) &SAddr, sizeof (struct sockaddr_in));
   dcc->port = ntohs (SAddr.sin_port);
   if (my_addr != 0)
      dcc->addr = ntohl (my_addr);
   else
      dcc->addr = ntohl (SAddr.sin_addr.s_addr);

   fcntl (dcc->sok, F_SETFL, O_NONBLOCK);
   listen (dcc->sok, 1);
   fcntl (dcc->sok, F_SETFL, 0);

   dcc->iotag = fe_input_add (dcc->sok, 1, 0, 1, dcc_accept, dcc);

   return TRUE;
}

static struct session *dccsess;
static char *dccto;                    /* lame!! */
static char *dcctbuf;

static void
dcc_send_wild (char *file)
{
   dcc_send (dccsess, dcctbuf, dccto, file);
}

void
dcc_send (struct session *sess, char *tbuf, char *to, char *file)
{
   struct stat st;
   struct DCC *dcc = new_dcc ();
   if (!dcc)
      return;

   dcc->file = expand_homedir (file);

   if (strchr (dcc->file, '*') || strchr (dcc->file, '?'))
   {
      char path[256];
      char wild[256];

      strcpy (wild, file_part (dcc->file));
      path_part (dcc->file, path);
      path[strlen (path) - 1] = 0;  /* remove trailing slash */
      dccsess = sess;
      dccto = to;
      dcctbuf = tbuf;
      for_files (path, wild, dcc_send_wild);
      dcc_close (dcc, 0, TRUE);
      return;
   }
   if (stat (dcc->file, &st) != -1)
   {
      if (*file_part (dcc->file) && !S_ISDIR (st.st_mode))
      {
         if (st.st_size > 0)
         {
            dcc->offertime = time (0);
            dcc->serv = sess->server;
            dcc->dccstat = STAT_QUEUED;
            dcc->size = st.st_size;
            dcc->type = TYPE_SEND;
            dcc->fp = open (dcc->file, OFLAGS | O_RDONLY);
            if (dcc->fp != -1)
            {
               if (dcc_listen_init (dcc))
               {
                  file = dcc->file;
                  while (*file)
                  {
                     if (*file == ' ')
                        *file = '_';
                     file++;
                  }
                  dcc->nick = strdup (to);
                  if (!prefs.noautoopendccsendwindow)
                     fe_dcc_open_send_win ();
                  else
                     fe_dcc_update_send_win ();
                  snprintf (tbuf, 255, "PRIVMSG %s :\001DCC SEND %s %lu %d %ld\001\r\n",
                            to, file_part (dcc->file), dcc->addr, dcc->port, dcc->size);
                  tcp_send (sess->server, tbuf);

                  EMIT_SIGNAL (XP_TE_DCCOFFER, sess, file_part (dcc->file), to,
                       NULL, NULL, 0);
               } else
               {
                  dcc_close (dcc, 0, TRUE);
               }
               return;
            }
         }
      }
   }
   snprintf (tbuf, 255, "Cannot access %s\n", dcc->file);
   PrintText (sess, tbuf);
   dcc_close (dcc, 0, TRUE);
}

static struct DCC *
find_dcc_from_port (int port, int type)
{
   struct DCC *dcc;
   GSList *list = dcc_list;
   while (list)
   {
      dcc = (struct DCC *) list->data;
      if (dcc->port == port &&
          dcc->dccstat == STAT_QUEUED &&
          dcc->type == type)
         return dcc;
      list = list->next;
   }
   return 0;
}

struct DCC *
find_dcc (char *nick, char *file, int type)
{
   GSList *list = dcc_list;
   struct DCC *dcc;
   while (list)
   {
      dcc = (struct DCC *) list->data;
      if (!strcasecmp (nick, dcc->nick))
      {
         if (type == -1 || dcc->type == type)
         {
            if (!file[0])
               return dcc;
            if (!strcasecmp (file, file_part (dcc->file)))
               return dcc;
            if (!strcasecmp (file, dcc->file))
               return dcc;
         }
      }
      list = list->next;
   }
   return 0;
}

/* called when we receive a NICK change from server */

void 
dcc_change_nick (struct server *serv, char *oldnick, char *newnick)
{
   struct DCC *dcc;
   GSList *list = dcc_list;

   while (list)
   {
      dcc = (struct DCC *) list->data;
      if (dcc->serv == serv)
      {
         if (!strcasecmp (dcc->nick, oldnick))
         {
            if (dcc->nick) free (dcc->nick);
            dcc->nick = strdup (newnick);
         }
      }
      list = list->next;
   }
}

void
dcc_get (struct DCC *dcc)
{
   if (dcc)
   {
      switch (dcc->dccstat)
      {
      case STAT_QUEUED:
         dcc->resumable = 0;
         dcc->pos = 0;
         dcc_connect (0, dcc);
         break;
      case STAT_DONE:
      case STAT_FAILED:
      case STAT_ABORTED:
         dcc_close (dcc, 0, TRUE);
         break;
      }
   }
}

void
dcc_get_nick (struct session *sess, char *nick)
{
   struct DCC *dcc;
   GSList *list = dcc_list;
   while (list)
   {
      dcc = (struct DCC *) list->data;
      if (!strcasecmp (nick, dcc->nick))
      {
         if (dcc->dccstat == STAT_QUEUED && dcc->type == TYPE_RECV)
         {
            dcc->resumable = 0;
            dcc->pos = 0;
            dcc_connect (sess, dcc);
            return;
         }
      }
      list = list->next;
   }
   if (sess)
      EMIT_SIGNAL (XP_TE_DCCIVAL, sess, NULL, NULL, NULL, NULL, 0);
}

struct DCC *
new_dcc (void)
{
   struct DCC *dcc = malloc (sizeof (struct DCC));
   if (!dcc)
      return 0;
   memset (dcc, 0, sizeof (struct DCC));
   dcc->perc = 0.00;
   dcc->sok = -1;
   dcc->fp = -1;
   dcc->wiotag = -1;
   dcc_list = g_slist_prepend (dcc_list, dcc);
   return (dcc);
}

void
dcc_chat (struct session *sess, char *nick)
{
   char outbuf[256];
   struct DCC *dcc;

   dcc = find_dcc (nick, "", TYPE_CHATSEND);
   if (dcc)
   {
      switch (dcc->dccstat)
      {
      case STAT_ACTIVE:
      case STAT_QUEUED:
      case STAT_CONNECTING:
         EMIT_SIGNAL (XP_TE_DCCCHATREOFFER, sess, nick, NULL, NULL, NULL, 0);
         return;
      case STAT_ABORTED:
      case STAT_FAILED:
         dcc_close (dcc, 0, TRUE);
      }
   }
   dcc = find_dcc (nick, "", TYPE_CHATRECV);
   if (dcc)
   {
      switch (dcc->dccstat)
      {
      case STAT_QUEUED:
         dcc_connect (0, dcc);
         break;
      case STAT_FAILED:
      case STAT_ABORTED:
         dcc_close (dcc, 0, TRUE);
      }
      return;
   }
   /* offer DCC CHAT */

   dcc = new_dcc ();
   if (!dcc)
      return;
   dcc->offertime = time (0);
   dcc->serv = sess->server;
   dcc->dccstat = STAT_QUEUED;
   dcc->type = TYPE_CHATSEND;
   time (&dcc->starttime);
   dcc->nick = strdup (nick);
   if (dcc_listen_init (dcc))
   {
      if (!prefs.noautoopendccchatwindow)
         fe_dcc_open_chat_win ();
      else
         fe_dcc_update_chat_win ();
      snprintf (outbuf, sizeof outbuf, "PRIVMSG %s :\001DCC CHAT chat %lu %d\001\r\n",
          nick, dcc->addr, dcc->port);
      tcp_send (dcc->serv, outbuf);
      EMIT_SIGNAL (XP_TE_DCCCHATOFFERING, sess, nick, NULL, NULL, NULL, 0);
   } else
      dcc_close (dcc, 0, TRUE);
}

static void
dcc_malformed (struct session *sess, char *nick, char *data)
{
   EMIT_SIGNAL (XP_TE_MALFORMED_FROM, sess, nick, NULL, NULL, NULL, 0);
   EMIT_SIGNAL (XP_TE_MALFORMED_PACKET, sess, data, NULL, NULL, NULL, 0);
}

void
dcc_resume (struct DCC *dcc)
{
   char tbuf[256];

   if (dcc)
   {
      if (dcc->dccstat == STAT_QUEUED && dcc->resumable)
      {
         snprintf (tbuf, sizeof tbuf, "PRIVMSG %s :\001DCC RESUME %s %d %lu\001\r\n",
                   dcc->nick, dcc->file, dcc->port, dcc->resumable);
         tcp_send (dcc->serv, tbuf);
      }
   }
}

void
handle_dcc (struct session *sess, char *outbuf, char *nick, char *word[], char *word_eol[])
{
   struct DCC *dcc;
   char *type = word[5];
   int port, size;
   unsigned long addr;  /* FIXME for ipv6 */

   if (!strcasecmp (type, "CHAT"))
   {
      port = atol (word[8]);
      sscanf (word[7], "%lu", &addr);
      addr = ntohl (addr);

      if (!addr || port < 1024)
      {
         dcc_malformed (sess, nick, word_eol[4] + 2);
         return;
      }
      dcc = find_dcc (nick, "", TYPE_CHATSEND);
      if (dcc)
         dcc_close (dcc, 0, TRUE);

      dcc = find_dcc (nick, "", TYPE_CHATRECV);
      if (dcc)
         dcc_close (dcc, 0, TRUE);

      dcc = new_dcc ();
      if (dcc)
      {
         dcc->serv = sess->server;
         dcc->type = TYPE_CHATRECV;
         dcc->dccstat = STAT_QUEUED;
         dcc->addr = addr;
         dcc->port = port;
         dcc->nick = strdup (nick);

         EMIT_SIGNAL (XP_TE_DCCCHATOFFER, sess->server->front_session, nick,
                 NULL, NULL, NULL, 0);
         if (!prefs.noautoopendccchatwindow)
            fe_dcc_open_chat_win ();
         else
            fe_dcc_update_chat_win ();
         if (prefs.autodccchat)
            dcc_connect (0, dcc);
         return;
      }
   }
   if (!strcasecmp (type, "RESUME"))
   {
      port = atol (word[7]);
      dcc = find_dcc_from_port (port, TYPE_SEND);
      if (!dcc)
         dcc = find_dcc (nick, word[6], TYPE_SEND);
      if (dcc)
      {
         sscanf (word[8], "%lu", &dcc->resumable);
         dcc->pos = dcc->resumable;
         dcc->ack = 0;
         lseek (dcc->fp, dcc->pos, SEEK_SET);
         snprintf (outbuf, 255, "PRIVMSG %s :\001DCC ACCEPT %s %d %lu\001\r\n",
                   dcc->nick, file_part(dcc->file), port, dcc->resumable);
         tcp_send (dcc->serv, outbuf);
         sprintf (outbuf, "%lu", dcc->pos);

         EMIT_SIGNAL (XP_TE_DCCRESUMEREQUEST, sess, nick, file_part (dcc->file),
                      outbuf, NULL, 0);
         return;
      }
   }
   if (!strcasecmp (type, "ACCEPT"))
   {
      port = atol (word[7]);
      dcc = find_dcc_from_port (port, TYPE_RECV);
      if (dcc && dcc->dccstat == STAT_QUEUED)
      {
         dcc_connect (0, dcc);
         return;
      }
   }
   if (!strcasecmp (type, "SEND"))
   {
      char *file = file_part (word[6]);
      port = atol (word[8]);
      size = atol (word[9]);

      sscanf (word[7], "%lu", &addr);
      addr = ntohl (addr);

      if (!addr || !size || port < 1024)
      {
         dcc_malformed (sess, nick, word_eol[4] + 2);
         return;
      }
      dcc = new_dcc ();
      if (dcc)
      {
         struct stat st;

         dcc->file = strdup (file);

         dcc->destfile = malloc (strlen(prefs.dccdir) + strlen(nick) + strlen(file) + 4);

         strcpy (dcc->destfile, prefs.dccdir);
         if (prefs.dccdir[strlen (prefs.dccdir) - 1] != '/')
            strcat (dcc->destfile, "/");
         if (prefs.dccwithnick)
         {
            strcat (dcc->destfile, nick);
            strcat (dcc->destfile, ".");
         }
         strcat (dcc->destfile, file);

         if (stat (dcc->destfile, &st) != -1)
            dcc->resumable = st.st_size;
         else
            dcc->resumable = 0;
         if (st.st_size == size)
            dcc->resumable = 0;
         dcc->serv = sess->server;
         dcc->type = TYPE_RECV;
         dcc->dccstat = STAT_QUEUED;
         dcc->addr = addr;
         dcc->SAddr.sin_addr.s_addr = addr;
         dcc->port = port;
         dcc->size = size;
         dcc->nick = strdup (nick);
         if (!prefs.noautoopendccrecvwindow)
            fe_dcc_open_recv_win ();
         else
            fe_dcc_update_recv_win ();
         if (prefs.autodccsend)
         {
            if (prefs.autoresume && dcc->resumable)
            {
               dcc_resume (dcc);
            } else
            {
               dcc->resumable = 0;
               dcc->pos = 0;
               dcc_connect (sess, dcc);
            }
         }
      }
      sprintf (outbuf, "%d", size);
      EMIT_SIGNAL (XP_TE_DCCSENDOFFER, sess->server->front_session, nick, file,
                   outbuf, NULL, 0);
   } else
      EMIT_SIGNAL (XP_TE_DCCGENERICOFFER, sess->server->front_session,
                   word_eol[4] + 2, nick, NULL, NULL, 0);
}

void
dcc_show_list (struct session *sess, char *outbuf)
{
   int i = 0;
   struct DCC *dcc;
   GSList *list = dcc_list;

   PrintText (sess,
              "\0038,2 Type  To/From    Status  Size    Pos     File    \017\n"
              "\002\0039--------------------------------------------------\017\n"
      );
   while (list)
   {
      dcc = (struct DCC *) list->data;
      i++;
      snprintf (outbuf, 255, " %-5.5s %-10.10s %-7.7s %-7ld %-7ld %s\n",
                dcctypes[(int) dcc->type], dcc->nick,
                dccstat[(int) dcc->dccstat].name, dcc->size, dcc->pos, file_part (dcc->file));
      PrintText (sess, outbuf);
      list = list->next;
   }
   if (!i)
      PrintText (sess, "No active DCCs\n");
}
