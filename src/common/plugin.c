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

/* plugin.c by Adam Langley */

#define	PLUGIN_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "xchat.h"
#include "util.h"
#include "plugin.h"
#include "util.h"
#include "fe.h"

#ifdef USE_PLUGIN

extern void notj_msg (struct session *sess);
extern void notc_msg (struct session *sess);
extern void PrintText (struct session *sess, unsigned char *text);
extern char *get_xdir (void);

void unhook_all_by_mod (struct module *mod);
int module_load (char *name, struct session *sess);

struct module *modules;
struct module_cmd_set *mod_cmds;

#endif

int current_signal;
GSList *sigroots[NUM_XP];
void *signal_data;
/*int (*sighandler[NUM_XP]) (void *, void *, void *, void *, void *, char);*/

extern void printevent_setup ();


void
module_autoload (char *mod)
{
#ifdef USE_PLUGIN
   char tbuf[256];

   snprintf (tbuf, sizeof tbuf, "\002Loading\00302: \017%s\00302...\017\n", file_part (mod));
   PrintText (0, tbuf);
   module_load (mod, 0);
#endif
}

void
signal_setup ()
{
   memset (sigroots, 0, NUM_XP * sizeof (void *));
   /*memset (sighandler, 0, NUM_XP * sizeof (void *));*/
   printevent_setup ();
}

int
fire_signal (int s, char *a, char *b, char *c, char *d,
             char *e, char f)
{
   GSList *cur;
   struct xp_signal *sig;
   int flag = 0;
   
   /*current_signal = s;
     if (sighandler[s] == NULL)
     return 0;
     return sighandler[s] (a, b, c, d, e, f);*/
   
   cur = sigroots[s];
   current_signal = s;
   while (cur) {
      sig = cur->data;

      signal_data = sig->data;
      if (sig->callback (a, b, c, d, e, f))
	 flag = 1;
      
      cur = cur->next;
   }

   return flag;
}

void
unhook_signal (struct xp_signal *sig)
{
   /*if (sig->next != NULL)
     sig->next->last = sig->last;
     if (sig->last != NULL)
     {
     sig->last->next = sig->next;
     if (sig->next != NULL)
     *(sig->last->naddr) = sig->next->callback;
     else
     *(sig->last->naddr) = NULL;
     } else
     {
     if (sig->next != NULL)
     sighandler[sig->signal] = sig->next->callback;
     else
     sighandler[sig->signal] = NULL;
     }
     if (sig == sigroots[sig->signal])
     sigroots[sig->signal] = NULL;
   */

   g_assert (sig);
   /*g_assert (sig->signal > 0);*/
   /* if sig->signal == -1 then it is an textevent */
   sigroots[sig->signal] = g_slist_remove (sigroots[sig->signal], sig);
}

int
hook_signal (struct xp_signal *sig)
{
   /*if (sig->signal > NUM_XP || sig->signal < 0)
     return 1;
     if (sig->naddr == NULL || sig->callback == NULL)
     return 2;
     #ifdef USE_PLUGIN
     if (sig->mod == NULL)
     return 3;
     #endif
     
     sig->next = sigroots[sig->signal];
     if (sigroots[sig->signal] != NULL)
     {
     sigroots[sig->signal]->last = sig;
     *(sig->naddr) = sigroots[sig->signal]->callback;
     } else
     *(sig->naddr) = NULL;
     
     sig->last = NULL;
     sighandler[sig->signal] = sig->callback;
     sigroots[sig->signal] = sig;
   */

   g_assert (sig);
   g_assert (sig->callback);
   
   sigroots[sig->signal] = g_slist_prepend (sigroots[sig->signal], sig);
   
   return 0;
}

#ifdef USE_PLUGIN

void
module_add_cmds (struct module_cmd_set *cmds)
{
   if (mod_cmds != NULL)
      mod_cmds->last = cmds;
   cmds->next = mod_cmds;
   cmds->last = NULL;
   mod_cmds = cmds;
}

void
module_rm_cmds (struct module_cmd_set *cmds)
{
   if (cmds->last != NULL)
      cmds->last->next = cmds->next;
   if (cmds->next != NULL)
      cmds->next->last = cmds->last;
   if (cmds == mod_cmds)
      mod_cmds = mod_cmds->next;
}

void
module_rm_cmds_mod (void *mod)
{
   struct module_cmd_set *m;
   int found;

   for (;;)
   {
      found = 0;
      m = mod_cmds;
      for (;;)
      {
         if (m == NULL)
            break;
         if (m->mod == mod)
         {
            module_rm_cmds (m);
            found = 1;
            break;
         }
         m = m->next;
      }
      if (found == 0)
         break;
   }
   return;
}

