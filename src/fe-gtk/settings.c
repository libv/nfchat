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
#include <fcntl.h>
#include <string.h>
#include "../common/xchat.h"
#include "../common/cfgfiles.h"
#include "../common/fe.h"
#include "../common/util.h"
#include "fe-gtk.h"
#include "settings.h"
#include "gtkutil.h"
#include "xtext.h"
#ifdef USE_IMLIB
#include <gdk_imlib.h>
#endif


extern struct xchatprefs prefs;
extern GdkColor colors[];
extern GdkFont *font_normal;
extern GdkFont *dialog_font_normal;
extern GSList *sess_list;
extern gint notify_tag;
extern GtkStyle *channelwin_style;
extern GtkStyle *dialogwin_style;
extern GtkWidget *main_window, *main_book;

extern void maingui_set_tab_pos (int);
extern int load_trans_table(char *full_path);
extern void add_tip (GtkWidget *, char *);
extern GdkFont *my_font_load (char *fontname);
extern void maingui_create_textlist (struct session *sess, GtkWidget * leftpane);
extern void size_request_textgad (GtkWidget * win, GtkRequisition * req);
extern void notify_checklist (void);
extern void path_part (char *file, char *path);
extern GtkStyle *my_widget_get_style (char *bg_pic);
extern GtkWidget *menu_quick_item_with_callback (void *callback, char *label, GtkWidget * menu, void *arg);

static GtkWidget *fontdialog = 0;


/* Generic interface creation functions */

static GtkWidget *
settings_create_group (GtkWidget * vvbox, gchar * title)
{
   GtkWidget *frame;
   GtkWidget *vbox;

   frame = gtk_frame_new (title);
   gtk_box_pack_start (GTK_BOX (vvbox), frame, FALSE, FALSE, 0);
   gtk_widget_show (frame);

   vbox = gtk_vbox_new (FALSE, 2);
   gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
   gtk_container_add (GTK_CONTAINER (frame), vbox);
   gtk_widget_show (vbox);

   return vbox;
}

static GtkWidget *
settings_create_table (GtkWidget * vvbox)
{
   GtkWidget *table;

   table = gtk_table_new (3, 2, FALSE);
   gtk_container_set_border_width (GTK_CONTAINER (table), 2);
   gtk_table_set_row_spacings (GTK_TABLE (table), 2);
   gtk_table_set_col_spacings (GTK_TABLE (table), 4);
   gtk_box_pack_start (GTK_BOX (vvbox), table, FALSE, FALSE, 0);
   gtk_widget_show (table);

   return table;
}

static void
settings_create_entry (char *text, int maxlen, GtkWidget * table, char *defval,
                       GtkWidget ** entry, char *suffix, void *suffixcallback,
                       int row)
{
   GtkWidget *label;
   GtkWidget *align;
   GtkWidget *hbox;
   GtkWidget *wid;

   label = gtk_label_new (text);
   gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
   gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
   gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (label), 0, 1, row, row + 1,
                     GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
   gtk_widget_show (label);

   wid = gtk_entry_new_with_max_length (maxlen);
   if (entry)
      *entry = wid;
   gtk_entry_set_text (GTK_ENTRY (wid), defval);
   gtk_widget_show (wid);

   align = gtk_alignment_new (0.0, 1.0, 0.0, 0.0);
   gtk_table_attach_defaults (GTK_TABLE (table), align, 2, 3, row, row + 1);
   gtk_widget_show (align);

   hbox = gtk_hbox_new (FALSE, 2);
   gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
   gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
   gtk_widget_show (hbox);

   if (suffixcallback || suffix)
   {
      GtkWidget *swid;

      if (suffixcallback)
      {
         swid = gtk_button_new_with_label (_("Browse..."));
         gtk_signal_connect (GTK_OBJECT (swid), "clicked",
                             GTK_SIGNAL_FUNC (suffixcallback), *entry);
      } else
      {
         swid = gtk_label_new (suffix);
      }
      gtk_widget_show (swid);
      gtk_box_pack_start (GTK_BOX (hbox), swid, FALSE, FALSE, 0);
   }
   gtk_container_add (GTK_CONTAINER (align), hbox);
}

static GtkWidget *
settings_create_toggle (char *label, GtkWidget * box, int state,
           void *callback, void *sess)
{
   GtkWidget *wid;

   wid = gtk_check_button_new_with_label (label);
   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wid), state);
   gtk_signal_connect (GTK_OBJECT (wid), "toggled",
    GTK_SIGNAL_FUNC (callback), sess);
   gtk_box_pack_start (GTK_BOX (box), wid, 0, 0, 0);
   gtk_widget_show (wid);

   return wid;
}

static void
settings_create_color_box (char *text, GtkWidget * table, int defval,
   GtkWidget ** entry, void *callback,
        struct session *sess, int row)
{
   GtkWidget *label;
   GtkWidget *align;
   GtkWidget *hbox;
   GtkWidget *swid;
   GtkWidget *wid;
   GtkStyle *style;
   char buf[127];

   label = gtk_label_new (text);
   gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
   gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
   gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (label), 0, 1, row, row + 1,
                     GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
   gtk_widget_show (label);

   sprintf (buf, "%d", defval);
   wid = gtk_entry_new ();
   gtk_entry_set_text (GTK_ENTRY (wid), buf);
   gtk_widget_set_usize (GTK_WIDGET (wid), 40, -1);
   gtk_signal_connect (GTK_OBJECT (wid), "activate",
    GTK_SIGNAL_FUNC (callback), sess);
   gtk_widget_show (wid);

   align = gtk_alignment_new (0.0, 1.0, 0.0, 0.0);
   gtk_table_attach_defaults (GTK_TABLE (table), align, 2, 3, row, row + 1);
   gtk_widget_show (align);

   hbox = gtk_hbox_new (FALSE, 2);
   gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
   gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
   gtk_widget_show (hbox);

   style = gtk_style_new ();
   style->bg[0] = colors[defval];

   swid = gtk_event_box_new ();
   if (entry)
      *entry = swid;
   gtk_widget_set_style (GTK_WIDGET (swid), style);
   gtk_style_unref (style);
   gtk_widget_set_usize (GTK_WIDGET (swid), 40, -1);
   gtk_widget_show (swid);
   gtk_box_pack_start (GTK_BOX (hbox), swid, FALSE, FALSE, 0);

   gtk_container_add (GTK_CONTAINER (align), hbox);
}

static GtkWidget *
settings_create_page (GtkWidget * book, gchar * book_label, GtkWidget * ctree,
                      gchar * tree_label, GtkCTreeNode * parent,
                      GtkCTreeNode ** node, gint page_index,
                      void (*draw_func) (struct session *, GtkWidget *),
                      struct session *sess)
{
   GtkWidget *frame;
   GtkWidget *label;
   GtkWidget *vvbox;
   GtkWidget *vbox;

   gchar *titles[1];

   vvbox = gtk_vbox_new (FALSE, 0);
   gtk_widget_show (vvbox);

   /* border for the label */
   frame = gtk_frame_new (NULL);
   gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
   gtk_box_pack_start (GTK_BOX (vvbox), frame, FALSE, TRUE, 0);
   gtk_widget_show (frame);

   /* label */
   label = gtk_label_new (book_label);
   gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
   gtk_misc_set_padding (GTK_MISC (label), 2, 1);
   gtk_container_add (GTK_CONTAINER (frame), label);
   gtk_widget_show (label);

   /* vbox for the tab */
   vbox = gtk_vbox_new (FALSE, 2);
   gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
   gtk_container_add (GTK_CONTAINER (vvbox), vbox);
   gtk_widget_show (vbox);

   /* label on the tree */
   titles[0] = tree_label;
   *node = gtk_ctree_insert_node (GTK_CTREE (ctree), parent, NULL, titles, 0,
                                  NULL, NULL, NULL, NULL, FALSE, FALSE);
   gtk_ctree_node_set_row_data (GTK_CTREE (ctree), *node,
               (gpointer) page_index);

