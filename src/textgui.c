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
#include "xchat.h"
#include "xtext.h"

extern struct xchatprefs prefs; 
extern int timecat (char *buf);

static void
PrintTextLine (GtkWidget *textwidget, unsigned char *text, int len)
{
  char *tab;
  int leftlen;
  session *sess;
  
  if (len == -1)
    len = strlen (text);
  
  if (len == 0)
    len = 1;
  
  sess = gtk_object_get_user_data (GTK_OBJECT (textwidget));
  
  tab = strchr (text, '\t');
  if (tab && (unsigned long)tab < (unsigned long)(text + len))
    {
      leftlen = (unsigned long)tab - (unsigned long)text;
      gtk_xtext_append_indent (GTK_XTEXT (textwidget),
			       text, leftlen,
			       tab+1, len - (leftlen + 1));
    } else
      gtk_xtext_append_indent (GTK_XTEXT (textwidget), 0, 0, text, len);
}

void
PrintTextRaw (GtkWidget * textwidget, unsigned char *text)
{
   char *cr;

   cr = strchr (text, '\n');
   if (cr)
   {
      while (1)
      {
         PrintTextLine (textwidget, text, (unsigned long)cr - (unsigned long)text);
         text = cr + 1;
         if (*text == 0)
            break;
         cr = strchr (text, '\n');
         if (!cr)
         {
            PrintTextLine (textwidget, text, -1);
            break;
         }
      }
   } else
   {
      PrintTextLine (textwidget, text, -1);
   }
}
