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
#include "../common/xchat.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#include <gdk/gdkkeysyms.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>


extern GdkColor colors[];


static int
close_rawlog (GtkWidget * wid, struct server *serv)
{
   serv->gui->rawlog_window = 0;
   return 0;
}

static void
rawlog_save (struct server *serv, void *unused, char *file)
{
   char *buf;
   int fh = -1;

   if (file)
   {
      if (serv->gui->rawlog_window)
         fh = open (file, O_TRUNC | O_WRONLY | O_CREAT, 0600);
      if (fh != -1)
      {
         buf = gtk_editable_get_chars ((GtkEditable *) serv->gui->rawlog_textlist, 0, -1);
         if (buf)
         {
            write (fh, buf, strlen (buf));
            g_free (buf);
         }
         close (fh);
      }
      free (file);
   }
}

static int
rawlog_keypress (GtkWidget * wid, GdkEventKey * event, struct server *serv)
{
   if (event->keyval == GDK_s && event->state & GDK_MOD1_MASK)
   {
      gtkutil_file_req ("Save rawlog", rawlog_save, serv, NULL, TRUE);
      return FALSE;
   }
   return TRUE;
}

void
open_rawlog (struct server *serv)
{
   GtkWidget *hbox, *vscrollbar;
   char tbuf[256];

   if (serv->gui->rawlog_window)
   {
      gdk_window_show (serv->gui->rawlog_window->window);
      return;
   }
   snprintf (tbuf, sizeof tbuf, "X-Chat: Rawlog (%s) [alt-s to save]", serv->servername);
   serv->gui->rawlog_window = gtkutil_window_new (tbuf, "rawlog", 450, 100,
                                                  close_rawlog, serv, TRUE);

   hbox = gtk_hbox_new (FALSE, 0);
   gtk_container_add (GTK_CONTAINER (serv->gui->rawlog_window), hbox);
   gtk_widget_show (hbox);

   serv->gui->rawlog_textlist = gtk_text_new (0, 0);
   gtk_text_set_word_wrap (GTK_TEXT (serv->gui->rawlog_textlist), TRUE);
   gtk_text_set_editable (GTK_TEXT (serv->gui->rawlog_textlist), FALSE);
   gtk_container_add (GTK_CONTAINER (hbox), serv->gui->rawlog_textlist);
   gtk_signal_connect (GTK_OBJECT (serv->gui->rawlog_textlist), "key_press_event",
                       GTK_SIGNAL_FUNC (rawlog_keypress), serv);
   gtk_widget_show (serv->gui->rawlog_textlist);
   gtk_widget_grab_focus (serv->gui->rawlog_textlist);

   vscrollbar = gtk_vscrollbar_new (GTK_TEXT (serv->gui->rawlog_textlist)->vadj);
   gtk_box_pack_start (GTK_BOX (hbox), vscrollbar, FALSE, FALSE, 0);
   gtk_widget_show (vscrollbar);

   gtk_widget_show (serv->gui->rawlog_window);
}

void
fe_add_rawlog (struct server *serv, char *text, int outbound)
{
   int scroll = FALSE;
   GtkAdjustment *adj;

   if (serv->gui->rawlog_window)
   {
      adj = (GTK_TEXT (serv->gui->rawlog_textlist))->vadj;
      if (adj->value == adj->upper - adj->lower - adj->page_size)
         scroll = TRUE;

      gtk_text_freeze (GTK_TEXT (serv->gui->rawlog_textlist));
      if (outbound)
         gtk_text_insert (GTK_TEXT (serv->gui->rawlog_textlist), 0, &colors[3], 0, "<< ", 3);
      else
         gtk_text_insert (GTK_TEXT (serv->gui->rawlog_textlist), 0, &colors[4], 0, ">> ", 3);
      gtk_text_insert (GTK_TEXT (serv->gui->rawlog_textlist), 0, 0, 0, text, -1);
      if (!outbound)
         gtk_text_insert (GTK_TEXT (serv->gui->rawlog_textlist), 0, 0, 0, "\n", 1);
      gtk_text_thaw (GTK_TEXT (serv->gui->rawlog_textlist));

      if (scroll)
         gtk_adjustment_set_value (adj, adj->upper - adj->lower - adj->page_size);
   }
}
