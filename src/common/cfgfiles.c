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
extern void PrintText (struct session *sess, unsigned char *text);


void
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

void
list_free (GSList ** list)
{
   void *data;
   while (*list)
   {
      data = (void *) (*list)->data;
      free (data);
      *list = g_slist_remove (*list, data);
   }
}

int
list_delentry (GSList ** list, char *name)
{
   struct popup *pop;
   GSList *alist = *list;

   while (alist)
   {
      pop = (struct popup *) alist->data;
      if (!strcasecmp (name, pop->name))
      {
         *list = g_slist_remove (*list, pop);
         return 1;
      }
      alist = alist->next;
   }
   return 0;
}

char *
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
         /*while (*cfg == ' ' || *cfg == '=')
            cfg++;*/
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

void
cfg_put_int (int fh, int value, char *var)
{
   char buf[400];

   if (value == -1)
      value = 1;

   snprintf (buf, sizeof buf, "%s = %d\n", var, value);
   write (fh, buf, strlen (buf));
}

int
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

int
cfg_get_int (char *cfg, char *var)
{
   char str[128];

   if (!cfg_get_str (cfg, var, str))
      return 0;

   return atoi (str);
}

char *xdir = 0;

char *
get_xdir (void)
{
   if (!xdir)
   {
      xdir = malloc (strlen (g_get_home_dir ()) + 8);
      sprintf (xdir, "%s/.xchat", g_get_home_dir ());
   }
   return xdir;
}

