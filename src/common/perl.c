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

/* perl.c by Erik Scrafford <eriks@chilisoft.com>. */

#include "../../config.h"
#undef PACKAGE

#ifdef USE_PERL

#include <EXTERN.h>
#ifndef _SEM_SEMUN_UNDEFINED
#define HAS_UNION_SEMUN
#endif
#include <perl.h>
#include <XSUB.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#undef PACKAGE
#include <stdio.h>
#include "xchat.h"
#include "cfgfiles.h"
#include "util.h"
#include "fe.h"

extern GSList *sess_list;
extern GSList *serv_list;

extern int is_session (session *sess);
extern int tcp_send_len (struct server *serv, char *buf, int len);
extern int tcp_send (struct server *serv, char *buf);
extern void PrintText (struct session *sess, char *text);
extern struct session *find_session_from_channel (char *chan, struct server *serv);
extern int handle_command (char *cmd, struct session *sess, int history, int);

int perl_load_file (char *script_name);
void perl_init (struct session *default_sess, int autoload);
void perl_end (void);


static PerlInterpreter *my_perl = NULL;
struct session *perl_sess = 0;
extern session *menu_sess;
static int perl_restart;

struct _perl_timeout_handlers
{
   char *handler_name;
   gint iotag;
};

struct _perl_inbound_handlers
{
   char *message_type;
   char *handler_name;
};

struct _perl_command_handlers
{
   char *command_name;
   char *handler_name;
};

struct _perl_print_handlers
{
   char *print_name;
   char *handler_name;
};

struct perlscript
{
   char *name;
   char *version;
   char *shutdowncallback;
};


static GSList *perl_timeout_handlers;
static GSList *perl_inbound_handlers;
static GSList *perl_command_handlers;
static GSList *perl_print_handlers;
static GSList *perl_list;

XS (XS_IRC_register);
XS (XS_IRC_add_message_handler);
XS (XS_IRC_add_command_handler);
XS (XS_IRC_add_print_handler);
XS (XS_IRC_add_timeout_handler);
XS (XS_IRC_print);
XS (XS_IRC_print_with_channel);
XS (XS_IRC_send_raw);
XS (XS_IRC_command);
XS (XS_IRC_command_with_server);
XS (XS_IRC_channel_list);
XS (XS_IRC_server_list);
XS (XS_IRC_user_list);
XS (XS_IRC_user_info);
XS (XS_IRC_get_info);


/* list some script information (handlers etc) */

int 
cmd_scpinfo (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   GSList *handler;

   PrintText (sess, "Registered Scripts:\n");
   handler = perl_list;
   while (handler)
   {
      struct perlscript *scp = handler->data;
      sprintf (tbuf, "  %s %s\n", scp->name, scp->version);
      PrintText (sess, tbuf);
      handler = handler->next;
   }

   PrintText (sess, "Inbound Handlers:\n");
   for (handler = perl_inbound_handlers; handler != NULL; handler = handler->next)
   {
      struct _perl_inbound_handlers *data = handler->data;
      sprintf (tbuf, "  %s\n", data->message_type);
      PrintText (sess, tbuf);
   }

   PrintText (sess, "Command Handlers:\n");
   for (handler = perl_command_handlers; handler != NULL; handler = handler->next)
   {
      struct _perl_command_handlers *data = handler->data;
      sprintf (tbuf, "  %s\n", data->command_name);
      PrintText (sess, tbuf);
   }

   PrintText (sess, "Print Handlers:\n");
   for (handler = perl_print_handlers; handler != NULL; handler = handler->next)
   {
      struct _perl_print_handlers *data = handler->data;
      sprintf (tbuf, "  %s\n", data->print_name);
      PrintText (sess, tbuf);
   }

   return TRUE;
}

int 
cmd_unloadall (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   perl_end ();
   perl_init (sess, FALSE);
   perl_restart = TRUE;
   return TRUE;
}