   /* call the draw func if there is one */
   if (draw_func)
      draw_func (sess, vbox);

   /* append page and return */
   gtk_notebook_append_page (GTK_NOTEBOOK (book), vvbox, NULL);
   return vbox;
}


/* Util functions and main callbacks */

static int
settings_closegui (GtkWidget * wid, struct session *sess)
{
   if (sess->setup)
   {
      free (sess->setup);
      sess->setup = 0;
   }
   return 0;
}

static void
settings_filereq_done (GtkWidget * entry, void *data2, char *file)
{
   if (file)
   {
      if (file[0])
         gtk_entry_set_text (GTK_ENTRY (entry), file);
      free (file);
   }
}

static void
settings_openfiledialog (GtkWidget * button, GtkWidget * entry)
{
   gtkutil_file_req (_ ("Choose File"), settings_filereq_done, entry, 0, FALSE);
/*#ifdef USE_IMLIB
   gtkutil_file_req (_ ("Choose Picture"), settings_filereq_done, entry, 0, FALSE);
#else
   gtkutil_file_req (_ ("Choose XPM"), settings_filereq_done, entry, 0, FALSE);
#endif*/
}

static void
settings_fontok (GtkWidget * ok_button, GtkWidget * entry)
{
   gchar *fontname;

   if (GTK_IS_WIDGET (entry))
   {
      fontname = gtk_font_selection_dialog_get_font_name (
                               GTK_FONT_SELECTION_DIALOG (fontdialog));
      if (fontname && fontname[0])
         gtk_entry_set_text (GTK_ENTRY (entry), fontname);
   }
   gtk_widget_destroy (fontdialog);
   fontdialog = 0;
}

static void
settings_openfontdialog (GtkWidget * button, GtkWidget * entry)
{
   GtkWidget *dialog;

   dialog = gtk_font_selection_dialog_new (_ ("Select Font"));
   gtk_signal_connect
      (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG (dialog)->ok_button), "clicked",
       GTK_SIGNAL_FUNC (settings_fontok), entry);
   gtk_signal_connect
      (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG (dialog)->cancel_button),
       "clicked", GTK_SIGNAL_FUNC (gtkutil_destroy), dialog);
   gtk_font_selection_dialog_set_font_name
      (GTK_FONT_SELECTION_DIALOG (dialog),
       gtk_entry_get_text (GTK_ENTRY (entry)));
   fontdialog = dialog;
   gtk_widget_show (dialog);
}

static void
expand_homedir_inplace (char *file)
{
   char *new_file = expand_homedir (file);
   strcpy (file, new_file);
   free (new_file);
}

