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
#include "../common/xchat.h"
#include "../common/cfgfiles.h"
#include "fe-gtk.h"
#include "themes.h"
#include "gtkutil.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef USE_IMLIB
#include <gdk_imlib.h>
#endif

#include "../pixmaps/theme.xpm"
#include "../pixmaps/globe.xpm"
#include "../pixmaps/op.xpm"
#include "../pixmaps/server.xpm"
#include "../pixmaps/voice.xpm"
#include "../pixmaps/xchat.xpm"

static GtkWidget *themewin = 0;
static GdkBitmap *themeicon_mask;
static GdkPixmap *themeicon_pixmap = 0;
static GtkWidget *select_button;
static GtkWidget *copy_button;
static GtkWidget *remove_button;
static GtkWidget *done_button;
static GtkWidget *notheme_button;
static GtkCList *themelist;
static gint selected_row = 0;

/*
 * create_pixmap_from_file [STATIC]
 *
 * Called from theme_pixmap and uses IMLIB or Gtk functions to render images
 *
 */
static GdkPixmap *
create_pixmap_from_file (GtkWidget *window, GdkBitmap **mask, GtkWidget *style_widget, char *filename)
{
  GdkPixmap *pixmap = 0;
#ifndef USE_IMLIB
  GtkStyle *style;
  if (!style_widget)
    style = gtk_widget_get_default_style ();
  else
    style = gtk_widget_get_style (style_widget);
  pixmap = gdk_pixmap_create_from_xpm (window->window, mask,
				       &style->bg[GTK_STATE_NORMAL], filename);
#else
  gdk_imlib_load_file_to_pixmap(filename, &pixmap, mask);
#endif /*USE_IMLIB*/
  return(pixmap);
}

/*
 * create_pixmap_from_data [STATIC]
 *
 * Called from theme_pixmap and uses IMLIB or Gtk functions to render images
 *
 */
static GdkPixmap *
create_pixmap_from_data (GtkWidget *window, GdkBitmap **mask, GtkWidget *style_widget, char **data)
{
  GdkPixmap *pixmap = 0;
#ifndef USE_IMLIB
  GtkStyle *style;
  if (!style_widget)
    style = gtk_widget_get_default_style ();
  else
    style = gtk_widget_get_style (style_widget);
  pixmap = gdk_pixmap_create_from_xpm_d (window->window, mask,
					 &style->bg[GTK_STATE_NORMAL], data);
#else
  gdk_imlib_data_to_pixmap (data, &pixmap, mask);
#endif /*USE_IMLIB*/
  return(pixmap);
}

/*
 * theme_pixmap [PUBLIC]
 * 
 * Called from each function which has themeable pixmaps
 *
 */
GdkPixmap *
theme_pixmap (GtkWidget *window, GdkBitmap **mask, GtkWidget *style_widget, int theme)
{
  GdkPixmap *pixmap = 0;
  char filename[256];
  GtkStyle *style;

  switch(theme) {
  case THEME_SERVER:
    snprintf (filename, sizeof filename, "%s/theme/server-icon.xpm", get_xdir());
    break;
  case THEME_GROUP:
    snprintf (filename, sizeof filename, "%s/theme/group-icon.xpm", get_xdir());
    break;
  case THEME_GROUP_EXPANDED:
    snprintf (filename, sizeof filename, "%s/theme/group-expanded-icon.xpm", get_xdir());
    break;
  case THEME_SERVERLIST_LOGO:
    snprintf (filename, sizeof filename, "%s/theme/serverlist-logo.xpm", get_xdir());
    break;
  case THEME_OP_ICON:
    snprintf (filename, sizeof filename, "%s/theme/op-icon.xpm", get_xdir());
    break;    
  case THEME_VOICE_ICON:
    snprintf (filename, sizeof filename, "%s/theme/voice-icon.xpm", get_xdir());
    break;
  default:
    return (NULL);
    break;
  }

  pixmap = create_pixmap_from_file (window, mask, style_widget, &filename[0]);

  if (pixmap)
     return (pixmap);

  /* If we end up here it means the file couldn't be found */
  if (!style_widget)
    style = gtk_widget_get_default_style ();
  else
    style = gtk_widget_get_style (style_widget);

  switch(theme) {
  case THEME_SERVER:
    return (create_pixmap_from_data (window, mask, style_widget, server_xpm));
  case THEME_GROUP:
    return (create_pixmap_from_data (window, mask, style_widget, globe_xpm));
  case THEME_GROUP_EXPANDED:
    return (create_pixmap_from_data (window, mask, style_widget, globe_xpm));
  case THEME_SERVERLIST_LOGO:
    return (create_pixmap_from_data (window, mask, style_widget, xchat_icon_xpm));
  case THEME_OP_ICON:
    return (create_pixmap_from_data (window, mask, style_widget, op_xpm));
  case THEME_VOICE_ICON:
    return (create_pixmap_from_data (window, mask, style_widget, voice_xpm));
  default:
    return (NULL);
  }
}