static char *
escape_quotes (char *buf)
{
   static char *tmp_buf = NULL;
   char *i, *j;

   if (tmp_buf)
      free (tmp_buf);
   tmp_buf = malloc (strlen (buf) * 2 + 1);
   for (i = buf, j = tmp_buf; *i; i++, j++)
   {
      if (*i == '\'' || *i == '\\')
         *j++ = '\\';
      *j = *i;
   }
   *j = '\0';

   return (tmp_buf);
}

static SV *
execute_perl (char *function, char *args)
{
   static char *perl_cmd = NULL;

   if (perl_cmd)
      free (perl_cmd);
   perl_cmd = malloc (strlen (function) + strlen (args) * 2 + 10);
   sprintf (perl_cmd, "&%s('%s')", function, escape_quotes (args));
#ifndef HAVE_PERL_EVAL_PV
   return (perl_eval_pv (perl_cmd, TRUE));
#else
   return (Perl_eval_pv (perl_cmd, TRUE));
#endif
}

int 
perl_load_file (char *script_name)
{
   SV *return_val;
   return_val = execute_perl ("load_file", script_name);
   return SvNV (return_val);
}

static void 
perl_autoload_file (char *script_name)
{
   char tbuf[256];

   snprintf (tbuf, sizeof tbuf, "\002Loading\00302: \017%s\00302...\017\n", file_part (script_name));
   PrintText (0, tbuf);
   perl_load_file (script_name);
}

void 
perl_init (struct session *default_sess, int autoload)
{
   char *perl_args[] = {"", "-e", "0"};
   char load_file[] =
"sub load_file()\n"
"{\n"
"  (my $file_name) = @_;\n"
"  open FH, $file_name or return 2;\n"
"  local($/) = undef;\n"
"  $file = <FH>;\n"
"  close FH;\n"
"  eval $file;\n"
"  eval $file if $@;\n"
"  return 1 if $@;\n"
"  return 0;\n"
"}";

   perl_sess = default_sess;

   my_perl = perl_alloc ();
   perl_construct (my_perl);
   perl_parse (my_perl, NULL, 3, perl_args, NULL);
#ifndef HAVE_PERL_EVAL_PV
   perl_eval_pv (load_file, TRUE);
#else
   Perl_eval_pv (load_file, TRUE);
#endif
   perl_inbound_handlers = 0;
   perl_command_handlers = 0;
   perl_print_handlers = 0;
   perl_list = 0;

   /* load up all the custom IRC perl functions */
   newXS ("IRC::register", XS_IRC_register, "IRC");
   newXS ("IRC::add_message_handler", XS_IRC_add_message_handler, "IRC");
   newXS ("IRC::add_command_handler", XS_IRC_add_command_handler, "IRC");
   newXS ("IRC::add_print_handler", XS_IRC_add_print_handler, "IRC");
   newXS ("IRC::add_timeout_handler", XS_IRC_add_timeout_handler, "IRC");
   newXS ("IRC::print", XS_IRC_print, "IRC");
   newXS ("IRC::print_with_channel", XS_IRC_print_with_channel, "IRC");
   newXS ("IRC::send_raw", XS_IRC_send_raw, "IRC");
   newXS ("IRC::command", XS_IRC_command, "IRC");
   newXS ("IRC::command_with_server", XS_IRC_command_with_server, "IRC");
   newXS ("IRC::channel_list", XS_IRC_channel_list, "IRC");
   newXS ("IRC::server_list", XS_IRC_server_list, "IRC");
   newXS ("IRC::user_list", XS_IRC_user_list, "IRC");
   newXS ("IRC::user_info", XS_IRC_user_info, "IRC");
   newXS ("IRC::get_info", XS_IRC_get_info, "IRC");

   if (autoload)
      for_files (get_xdir (), "*.pl", perl_autoload_file);
}

