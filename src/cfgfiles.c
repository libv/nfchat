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

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "xchat.h"
#include "cfgfiles.h"
#include "popup.h" 
#include "fe.h"


extern struct xchatprefs prefs;

extern int buf_get_line (char *, char **, int *, int len);

static void
list_addentry (GSList ** list, char *cmd, char *name)
{
   struct popup *pop = malloc (sizeof (struct popup));
   if (!pop)
      return;
   strncpy (pop->name, name, 81);
   if (!cmd)
      pop->cmd[0] = 0;
   else
      strncpy (pop->cmd, cmd, 255);
   *list = g_slist_append (*list, pop);
} 

void
list_loadconf (char *file, GSList ** list, char *defaultconf)
{
   char cmd[256];
   char name[82];
   char *buf, *ibuf;
   int fh, pnt = 0;
   struct stat st;

   buf = malloc (1000);
   snprintf (buf, 1000, "%s/%s", get_xdir (), file);
   fh = open (buf, O_RDONLY | OFLAGS);
   if (fh == -1)
   {
      fh = open (buf, O_TRUNC | O_WRONLY | O_CREAT | OFLAGS, 0600);
      if (fh != -1)
      {
         write (fh, defaultconf, strlen (defaultconf));
         close (fh);
         free (buf);
         list_loadconf (file, list, defaultconf);
      }
      return;
   }
   free (buf);
   if (fstat (fh, &st) != 0)
   {
      perror ("fstat");
      abort ();
   }
   ibuf = malloc (st.st_size);
   read (fh, ibuf, st.st_size);
   close (fh);

   cmd[0] = 0;
   name[0] = 0;

   while (buf_get_line (ibuf, &buf, &pnt, st.st_size))
   {
      if (*buf != '#')
      {
         if (!strncasecmp (buf, "NAME ", 5))
            strncpy (name, buf + 5, 81);
         if (!strncasecmp (buf, "CMD ", 4))
         {
            strncpy (cmd, buf + 4, 255);
            if (*name)
            {
               list_addentry (list, cmd, name);
               cmd[0] = 0;
               name[0] = 0;
            }
         }
      }
   }
   free (ibuf);
}

static char *
cfg_get_str (char *cfg, char *var, char *dest)
{
   while (1)
   {
      if (!strncasecmp (var, cfg, strlen (var)))
      {
         char *value, t;
         cfg += strlen (var);
         while (*cfg == ' ')
            cfg++;
         if (*cfg == '=')
            cfg++;
         while (*cfg == ' ')
            cfg++;
         value = cfg;
         while (*cfg != 0 && *cfg != '\n')
            cfg++;
         t = *cfg;
         *cfg = 0;
         strcpy (dest, value);
         *cfg = t;
         return cfg;
      }
      while (*cfg != 0 && *cfg != '\n')
         cfg++;
      if (*cfg == 0)
         return 0;
      cfg++;
      if (*cfg == 0)
         return 0;
   }
}

static void
cfg_put_str (int fh, char *var, char *value)
{
   char buf[512];

   snprintf (buf, sizeof buf, "%s = %s\n", var, value);
   write (fh, buf, strlen (buf));
}

static void
cfg_put_int (int fh, int value, char *var)
{
   char buf[400];

   if (value == -1)
      value = 1;

   snprintf (buf, sizeof buf, "%s = %d\n", var, value);
   write (fh, buf, strlen (buf));
}

static int
cfg_get_int_with_result (char *cfg, char *var, int *result)
{
   char str[128];

   if (!cfg_get_str (cfg, var, str))
   {
      *result = 0;
      return 0;
   }

   *result = 1;
   return atoi (str);
}

char *xdir = 0;

char *
get_xdir (void)
{
   if (!xdir)
   {
      xdir = malloc (strlen (g_get_home_dir ()) + 9);
      sprintf (xdir, "%s/.nfchat", g_get_home_dir ());
   }
   return xdir;
}