static void
settings_ok_clicked (GtkWidget * wid, struct session *sess)
{
   int noapply = FALSE;
   GtkStyle *old_style;
   struct session *s;
   GSList *list;

   strcpy (sess->setup->prefs.hostname, gtk_entry_get_text (GTK_ENTRY (sess->setup->entry_hostname)));

   strcpy (sess->setup->prefs.background, gtk_entry_get_text (GTK_ENTRY (sess->setup->background)));
   expand_homedir_inplace (sess->setup->prefs.background);
   strcpy (sess->setup->prefs.background_dialog, gtk_entry_get_text (GTK_ENTRY (sess->setup->background_dialog)));
   expand_homedir_inplace (sess->setup->prefs.background_dialog);
   strcpy (sess->setup->prefs.soundcmd, gtk_entry_get_text (GTK_ENTRY (sess->setup->entry_soundcmd)));
   strcpy (sess->setup->prefs.sounddir, gtk_entry_get_text (GTK_ENTRY (sess->setup->entry_sounddir)));

   strcpy (sess->setup->prefs.dnsprogram, gtk_entry_get_text ((GtkEntry *) sess->setup->entry_dnsprogram));
   strcpy (sess->setup->prefs.bluestring, gtk_entry_get_text ((GtkEntry *) sess->setup->entry_bluestring));
   strcpy (sess->setup->prefs.doubleclickuser, gtk_entry_get_text ((GtkEntry *) sess->setup->entry_doubleclickuser));
   strcpy (sess->setup->prefs.awayreason, gtk_entry_get_text ((GtkEntry *) sess->setup->entry_away));
   strcpy (sess->setup->prefs.quitreason, gtk_entry_get_text ((GtkEntry *) sess->setup->entry_quit));
   strcpy (sess->setup->prefs.logmask, gtk_entry_get_text ((GtkEntry *) sess->setup->logmask_entry));
   sess->setup->prefs.max_lines = atoi (gtk_entry_get_text ((GtkEntry *) sess->setup->entry_max_lines));
   sess->setup->prefs.notify_timeout = atol (gtk_entry_get_text ((GtkEntry *) sess->setup->entry_timeout));
   sess->setup->prefs.recon_delay = atoi (gtk_entry_get_text ((GtkEntry *) sess->setup->entry_recon_delay));

   sscanf (gtk_entry_get_text ((GtkEntry *) sess->setup->entry_permissions),
           "%o", &sess->setup->prefs.dccpermissions);

   sess->setup->prefs.dcc_blocksize = atoi (gtk_entry_get_text ((GtkEntry *) sess->setup->entry_dcc_blocksize));
   sess->setup->prefs.dccstalltimeout = atoi (gtk_entry_get_text ((GtkEntry *) sess->setup->entry_dccstalltimeout));
   sess->setup->prefs.dcctimeout = atoi (gtk_entry_get_text ((GtkEntry *) sess->setup->entry_dcctimeout));
   strcpy (sess->setup->prefs.dccdir, gtk_entry_get_text (GTK_ENTRY (sess->setup->entry_dccdir)));
   expand_homedir_inplace (sess->setup->prefs.dccdir);

   strcpy (sess->setup->prefs.font_normal, gtk_entry_get_text (GTK_ENTRY (sess->setup->font_normal)));
   strcpy (sess->setup->prefs.dialog_font_normal, gtk_entry_get_text (GTK_ENTRY (sess->setup->dialog_font_normal)));

   sess->setup->prefs.mainwindow_left = atoi (gtk_entry_get_text (GTK_ENTRY (sess->setup->entry_mainw_left)));
   sess->setup->prefs.mainwindow_top = atoi (gtk_entry_get_text (GTK_ENTRY (sess->setup->entry_mainw_top)));
   sess->setup->prefs.mainwindow_width = atoi (gtk_entry_get_text (GTK_ENTRY (sess->setup->entry_mainw_width)));
   sess->setup->prefs.mainwindow_height = atoi (gtk_entry_get_text (GTK_ENTRY (sess->setup->entry_mainw_height)));

   strcpy (sess->setup->prefs.trans_file, gtk_entry_get_text (GTK_ENTRY (sess->setup->entry_trans_file)));

   if (sess->setup->prefs.use_trans)
   {
      if (load_trans_table (sess->setup->prefs.trans_file) == 0)
      {
         gtkutil_simpledialog ("Failed to load translation table.");
         sess->setup->prefs.use_trans = 0;
      }
   }

   if (sess->setup->prefs.notify_timeout != prefs.notify_timeout)
   {
      if (notify_tag != -1)
         gtk_timeout_remove (notify_tag);
      if (sess->setup->prefs.notify_timeout)
         notify_tag = gtk_timeout_add (sess->setup->prefs.notify_timeout*1000,
                                       (GtkFunction) notify_checklist, 0);
   }
   if (strcmp (prefs.font_normal, sess->setup->prefs.font_normal) != 0)
   {
      gdk_font_unref (font_normal);
      font_normal = my_font_load (sess->setup->prefs.font_normal);
   }
   if (strcmp (prefs.dialog_font_normal, sess->setup->prefs.dialog_font_normal) != 0)
   {
      gdk_font_unref (dialog_font_normal);
      dialog_font_normal = my_font_load (sess->setup->prefs.dialog_font_normal);
   }
   if (prefs.tabchannels != sess->setup->prefs.tabchannels)
      noapply = TRUE;

   if (prefs.nopaned != sess->setup->prefs.nopaned)
      noapply = TRUE;

   if (prefs.nochanmodebuttons != sess->setup->prefs.nochanmodebuttons)
      noapply = TRUE;

   if (prefs.treeview != sess->setup->prefs.treeview)
      noapply = TRUE;

   if (prefs.nu_color != sess->setup->prefs.nu_color)
      noapply = TRUE;

   if (prefs.panel_vbox != sess->setup->prefs.panel_vbox)
      noapply = TRUE;

   if (prefs.auto_indent != sess->setup->prefs.auto_indent)
      noapply = TRUE;

   memcpy (&prefs, &sess->setup->prefs, sizeof (struct xchatprefs));

   if (main_window)
      maingui_set_tab_pos (prefs.tabs_position);

   old_style = channelwin_style;
   list = sess_list;
   channelwin_style = my_widget_get_style (prefs.background);
   while (list)
   {
      s = (struct session *) list->data;
      if (!s->is_dialog && !s->is_shell)
      {
         GTK_XTEXT (s->gui->textgad) -> tint_red = prefs.tint_red;
         GTK_XTEXT (s->gui->textgad) -> tint_green = prefs.tint_green;
         GTK_XTEXT (s->gui->textgad) -> tint_blue = prefs.tint_blue;

         gtk_xtext_set_background (GTK_XTEXT (s->gui->textgad),
                                   channelwin_style->bg_pixmap[0],
                                   prefs.transparent,
                                   prefs.tint);

         if (!prefs.indent_nicks)
            GTK_XTEXT (s->gui->textgad)->indent = 0;
         else if (GTK_XTEXT (s->gui->textgad)->indent == 0)
            GTK_XTEXT (s->gui->textgad)->indent = prefs.indent_pixels * prefs.indent_nicks;

         GTK_XTEXT (s->gui->textgad) -> wordwrap = prefs.wordwrap;
         GTK_XTEXT (s->gui->textgad) -> max_lines = prefs.max_lines;
         GTK_XTEXT (s->gui->textgad) -> separator = prefs.show_separator;

         gtk_xtext_set_font (GTK_XTEXT (s->gui->textgad), font_normal, 0);

         if (prefs.timestamp && prefs.indent_nicks)
            GTK_XTEXT (s->gui->textgad) -> time_stamp = TRUE;
         else
            GTK_XTEXT (s->gui->textgad) -> time_stamp = FALSE;

         gtk_xtext_refresh (GTK_XTEXT (s->gui->textgad));

         fe_buttons_update (s);
      }
      list = list->next;
   }
   if (old_style)
   {
      if (old_style->bg_pixmap[GTK_STATE_NORMAL])
#ifdef USE_IMLIB
         gdk_imlib_free_pixmap (old_style->bg_pixmap[GTK_STATE_NORMAL]);
#else
         gdk_pixmap_unref (old_style->bg_pixmap[GTK_STATE_NORMAL]);
#endif
      gtk_style_unref (old_style);
   }

   old_style = dialogwin_style;
   list = sess_list;
   dialogwin_style = my_widget_get_style (prefs.background_dialog);
   while (list)
   {
      s = (struct session *) list->data;
      if (s->is_dialog)
      {
         GTK_XTEXT (s->gui->textgad) -> tint_red = prefs.dialog_tint_red;
         GTK_XTEXT (s->gui->textgad) -> tint_green = prefs.dialog_tint_green;
         GTK_XTEXT (s->gui->textgad) -> tint_blue = prefs.dialog_tint_blue;

         gtk_xtext_set_background (GTK_XTEXT (s->gui->textgad),
                                   dialogwin_style->bg_pixmap[0],
                                   prefs.dialog_transparent,
                                   prefs.dialog_tint);

         if (!prefs.dialog_indent_nicks)
            GTK_XTEXT (s->gui->textgad)->indent = 0;
         else if (GTK_XTEXT (s->gui->textgad)->indent == 0)
            GTK_XTEXT (s->gui->textgad)->indent = prefs.dialog_indent_pixels * prefs.dialog_indent_nicks;

         GTK_XTEXT (s->gui->textgad) -> wordwrap = prefs.dialog_wordwrap;
         GTK_XTEXT (s->gui->textgad) -> max_lines = prefs.max_lines;
         GTK_XTEXT (s->gui->textgad) -> separator = prefs.dialog_show_separator;

         gtk_xtext_set_font (GTK_XTEXT (s->gui->textgad), dialog_font_normal, 0);

         if (prefs.timestamp && prefs.dialog_indent_nicks)
            GTK_XTEXT (s->gui->textgad) -> time_stamp = TRUE;
         else
            GTK_XTEXT (s->gui->textgad) -> time_stamp = FALSE;

         gtk_xtext_refresh (GTK_XTEXT (s->gui->textgad));
      }
      list = list->next;
   }
   if (old_style)
   {
      if (old_style->bg_pixmap[GTK_STATE_NORMAL])
#ifdef USE_IMLIB
         gdk_imlib_free_pixmap (old_style->bg_pixmap[GTK_STATE_NORMAL]);
#else
         gdk_pixmap_unref (old_style->bg_pixmap[GTK_STATE_NORMAL]);
#endif
      gtk_style_unref (old_style);
   }

   if (wid)
      gtk_widget_destroy (sess->setup->settings_window);
   else
      gtk_widget_set_sensitive (sess->setup->cancel_button, FALSE);

   if (noapply)
      gtkutil_simpledialog ("The following prefs do not take effect\n"
                            "immediately, you will have to close the\n"
                            "window and re-open it:\n\n"
                   " - Channel Tabs\n"
           " - Channel Mode Buttons\n"
               " - Userlist Buttons\n"
         " - Disable Paned Userlist\n"
              " - Notify User color\n"
    " - Layout for a vertical panel\n"
                    " - Auto Indent");
}

static void
settings_apply_clicked (GtkWidget * wid, struct session *sess)
{
   settings_ok_clicked (0, sess);
}

static void
settings_color_clicked (GtkWidget * igad, GtkWidget * colgad, int *colset)
{
   GtkStyle *stylefg = gtk_style_new ();
   char buf[16];
   int col = atoi (gtk_entry_get_text (GTK_ENTRY (igad)));
   if (col < 0 || col > 15)
      col = 0;
   sprintf (buf, "%d", col);
   gtk_entry_set_text (GTK_ENTRY (igad), buf);
   stylefg->bg[0] = colors[col];
   gtk_widget_set_style (colgad, stylefg);
   gtk_style_unref (stylefg);
   *colset = col;
}

static void
settings_ctree_select (GtkWidget * ctree, GtkCTreeNode * node)
{
   GtkWidget *book;
   gint page;

   if (!GTK_CLIST (ctree)->selection)
      return;

   book = GTK_WIDGET (gtk_object_get_user_data (GTK_OBJECT (ctree)));
   page = (gint) gtk_ctree_node_get_row_data (GTK_CTREE (ctree), node);

   gtk_notebook_set_page (GTK_NOTEBOOK (book), page);
}

static void
settings_ptoggle_check (GtkWidget * widget, int *pref)
{
   *pref = (int) (GTK_TOGGLE_BUTTON (widget)->active);
}

static void
settings_pinvtoggle_check (GtkWidget * widget, int *pref)
{
   *pref = ((int) (GTK_TOGGLE_BUTTON (widget)->active) ? 0 : 1);
}

/* Misc callbacks */

static void
settings_transparent_check (GtkWidget * widget, struct session *sess)
{
   if (GTK_TOGGLE_BUTTON (widget)->active)
   {
      sess->setup->prefs.transparent = TRUE;
      gtk_widget_set_sensitive (sess->setup->background, FALSE);
#ifdef USE_IMLIB
      gtk_widget_set_sensitive (sess->setup->check_tint, TRUE);
#endif
   } else
   {
      sess->setup->prefs.transparent = FALSE;
      gtk_toggle_button_set_active ((GtkToggleButton *) sess->setup->check_tint, FALSE);
      gtk_widget_set_sensitive (sess->setup->background, TRUE);
      gtk_widget_set_sensitive (sess->setup->check_tint, FALSE);
   }
}