void 
perl_end (void)
{
   struct perlscript *scp;
   struct _perl_command_handlers *chand;
   struct _perl_inbound_handlers *ihand;
   struct _perl_print_handlers *phand;
   struct _perl_timeout_handlers *thand;

   while (perl_list)
   {
      scp = perl_list->data;
      perl_list = g_slist_remove (perl_list, scp);
      if (scp->shutdowncallback[0])
         execute_perl (scp->shutdowncallback, "");
      free (scp->name);
      free (scp->version);
      free (scp->shutdowncallback);
      free (scp);
   }

   while (perl_command_handlers)
   {
      chand = perl_command_handlers->data;
      perl_command_handlers = g_slist_remove (perl_command_handlers, chand);
      free (chand->command_name);
      free (chand->handler_name);
      free (chand);
   }

   while (perl_print_handlers)
   {
      phand = perl_print_handlers->data;
      perl_print_handlers = g_slist_remove (perl_print_handlers, phand);
      free (phand->print_name);
      free (phand->handler_name);
      free (phand);
   }

   while (perl_inbound_handlers)
   {
      ihand = perl_inbound_handlers->data;
      perl_inbound_handlers = g_slist_remove (perl_inbound_handlers, ihand);
      free (ihand->message_type);
      free (ihand->handler_name);
      free (ihand);
   }

   while (perl_timeout_handlers)
   {
      thand = perl_timeout_handlers->data;
      perl_timeout_handlers = g_slist_remove (perl_timeout_handlers, thand);
      fe_timeout_remove (thand->iotag);
      free (thand->handler_name);
      free (thand);
   }

   if (my_perl != NULL)
   {
      perl_destruct (my_perl);
      perl_free (my_perl);
      my_perl = NULL;
   }
}

int 
perl_inbound (struct session *sess, struct server *serv, char *buf)
{
   GSList *handler;
   struct _perl_inbound_handlers *data;
   char *message_type, *tmp_mtype;
   char *tmp;
   SV *handler_return;

   if (!buf)
      return 0;
   perl_sess = sess;

   tmp_mtype = strdup (buf);
   message_type = tmp_mtype;
   tmp = strchr (message_type, ' ');
   if (tmp)
      *tmp = 0;
   if (message_type[0] == ':')  /* server message */
   {
      message_type = ++tmp;
      tmp = strchr (message_type, ' ');
      *tmp = 0;
   }
   for (handler = perl_inbound_handlers; handler != NULL; handler = handler->next)
   {
      data = handler->data;
      if (!strcmp (message_type, data->message_type) || !strcmp ("INBOUND", data->message_type))
      {
         handler_return = execute_perl (data->handler_name, buf);
         if (SvIV (handler_return))
         {
            if (tmp_mtype)
               free (tmp_mtype);
            return SvIV (handler_return);
         }
      }
   }
   if (tmp_mtype)
      free (tmp_mtype);
   return 0;
}

int 
perl_command (char *cmd, struct session *sess)
{
   GSList *handler;
   struct _perl_command_handlers *data;
   char *command_name;
   char *tmp;
   char *args;
   char nullargs[] = "";
   SV *handler_return;
   int command = FALSE;

   args = NULL;
   perl_sess = sess;
   if (*cmd == '/')
   {
      cmd++;
      command = TRUE;
   }
   command_name = strdup (cmd);
   tmp = strchr (command_name, ' ');
   if (tmp)
   {
      *tmp = 0;
      args = ++tmp;
   }
   if (!args)
      args = nullargs;

   perl_restart = FALSE;

   for (handler = perl_command_handlers; handler != NULL; handler = handler->next)
   {
      data = handler->data;
      if (
            ((!strcasecmp (command_name, data->command_name)) && command)
            ||
            (!command && data->command_name[0] == 0)
         )
      {
         if (!command)
            handler_return = execute_perl (data->handler_name, cmd);
         else
            handler_return = execute_perl (data->handler_name, args);
         if (perl_restart)
         {
            free (command_name);
            return TRUE;
         }
         if (SvIV (handler_return))
         {
            free (command_name);
            return SvIV (handler_return);
         }
      }
   }

   free (command_name);
   return 0;
}