/*
 * check_themes_dir [STATIC]
 * 
 * Called from load_themelist, checks a dir and adds all dirs found in it to list
 *
 */
static void
check_themes_dir (gchar *dir)
{
  DIR *directory;
  struct dirent *entry;
  struct stat buf;
  struct theme_entry *pointer;
  gchar *text[1][2] = {{ "",  "" }};
  int row = 0;
  gchar buffer[256];
  GdkBitmap *mask;
  GdkPixmap *pixmap;

  if(!dir)
    return;
  directory = opendir(dir);
  if (!directory)
    return;

  /* We dont want the first two (. and ..) */
  for (row=0;row<2;row++) {
    entry = readdir(directory);
    if (entry == 0)
      return;
  }

  /* Now do the magic */
  while (1) {
    entry = readdir(directory);
    if (entry == 0)
      break;
    snprintf (&buffer[0], sizeof(buffer), "%s%s", dir, entry->d_name);
    stat(&buffer[0], &buf);
    if (S_ISDIR(buf.st_mode)) {
      row = gtk_clist_append (themelist, text[0]);

      pointer = malloc(sizeof(struct theme_entry));
      strncpy (pointer->name, entry->d_name, sizeof(pointer->name));
      strncpy (pointer->path, buffer, sizeof(pointer->path));
      pointer->fstat = buf;

      snprintf (buffer, sizeof(buffer), "%s/theme-icon.xpm", pointer->path);
      pixmap = create_pixmap_from_file (themewin, &mask, 0, buffer);
      if (!pixmap)
	gtk_clist_set_pixtext (themelist, row, 0, entry->d_name, 5, themeicon_pixmap, themeicon_mask);
      else {
	gtk_clist_set_pixtext (themelist, row, 0, entry->d_name, 5, pixmap, mask);
#ifdef USE_IMLIB
	gdk_imlib_free_pixmap(pixmap);
#else
	gdk_pixmap_unref(pixmap);
#endif
      }

      snprintf (buffer, sizeof(buffer), "%s/themes/", get_xdir());
      if (!strcmp(buffer,dir))
	pointer->is_local = TRUE;
      else
	pointer->is_local = FALSE;

      gtk_clist_set_row_data (themelist, row, pointer);
    }
  }

  closedir(directory);
}


/*
 * load_themelist [STATIC]
 * 
 * Called whenever we want to populate the themelist
 *
 */
static void
load_themelist (void)
{
  gint row;
  GdkColor white = {0, 0xffff, 0xffff, 0xffff};
  GdkColor black = {0, 0x0000, 0x0000, 0x0000};
  GdkColor green = {0, 0x0000, 0x8000, 0x0000};
  GdkColor red = {0, 0x8000, 0x0000, 0x0000};
  gchar *text[2][2] = { { "System themes",    "" }, { "User themes",  "" } };
  gchar dir[256];
  gchar buffer1[256];
  gchar buffer2[256];
  struct theme_entry *pointer;

  gtk_clist_freeze (themelist);
  row = gtk_clist_append (themelist, text[0]);
  gtk_clist_set_selectable (themelist, row, FALSE);
  gtk_clist_set_foreground (themelist, row, &white);
  gtk_clist_set_background (themelist, row, &black);

  check_themes_dir (PREFIX"/share/xchat/");

  row = gtk_clist_append (themelist, text[1]);
  gtk_clist_set_selectable (themelist, row, FALSE);
  gtk_clist_set_foreground (themelist, row, &white);
  gtk_clist_set_background (themelist, row, &black);

  snprintf (dir, sizeof(dir), "%s/themes/", get_xdir());
  check_themes_dir (dir);

  /* Lets check what the theme-link points to */
  getcwd(buffer1, sizeof(buffer1));
  snprintf(buffer2, sizeof(buffer2), "%s/theme/", get_xdir());
  row = chdir(buffer2);

  if (!row) {
    getcwd(buffer2, sizeof(buffer2));
    chdir(buffer1);
    /* buffer2 is now the location pointed to by the link */
    row = 0;
    while (row != themelist->rows) {
      pointer = gtk_clist_get_row_data (themelist, row);
      if (pointer && !strcmp(buffer2, pointer->path)) { 
	gtk_clist_set_text (themelist, row, 1, "Current");
	gtk_clist_set_foreground (themelist, row, &green);
	break;
      }
      row++;
    }
  }

  /* Lets check what the new-theme-link points to */
  getcwd(buffer1, sizeof(buffer1));
  snprintf(buffer2, sizeof(buffer2), "%s/new-theme/", get_xdir());
  row = chdir(buffer2);

  if (!row) {
    getcwd(buffer2, sizeof(buffer2));
    chdir(buffer1);
    /* buffer2 is now the location pointed to by the link */
    row = 0;
    while (row != themelist->rows) {
      pointer = gtk_clist_get_row_data (themelist, row);
      if (pointer && !strcmp(buffer2, pointer->path)) { 
	gtk_clist_set_text (themelist, row, 1, "Selected");
	gtk_clist_set_foreground (themelist, row, &red);
	break;
      }
      row++;
    }
  }

  gtk_clist_thaw (themelist);
}