static void
settings_transparent_dialog_check (GtkWidget * widget, struct session *sess)
{
   if (GTK_TOGGLE_BUTTON (widget)->active)
   {
      sess->setup->prefs.dialog_transparent = TRUE;
      gtk_widget_set_sensitive (sess->setup->background_dialog, FALSE);
#ifdef USE_IMLIB
      gtk_widget_set_sensitive (sess->setup->dialog_check_tint, TRUE);
#endif
   } else
   {
      sess->setup->prefs.dialog_transparent = FALSE;
      gtk_toggle_button_set_active ((GtkToggleButton *) sess->setup->dialog_check_tint, FALSE);
      gtk_widget_set_sensitive (sess->setup->background_dialog, TRUE);
      gtk_widget_set_sensitive (sess->setup->dialog_check_tint, FALSE);
   }
}

static void
settings_nu_color_clicked (GtkWidget * igad, struct session *sess)
{
   settings_color_clicked (igad, sess->setup->nu_color,
        &sess->setup->prefs.nu_color);
}

static void
settings_bt_color_clicked (GtkWidget * igad, struct session *sess)
{
   settings_color_clicked (igad, sess->setup->bt_color,
        &sess->setup->prefs.bt_color);
}

static void
settings_slider_cb (GtkAdjustment *adj, int *value)
{
   session *sess;
   GtkWidget *tog;

   *value = adj->value;

   tog = gtk_object_get_user_data (GTK_OBJECT (adj));
   sess = gtk_object_get_user_data (GTK_OBJECT (tog));
   if (GTK_TOGGLE_BUTTON (tog)->active)
   {
      GTK_XTEXT (sess->gui->textgad)->tint_red = sess->setup->prefs.tint_red;
      GTK_XTEXT (sess->gui->textgad)->tint_green = sess->setup->prefs.tint_green;
      GTK_XTEXT (sess->gui->textgad)->tint_blue = sess->setup->prefs.tint_blue;
      if (GTK_XTEXT (sess->gui->textgad)->transparent)
         gtk_xtext_refresh (GTK_XTEXT (sess->gui->textgad));
   }
}

static void
settings_slider (GtkWidget *vbox, char *label, int *value, GtkWidget *tog)
{
   GtkAdjustment *adj;
   GtkWidget *wid, *hbox, *lbox, *lab;

   hbox = gtk_hbox_new (0, 0);
   gtk_container_add (GTK_CONTAINER (vbox), hbox);
   gtk_widget_show (hbox);

   lbox = gtk_hbox_new (0, 0);
   gtk_box_pack_start (GTK_BOX (hbox), lbox, 0, 0, 2);
   gtk_widget_set_usize (lbox, 60, 0);
   gtk_widget_show (lbox);

   lab = gtk_label_new (label);
   gtk_box_pack_end (GTK_BOX (lbox), lab, 0, 0, 0);
   gtk_widget_show (lab);

   adj = (GtkAdjustment *) gtk_adjustment_new (*value, 0, 255.0, 1, 25, 0);
   gtk_object_set_user_data (GTK_OBJECT (adj), tog);
   gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
                       GTK_SIGNAL_FUNC (settings_slider_cb), value);

   wid = gtk_hscale_new (adj);
   gtk_scale_set_value_pos ((GtkScale*)wid, GTK_POS_RIGHT);
   gtk_scale_set_digits ((GtkScale*)wid, 0);
   gtk_container_add (GTK_CONTAINER (hbox), wid);
   gtk_widget_show (wid);
}

/* Functions to help build a page type */

static void
settings_create_window_page (GtkWidget * vvbox,
         GtkWidget ** font_normal_wid,
                char *font_normal_str,
                    GdkFont * font_nn,
           GtkWidget ** backgroundwid,
                     char *background,
                      int transparent,
           void *transparent_callback,
                             int tint,
                       int *tint_pref,
                       int *separator,
                    int *indent_nicks,
                        int *wordwrap,
                GtkWidget ** transwid,
                 GtkWidget ** tintwid,
      int *red, int *green, int *blue,
                      int *autoindent,
                 struct session *sess)
{
   GtkWidget *vbox, *table;
   gint row_index = 0;
   GtkWidget *wid;

   vbox = settings_create_group (vvbox, _ ("Appearance"));
   table = settings_create_table (vbox);
   settings_create_toggle (_ ("Use gdk_fontset_load instead of gdk_font_load"), vbox,
                                 prefs.use_fontset, settings_ptoggle_check,
                                 &(sess->setup->prefs.use_fontset));
   settings_create_entry (_ ("Font:"), FONTNAMELEN, table,
   font_normal_str, font_normal_wid, 0,
                          settings_openfontdialog, row_index++);
#ifdef USE_IMLIB
   settings_create_entry (_ ("Background Picture:"), PATH_MAX, table,
                          background, backgroundwid, 0,
                          settings_openfiledialog, row_index++);
#else
   settings_create_entry (_ ("Background XPM:"), PATH_MAX, table, background,
                          backgroundwid, 0, settings_openfiledialog,
                          row_index++);
#endif
   if (transparent)
      gtk_widget_set_sensitive (*backgroundwid, FALSE);

   wid = settings_create_toggle (_ ("Indent Nicks"), vbox, *indent_nicks,
                                 settings_ptoggle_check, indent_nicks);
   add_tip (wid, _ ("Make nicknames right justified."));

   wid = settings_create_toggle (_ ("Auto Indent"), vbox, *autoindent,
                                 settings_ptoggle_check, autoindent);
   add_tip (wid, _ ("Auto adjust the separator bar position as needed."));

   wid = settings_create_toggle (_ ("Draw Separator Bar"), vbox, *separator,
                                 settings_ptoggle_check, separator);
   add_tip (wid, _ ("Make the separator an actual visible line."));

   wid = settings_create_toggle (_ ("Word Wrap"), vbox, *wordwrap,
                                 settings_ptoggle_check, wordwrap);
   add_tip (wid, _ ("Don't split words from one line to the next"));

   *transwid = settings_create_toggle (_ ("Transparent Background"), vbox,
                                       transparent, transparent_callback, sess);
   add_tip (*transwid, _ ("Make the text box seem see-through"));

   *tintwid = settings_create_toggle (_ ("Tint Transparency"), vbox, tint,
                                      settings_ptoggle_check, tint_pref);
   add_tip (*tintwid, _ ("Tint the see-through text box to make it darker"));

#ifdef USE_IMLIB
   if (!transparent)
#endif
      gtk_widget_set_sensitive (*tintwid, FALSE);

   vbox = settings_create_group (vvbox, _ ("Tint Setting"));

   wid = gtk_check_button_new_with_label (_ ("Change in realtime"));
   gtk_object_set_user_data (GTK_OBJECT (wid), sess);

   settings_slider (vbox, _("Red:"), red, wid);
   settings_slider (vbox, _("Green:"), green, wid);
   settings_slider (vbox, _("Blue:"), blue, wid);

   gtk_container_add (GTK_CONTAINER (vbox), wid);
   gtk_widget_show (wid);
}

static void
settings_sortset (GtkWidget *item, int num)
{
   session *sess = gtk_object_get_user_data (GTK_OBJECT(item->parent));
   sess->setup->prefs.userlist_sort = num;
}

/* Functions for each "Page" */

