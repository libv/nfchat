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
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include "fe-text.h"
#include "../common/xchat.h"
#include "../common/fe.h"

extern char *get_cpu_str (int color);
extern int handle_command (char *cmd, struct session *sess, int history, int nocommand);
extern int get_mhz (void);
extern int timecat (char *buf);
extern void kill_session_callback (struct session *sess);

extern struct xchatprefs prefs;
extern struct session *current_tab;
extern GSList *sess_list;


static GSList *se_list;                /* socket event list */
static int se_list_count;
static int done = FALSE;               /* finished ? */


static void
send_command (char *cmd)
{
   handle_command (cmd, sess_list->data, TRUE, FALSE);
}

static void
read_stdin (void)
{
   int len, i = 0;
   static int pos = 0;
   static char inbuf[1024];
   char tmpbuf[512];

   len = read (STDIN_FILENO, tmpbuf, sizeof tmpbuf - 1);

   while (i < len)
   {
      switch (tmpbuf[i])
      {
      case '\r':
         break;

      case '\n':
         inbuf[pos] = 0;
         pos = 0;
         send_command (inbuf);
         break;

      default:
         inbuf[pos] = tmpbuf[i];
         if (pos < (sizeof inbuf - 2))
            pos++;
      }
      i++;
   }
}

static int done_intro = 0;

void
fe_new_window (struct session *sess)
{
   char buf[512];
   time_t tim = time (0);

   sess->gui = malloc (4);

   if (!sess->server->front_session)
      sess->server->front_session = sess;
   if (!current_tab)
      current_tab = sess;

   if (done_intro)   
      return;
   done_intro = 1;

   fe_print_text (sess,
"\n\00314\002"
"          ######\n"
"    ##### #####  ######## #### #### ######### ##########\n"
"     #### ####   ######## #### #### ######### ##########\n"
"      #######    ####     ######### #### ####    ####\n"
"      #######    ####     ######### #########    ####\n"
"     #### ####   ######## #### #### #### ####    ####\n"
"    ##### #####  ######## #### #### #### ####    ####\n"
"   ######\002\n"
"\n");

   snprintf (buf, sizeof buf,
"   \00313xc\0032! \00310"VERSION"\017 on %s \017GLIB\00310 %d.%d.%d\n"
"\n"
"   \00310Launched\0032: \017%s\n",
      get_cpu_str(1),
      glib_major_version, glib_minor_version, glib_micro_version,
      ctime (&tim));
   fe_print_text (sess, buf);

   strcpy (buf, "   \00310Features\0032:\017 TextFE ");
#ifdef USE_PERL
   strcat (buf, "Perl ");
#endif
#ifdef USE_PYTHON
   strcat (buf, "Python ");
#endif
#ifdef USE_PLUGIN
   strcat (buf, "Plugin ");
#endif
#ifdef ENABLE_NLS
   strcat (buf, "NLS ");
#endif
#ifdef SOCKS
   strcat (buf, "Socks5 ");
#endif
   strcat (buf, "\n\n");
   fe_print_text (sess, buf);
   fflush (stdout);
   fflush (stdin);
}

/*                0  1  2  3  4  5  6  7   8   9   10  11  12  13  14  15 */
static int colconv[] = {7, 0, 4, 2, 1, 3, 5, 11, 13, 12, 6, 16, 14, 15, 10, 7};

