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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../common/xchat.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#include "../common/util.h"
#include "xtext.h"
#include "../common/themes-common.h"

extern GdkColor colors[];
extern int cfg_get_int_with_result (char *cfg, char *var, int *result);
extern int cfg_get_int (char *cfg, char *var);
extern void cfg_put_int (int fh, int val, char *var);
extern char *get_xdir (void);
extern int load_themeconfig (int themefile);
extern int save_themeconfig (int themefile);

void
palette_alloc (GtkWidget *widget)
{
   int i;

   if (!colors[0].pixel)        /* don't do it again */
   {
      for (i = 0; i < 20; i++)
      {
         colors[i].pixel = (gulong) ((colors[i].red & 0xff00) * 256 +
                                     (colors[i].green & 0xff00) +
                                     (colors[i].blue & 0xff00) / 256);
         if (!gdk_color_alloc (gtk_widget_get_colormap (widget), &colors[i]))
            fprintf (stderr, "*** X-CHAT: cannot alloc colors\n");
      }
   }
}

void
palette_load (void)
{
   int i, fh, res;
   char prefname[128];
   struct stat st;
   char *cfg;
   unsigned long red, green, blue;

   fh = load_themeconfig (THEME_CONF_PALETTE);
   if (fh != -1)
   {
      fstat (fh, &st);
      cfg = malloc (st.st_size);
      if (cfg)
      {
         read (fh, cfg, st.st_size);

         for (i = 0; i < 20; i++)
         {
            snprintf (prefname, sizeof prefname, "color_%d_red ", i);
            red = cfg_get_int (cfg, prefname);

            snprintf (prefname, sizeof prefname, "color_%d_grn ", i);
            green = cfg_get_int (cfg, prefname);

            snprintf (prefname, sizeof prefname, "color_%d_blu ", i);
            blue = cfg_get_int_with_result (cfg, prefname, &res);

            if (res)
            {
               colors[i].red = red;
               colors[i].green = green;
               colors[i].blue = blue;
            }
         }
         free (cfg);
      }
      close (fh);
   }
}