static void
check_prefs_dir (void)
{
   char *xdir = get_xdir ();
   if (access (xdir, F_OK) != 0)
     if (mkdir (xdir, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
       fprintf (stderr, "Error: Cannot create ~/.nfchat");
}

static char *
default_file (void)
{
   static char *dfile = 0;

   if (!dfile)
   {
      dfile = malloc (strlen (get_xdir ()) + 12);
      sprintf (dfile, "%s/nfchat.conf", get_xdir ());
   }
   return dfile;
}

static struct prefs vars[] = {
{"nickname1",           PREFS_OFFSET(nick1),             TYPE_STR},
{"nickname2",           PREFS_OFFSET(nick2),             TYPE_STR},
{"nickname3",           PREFS_OFFSET(nick3),             TYPE_STR},
{"realname",            PREFS_OFFSET(realname),          TYPE_STR},
{"username",            PREFS_OFFSET(username),          TYPE_STR},
{"quitreason",          PREFS_OFFSET(quitreason),        TYPE_STR},
{"font_normal",         PREFS_OFFSET(font_normal),       TYPE_STR},
{"background_pic",      PREFS_OFFSET(background),        TYPE_STR},
{"bluestring",          PREFS_OFFSET(bluestring),        TYPE_STR},
{"hostname",            PREFS_OFFSET(hostname),          TYPE_STR},
{"autoreconnect",       PREFS_OFFINT(autoreconnect),     TYPE_BOOL},
{"autoreconnectonfail", PREFS_OFFINT(autoreconnectonfail),TYPE_BOOL},
{"invisible",           PREFS_OFFINT(invisible),         TYPE_BOOL},
{"skipmotd",            PREFS_OFFINT(skipmotd),          TYPE_BOOL},
{"autorejoin",          PREFS_OFFINT(autorejoin),        TYPE_BOOL},
{"nickcompletion",      PREFS_OFFINT(nickcompletion),    TYPE_BOOL},
{"tabchannels",         PREFS_OFFINT(tabchannels),       TYPE_BOOL},
{"transparent",         PREFS_OFFINT(transparent),       TYPE_BOOL},
{"tint",                PREFS_OFFINT(tint),              TYPE_BOOL},
{"use_server_tab",      PREFS_OFFINT(use_server_tab),    TYPE_BOOL},
{"use_fontset",         PREFS_OFFINT(use_fontset),       TYPE_BOOL},
{"mainwindow_left",     PREFS_OFFINT(mainwindow_left),   TYPE_INT},
{"mainwindow_top",      PREFS_OFFINT(mainwindow_top),    TYPE_INT},
{"mainwindow_width",    PREFS_OFFINT(mainwindow_width),  TYPE_INT},
{"mainwindow_height",   PREFS_OFFINT(mainwindow_height), TYPE_INT},
{"max_lines",           PREFS_OFFINT(max_lines),         TYPE_INT},
{"bt_color",            PREFS_OFFINT(bt_color),          TYPE_INT},
{"reconnect_delay",     PREFS_OFFINT(recon_delay),       TYPE_INT},
{"tint_red",            PREFS_OFFINT(tint_red),          TYPE_INT},
{"tint_green",          PREFS_OFFINT(tint_green),        TYPE_INT},
{"tint_blue",           PREFS_OFFINT(tint_blue),         TYPE_INT},
{"indent_pixels",       PREFS_OFFINT(indent_pixels),     TYPE_INT},
{"max_auto_indent",     PREFS_OFFINT (max_auto_indent),  TYPE_INT},
{"tabs_position",       PREFS_OFFINT (tabs_position),    TYPE_INT},
{0, 0, 0},
};

int
save_config (void)
{
   int fh, i;

   check_prefs_dir ();

   fh = open (default_file (), O_TRUNC | O_WRONLY | O_CREAT, 0600);
   if (fh == -1)
      return 0;

   cfg_put_str (fh, "version", VERSION);
   i = 0;
   do
   {
      switch (vars[i].type)
      {
      case TYPE_STR:
         cfg_put_str (fh, vars[i].name, (char *) &prefs + vars[i].offset);
         break;
      case TYPE_INT:
      case TYPE_BOOL:
         cfg_put_int (fh, *((int *)&prefs + vars[i].offset), vars[i].name);
      }
      i++;
   }
   while (vars[i].type != 0);

   close (fh);

   return 1;
}

void
load_config (void)
{
   struct stat st;
   char *cfg, *username;
   int res, val, i, fh;

   memset (&prefs, 0, sizeof (struct xchatprefs));
   /* Just for ppl upgrading --AGL */
   prefs.max_auto_indent = 112;

   check_prefs_dir ();
   username = g_get_user_name ();

   fh = open (default_file (), O_RDONLY);
   if (fh != -1)
   {
      fstat (fh, &st);
      cfg = malloc (st.st_size);
      read (fh, cfg, st.st_size);
      close (fh);
      i = 0;
      do
      {
         switch (vars[i].type)
         {
            case TYPE_STR:
               cfg_get_str (cfg, vars[i].name, (char *)&prefs + vars[i].offset);
               break;
            case TYPE_BOOL:
            case TYPE_INT:
               val = cfg_get_int_with_result (cfg, vars[i].name, &res);
               if (res)
                  *((int *)&prefs + vars[i].offset) = val;
               break;
         }
         i++;
      }
      while (vars[i].type != 0);

      free (cfg);

   } else
   {
      /* put in default values, anything left out is automatically zero */
      prefs.indent_pixels = 80;
      prefs.autorejoin = 1;
      prefs.autoreconnect = 1;
      prefs.recon_delay = 2;
      prefs.tabchannels = 1;
      prefs.use_server_tab = 1;
      prefs.nickcompletion = 1;
      prefs.bt_color = 8;
      prefs.max_lines = 50;
      prefs.mainwindow_width = 601;
      prefs.mainwindow_height = 422;
      prefs.tint_red = 0;
      prefs.tint_green = 0;
      prefs.tint_blue = 0;
      strcpy (prefs.nick1, username);
      strcpy (prefs.nick2, username);
      strcat (prefs.nick2, "_");
      strcpy (prefs.nick3, username);
      strcat (prefs.nick3, "__");
      strcpy (prefs.realname, username);
      strcpy (prefs.username, username);
      strcpy (prefs.bluestring, "netforce");
      strcpy (prefs.quitreason, "[nf]chat by NetForce (www.netforce.be)");
      strcpy (prefs.font_normal, "-b&h-lucidatypewriter-medium-r-normal-*-*-120-*-*-m-*-*-*");
      save_config ();
   }
   if (prefs.mainwindow_height < 106)
      prefs.mainwindow_height = 106;
   if (prefs.mainwindow_width < 116)
      prefs.mainwindow_width = 116;
   if (prefs.indent_pixels < 1)
      prefs.indent_pixels = 80;
}