static void
settings_page_interface (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tab;
   GtkWidget *tog;
   GtkWidget *menu;
   gint row_index;

   wid = settings_create_group (vbox, _ ("Startup and Shutdown"));
   tog = settings_create_toggle (_ ("No Server List On Startup"), wid,
                                 prefs.skipserverlist, settings_ptoggle_check,
                                 &(sess->setup->prefs.skipserverlist));
   add_tip (tog, _ ("Don't display the server list on X-Chat startup"));
   tog = settings_create_toggle (_ ("Auto Save URL list"), wid,
                                 prefs.autosave_url, settings_ptoggle_check,
   &(sess->setup->prefs.autosave_url));
   add_tip (tog, _ ("Auto save your URL list when exiting from X-Chat"));

   wid = settings_create_group (vbox, _ ("User List"));
   tab = settings_create_table (wid);
   row_index = 0;
   settings_create_entry (_ ("Double Click Command:"),
                          sizeof (prefs.doubleclickuser) - 1, tab,
                          prefs.doubleclickuser,
                          &sess->setup->entry_doubleclickuser, 0, 0,
                          row_index++);

   wid = gtk_label_new (_("Userlist sorted by: "));
   gtk_table_attach (GTK_TABLE(tab), wid, 0, 1, row_index, row_index+1, 
                     GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
   gtk_widget_show (wid);

   wid = gtk_option_menu_new ();
   menu = gtk_menu_new ();
   menu_quick_item_with_callback (settings_sortset, _("A-Z, Ops first"), menu, (void *)0);
   menu_quick_item_with_callback (settings_sortset, _("A-Z"), menu, (void *)1);
   menu_quick_item_with_callback (settings_sortset, _("Z-A, Ops last"), menu, (void *)2);
   menu_quick_item_with_callback (settings_sortset, _("Z-A"), menu, (void *)3);
   menu_quick_item_with_callback (settings_sortset, _("Unsorted"), menu, (void *)4);
   gtk_object_set_user_data (GTK_OBJECT(menu), sess);
   gtk_option_menu_set_menu (GTK_OPTION_MENU (wid), menu);
   gtk_option_menu_set_history (GTK_OPTION_MENU (wid), prefs.userlist_sort);
   gtk_widget_show (menu);
   gtk_table_attach (GTK_TABLE(tab), wid, 2, 3, row_index, row_index+1, 
                     GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
   gtk_widget_show (wid);
}

static void
settings_page_interface_inout (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tab;
   GtkWidget *tog;
   gint row_index;
   char buf[127];

   wid = settings_create_group (vbox, _ ("Input Box"));
   tog = settings_create_toggle (_ ("Nick Name Completion"), wid,
                                 prefs.nickcompletion, settings_ptoggle_check,
                                 &(sess->setup->prefs.nickcompletion));
   add_tip (tog, _ ("Complete nicknames when a partial one is entered"));
   tog = settings_create_toggle (_ ("Give the Input Box style"), wid,
                                 prefs.style_inputbox, settings_ptoggle_check,
                                 &(sess->setup->prefs.style_inputbox));
   add_tip (tog, _ ("Input box gets same style as main text area"));

   wid = settings_create_group (vbox, _ ("Output Box"));
   tog = settings_create_toggle (_ ("Time Stamp All Text"), wid,
                                 prefs.timestamp, settings_ptoggle_check,
                                 &(sess->setup->prefs.timestamp));
   add_tip (tog, _ ("Prefix all text with the current time stamp"));
   tog = settings_create_toggle (_ ("Colored Nicks"), wid,
                                 prefs.colorednicks, settings_ptoggle_check,
                                 &(sess->setup->prefs.colorednicks));
   add_tip (tog, _ ("Output nicknames in different colors"));

   wid = settings_create_group (vbox, _ ("Output Filtering"));
   tog = settings_create_toggle (_ ("Strip MIRC Color"), wid,
                                 prefs.stripcolor, settings_ptoggle_check,
                                 &(sess->setup->prefs.stripcolor));
   add_tip (tog, _ ("Strip MIRC color codes from text before displaying"));
   tog = settings_create_toggle (_ ("Filter out BEEPs"), wid,
                                 prefs.filterbeep, settings_ptoggle_check,
                                 &(sess->setup->prefs.filterbeep));
   add_tip (tog, _ ("Remove ^G BEEP codes from text before displaying"));

   wid = settings_create_group (vbox, _ ("Buffer Settings"));
   tab = settings_create_table (wid);
   row_index = 0;
   sprintf (buf, "%d", prefs.max_lines);
   settings_create_entry (_ ("Text Buffer Size:"), sizeof (buf) - 1, tab, buf,
                          &sess->setup->entry_max_lines, _ ("lines (0=Unlimited)."), 0,
                          row_index++);
}

static void
settings_tabsset (GtkWidget *item, int num)
{
   session *sess = gtk_object_get_user_data (GTK_OBJECT(item->parent));
   sess->setup->prefs.tabs_position = num;
}

static void
settings_page_interface_layout (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tog, *menu, *omenu;

   wid = settings_create_group (vbox, _ ("Buttons"));
   tog = settings_create_toggle (_ ("Channel Mode Buttons"), wid,
             !prefs.nochanmodebuttons,
            settings_pinvtoggle_check,
                                 &(sess->setup->prefs.nochanmodebuttons));
   add_tip (tog, _ ("Show the TNSIPMLK buttons"));
   tog = settings_create_toggle (_ ("User List Buttons"), wid,
             !prefs.nouserlistbuttons,
            settings_pinvtoggle_check,
                                 &(sess->setup->prefs.nouserlistbuttons));
   add_tip (tog, _ ("Show the buttons below the user list"));

   wid = settings_create_group (vbox, _ ("Tabs"));

   omenu = gtk_option_menu_new ();
   menu = gtk_menu_new ();
   menu_quick_item_with_callback (settings_tabsset, _("Bottom"), menu, (void *)0);
   menu_quick_item_with_callback (settings_tabsset, _("Top"), menu, (void *)1);
   menu_quick_item_with_callback (settings_tabsset, _("Left"), menu, (void *)2);
   menu_quick_item_with_callback (settings_tabsset, _("Right"), menu, (void *)3);
   menu_quick_item_with_callback (settings_tabsset, _("Hidden"), menu, (void *)4);
   gtk_object_set_user_data (GTK_OBJECT(menu), sess);
   gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
   gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), prefs.tabs_position);
   gtk_widget_show (menu);
   gtk_widget_set_usize (menu, 120, 0);
   gtk_box_pack_start (GTK_BOX (wid), omenu, 0, 0, 0);
   gtk_widget_show (omenu);

   tog = settings_create_toggle (_ ("New Tabs to front"), wid,
                                 prefs.newtabstofront, settings_ptoggle_check,
                                 &(sess->setup->prefs.newtabstofront));
   add_tip (tog, _ ("Bring new query/channel tabs to front"));
   tog = settings_create_toggle (_ ("Channel Tabs"), wid,
                                 prefs.tabchannels, settings_ptoggle_check,
   &(sess->setup->prefs.tabchannels));
   add_tip (tog, _ ("Use tabs for channels instead of separate windows"));
   tog = settings_create_toggle (_ ("Private Message Tabs"), wid,
                                 prefs.privmsgtab, settings_ptoggle_check,
    &(sess->setup->prefs.privmsgtab));
   add_tip (tog, _ ("Use tabs for /query instead of separate windows"));
   tog = settings_create_toggle (_ ("Use a separate tab/window for server messages"), wid,
                                 prefs.use_server_tab, settings_ptoggle_check,
                                 &(sess->setup->prefs.use_server_tab));
   tog = settings_create_toggle (_ ("Use tabs for DCC, Ignore, Notify etc windows."), wid,
                                 prefs.windows_as_tabs, settings_ptoggle_check,
                                 &(sess->setup->prefs.windows_as_tabs));

   wid = settings_create_group (vbox, _ ("Other Layout Settings"));
   tog = settings_create_toggle (_ ("Disable Paned Userlist"), wid,
                                 prefs.nopaned, settings_ptoggle_check,
       &(sess->setup->prefs.nopaned));
   add_tip (tog, _ ("Use a fixed width user list instead of the paned one"));
}