/*
 * clear_themelist [STATIC]
 * 
 * Called whenever we want to clear the themelist
 *
 */
static void
clear_themelist (void)
{
  struct theme_entry *pointer;
  int counter = 0;

  if (!themelist)
    return;

  gtk_clist_freeze (themelist);
  while(counter != themelist->rows) {
    pointer = gtk_clist_get_row_data (themelist, counter);
    if (pointer)
      free(pointer);
    counter++;
  }
  gtk_clist_clear (themelist);
  gtk_clist_thaw (themelist);
}


/*
 * close_themelist [STATIC]
 * 
 * Called whenever we want to close the themelist
 *
 */
static void
close_themelist (void)
{
  if (!themewin)
    return;
  clear_themelist ();
  gtk_widget_destroy (themewin);
  themewin = 0;
  themelist = 0;
  selected_row = 0;
}

/*
 * row_selected [STATIC]
 * 
 * Called when the user clicks one of the rows in the list
 *
 */
static void 
row_selected (GtkWidget *widget, gint row, gint column, GdkEventButton *event, gpointer data) 
{
  struct theme_entry *pointer = gtk_clist_get_row_data (themelist, row);
  selected_row = row;

  if (!pointer)
    return;

  if (pointer->is_local) {
    /* Non local themes shouldn't be removeable by user, just safety */
    gtk_widget_set_sensitive (remove_button, TRUE);
    gtk_widget_set_sensitive (select_button, TRUE);
  }
  else /* theme isnt local */ {
    gtk_widget_set_sensitive (select_button, TRUE);
    gtk_widget_set_sensitive (copy_button, TRUE);
  }
}

/*
 * row_unselected [STATIC]
 * 
 * Called when the currently selected row is unselected
 *
 */
static void 
row_unselected (GtkWidget *widget, gint row, gint column, GdkEventButton *event, gpointer data)
{
   selected_row = 0;
   gtk_widget_set_sensitive (select_button, FALSE);
   gtk_widget_set_sensitive (remove_button, FALSE);
   gtk_widget_set_sensitive (copy_button, FALSE);
}

/*
 * select_clicked [STATIC]
 * 
 * Called when the "select theme" button is clicked
 *
 */
static void
select_clicked (GtkWidget *widget, gpointer data)
{
  char path1[256];
  char path2[256];
  struct theme_entry *pointer = gtk_clist_get_row_data (themelist, selected_row);

  if (!pointer)
    return;

  if (!(pointer->path))
    return;

  snprintf (path1, sizeof(path1), "%s/new-theme", get_xdir());
  unlink (path1);
  snprintf (path2, sizeof(path2), pointer->path);
  symlink (path2, path1);

  clear_themelist ();
  load_themelist ();
}

/*
 * copy_clicked [STATIC]
 * 
 * Called when the "copy theme" button is clicked
 *
 */
static void
copy_clicked (GtkWidget *widget, gpointer data)
{
  /* 2 times 256 chr filename + 7 chr command + 1 space between filenames */
  char cmd[256+256+7+1];
  struct theme_entry *pointer = gtk_clist_get_row_data (themelist, selected_row);

  if (!pointer)
    return;

  if (!(pointer->path))
    return;

  /* TODO: recursive copy function instead of system */
  snprintf (cmd, sizeof(cmd), "cp -Rf %s %s/themes", pointer->path, get_xdir());
  system (cmd);

  clear_themelist ();
  load_themelist ();
}

/*
 * notheme_clicked [STATIC]
 * 
 * Called when the "no theme" button is clicked
 *
 */
