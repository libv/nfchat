#include <stdio.h>
#include "../common/xchat.h"

#ifdef USE_PANEL

#include "fe-gtk.h"
#include "gtkutil.h"
#include <applet-widget.h>

extern struct xchatprefs prefs;
static GtkWidget *panel_applet = NULL, *panel_box;
static GtkWidget *panel_popup = NULL;
int nopanel;

extern GtkWidget *main_window;
extern GtkWidget *main_book;
extern GtkStyle *normaltab_style;
extern GtkStyle *redtab_style;
extern GtkStyle *bluetab_style;

extern void add_tip (GtkWidget * wid, char *text);
extern void menu_about (GtkWidget * wid, gpointer sess);
extern int maingui_pagetofront (int page);
extern int relink_window (GtkWidget * w, struct session *sess);


static void
panel_invalidate (GtkWidget * wid, gpointer * arg2)
{
   panel_applet = NULL;
}

static void
create_panel_widget ()
{
   panel_applet = applet_widget_new ("xchat_applet");
   gtk_widget_realize (panel_applet);

   if (prefs.panel_vbox)
      panel_box = gtk_vbox_new (0, 0);
   else
      panel_box = gtk_hbox_new (0, 0);
   applet_widget_add (APPLET_WIDGET (panel_applet), panel_box);
   gtk_widget_show (panel_box);

   gtkutil_label_new ("X-Chat:", panel_box);

   applet_widget_register_stock_callback (APPLET_WIDGET (panel_applet),
                              "about",
               GNOME_STOCK_MENU_ABOUT,
                       _ ("About..."),
         GTK_SIGNAL_FUNC (menu_about),
                                NULL);

   gtk_signal_connect (GTK_OBJECT (panel_applet), "destroy", panel_invalidate, NULL);
   gtk_widget_show (panel_applet);
}

static void
gui_panel_destroy_popup (GtkWidget * wid, GtkWidget * popup)
{
   gtk_widget_destroy (panel_popup);
   panel_popup = NULL;
}

static void
gui_panel_remove_clicked (GtkWidget * button, struct session *sess)
{
   gtk_widget_show (sess->gui->window);
   gtk_widget_destroy (sess->gui->panel_button);
   sess->gui->panel_button = 0;
   if (main_window)
   {
      int page = gtk_notebook_page_num (GTK_NOTEBOOK (main_book), sess->gui->window);
      gtk_idle_add ((GtkFunction) maingui_pagetofront, (gpointer) page);
   }
   if (panel_popup)
      gui_panel_destroy_popup (NULL, NULL);
}

static void
gui_panel_hide_clicked (GtkWidget * button, struct session *sess)
{
   gtk_widget_hide (sess->gui->window);
}

static void
gui_panel_show_clicked (GtkWidget * button, struct session *sess)
{
   gtk_widget_show (sess->gui->window);
}

static void
gui_panel_here_clicked (GtkWidget * button, struct session *sess)
{
   if (sess->is_tab)
   {
      if (main_window)
      {
         gtk_widget_hide (main_window);
         gtk_window_set_position (GTK_WINDOW (main_window), GTK_WIN_POS_MOUSE);
         gtk_widget_show (main_window);
      }
   } else
   {
      gtk_widget_hide (sess->gui->window);
      gtk_window_set_position (GTK_WINDOW (sess->gui->window), GTK_WIN_POS_MOUSE);
      gtk_widget_show (sess->gui->window);
   }
}

static void
gui_panel_destroy_view_popup (GtkWidget * popup, /* Event */ gpointer * arg2, struct session *sess)
{
   /* BODGE ALERT !! BODGE ALERT !! --AGL */
   gtk_widget_reparent (sess->gui->textgad, sess->gui->leftpane);
   gtk_box_reorder_child (GTK_BOX (sess->gui->leftpane), sess->gui->textgad, 0);

   gtk_widget_destroy (popup);
}

static void
gui_panel_view_clicked (GtkWidget * button, struct session *sess)
{
   GtkWidget *view_popup;

   view_popup = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_widget_show_all (view_popup);
   gtk_widget_reparent (sess->gui->textgad, view_popup);
   gtk_signal_connect (GTK_OBJECT (view_popup), "leave_notify_event", gui_panel_destroy_view_popup, sess);
}