int 
perl_print (char *cmd, struct session *sess, char *b, char *c, char *d, char *e)
{
   GSList *handler;
   struct _perl_print_handlers *data;
   char *args;
   SV *handler_return;

   args = malloc (1);
   *args = 0;
   perl_sess = sess;

   if (b)
   {
      args = realloc (args, strlen (args) + strlen (b) + 2);
      strcat (args, " ");
      strcat (args, b);
   }
   if (c)
   {
      args = realloc (args, strlen (args) + strlen (c) + 2);
      strcat (args, " ");
      strcat (args, c);
   }
   if (d)
   {
      args = realloc (args, strlen (args) + strlen (d) + 2);
      strcat (args, " ");
      strcat (args, d);
   }
   if (e)
   {
      args = realloc (args, strlen (args) + strlen (e) + 2);
      strcat (args, " ");
      strcat (args, e);
   }
   perl_restart = FALSE;
   for (handler = perl_print_handlers; handler != NULL; handler = handler->next)
   {
      data = handler->data;
      if (!strcasecmp (cmd, data->print_name))
      {
         handler_return = execute_perl (data->handler_name, args);
         if (perl_restart)
         {
            free (args);
            return TRUE;
         }
         if (SvIV (handler_return))
         {
            free (args);
            return SvIV (handler_return);
         }
      }
   }

   free (args);
   return 0;
}

static int
perl_timeout (struct _perl_timeout_handlers *handler)
{
   if (perl_sess && !is_session (perl_sess))  /* sanity check */
      perl_sess = menu_sess;

   execute_perl (handler->handler_name, "");
   perl_timeout_handlers = g_slist_remove (perl_timeout_handlers, handler);
   free (handler->handler_name);
   free (handler);

   return 0; /* returning zero removes the timeout handler */
}

/* custom IRC perl functions for scripting */

/* IRC::register (scriptname, version, shutdowncallback, unused)

 *  all scripts should call this at startup
 *
 */

XS (XS_IRC_register)
{
   char *name, *ver, *callback, *unused;
   int junk;
   struct perlscript *scp;
   dXSARGS;
   items = 0;

   name = SvPV (ST (0), junk);
   ver = SvPV (ST (1), junk);
   callback = SvPV (ST (2), junk);
   unused = SvPV (ST (3), junk);

   scp = malloc (sizeof (struct perlscript));
   scp->name = strdup (name);
   scp->version = strdup (ver);
   scp->shutdowncallback = strdup (callback);
   perl_list = g_slist_prepend (perl_list, scp);

   XST_mPV (0, VERSION);
   XSRETURN (1);
}


/* print to main window */
/* IRC::main_print(output) */
XS (XS_IRC_print)
{
   int junk;
   int i;
   char *output;
   dXSARGS;

   /*if (perl_sess)
      { */
   for (i = 0; i < items; ++i)
   {
      output = SvPV (ST (i), junk);
      PrintText (perl_sess, output);
   }
   /*} */

   XSRETURN_EMPTY;
}

/*
 * IRC::print_with_channel( text, channelname, servername )
 *    
 *   The servername is optional, channelname is required.
 *   Returns 1 on success, 0 on fail.
 */

XS (XS_IRC_print_with_channel)
{
   int junk;
   char *output;
   struct session *sess;
   GSList *list = sess_list;
   char *channel, *server;
   dXSARGS;
   items = 0;

   output = SvPV (ST (0), junk);
   channel = SvPV (ST (1), junk);
   server = SvPV (ST (2), junk);

   while (list)
   {
      sess = (struct session *) list->data;
      if (!server || !server[0] || !strcasecmp (server, sess->server->servername))
      {
         if (sess->channel[0])
         {
            if (!strcasecmp (sess->channel, channel))
            {
               PrintText (sess, output);
               XSRETURN (1);
               return;          /* needed or not? */
            }
         }
      }
      list = list->next;
   }

   XSRETURN (0);
}