static void
notheme_clicked (GtkWidget *widget, gpointer data)
{
  char path[256];
  
  snprintf (path, sizeof(path), "%s/theme", get_xdir());
  unlink (path);
  snprintf (path, sizeof(path), "%s/new-theme", get_xdir());
  unlink (path);

  clear_themelist ();
  load_themelist ();
}

/*
 * remove_clicked [STATIC]
 * 
 * Called when the "remove theme" button is clicked
 *
 */
static void
remove_clicked (GtkWidget *widget, gpointer data)
{
  /* "rm -rf " = 7 chars + 256 char filename */
   char buffer[256+7];
   struct theme_entry *pointer;
   gchar *celltext;

   if (!selected_row)
     /* Shouldn't happen */
     return;

   pointer = gtk_clist_get_row_data (themelist, selected_row);
   if (!pointer)
     return;

   gtk_clist_get_text (themelist, selected_row, 1, &celltext);
   if (celltext == "Current") {
     /* The theme about to be removed is our current theme */
     snprintf (buffer, sizeof(buffer), "%s/theme", get_xdir());
     unlink (buffer);
   }

   /* TODO: Recursive delete function instead of system */
   snprintf (buffer, sizeof(buffer), "rm -rf %s", pointer->path);
   system (buffer);
   clear_themelist ();
   load_themelist ();
}

/*
 * menu_themehandler [PUBLIC]
 * 
 * Called when the user clicks "Select theme" in the menu
 *
 */
void
menu_themehandler (void)
{
   GtkWidget *vbox;
   GtkWidget *hbox;
   GtkWidget *wid1;
   gchar *titles[] = {"Theme", "Status"};

   if (themewin) {
     gdk_window_show (themewin->window);
     return;
   }

   themewin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_signal_connect (GTK_OBJECT(themewin), "delete_event",
		       GTK_SIGNAL_FUNC(close_themelist),  NULL);
   gtk_window_set_policy (GTK_WINDOW (themewin), TRUE, TRUE, FALSE);
   gtk_window_set_title (GTK_WINDOW (themewin), "X-Chat: Theme Selector");
   gtk_window_set_wmclass (GTK_WINDOW (themewin), "themelist", "X-Chat");
   gtk_container_border_width (GTK_CONTAINER (themewin), 5);
   gtk_widget_realize (themewin);

   if (!themeicon_pixmap)
     themeicon_pixmap = create_pixmap_from_data (themewin, &themeicon_mask, 
						 0, theme_xpm);

   vbox = gtk_vbox_new (FALSE, 0);
   gtk_container_add (GTK_CONTAINER (themewin), vbox);
   gtk_widget_show (vbox);

   wid1 = gtk_frame_new ("Available themes:");
   gtk_box_pack_start (GTK_BOX (vbox), wid1, TRUE, TRUE, 0);
   gtk_widget_show (wid1);

   themelist = (GtkCList *) gtkutil_clist_new (2, titles, wid1, GTK_POLICY_ALWAYS, 
				  row_selected, 0, 
				  row_unselected, 0, 
				  GTK_SELECTION_SINGLE);
   gtk_widget_show (GTK_WIDGET(themelist));
   gtk_clist_set_column_width(GTK_CLIST(themelist), 0, 440);
   gtk_clist_set_column_width(GTK_CLIST(themelist), 1, 50);

   wid1 = gtk_hseparator_new ();
   gtk_box_pack_start (GTK_BOX (vbox), wid1, FALSE, FALSE, 8);
   gtk_widget_show (wid1);

   hbox = gtk_hbox_new (TRUE, 8);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   gtk_widget_show (hbox);

   done_button = gtkutil_button (themewin, GNOME_STOCK_BUTTON_OK, "OK", 
				 close_themelist, 0, hbox);
   select_button = gtkutil_button (themewin, GNOME_STOCK_BUTTON_YES, "Select theme", 
				   select_clicked, 0, hbox);
   notheme_button = gtkutil_button (themewin, GNOME_STOCK_BUTTON_NO, "No theme", 
				    notheme_clicked, 0, hbox);
   copy_button = gtkutil_button (themewin, GNOME_STOCK_PIXMAP_COPY, "Copy to local", 
				 copy_clicked, 0, hbox);
   remove_button = gtkutil_button (themewin, GNOME_STOCK_PIXMAP_REMOVE, "Remove theme", 
				   remove_clicked, 0, hbox);
   gtk_widget_set_sensitive (select_button, FALSE);
   gtk_widget_set_sensitive (remove_button, FALSE);   
   gtk_widget_set_sensitive (copy_button, FALSE);   

   load_themelist ();

   gtk_widget_set_usize (themewin, 550, 350);
   gtk_widget_show (themewin);
}