static gint
gui_panel_button_event (GtkWidget * button, GdkEvent * event, struct session *sess)
{
   if (event->type == GDK_BUTTON_PRESS &&
       event->button.button == 3)
   {
      GtkWidget *vbox, *wid;

      panel_popup = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_position (GTK_WINDOW (panel_popup), GTK_WIN_POS_MOUSE);
      vbox = gtk_vbox_new (0, 0);
      gtk_container_add (GTK_CONTAINER (panel_popup), vbox);

      wid = gtk_label_new ("");
      if (sess->channel[0])
         gtk_label_set_text (GTK_LABEL (wid), sess->channel);
      else
         gtk_label_set_text (GTK_LABEL (wid), "No Channel");
      gtk_box_pack_start (GTK_BOX (vbox), wid, 0, 0, 0);

      wid = gtk_label_new ("");
      if (sess->server->hostname[0])
         gtk_label_set_text (GTK_LABEL (wid), sess->server->servername);
      else
         gtk_label_set_text (GTK_LABEL (wid), "No Server");
      gtk_box_pack_start (GTK_BOX (vbox), wid, 0, 0, 0);

      wid = gtk_label_new ("");
      if (sess->is_tab)
         gtk_label_set_text (GTK_LABEL (wid), "Is Tab");
      else
         gtk_label_set_text (GTK_LABEL (wid), "Is Not Tab");
      gtk_box_pack_start (GTK_BOX (vbox), wid, 0, 0, 0);

      wid = gtk_button_new_with_label ("Close");
      gtk_signal_connect (GTK_OBJECT (wid), "clicked", GTK_SIGNAL_FUNC (gtkutil_destroy), sess->gui->window);
      gtk_box_pack_start (GTK_BOX (vbox), wid, 0, 0, 0);
      wid = gtk_button_new_with_label ("Remove");
      gtk_signal_connect (GTK_OBJECT (wid), "clicked", GTK_SIGNAL_FUNC (gui_panel_remove_clicked), sess);
      gtk_box_pack_start (GTK_BOX (vbox), wid, 0, 0, 0);
      wid = gtk_button_new_with_label ("Hide");
      gtk_signal_connect (GTK_OBJECT (wid), "clicked", GTK_SIGNAL_FUNC (gui_panel_hide_clicked), sess);
      gtk_box_pack_start (GTK_BOX (vbox), wid, 0, 0, 0);
      wid = gtk_button_new_with_label ("Show");
      gtk_signal_connect (GTK_OBJECT (wid), "clicked", GTK_SIGNAL_FUNC (gui_panel_show_clicked), sess);
      gtk_box_pack_start (GTK_BOX (vbox), wid, 0, 0, 0);
      wid = gtk_button_new_with_label ("De/Link");
      gtk_signal_connect (GTK_OBJECT (wid), "clicked", GTK_SIGNAL_FUNC (relink_window), sess);
      gtk_box_pack_start (GTK_BOX (vbox), wid, 0, 0, 0);
      wid = gtk_button_new_with_label ("Move Here");
      gtk_signal_connect (GTK_OBJECT (wid), "clicked", GTK_SIGNAL_FUNC (gui_panel_here_clicked), sess);
      gtk_box_pack_start (GTK_BOX (vbox), wid, 0, 0, 0);
      wid = gtk_button_new_with_label ("View");
      gtk_signal_connect (GTK_OBJECT (wid), "clicked", GTK_SIGNAL_FUNC (gui_panel_view_clicked), sess);
      gtk_box_pack_start (GTK_BOX (vbox), wid, 0, 0, 0);

      gtk_signal_connect (GTK_OBJECT (panel_popup), "leave_notify_event", gui_panel_destroy_popup, panel_popup);
      gtk_widget_show_all (panel_popup);

   }
   return 0;
}

static void
maingui_unpanelize (GtkWidget * button, struct session *sess)
{
   if (!sess->is_tab)
      gtk_widget_set_style (GTK_BIN (sess->gui->panel_button)->child, normaltab_style);
   if (prefs.panelize_hide)
   {
      gtk_container_remove (GTK_CONTAINER (button->parent), button);
      gtk_widget_destroy (button);
   }
   gtk_widget_show (sess->gui->window);
   if (prefs.panelize_hide)
      sess->gui->panel_button = 0;
   if (main_window)             /* this fixes a little refresh glitch */
   {
      maingui_pagetofront (gtk_notebook_page_num (GTK_NOTEBOOK (main_book),
                           sess->gui->window));
   }
}

void
maingui_panelize (GtkWidget * button, struct session *sess)
{
   char tbuf[128];

   if (!panel_applet)
      create_panel_widget ();

   if (sess->gui->panel_button != NULL)
      return;

   if (prefs.panelize_hide)
      gtk_widget_hide (sess->gui->window);

   if (sess->channel[0] == 0)
      button = gtk_button_new_with_label ("<none>");
   else
      button = gtk_button_new_with_label (sess->channel);
   gtk_signal_connect (GTK_OBJECT (button), "clicked",
                       GTK_SIGNAL_FUNC (maingui_unpanelize), sess);
   gtk_container_add (GTK_CONTAINER (panel_box), button);
   gtk_signal_connect (GTK_OBJECT (button), "button_press_event", GTK_SIGNAL_FUNC (gui_panel_button_event), sess);
   gtk_widget_show (button);
   sess->gui->panel_button = button;

   if (sess->channel[0])
   {
      snprintf (tbuf, sizeof tbuf, "%s: %s", sess->server->servername, sess->channel);
      add_tip (button, tbuf);
   }
}
#endif