void
fe_print_text (struct session *sess, char *text)
{
   int dotime = FALSE;
   char num[8];
   int reverse = 0, under = 0, bold = 0,
       comma, k, i = 0, j = 0, len = strlen (text);
   unsigned char *newtext = malloc (len + 1024);

   if (prefs.timestamp)
   {
      newtext[0] = 0;
      j += timecat (newtext);
   }
   while (i < len)
   {
      if (dotime && text[i] != 0)
      {
         dotime = FALSE;
         newtext[j] = 0;
         j += timecat (newtext);
      }
      switch (text[i])
      {
      case 3:
         i++;
         if (!isdigit (text[i]))
         {
            newtext[j] = 27;
            j++;
            newtext[j] = '[';
            j++;
            /*newtext[j] = '0';
            j++;*/
            newtext[j] = 'm';
            j++;
            i--;
            goto jump2;
         }
         k = 0;
         comma = FALSE;
         while (i < len)
         {
            if (text[i] >= '0' && text[i] <= '9' && k < 2)
            {
               num[k] = text[i];
               k++;
            } else
            {
               int col, mirc;
               num[k] = 0;
               newtext[j] = 27;
               j++;
               newtext[j] = '[';
               j++;
               if (k == 0)
               {
                  /*newtext[j] = '0';
                  j++;*/
                  newtext[j] = 'm';
                  j++;
               } else
               {
                  if (comma)
                     col = 40;
                  else
                     col = 30;
                  mirc = atoi (num);
                  mirc = colconv[mirc];
                  if (mirc > 9)
                  {
                     mirc += 50;
                     sprintf ((char *) &newtext[j], "%dm", mirc + col);
                  } else
                  {
                     sprintf ((char *) &newtext[j], "%dm", mirc + col);
                  }
                  j = strlen (newtext);
               }
               switch (text[i])
               {
               case ',':
                  comma = TRUE;
                  break;
               default:
                  goto jump;
               }
               k = 0;
            }
            i++;
         }
         break;
      case '\026':             /* REVERSE */
         if (reverse)
         {
            reverse = FALSE;
            strcpy (&newtext[j], "\033[27m");
         } else
         {
            reverse = TRUE;
            strcpy (&newtext[j], "\033[7m");
         }
         j = strlen (newtext);
         break;
      case '\037':             /* underline */
         if (under)
         {
            under = FALSE;
            strcpy (&newtext[j], "\033[24m");
         } else
         {
            under = TRUE;
            strcpy (&newtext[j], "\033[4m");
         }
         j = strlen (newtext);
         break;
      case '\002':             /* bold */
         if (bold)
         {
            bold = FALSE;
            strcpy (&newtext[j], "\033[22m");
         } else
         {
            bold = TRUE;
            strcpy (&newtext[j], "\033[1m");
         }
         j = strlen (newtext);
         break;
      case '\007':
         if (!prefs.filterbeep)
         {
            newtext[j] = text[i];
            j++;
         }
         break;
      case '\017':             /* reset all */
         strcpy (&newtext[j], "\033[m");
         j += 3;
         reverse = FALSE;
         bold = FALSE;
         under = FALSE;
         break;
      case '\t':
         newtext[j] = ' ';
         j++;
         break;
      case '\n':
         newtext[j] = '\r';
         j++;
         if (prefs.timestamp)
            dotime = TRUE;
      default:
         newtext[j] = text[i];
         j++;
      }
    jump2:
      i++;
    jump:
   }
   newtext[j] = 0;
   write (STDOUT_FILENO, newtext, j);
   free (newtext);
}

/* Anyone want to code these 2 functions ?? */

void
fe_timeout_remove (int tag)
{
}

int
fe_timeout_add (int interval, void *callback, void *userdata)
{
   return -1;
}

void
fe_input_remove (int tag)
{
   socketevent *se;
   GSList *list;

   list = se_list;
   while (list)
   {
      se = (socketevent *) list->data;
      if (se->tag == tag)
      {
         se_list = g_slist_remove (se_list, se);
         free (se);
         return;
      }
      list = list->next;
   }
}

int
fe_input_add (int sok, int rread, int wwrite, int eexcept, void *func, void *data)
{
   socketevent *se = malloc (sizeof (socketevent));

   se->tag = se_list_count;
   se->sok = sok;
   se->rread = rread;
   se->wwrite = wwrite;
   se->eexcept = eexcept;
   se->callback = func;
   se->userdata = data;
   se_list = g_slist_prepend (se_list, se);

   se_list_count++;	/* this overflows at 2.2Billion, who cares!! */

   return se->tag;
}

int
fe_args (int argc, char *argv[])
{
   if (argc > 1)
   {
      if (!strcasecmp (argv[1], "--version") || !strcasecmp (argv[1], "-v"))
      {
         puts (VERSION);
         return 0;
      }
   }
   return 1;
}

void
fe_init (void)
{
   se_list = 0;
   se_list_count = 0;
   prefs.autosave = 0;
   prefs.use_server_tab = 0;
   prefs.autodialog = 0;
}