XS (XS_IRC_get_info)
{
   int junk;
   dXSARGS;
   items = 0;

   if (!perl_sess)
   {
      XST_mPV (0, "Error1");
      XSRETURN (1);
   }
   switch (atoi (SvPV (ST (0), junk)))
   {
   case 0:
      XST_mPV (0, VERSION);
      break;

   case 1:
      XST_mPV (0, perl_sess->server->nick);
      break;

   case 2:
      XST_mPV (0, perl_sess->channel);
      break;

   case 3:
      XST_mPV (0, perl_sess->server->servername);
      break;

   default:
      XST_mPV (0, "Error2");
   }

   XSRETURN (1);
}

/* add handler for messages with message_type(ie PRIVMSG, 400, etc) */
/* IRC::add_message_handler(message_type, handler_name) */
XS (XS_IRC_add_message_handler)
{
   int junk;
   struct _perl_inbound_handlers *handler;
   dXSARGS;
   items = 0;

   handler = malloc (sizeof (struct _perl_inbound_handlers));
   handler->message_type = strdup (SvPV (ST (0), junk));
   handler->handler_name = strdup (SvPV (ST (1), junk));
   perl_inbound_handlers = g_slist_prepend (perl_inbound_handlers, handler);
   XSRETURN_EMPTY;
}

/* add handler for commands with command_name */
/* IRC::add_command_handler(command_name, handler_name) */
XS (XS_IRC_add_command_handler)
{
   int junk;
   struct _perl_command_handlers *handler;
   dXSARGS;
   items = 0;

   handler = malloc (sizeof (struct _perl_command_handlers));
   handler->command_name = strdup (SvPV (ST (0), junk));
   handler->handler_name = strdup (SvPV (ST (1), junk));
   perl_command_handlers = g_slist_prepend (perl_command_handlers, handler);
   XSRETURN_EMPTY;
}

/* add handler for commands with print_name */
/* IRC::add_print_handler(print_name, handler_name) */
XS (XS_IRC_add_print_handler)
{
   int junk;
   struct _perl_print_handlers *handler;
   dXSARGS;
   items = 0;

   handler = malloc (sizeof (struct _perl_print_handlers));
   handler->print_name = strdup (SvPV (ST (0), junk));
   handler->handler_name = strdup (SvPV (ST (1), junk));
   perl_print_handlers = g_slist_prepend (perl_print_handlers, handler);
   XSRETURN_EMPTY;
}

XS (XS_IRC_add_timeout_handler)
{
   int junk;
   long timeout;
   struct _perl_timeout_handlers *handler;
   dXSARGS;
   items = 0;

   handler = malloc (sizeof (struct _perl_timeout_handlers));
   timeout = atol (SvPV (ST (0), junk));
   handler->handler_name = strdup (SvPV (ST (1), junk));
   perl_timeout_handlers = g_slist_prepend (perl_timeout_handlers, handler);
   handler->iotag = fe_timeout_add (timeout, perl_timeout, handler);
   XSRETURN_EMPTY;
}

/* send raw data to server */
/* IRC::send_raw(data) */
XS (XS_IRC_send_raw)
{
   char *data;
   int junk;
   dXSARGS;
   items = 0;

   if (perl_sess)
   {
      data = strdup (SvPV (ST (0), junk));
      tcp_send (perl_sess->server, data);
      free (data);
   }
   XSRETURN_EMPTY;
}

XS (XS_IRC_channel_list)
{
   struct session *sess;
   GSList *list = sess_list;
   int i = 0;
   dXSARGS;
   items = 0;

   while (list)
   {
      sess = (struct session *) list->data;
      if (sess->channel[0])
      {
         XST_mPV (i, sess->channel);
         i++;
         XST_mPV (i, sess->server->servername);
         i++;
         XST_mPV (i, sess->server->nick);
         i++;
      }
      list = list->next;
   }

   XSRETURN (i);
}