static void
settings_page_interface_mainwindow (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tab;
   gint row_index;
   char buf[127];

   wid = settings_create_group (vbox, _ ("Window Position"));
   gtkutil_label_new (_ ("If Left and Top are set to zero, X-Chat will use "
                         "your window manager defaults."), wid);
   tab = settings_create_table (wid);
   row_index = 0;
   sprintf (buf, "%d", prefs.mainwindow_left);
   settings_create_entry (_ ("Left:"), sizeof (buf) - 1, tab, buf,
                          &sess->setup->entry_mainw_left, 0, 0,
                          row_index++);
   sprintf (buf, "%d", prefs.mainwindow_top);
   settings_create_entry (_ ("Top:"), sizeof (buf) - 1, tab, buf,
   &sess->setup->entry_mainw_top, 0, 0,
                          row_index++);

   wid = settings_create_group (vbox, _ ("Window Size"));
   tab = settings_create_table (wid);
   row_index = 0;
   sprintf (buf, "%d", prefs.mainwindow_width);
   settings_create_entry (_ ("Width:"), sizeof (buf) - 1, tab, buf,
                          &sess->setup->entry_mainw_width, 0, 0,
                          row_index++);
   sprintf (buf, "%d", prefs.mainwindow_height);
   settings_create_entry (_ ("Height:"), sizeof (buf) - 1, tab, buf,
                          &sess->setup->entry_mainw_height, 0, 0,
                          row_index++);
   settings_create_toggle (_ ("Show Session Tree View"), wid, prefs.treeview,
			   settings_ptoggle_check,
			   &(sess->setup->prefs.treeview));
}

static void
settings_page_interface_channelwindow (struct session *sess, GtkWidget * vbox)
{
   settings_create_window_page (vbox,
            &sess->setup->font_normal,
                    prefs.font_normal,
                          font_normal,
             &sess->setup->background,
                     prefs.background,
                    prefs.transparent,
           settings_transparent_check,
                           prefs.tint,
           &(sess->setup->prefs.tint),
 &(sess->setup->prefs.show_separator),
   &(sess->setup->prefs.indent_nicks),
         &sess->setup->prefs.wordwrap,
      &sess->setup->check_transparent,
             &sess->setup->check_tint,
         &sess->setup->prefs.tint_red,
       &sess->setup->prefs.tint_green,
        &sess->setup->prefs.tint_blue,
      &sess->setup->prefs.auto_indent,
                                sess);
}

static void
settings_page_interface_dialogwindow (struct session *sess, GtkWidget * vbox)
{
   settings_create_window_page (vbox,
     &sess->setup->dialog_font_normal,
             prefs.dialog_font_normal,
                   dialog_font_normal,
      &sess->setup->background_dialog,
              prefs.background_dialog,
             prefs.dialog_transparent,
    settings_transparent_dialog_check,
                    prefs.dialog_tint,
    &(sess->setup->prefs.dialog_tint),
&(sess->setup->prefs.dialog_show_separator),
&(sess->setup->prefs.dialog_indent_nicks),
  &sess->setup->prefs.dialog_wordwrap,
&sess->setup->dialog_check_transparent,
      &sess->setup->dialog_check_tint,
  &sess->setup->prefs.dialog_tint_red,
&sess->setup->prefs.dialog_tint_green,
 &sess->setup->prefs.dialog_tint_blue,
      &sess->setup->prefs.auto_indent,
                                sess);
}

#ifdef USE_PANEL
static void
settings_page_interface_panel (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tog;

   wid = settings_create_group (vbox, _ ("General"));
   tog = settings_create_toggle (_ ("Hide Session on Panelize"), wid,
                                 prefs.panelize_hide, settings_ptoggle_check,
                                 &(sess->setup->prefs.panelize_hide));
   add_tip (tog, _ ("Hide X-Chat when window moved to the panel"));

   wid = settings_create_group (vbox, _ ("Panel Applet"));
   tog = settings_create_toggle (_ ("Layout For a Vertical Panel"), wid,
                                 prefs.panel_vbox, settings_ptoggle_check,
    &(sess->setup->prefs.panel_vbox));
   add_tip (tog, _ ("Layout the X-Chat panel applet for a vertical panel"));
}
#endif

static void
settings_page_irc (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tab;
   GtkWidget *tog;
   gint row_index;
   char buf[16];

   wid = settings_create_group (vbox, _ ("General"));
   tog = settings_create_toggle (_ ("Raw Mode Display"), wid,
                                 prefs.raw_modes, settings_ptoggle_check,
     &(sess->setup->prefs.raw_modes));
   add_tip (tog, _ ("Display raw mode changes instead of interpretations"));
   tog = settings_create_toggle (_ ("Beep on Private Messages"), wid,
                                 prefs.beepmsg, settings_ptoggle_check,
       &(sess->setup->prefs.beepmsg));
   add_tip (tog, _ ("Beep when a private message for you is received"));
   tog = settings_create_toggle (_ ("Don't send /who #chan on join."), wid,
                                 prefs.nouserhost, settings_ptoggle_check,
                                 &(sess->setup->prefs.nouserhost));
   add_tip (tog, _ ("Don't find user information when joining a channel."));
   tog = settings_create_toggle (_ ("Perform a periodic mail check."), wid,
                                 prefs.mail_check, settings_ptoggle_check,
                                 &(sess->setup->prefs.mail_check));
   wid = settings_create_group (vbox, _ ("Your irc settings"));
   tab = settings_create_table (wid);
   row_index = 0;
   settings_create_entry (_ ("Quit Message:"),
   sizeof (prefs.quitreason) - 1, tab,
                     prefs.quitreason,
       &sess->setup->entry_quit, 0, 0,
                          row_index++);

   wid = settings_create_group (vbox, _ ("External Programs"));
   tab = settings_create_table (wid);
   row_index = 0;
   settings_create_entry (_ ("DNS Lookup Program:"),
   sizeof (prefs.dnsprogram) - 1, tab,
                     prefs.dnsprogram,
                          &sess->setup->entry_dnsprogram, 0, 0,
                          row_index++);

   wid = settings_create_group (vbox, _ ("Timing"));
   tab = settings_create_table (wid);
   row_index = 0;
   sprintf (buf, "%d", prefs.recon_delay);
   settings_create_entry (_ ("Auto ReConnect Delay:"),
   sizeof (prefs.recon_delay) - 1, tab,
                          buf,
                          &sess->setup->entry_recon_delay, _ ("seconds."), 0,
                          row_index++);
}

static void
settings_page_irc_ipaddress (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tab;
   GtkWidget *tog;
   gint row_index;

   wid = settings_create_group (vbox, _ ("Address"));
   tab = settings_create_table (wid);
   row_index = 0;
   settings_create_entry (_ ("Hostname / IP Number:"), sizeof (prefs.hostname) - 1, tab,
                          prefs.hostname, &sess->setup->entry_hostname, 0, 0,
                          row_index++);

   gtkutil_label_new (_("Most people should leave this blank, it's only\n"
                      "usefull for machines with multiple addresses."), wid);

   tog = sess->setup->check_ip = settings_create_toggle
      (_ ("Get my IP from Server (for use in DCC Send only)"), wid,
       prefs.ip_from_server, settings_ptoggle_check,
       &(sess->setup->prefs.ip_from_server));
   add_tip (tog, _ ("For people using a 10.* or 192.168.* IP number."));
}

static void
settings_page_irc_away (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tab;
   GtkWidget *tog;
   gint row_index;

   wid = settings_create_group (vbox, _ ("General"));
   tog = settings_create_toggle (_ ("Show away once"), wid,
                                 prefs.show_away_once, settings_ptoggle_check,
                                 &(sess->setup->prefs.show_away_once));
   add_tip (tog, _ ("Only show away messages the first time they're seen"));
   tog = settings_create_toggle (_ ("Announce away messsages"), wid,
              prefs.show_away_message,
               settings_ptoggle_check,
                                 &(sess->setup->prefs.show_away_message));
   add_tip (tog, _ ("Announce your away message to the channel(s) you are in"));

   wid = settings_create_group (vbox, _ ("Your away settings"));
   tab = settings_create_table (wid);
   row_index = 0;
   settings_create_entry (_ ("Away Reason:"),
   sizeof (prefs.awayreason) - 1, tab,
                     prefs.awayreason,
       &sess->setup->entry_away, 0, 0,
                          row_index++);
}