void
fe_main (void)
{
   socketevent *se;
   fd_set rd, wd, ex;
   GSList *list;

   while (!done)
   {
      FD_ZERO (&rd);
      FD_ZERO (&wd);
      FD_ZERO (&ex);

      list = se_list;
      while (list)
      {
         se = (socketevent *) list->data;
         if (se->rread)
            FD_SET (se->sok, &rd);
         if (se->wwrite)
            FD_SET (se->sok, &wd);
         if (se->eexcept)
            FD_SET (se->sok, &ex);
         list = list->next;
      }

      FD_SET (STDIN_FILENO, &rd);  /* for reading keyboard */

      select (FD_SETSIZE, &rd, &wd, &ex, 0);

      if (FD_ISSET (STDIN_FILENO, &rd))
         read_stdin ();

      list = se_list;
      while (list)
      {
         se = (socketevent *) list->data;
	      list = list -> next;
         if (se->rread && FD_ISSET (se->sok, &rd))
         {
            se->callback (se->userdata, se->sok);
         } else if (se->wwrite && FD_ISSET (se->sok, &wd))
         {
            se->callback (se->userdata, se->sok);
         } else if (se->eexcept && FD_ISSET (se->sok, &ex))
         {
            se->callback (se->userdata, se->sok);
         }
      }

   }
}

void
fe_exit (void)
{
   done = TRUE;
}

void
fe_new_server (struct server *serv)
{
   serv->gui = malloc (4);
}

void
fe_message (char *msg, int wait)
{
   puts (msg);
}

struct session *
fe_new_window_popup (char *target, struct server *serv)
{
   strcpy (((struct session *) sess_list->data)->channel, target);
   return sess_list->data;
}

void
fe_close_window (struct session *sess)
{
   kill_session_callback (sess);
   done = TRUE;
}

void
fe_beep (void)
{
   putchar (7);
}

void 
fe_add_rawlog (struct server *serv, char *text, int outbound)
{
}
void 
fe_set_topic (struct session *sess, char *topic)
{
}
void 
fe_cleanup (void)
{
}
void 
fe_set_hilight (struct session *sess)
{
}
void 
fe_update_mode_buttons (struct session *sess, char mode, char sign)
{
}
void 
fe_update_channel_key (struct session *sess)
{
}
void 
fe_update_channel_limit (struct session *sess)
{
}
int 
fe_is_chanwindow (struct server *serv)
{
   return 0;
}
void 
fe_add_chan_list (struct server *serv, char *chan, char *users, char *topic)
{
}
void 
fe_chan_list_end (struct server *serv)
{
}
void 
fe_notify_update (char *name)
{
}
void 
fe_text_clear (struct session *sess)
{
}
void 
fe_progressbar_start (struct session *sess)
{
}
void 
fe_progressbar_end (struct session *sess)
{
}
void 
fe_userlist_insert (struct session *sess, struct user *newuser, int row)
{
}
void 
fe_userlist_remove (struct session *sess, struct user *user)
{
}
void 
fe_userlist_move (struct session *sess, struct user *user, int new_row)
{
}
void 
fe_userlist_numbers (struct session *sess)
{
}
void 
fe_userlist_clear (struct session *sess)
{
}
void 
fe_dcc_update_recv_win (void)
{
}
void 
fe_dcc_update_send_win (void)
{
}
void 
fe_dcc_update_chat_win (void)
{
}
void 
fe_dcc_update_send (struct DCC *dcc)
{
}
void 
fe_dcc_update_recv (struct DCC *dcc)
{
}
void 
fe_clear_channel (struct session *sess)
{
}
void 
fe_session_callback (struct session *sess)
{
}
void 
fe_server_callback (struct server *serv)
{
}
void 
fe_checkurl (char *text)
{
}
void 
fe_pluginlist_update (void)
{
}
void 
fe_buttons_update (struct session *sess)
{
}
void 
fe_dcc_send_filereq (struct session *sess, char *nick)
{
}
void 
fe_set_channel (struct session *sess)
{
}
void 
fe_set_title (struct session *sess)
{
}
void 
fe_set_nonchannel (struct session *sess, int state)
{
}
void 
fe_set_nick (struct server *serv, char *newnick)
{
}
void 
fe_change_nick (struct server *serv, char *nick, char *newnick)
{
}
void 
fe_ignore_update (int level)
{
}
void 
fe_dcc_open_recv_win (void)
{
}
void 
fe_dcc_open_send_win (void)
{
}
void 
fe_dcc_open_chat_win (void)
{
}
int
fe_is_confmode (struct session *sess)
{
   return 0;
}
void
fe_userlist_hide (session *sess)
{
}
char *
fe_buffer_get (session *sess)
{
   return NULL;
}
