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
#include "../common/xchat.h"
#include "fe-gtk.h"
#include "gtkutil.h"

#ifndef __DATE__
#define __DATE__ "<unknown>"
#endif

extern GdkColor colors[];

extern char *get_cpu_str (int color);

static GtkWidget *about = 0;

static int
about_close (void)
{
   about = 0;
   return 0;
}

void
menu_about (GtkWidget *wid, gpointer sess)
{
   char buf[512];
   GtkWidget *vbox, *label, *hbox;
   GtkStyle *about_style;
   GtkStyle *head_style;

   if (about)
   {
      gdk_window_show (about->window);
      return;
   }
   head_style = gtk_style_new ();
   gdk_font_unref (head_style->font);
   head_style->font = gdk_font_load ("-*-new century schoolbook-bold-r-normal-*-*-240-*-*-*-*-*");
   if (!head_style->font)
      head_style->font = gdk_font_load ("fixed");
   memcpy (head_style->fg, &colors[2], sizeof (GdkColor));

   about_style = gtk_style_new ();
   gdk_font_unref (about_style->font);
   about_style->font = gdk_font_load ("fixed");

   about = gtk_window_new (GTK_WINDOW_DIALOG);
   gtk_window_position (GTK_WINDOW (about), GTK_WIN_POS_CENTER);
   gtk_window_set_title (GTK_WINDOW (about), "About X-Chat");
   gtk_container_set_border_width (GTK_CONTAINER (about), 6);
   gtk_signal_connect (GTK_OBJECT (about), "destroy",
                       GTK_SIGNAL_FUNC (about_close), 0);
   gtk_widget_realize (about);

   vbox = gtk_vbox_new (0, 2);
   gtk_container_add (GTK_CONTAINER (about), vbox);
   gtk_widget_show (vbox);

   label = gtk_entry_new ();
   gtk_entry_set_editable (GTK_ENTRY (label), FALSE);
   gtk_entry_set_text (GTK_ENTRY (label), "X-Chat " VERSION);
   gtk_widget_set_style (label, head_style);
   gtk_style_unref (head_style);
   gtk_container_add (GTK_CONTAINER (vbox), label);
   gtk_widget_show (label);

   label = gtk_label_new (
                     "\n"
                     "  X-Chat "VERSION" Copyright (c) 1998-2000 By Peter Zelezny\n"
                     "    This software comes with NO WARRANTY at all, please\n"
                     "         refer to COPYING file for more details.\n"
      );
   gtk_container_add (GTK_CONTAINER (vbox), label);
   gtk_widget_show (label);

   snprintf (buf, sizeof buf,
             "  X-Chat Project Started ... : Jul 27 1998\n"
             "  This Binary Compiled ..... : " __DATE__ "\n"
             "  GTK+ Version ............. : %d.%d.%d\n"
             "  Currently Running on ..... : %s\n"
             "  Author EMail ............. : zed@linux.com\n"
             "  X-Chat Web Page .......... : http://xchat.org\n",
             gtk_major_version, gtk_minor_version, gtk_micro_version,
             get_cpu_str (0));

   label = gtk_label_new (buf);
   gtk_container_add (GTK_CONTAINER (vbox), label);
   gtk_widget_set_style (label, about_style);
   gtk_style_unref (about_style);
   gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
   gtk_widget_show (label);

   wid = gtk_hseparator_new ();
   gtk_container_add (GTK_CONTAINER (vbox), wid);
   gtk_widget_show (wid);

   hbox = gtk_hbox_new (0, 2);
   gtk_container_add (GTK_CONTAINER (vbox), hbox);
   gtk_widget_show (hbox);

   wid = gtk_button_new_with_label ("  Continue  ");
   gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
   GTK_WIDGET_SET_FLAGS (GTK_WIDGET (wid), GTK_CAN_DEFAULT);
   gtk_box_pack_end (GTK_BOX (hbox), wid, 0, 0, 0);
   gtk_widget_grab_default	(wid);
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
                       GTK_SIGNAL_FUNC (gtkutil_destroy), about);
   gtk_widget_show (wid);

   gtk_widget_show (about);
}