#ifndef RTLD_NOW                /* OpenBSD lameness */
#ifndef RTLD_LAZY
#define RTLD_NOW 0
#else
#define RTLD_NOW RTLD_LAZY
#endif
#endif

int
module_load (char *name, struct session *sess)
{
   void *handle;
   struct module *m;
   int (*module_init) (int version, struct module * mod,
               struct session * sess);

   handle = dlopen (name, RTLD_NOW);

   if (handle == NULL)
#ifdef HAVE_DLERROR
   {
      PrintText (sess, (char *) dlerror ());
      PrintText (sess, "\n");
#endif
      return 1;
#ifdef HAVE_DLERROR
   }
#endif

   module_init = dlsym (handle, "module_init");
   if (module_init == NULL)
      return 1;
   m = malloc (sizeof (struct module));
   if (module_init (MODULE_IFACE_VER, m, sess) != 0)
   {
      dlclose (handle);
      return 1;
   }
   if (modules != NULL)
      modules->last = m;
   m->handle = handle;
   m->next = modules;
   m->last = NULL;
   modules = m;
   fe_pluginlist_update ();

   return 0;
}

struct module *
module_find (char *name)
{
   struct module *mod;

   mod = modules;
   for (;;)
   {
      if (mod == NULL)
         return NULL;
      if (strcasecmp (name, mod->name) == 0)
         return mod;
      mod = mod->next;
   }
}

int
module_command (char *cmd, struct session *sess, char *tbuf,
       char *word[], char *word_eol[])
{
   struct module_cmd_set *m;
   int i;
   struct commands *cmds;

   m = mod_cmds;

   for (;;)
   {
      if (m == NULL)
         return 1;
      cmds = m->cmds;
      for (i = 0;; i++)
      {
         if (cmds[i].name == NULL)
            break;
         if (strcasecmp (cmd, cmds[i].name) == 0)
         {
            if (cmds[i].needserver && !(sess->server->connected))
            {
               notc_msg (sess);
               return 0;
            }
            if (cmds[i].needchannel && sess->channel[0] == 0)
            {
               notj_msg (sess);
               return 0;
            }
            if (cmds[i].callback (sess, tbuf, word, word_eol) == TRUE)
               PrintText (sess, cmds[i].help);
            return FALSE;
         }
      }
      m = m->next;
   }
   return TRUE;
}

int
module_list (struct session *sess, char *tbuf, char *word[], char *word_eol[])
{
   struct module *mod;
   char buf[2048];

   PrintText (sess, "\nList of currently loaded modules:\n");
   mod = modules;

   for (;;)
   {
      if (mod == NULL)
         break;
      snprintf (buf, 2048, "%s:\t%s\n", mod->name, mod->desc);
      PrintText (sess, buf);
      mod = mod->next;
   }
   PrintText (sess, "End of module list\n\n");

   return TRUE;
}

void
module_unlink_mod (struct module *mod)
{
   if (mod->last != NULL)
      mod->last->next = mod->next;
   if (mod->next != NULL)
      mod->next->last = mod->last;
   if (mod == modules)
      modules = mod->next;
}

int
module_unload (char *name, struct session *sess)
{
   struct module *m;
   void (*module_cleanup) (struct module * mod, struct session * sess);

   m = modules;
   for (;;)
   {
      if (m == NULL)
         break;
      if (!name || strcasecmp (name, m->name) == 0)
      {
         module_cleanup = dlsym (m->handle, "module_cleanup");
         if (module_cleanup != NULL)
            module_cleanup (m, sess);
         module_rm_cmds_mod (m);
         unhook_all_by_mod (m);
         dlclose (m->handle);
         module_unlink_mod (m);
         free (m);
         if (name)
         {
            fe_pluginlist_update ();
            return 0;
         }
      }
      m = m->next;
   }
   fe_pluginlist_update ();
   return 0;
}

void
unhook_all_by_mod (struct module *mod)
{
   int c;
   struct xp_signal *sig;

   for (c = 0; c < NUM_XP; c++)
   {
      if (!sigroots[c])
	 continue;
      for (;;)
      {
	 if (!sigroots[c])
	    break;
	 sig = sigroots[c]->data;
	 if (!sig)
	    break;
         if (sig->mod == mod) {
            unhook_signal (sig);
	    continue;
	 }
	 break;
      }
   }
}

void
module_setup ()
{
   modules = NULL;
   mod_cmds = NULL;
   for_files (get_xdir (), "*.so", module_autoload);
}

#endif