static void
settings_page_irc_highlighting (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tab;
   gint row_index;

   wid = settings_create_group (vbox, _ ("General"));
   tab = settings_create_table (wid);
   row_index = 0;
   settings_create_entry (_ ("Extra Words to Highlight on:"),
   sizeof (prefs.bluestring) - 1, tab,
                     prefs.bluestring,
       &sess->setup->entry_bluestring,
      _ ("(separate with commas)"), 0,
                          row_index++);
   settings_create_color_box (_ ("Hilighted Nick Color:"), tab,
                       prefs.bt_color,
               &sess->setup->bt_color,
                              settings_bt_color_clicked, sess, row_index++);
}

static void
settings_page_irc_logging (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tog, *tab;

   wid = settings_create_group (vbox, _ ("General"));
   tab = settings_create_table (wid);
   tog = settings_create_toggle (_ ("Logging"), wid,
                                 prefs.logging, settings_ptoggle_check,
                                 &(sess->setup->prefs.logging));
   add_tip (tog, _ ("Enable logging conversations to disk"));

   settings_create_entry (_ ("Log name mask:"),
			  sizeof (prefs.logmask) - 1, tab,
			  prefs.logmask, &sess->setup->logmask_entry,
			  NULL, NULL, 0);
}

static void
settings_page_irc_notification (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tab;
   GtkWidget *tog;
   gint row_index;
   char buf[127];

   wid = settings_create_group (vbox, _ ("User List Notify Highlighting"));
   tog = settings_create_toggle (_ ("Highlight Notifies"), wid,
                                 prefs.hilitenotify, settings_ptoggle_check,
   &(sess->setup->prefs.hilitenotify));
   add_tip (tog, _ ("Highlight notified users in the user list"));
   tab = settings_create_table (wid);
   row_index = 0;
   settings_create_color_box (_ ("Notified User Color:"), tab,
                       prefs.nu_color,
               &sess->setup->nu_color,
                              settings_nu_color_clicked, sess, row_index++);

   wid = settings_create_group (vbox, _ ("Notification Timeouts"));
   tab = settings_create_table (wid);
   row_index = 0;
   sprintf (buf, "%d", prefs.notify_timeout);
   settings_create_entry (_ ("Notify Check Interval:"),
           sizeof (buf) - 1, tab, buf,
          &sess->setup->entry_timeout,
                          _ ("seconds. (15=Normal, 0=Disable)"), 0,
                          row_index++);
}

static void
settings_page_irc_charset (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid, *tog, *tab;

   wid = settings_create_group (vbox, _ ("General"));
   tog = settings_create_toggle (_ ("Enable Character Translation"), wid,
                                 prefs.use_trans, settings_ptoggle_check,
                                 &(sess->setup->prefs.use_trans));
   tab = settings_create_table (wid);

   settings_create_entry (_ ("Translation File:"),
   sizeof (prefs.trans_file) - 1, tab,
                     prefs.trans_file,
       &sess->setup->entry_trans_file, 0, settings_openfiledialog,
                          1);
   gtkutil_label_new (_ ("Use a ircII style translation file."), wid);
}

static void
settings_page_dcc (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tog;

   wid = settings_create_group (vbox, _ ("General"));
   tog = settings_create_toggle (_ ("Auto Open DCC Send Window"), wid,
       !prefs.noautoopendccsendwindow,
            settings_pinvtoggle_check,
                                 &(sess->setup->prefs.noautoopendccsendwindow));
   add_tip (tog, _ ("Automatically open DCC Send Window"));
   tog = settings_create_toggle (_ ("Auto Open DCC Recv Window"), wid,
       !prefs.noautoopendccrecvwindow,
            settings_pinvtoggle_check,
                                 &(sess->setup->prefs.noautoopendccrecvwindow));
   add_tip (tog, _ ("Automatically open DCC Recv Window"));
   tog = settings_create_toggle (_ ("Auto Open DCC Chat Window"), wid,
       !prefs.noautoopendccchatwindow,
            settings_pinvtoggle_check,
                                 &(sess->setup->prefs.noautoopendccchatwindow));
   add_tip (tog, _ ("Automatically open DCC Chat Window"));
   tog = settings_create_toggle (_ ("Resume on Auto Accept"), wid,
                     prefs.autoresume,
               settings_ptoggle_check,
    &(sess->setup->prefs.autoresume));
   add_tip (tog, _ ("When Auto-Accepting DCC, try to resume."));
}

static void
settings_page_dcc_filetransfer (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tab;
   GtkWidget *tog;
   gint row_index;
   char buf[127];

   wid = settings_create_group (vbox, _ ("Timeouts"));
   tab = settings_create_table (wid);
   row_index = 0;
   sprintf (buf, "%d", prefs.dcctimeout);
   settings_create_entry (_ ("DCC Offers Timeout:"),
           sizeof (buf) - 1, tab, buf,
                          &sess->setup->entry_dcctimeout, _ ("seconds."), 0,
                          row_index++);
   sprintf (buf, "%d", prefs.dccstalltimeout);
   settings_create_entry (_ ("DCC Stall Timeout:"),
           sizeof (buf) - 1, tab, buf,
   &sess->setup->entry_dccstalltimeout,
      _ ("seconds."), 0, row_index++);

   wid = settings_create_group (vbox, _ ("Received files"));
   tab = settings_create_table (wid);
   row_index = 0;
   sprintf (buf, "%04o", prefs.dccpermissions);
   settings_create_entry (_ ("File Permissions:"),
                          5, tab, buf,
                          &sess->setup->entry_permissions, _ ("(octal)"), 0,
                          row_index++);
   settings_create_entry (_ ("Directory to save to:"),
                          sizeof (prefs.dccdir) - 1, tab, prefs.dccdir,
     &sess->setup->entry_dccdir, 0, 0,
                          row_index++);
   tog = settings_create_toggle (_ ("Save file with Nickname"), wid,
                                 prefs.dccwithnick, settings_ptoggle_check,
   &(sess->setup->prefs.dccwithnick));
   add_tip (tog, _ ("Put the sender\'s nickname in incoming filenames"));

   wid = settings_create_group (vbox, _ ("DCC Send Options"));
   tab = settings_create_table (wid);
   tog = settings_create_toggle (_ ("Fast DCC Send"), wid,
                                 prefs.fastdccsend, settings_ptoggle_check,
   &(sess->setup->prefs.fastdccsend));
   add_tip (tog, _ ("Don\'t wait for ACKs to send more data"));

   sprintf (buf, "%d", prefs.dcc_blocksize);
   settings_create_entry (_ ("Send Block Size:"),
                          5, tab, buf,
                          &sess->setup->entry_dcc_blocksize, _ ("(1024=Normal)"), 0,
                          row_index++);
}

static void
settings_page_ctcp (struct session *sess, GtkWidget * vbox)
{
   GtkWidget *wid;
   GtkWidget *tab;
   GtkWidget *tog;
   gint row_index;

   wid = settings_create_group (vbox, _ ("Built-in Replies"));
   tog = settings_create_toggle (_ ("Hide Version"), wid,
                                 prefs.hidever, settings_ptoggle_check,
       &(sess->setup->prefs.hidever));
   add_tip (tog, _ ("Do not reply to CTCP version"));

   wid = settings_create_group (vbox, _ ("Sound"));
   tab = settings_create_table (wid);
   row_index = 0;
   settings_create_entry (_ ("Sound Dir:"),
     sizeof (prefs.sounddir) - 1, tab,
                       prefs.sounddir,
   &sess->setup->entry_sounddir, 0, 0,
                          row_index++);
   settings_create_entry (_ ("Play Command:"),
     sizeof (prefs.soundcmd) - 1, tab,
                       prefs.soundcmd,
   &sess->setup->entry_soundcmd, 0, 0,
                          row_index++);
}


