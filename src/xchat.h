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

#include "../config.h"

#ifndef VERSION               /* for broken autoconf */
#define VERSION "0.3.3"
#endif

#ifndef PACKAGE
#define PACKAGE "NF-chat"
#endif

#ifndef PREFIX
#define PREFIX "/usr"
#endif

#include <glib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef HAVE_SNPRINTF
#define snprintf g_snprintf 
#endif

#ifdef USE_DEBUG
#define malloc xchat_malloc
#define realloc xchat_realloc
#define free xchat_free
#define strdup xchat_strdup
#endif

#ifdef SOCKS
#ifdef __sgi
#include <sys/time.h>
#define INCLUDE_PROTOTYPES 1
#endif
#include <socks.h>
#endif

#ifdef __EMX__ /* for o/s 2 */
#define OFLAGS O_BINARY
#else
#define OFLAGS 0
#endif

#define	FONTNAMELEN	127
#define	PATHLEN	255

struct xchatprefs
{
  char nick1[64];
  char nick2[64];
  char nick3[64];
  char realname[127];
  char username[127];
  char quitreason[256];
  char font_normal[FONTNAMELEN + 1];
  char background[PATHLEN + 1];
  char bluestring[64];
  char server[127];
  char serverpass[86];
  char channel[127];
  char hostname[127];
  
  int tint_red;
  int tint_green;
  int tint_blue;
  int port;
  int max_auto_indent;
  int indent_pixels;
  int max_lines;
  int mainwindow_left;
  int mainwindow_top;
  int mainwindow_width;
  int mainwindow_height;
  int recon_delay;
  int rejoin_delay;
  int userlist_sort;
  int bt_color;
  unsigned long local_ip;
  
  unsigned int autoreconnect;
  unsigned int autoreconnectonfail;
  unsigned int invisible;
  unsigned int skipmotd;
  unsigned int nickcompletion;
  unsigned int transparent;
  unsigned int tint;
  unsigned int use_fontset;
  unsigned int allow_nick;
};

struct session_t
{
   GSList *userlist;
   char channel[202];
   char waitchannel[202];       /* waiting to join this channel */
   char nick[202];
   char channelkey[64];         /* XXX correct max length? */
   int limit;			  /* channel user limit */
   char lastnick[64];           /* last nick you /msg'ed */
   int ops;                     /* num. of ops in channel */
   int total;                   /* num. of users in channel */
   char *quitreason;
   struct session_gui *gui;	     /* initialized by fe_new_window */
   int end_of_names:1;
   int doing_who:1;             /* /who sent on this channel */
};

typedef struct session_t session_t;

struct server_t
{
   int port;
   int sok;
   int childread;
   int childwrite;
   int childpid;
   int iotag;
   int bartag;
   char hostname[128];          /* real ip number */
   char servername[128];        /* what the server says is its name */
   char password[86];
   char nick[64];
   char linebuf[2048];
   long pos;
   int nickcount;

   GSList *outbound_queue;
   time_t last_send;
   int bytes_sent;              /* this second only */

   int motd_skipped:1;
   int connected:1;
   int connecting:1;
   int no_login:1;
   int skip_next_who:1;         /* used for "get my ip from server" */
   int doing_who:1;             /* /dns has been done */
   int end_of_motd:1;		/* end of motd reached (logged in) */
   int sent_quit:1;        /* sent a QUIT already? */
};

typedef struct server_t server_t;

extern server_t *server;
extern session_t *session;

typedef int (*cmd_callback) (char *tbuf, char *word[], char *word_eol[]);

typedef struct commands_t
{
   char *name;
   cmd_callback callback;
   char needserver;
   char needchannel;
   char *help;
} commands_t;

extern commands_t **cmds;