XS (XS_IRC_server_list)
{
   struct server *serv;
   GSList *list = serv_list;
   int i = 0;
   dXSARGS;
   items = 0;

   while (list)
   {
      serv = (struct server *) list->data;
      if (serv->connected && serv->servername[0])
      {
         XST_mPV (i, serv->servername);
         i++;
      }
      list = list->next;
   }

   XSRETURN (i);
}


/*

   IRC::user_info( nickname )

 */

XS (XS_IRC_user_info)
{
   int junk;
   struct user *user;
   char *nick;
   dXSARGS;
   items = 0;

   if (perl_sess)
   {
      nick = SvPV (ST (0), junk);
      if (nick[0] == 0)
         nick = perl_sess->server->nick;
      user = find_name (perl_sess, nick);
      if (user)
      {
         XST_mPV (0, user->nick);
         if (user->hostname)
            XST_mPV (1, user->hostname);
         else
            XST_mPV (1, "FETCHING");
         XST_mIV (2, user->op);
         XST_mIV (3, user->voice);
         XSRETURN (4);
      }
   }
   XSRETURN (0);
}

/*

   IRC::user_list( channel, server )

 */

XS (XS_IRC_user_list)
{
   struct user *user;
   struct session *sess;
   char *channel, *server;
   GSList *list = sess_list;
   int i = 0, junk;
   dXSARGS;
   items = 0;

   channel = SvPV (ST (0), junk);
   server = SvPV (ST (1), junk);

   while (list)
   {
      sess = (struct session *) list->data;
      if (!server[0] || !strcasecmp (sess->server->servername, server))
      {
         if (!strcasecmp (sess->channel, channel) && !sess->is_dialog && !sess->is_shell)
         {
            list = sess->userlist;
            while (list)
            {
               user = (struct user *)list->data;
               XST_mPV (i, user->nick);
               i++;
               if (user->hostname)
                  XST_mPV (i, user->hostname);
               else
                  XST_mPV (i, "FETCHING");
               i++;
               XST_mIV (i, user->op);
               i++;
               XST_mIV (i, user->voice);
               i++;
               XST_mPV (i, ":");
               i++;
               list = list->next;
            }
            XSRETURN (i);
         }
      }
      list = list->next;
   }
   XSRETURN (i);
}

/* run internal xchat command */
/* IRC::command(command) */
XS (XS_IRC_command)
{
   char *command;
   int junk;
   dXSARGS;
   items = 0;

   if (perl_sess)
   {
      command = strdup (SvPV (ST (0), junk));
      handle_command (command, perl_sess, FALSE, FALSE);
      free (command);
   }
   XSRETURN_EMPTY;
}

XS (XS_IRC_command_with_server)
{
   GSList *list = serv_list;
   struct server *serv;
   char *server, *command;
   int junk;
   dXSARGS;
   items = 0;

   server = strdup (SvPV (ST (1), junk));

   while (list)
   {
      serv = (struct server *) list->data;
      if (!strcmp (serv->servername, server))
      {
         command = strdup (SvPV (ST (0), junk));
         if (!serv->front_session)
         {
            struct session *sess;
            GSList *list = sess_list;
            /*fprintf(stderr, "*** Perl Error: no front_session\n"); */
            while (list)
            {
               sess = (struct session *) list->data;
               if (sess->server == serv)
               {
                  /*fprintf(stderr, "*** Using %lx instead\n", (unsigned long)sess); */
                  handle_command (command, sess, FALSE, FALSE);
                  break;
               }
               list = list->next;
            }
         } else
            handle_command (command, serv->front_session, FALSE, FALSE);
         free (command);
         free (server);
         XSRETURN_EMPTY;
      }
      list = list->next;
   }

   free (server);
   XSRETURN_EMPTY;
}

#endif