void
check_prefs_dir (void)
{
   char *xdir = get_xdir ();
   if (access (xdir, F_OK) != 0)
   {
      if (mkdir (xdir, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
         fe_message ("Cannot create ~/.xchat", FALSE);
   }
}

char *
default_file (void)
{
   static char *dfile = 0;

   if (!dfile)
   {
      dfile = malloc (strlen (get_xdir ()) + 12);
      sprintf (dfile, "%s/xchat.conf", get_xdir ());
   }
   return dfile;
}

static struct prefs vars[] = {
{"nickname1",           PREFS_OFFSET(nick1),             TYPE_STR},
{"nickname2",           PREFS_OFFSET(nick2),             TYPE_STR},
{"nickname3",           PREFS_OFFSET(nick3),             TYPE_STR},
{"realname",            PREFS_OFFSET(realname),          TYPE_STR},
{"username",            PREFS_OFFSET(username),          TYPE_STR},
{"awayreason",          PREFS_OFFSET(awayreason),        TYPE_STR},
{"quitreason",          PREFS_OFFSET(quitreason),        TYPE_STR},
{"font_normal",         PREFS_OFFSET(font_normal),       TYPE_STR},
{"font_dialog_normal",  PREFS_OFFSET(dialog_font_normal),TYPE_STR},
{"sounddir",            PREFS_OFFSET(sounddir),          TYPE_STR},
{"soundcmd",            PREFS_OFFSET(soundcmd),          TYPE_STR},
{"background_pic",      PREFS_OFFSET(background),        TYPE_STR},
{"background_dialog_pic",PREFS_OFFSET(background_dialog),TYPE_STR},
{"doubleclickuser",     PREFS_OFFSET(doubleclickuser),   TYPE_STR},
{"bluestring",          PREFS_OFFSET(bluestring),        TYPE_STR},
{"dnsprogram",          PREFS_OFFSET(dnsprogram),        TYPE_STR},
{"hostname",            PREFS_OFFSET(hostname),          TYPE_STR},
{"trans_file",          PREFS_OFFSET(trans_file),        TYPE_STR},
{"logmask",             PREFS_OFFSET(logmask),           TYPE_STR},
{"autosave",            PREFS_OFFINT(autosave),          TYPE_BOOL},
{"autodialog",          PREFS_OFFINT(autodialog),        TYPE_BOOL},
{"autoreconnect",       PREFS_OFFINT(autoreconnect),     TYPE_BOOL},
{"autoreconnectonfail", PREFS_OFFINT(autoreconnectonfail),TYPE_BOOL},
{"invisible",           PREFS_OFFINT(invisible),         TYPE_BOOL},
{"servernotice",        PREFS_OFFINT(servernotice),      TYPE_BOOL},
{"wallops",             PREFS_OFFINT(wallops),           TYPE_BOOL},
{"skipmotd",            PREFS_OFFINT(skipmotd),          TYPE_BOOL},
{"autorejoin",          PREFS_OFFINT(autorejoin),        TYPE_BOOL},
{"colorednicks",        PREFS_OFFINT(colorednicks),      TYPE_BOOL},
{"nochanmodebuttons",   PREFS_OFFINT(nochanmodebuttons), TYPE_BOOL},
{"nouserlistbuttons",   PREFS_OFFINT(nouserlistbuttons), TYPE_BOOL},
{"nickcompletion",      PREFS_OFFINT(nickcompletion),    TYPE_BOOL},
{"tabchannels",         PREFS_OFFINT(tabchannels),       TYPE_BOOL},
{"nopaned",             PREFS_OFFINT(nopaned),           TYPE_BOOL},
{"transparent",         PREFS_OFFINT(transparent),       TYPE_BOOL},
{"tint",                PREFS_OFFINT(tint),              TYPE_BOOL},
{"dialog_transparent",  PREFS_OFFINT(dialog_transparent),TYPE_BOOL},
{"dialog_tint",         PREFS_OFFINT(dialog_tint),       TYPE_BOOL},
{"stripcolor",          PREFS_OFFINT(stripcolor),        TYPE_BOOL},
{"timestamp",           PREFS_OFFINT(timestamp),         TYPE_BOOL},
{"skipserverlist",      PREFS_OFFINT(skipserverlist),    TYPE_BOOL},
{"filterbeep",          PREFS_OFFINT(filterbeep),        TYPE_BOOL},
{"hilight_notify",      PREFS_OFFINT(hilitenotify),      TYPE_BOOL},
{"beep_msg",            PREFS_OFFINT(beepmsg),           TYPE_BOOL},
{"priv_msg_tabs",       PREFS_OFFINT(privmsgtab),        TYPE_BOOL},
{"logging",             PREFS_OFFINT(logging),           TYPE_BOOL},
{"newtabs_to_front",    PREFS_OFFINT(newtabstofront),    TYPE_BOOL},
{"hide_version",        PREFS_OFFINT(hidever),           TYPE_BOOL},
{"raw_modes",           PREFS_OFFINT(raw_modes),         TYPE_BOOL},
{"show_away_once",      PREFS_OFFINT(show_away_once),    TYPE_BOOL},
{"show_away_message",   PREFS_OFFINT(show_away_message), TYPE_BOOL},
{"nouserhost",          PREFS_OFFINT(nouserhost),        TYPE_BOOL},
{"autosaveurl",         PREFS_OFFINT(autosave_url),      TYPE_BOOL},
{"ip_from_server",      PREFS_OFFINT(ip_from_server),    TYPE_BOOL},
{"use_server_tab",      PREFS_OFFINT(use_server_tab),    TYPE_BOOL},
{"style_inputbox",      PREFS_OFFINT(style_inputbox),    TYPE_BOOL},
{"windows_as_tabs",     PREFS_OFFINT(windows_as_tabs),   TYPE_BOOL},
{"use_fontset",         PREFS_OFFINT(use_fontset),       TYPE_BOOL},
{"indent_nicks",        PREFS_OFFINT(indent_nicks),      TYPE_BOOL},
{"show_separator",      PREFS_OFFINT(show_separator),    TYPE_BOOL},
{"thin_separator",      PREFS_OFFINT(thin_separator),    TYPE_BOOL},
{"dialog_indent_nicks", PREFS_OFFINT(dialog_indent_nicks),TYPE_BOOL},
{"dialog_show_separator",PREFS_OFFINT(dialog_show_separator),TYPE_BOOL},
{"use_trans",           PREFS_OFFINT(use_trans),         TYPE_BOOL},
{"treeview",            PREFS_OFFINT (treeview),         TYPE_BOOL},
{"auto_indent",         PREFS_OFFINT(auto_indent),       TYPE_BOOL},
{"wordwrap",            PREFS_OFFINT(wordwrap),          TYPE_BOOL},
{"dialog_wordwrap",     PREFS_OFFINT(dialog_wordwrap),   TYPE_BOOL},
{"mail_check",          PREFS_OFFINT(mail_check),        TYPE_BOOL},
{"double_buffer",       PREFS_OFFINT(double_buffer),     TYPE_BOOL},
{"notify_timeout",      PREFS_OFFINT(notify_timeout),    TYPE_INT},
{"nu_color",            PREFS_OFFINT(nu_color),          TYPE_INT},
{"mainwindow_left",     PREFS_OFFINT(mainwindow_left),   TYPE_INT},
{"mainwindow_top",      PREFS_OFFINT(mainwindow_top),    TYPE_INT},
{"mainwindow_width",    PREFS_OFFINT(mainwindow_width),  TYPE_INT},
{"mainwindow_height",   PREFS_OFFINT(mainwindow_height), TYPE_INT},
{"max_lines",           PREFS_OFFINT(max_lines),         TYPE_INT},
{"bt_color",            PREFS_OFFINT(bt_color),          TYPE_INT},
{"reconnect_delay",     PREFS_OFFINT(recon_delay),       TYPE_INT},
{"ban_type",            PREFS_OFFINT(bantype),           TYPE_INT},
{"userlist_sort",       PREFS_OFFINT(userlist_sort),     TYPE_INT},
{"tint_red",            PREFS_OFFINT(tint_red),          TYPE_INT},
{"tint_green",          PREFS_OFFINT(tint_green),        TYPE_INT},
{"tint_blue",           PREFS_OFFINT(tint_blue),         TYPE_INT},
{"dialog_tint_red",     PREFS_OFFINT(dialog_tint_red),   TYPE_INT},
{"dialog_tint_green",   PREFS_OFFINT(dialog_tint_green), TYPE_INT},
{"dialog_tint_blue",    PREFS_OFFINT(dialog_tint_blue),  TYPE_INT},
{"indent_pixels",       PREFS_OFFINT(indent_pixels),     TYPE_INT},
{"dialog_indent_pixels",PREFS_OFFINT(dialog_indent_pixels),TYPE_INT},
{"max_auto_indent",     PREFS_OFFINT (max_auto_indent),  TYPE_INT},
{"tabs_position",       PREFS_OFFINT (tabs_position),    TYPE_INT},
{0, 0, 0},
};

void
load_config (void)
{
   struct stat st;
   char *cfg, *username;
   int res, val, i, fh;

   memset (&prefs, 0, sizeof (struct xchatprefs));
   /* Just for ppl upgrading --AGL */
   strcpy (prefs.logmask, "%s,%c.xchatlog");
   prefs.auto_indent = 1;
   prefs.max_auto_indent = 112;
   prefs.mail_check = 1;
   prefs.show_separator = 1;
   prefs.dialog_show_separator = 1;
   prefs.double_buffer = 1;

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
      prefs.show_away_once = 1;
      prefs.show_away_message = 1;
      prefs.indent_pixels = 80;
      prefs.dialog_indent_pixels = 80;
      prefs.indent_nicks = 1;
      prefs.dialog_indent_nicks = 1;
      prefs.thin_separator = 1;
      prefs.autosave = 1;
      prefs.autodialog = 1;
      prefs.autorejoin = 1;
      prefs.autoreconnect = 1;
      prefs.recon_delay = 2;
      prefs.tabchannels = 1;
      prefs.use_server_tab = 1;
      prefs.privmsgtab = 1;
      prefs.nickcompletion = 1;
      prefs.style_inputbox = 1;
      prefs.nu_color = 4;
      prefs.bt_color = 8;
      prefs.max_lines = 300;
      prefs.mainwindow_width = 601;
      prefs.mainwindow_height = 422;
      prefs.notify_timeout = 15;
      prefs.tint_red =
      prefs.tint_green =
      prefs.tint_blue =
      prefs.dialog_tint_red =
      prefs.dialog_tint_green =
      prefs.dialog_tint_blue = 208;
      strcpy (prefs.nick1, username);
      strcpy (prefs.nick2, username);
      strcat (prefs.nick2, "_");
      strcpy (prefs.nick3, username);
      strcat (prefs.nick3, "__");
      strcpy (prefs.realname, username);
      strcpy (prefs.username, username);
      strcpy (prefs.doubleclickuser, "/QUOTE WHOIS %s");
      strcpy (prefs.awayreason, "I'm busy");
      strcpy (prefs.quitreason, "[x]chat");
      strcpy (prefs.font_normal, "-b&h-lucidatypewriter-medium-r-normal-*-*-120-*-*-m-*-*-*");
      strcpy (prefs.dialog_font_normal, prefs.font_normal);
      strcpy (prefs.sounddir, g_get_home_dir ());
      strcpy (prefs.soundcmd, "play");
      strcpy (prefs.dnsprogram, "host");
      strcpy (prefs.logmask, "%s,%c.xchatlog");
   }
   if (prefs.mainwindow_height < 106)
      prefs.mainwindow_height = 106;
   if (prefs.mainwindow_width < 116)
      prefs.mainwindow_width = 116;
   if (prefs.notify_timeout > 1000)
      fprintf (stderr, "*** X-Chat: fix your notify timeout! It's too high\n");
   if (prefs.indent_pixels < 1)
      prefs.indent_pixels = 80;
   if (prefs.dialog_indent_pixels < 1)
      prefs.dialog_indent_pixels = 80;
}

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

static void
set_list (session *sess, char *tbuf)
{
   struct prefs tmp_pref;
   struct prefs sorted_vars[sizeof (vars) / sizeof (tmp_pref)];
   int i, j;

   memcpy (&sorted_vars[0], &vars[0], sizeof (vars));

   /* lame & dirty bubble sort */
   i = 0;
   while (sorted_vars[i].type != 0)
   {
      j = 0;
      while (sorted_vars[j].type != 0)
      {
         if (strcmp (sorted_vars[j].name, sorted_vars[i].name) > 0)
         {
            memcpy (&tmp_pref, &sorted_vars[j], sizeof (tmp_pref));
            memcpy (&sorted_vars[j], &sorted_vars[i], sizeof (tmp_pref));
            memcpy (&sorted_vars[i], &tmp_pref, sizeof (tmp_pref));
         }
         j++;
      }
      i++;
   }      

   i = 0;
   do
   {
      switch (sorted_vars[i].type)
      {
      case TYPE_STR:
         sprintf(tbuf, "%s \0033=\017 %s\n", sorted_vars[i].name, (char *)&prefs + sorted_vars[i].offset);
         break;
      case TYPE_INT:
      case TYPE_BOOL:
         sprintf(tbuf, "%s \0033=\017 %d\n", sorted_vars[i].name, *((int *)&prefs + sorted_vars[i].offset));
         break;
      }
      PrintText (sess, tbuf);
      i++;
   }
   while (sorted_vars[i].type != 0);
}

int
cmd_set (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   int i = 0;
   char *var = word[2];
   char *val = word_eol[3];

   if (!*var)
   {
      set_list (sess, tbuf);
      return TRUE;
   }

   if (*val == '=')
      val++;

   do
   {
      if (!strcasecmp (var, vars[i].name))
      {
         switch (vars[i].type)
         {
         case TYPE_STR:
            if (*val)
            {
               strcpy ((char *)&prefs + vars[i].offset, val);
               sprintf (tbuf, "%s set to: %s\n", var, val);
            } else
            {
               sprintf (tbuf, "%s (string) has value: %s\n", var, (char *)&prefs + vars[i].offset);
            }
            PrintText (sess, tbuf);
            break;
         case TYPE_INT:
         case TYPE_BOOL:
            if (*val)
            {
               if (vars[i].type == TYPE_BOOL)
               {
                  if (atoi (val))
                     *((int *)&prefs + vars[i].offset) = 1;
                  else
                     *((int *)&prefs + vars[i].offset) = 0;
               } else
               {
                  *((int *)&prefs + vars[i].offset) = atoi (val);
               }
               sprintf (tbuf, "%s set to: %d\n", var, *((int *)&prefs + vars[i].offset));
            } else
            {
               if (vars[i].type == TYPE_BOOL)
                  sprintf (tbuf, "%s (boolean) has value: %d\n", var, *((int *)&prefs + vars[i].offset));
               else
                  sprintf (tbuf, "%s (number) has value: %d\n", var, *((int *)&prefs + vars[i].offset));
            }
            PrintText (sess, tbuf);
            break;

         }
         return TRUE;
      }
      i++;
   }
   while (vars[i].type != 0);

   PrintText (sess, "No such variable.\n");

   return TRUE;
}