/* Create Interface */

void
settings_opengui (struct session *sess)
{
   GtkCTreeNode *last_top;
   GtkCTreeNode *last_child;

   GtkWidget *dialog;
   GtkWidget *hbbox;
   GtkWidget *frame;
   GtkWidget *ctree;
   GtkWidget *book;
   GtkWidget *hbox;
   GtkWidget *vbox;
   GtkWidget *wid;

   gchar *titles[1];
   gint page_index;

   if (sess->setup)
   {
      gdk_window_show (sess->setup->settings_window->window);
      return;
   }
   sess->setup = malloc (sizeof (struct setup));
   memcpy (&sess->setup->prefs, &prefs, sizeof (struct xchatprefs));

   /* prepare the dialog */
   dialog = gtkutil_dialog_new (_ ("X-Chat: Preferences"), "preferences",
             settings_closegui, sess);
   sess->setup->settings_window = dialog;

   /* prepare the action area */
   gtk_container_set_border_width
      (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 2);
   gtk_box_set_homogeneous (GTK_BOX (GTK_DIALOG (dialog)->action_area), FALSE);

   /* prepare the button box */
   hbbox = gtk_hbutton_box_new ();
   gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbbox), 4);
   gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->action_area), hbbox,
                     FALSE, FALSE, 0);
   gtk_widget_show (hbbox);

   /* i love buttons */
#ifdef USE_GNOME
   wid = gnome_stock_button (GNOME_STOCK_BUTTON_OK);
#else
   wid = gtk_button_new_with_label (_ ("Ok"));
#endif
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
                       GTK_SIGNAL_FUNC (settings_ok_clicked), sess);
   gtk_box_pack_start (GTK_BOX (hbbox), wid, FALSE, FALSE, 0);
   gtk_widget_show (wid);

#ifdef USE_GNOME
   wid = gnome_stock_button (GNOME_STOCK_BUTTON_APPLY);
#else
   wid = gtk_button_new_with_label (_ ("Apply"));
#endif
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
                       GTK_SIGNAL_FUNC (settings_apply_clicked), sess);
   gtk_box_pack_start (GTK_BOX (hbbox), wid, FALSE, FALSE, 0);
   gtk_widget_show (wid);

#ifdef USE_GNOME
   wid = gnome_stock_button (GNOME_STOCK_BUTTON_CANCEL);
#else
   wid = gtk_button_new_with_label (_ ("Cancel"));
#endif
   gtk_signal_connect (GTK_OBJECT (wid), "clicked",
    GTK_SIGNAL_FUNC (gtkutil_destroy),
        sess->setup->settings_window);
   gtk_box_pack_start (GTK_BOX (hbbox), wid, FALSE, FALSE, 0);
   gtk_widget_show (wid);
   sess->setup->cancel_button = wid;

   /* the main hbox */
   hbox = gtk_hbox_new (FALSE, 6);
   gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
   gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                 hbox, TRUE, TRUE, 0);
   gtk_widget_show (hbox);

   /* the tree */
   titles[0] = _ ("Categories");
   ctree = gtk_ctree_new_with_titles (1, 0, titles);
   gtk_clist_set_selection_mode (GTK_CLIST (ctree), GTK_SELECTION_BROWSE);
   gtk_ctree_set_indent (GTK_CTREE (ctree), 15);
   gtk_widget_set_usize (ctree, 140, 0);
   gtk_box_pack_start (GTK_BOX (hbox), ctree, FALSE, FALSE, 0);
   gtk_widget_show (ctree);

   /* the preferences frame */
   frame = gtk_frame_new (NULL);
   gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
   gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
   gtk_widget_show (frame);

   /* the notebook */
   book = gtk_notebook_new ();
   gtk_notebook_set_show_tabs (GTK_NOTEBOOK (book), FALSE);
   gtk_notebook_set_show_border (GTK_NOTEBOOK (book), FALSE);
   gtk_container_add (GTK_CONTAINER (frame), book);
   gtk_object_set_user_data (GTK_OBJECT (ctree), book);
   gtk_signal_connect (GTK_OBJECT (ctree), "tree_select_row",
                       GTK_SIGNAL_FUNC (settings_ctree_select), NULL);
   page_index = 0;

   vbox = settings_create_page (book, _ ("Interface Settings"),
               ctree, _ ("Interface"),
        NULL, &last_top, page_index++,
       settings_page_interface, sess);
   gtk_ctree_select (GTK_CTREE (ctree), last_top);

   vbox = settings_create_page (book, _ ("IRC Input/Output Settings"),
        ctree, _ ("IRC Input/Output"),
   last_top, &last_child, page_index++,
                                settings_page_interface_inout, sess);

   vbox = settings_create_page (book, _ ("Window Layout Settings"),
           ctree, _ ("Window Layout"),
   last_top, &last_child, page_index++,
                                settings_page_interface_layout, sess);

   vbox = settings_create_page (book, _ ("Main Window Settings"),
             ctree, _ ("Main Window"),
   last_top, &last_child, page_index++,
                                settings_page_interface_mainwindow, sess);

   vbox = settings_create_page (book, _ ("Channel Window Settings"),
         ctree, _ ("Channel Windows"),
   last_top, &last_child, page_index++,
                                settings_page_interface_channelwindow, sess);

   vbox = settings_create_page (book, _ ("Dialog Window Settings"),
          ctree, _ ("Dialog Windows"),
   last_top, &last_child, page_index++,
                                settings_page_interface_dialogwindow, sess);

#ifdef USE_PANEL
   vbox = settings_create_page (book, _ ("Panel Settings"),
                   ctree, _ ("Panel"),
   last_top, &last_child, page_index++,
                                settings_page_interface_panel, sess);
#endif

   vbox = settings_create_page (book, _ ("IRC Settings"),
                     ctree, _ ("IRC"),
        NULL, &last_top, page_index++,
             settings_page_irc, sess);

   vbox = settings_create_page (book, _ ("IP Address Settings"),
              ctree, _ ("IP Address"),
   last_top, &last_child, page_index++,
   settings_page_irc_ipaddress, sess);

   vbox = settings_create_page (book, _ ("Away Settings"),
                    ctree, _ ("Away"),
   last_top, &last_child, page_index++,
        settings_page_irc_away, sess);

   vbox = settings_create_page (book, _ ("Highlighting Settings"),
            ctree, _ ("Highlighting"),
   last_top, &last_child, page_index++,
                                settings_page_irc_highlighting, sess);

   vbox = settings_create_page (book, _ ("Logging Settings"),
                 ctree, _ ("Logging"),
   last_top, &last_child, page_index++,
     settings_page_irc_logging, sess);

   vbox = settings_create_page (book, _ ("Notification Settings"),
            ctree, _ ("Notification"),
   last_top, &last_child, page_index++,
                                settings_page_irc_notification, sess);

   vbox = settings_create_page (book, _ ("Character Set (Translation Tables)"),
            ctree, _ ("Character Set"),
   last_top, &last_child, page_index++,
                                settings_page_irc_charset, sess);

   vbox = settings_create_page (book, _ ("CTCP Settings"),
                    ctree, _ ("CTCP"),
   last_top, &last_child, page_index++,
            settings_page_ctcp, sess);

   vbox = settings_create_page (book, _ ("DCC Settings"),
                     ctree, _ ("DCC"),
        NULL, &last_top, page_index++,
             settings_page_dcc, sess);

   vbox = settings_create_page (book, _ ("File Transfer Settings"),
           ctree, _ ("File Transfer"),
   last_top, &last_child, page_index++,
                                settings_page_dcc_filetransfer, sess);

   /* since they all fit in the window, why not expand them all */
   gtk_ctree_expand_recursive ((GtkCTree *) ctree, 0);
   gtk_clist_select_row (GTK_CLIST (ctree), 0, 0);

   gtk_widget_show (book);
   gtk_widget_show (sess->setup->settings_window);
}
