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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 * =========================================================================
 *
 * xtext, the text widget used by X-Chat.
 *
 * By Peter Zelezny <zed@linux.com>.
 * Some functions used from Zvt and Eterm (transparency stuff).
 *
 */

#define REFRESH_TIMEOUT 20
#define WORDWRAP_LIMIT 24
#define TINT_VALUE 195           /* 195/255 of the brightness. */
#define MOTION_MONITOR 1         /* URL hilights. */
#define MARGIN 2
#undef USE_MITSHM                /* this causes problems on Solaris, plus it
                                    doesn't seem to be a performance advantage */

#include "../../config.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkselection.h>
#include <gdk/gdkx.h>
#include "xtext.h"
#ifdef USE_IMLIB
#include <gdk_imlib.h>
#endif

#undef GTK_WIDGET
#define GTK_WIDGET(n) ((GtkWidget*)n)
#undef GTK_OBJECT
#define GTK_OBJECT(n) ((GtkObject*)n)
#undef GTK_OBJECT_CLASS(n)
#define GTK_OBJECT_CLASS(n) ((GtkObjectClass*)n)

static GtkWidgetClass *parent_class = NULL;

enum 
{
   WORD_CLICK,
   LAST_SIGNAL
};
static guint xtext_signals[LAST_SIGNAL] = { 0 };

char *nocasestrstr (char *text, char *tofind);  /* util.c */
static void gtk_xtext_render_page (GtkXText *xtext, int startline);
static void gtk_xtext_calc_lines (GtkXText *xtext, int);
static void gtk_xtext_load_trans (GtkXText *xtext);
static void gtk_xtext_free_trans (GtkXText *xtext);
static textentry *gtk_xtext_nth (GtkXText *xtext, textentry *start_ent, int line, int width, int *subline);
static gint gtk_xtext_selection_kill (GtkWidget *widget, GdkEventSelection *event);
static void gtk_xtext_selection_get (GtkWidget        *widget, 
			GtkSelectionData *selection_data_ptr,
			guint             info,
			guint             time);
static int gtk_xtext_text_width (GtkXText *xtext, unsigned char *text, int len);
static void gtk_xtext_adjustment_changed (GtkAdjustment *adj, GtkXText *xtext);
static void gtk_xtext_draw_sep (GtkXText *xtext, int height);
static void gtk_xtext_render_ents (GtkXText *xtext, textentry *, textentry *, int startline, int);
static void gtk_xtext_recalc_widths (GtkXText *xtext, int);
static void gtk_xtext_fix_indent (GtkXText *xtext);


static int
is_del (char c)
{
   switch (c)
   {
   case ' ':
   case 0:
   case '\n':
   /*case '[':
   case ']':*/
   case ')':
   case '(':
   case '>':
   case '<':
      return 1;
   }
   return 0;
}

static void
gtk_xtext_init (GtkXText *xtext)
{
   xtext->old_value = -1;
   xtext->pixmap = NULL;
   xtext->text_first = NULL;
   xtext->text_last = NULL;
   xtext->io_tag = -1;
   xtext->add_io_tag = -1;
   xtext->scroll_tag = -1;
/*   xtext->frozen = 0;*/
   xtext->num_lines = 0;
   xtext->max_lines = 0;
   xtext->col_back = 19;
   xtext->col_fore = 18;
   xtext->nc = 0;
   xtext->scrollbar_down = TRUE;
   xtext->bold = FALSE;
   xtext->underline = FALSE;
   xtext->reverse = FALSE;
   xtext->font = NULL;
   xtext->xfont = NULL;
   xtext->error_function = NULL;
   xtext->urlcheck_function = NULL;
   xtext->color_paste = FALSE;
   xtext->tint_red = xtext->tint_green = xtext->tint_blue = TINT_VALUE;

   xtext->adj = (GtkAdjustment *) gtk_adjustment_new (0, 0, 0, 1, 0, 0);
   gtk_object_ref ((GtkObject*)xtext->adj);
   gtk_object_sink ((GtkObject*)xtext->adj);

   gtk_signal_connect (GTK_OBJECT (xtext->adj), "value_changed",
                       GTK_SIGNAL_FUNC (gtk_xtext_adjustment_changed), xtext);
   gtk_signal_connect (GTK_OBJECT (xtext), "selection_clear_event",
                       GTK_SIGNAL_FUNC (gtk_xtext_selection_kill), xtext);
   gtk_selection_add_target (GTK_WIDGET (xtext),
                             GDK_SELECTION_PRIMARY,
                             GDK_SELECTION_TYPE_STRING,
                             1);
   gtk_signal_connect (GTK_OBJECT (xtext), "selection_get",
                       GTK_SIGNAL_FUNC (gtk_xtext_selection_get), xtext);
}

static void
gtk_xtext_adjustment_set (GtkXText *xtext, int fire_signal)
{
   GtkAdjustment *adj = xtext->adj;

   adj->lower = 0;
   adj->upper = xtext->num_lines;

   adj->page_size = (GTK_WIDGET (xtext)->allocation.height - xtext->font->descent) / xtext->fontsize;
   adj->page_increment = adj->page_size;

   if (adj->value > adj->upper - adj->page_size)
      adj->value = adj->upper - adj->page_size;

   if (fire_signal)
      gtk_adjustment_changed (adj);
}

static gint
gtk_xtext_adjustment_timeout (GtkXText *xtext)
{
   gtk_xtext_render_page (xtext, xtext->adj->value);
   xtext->io_tag = -1;
   return 0;
}

static void
gtk_xtext_adjustment_changed (GtkAdjustment *adj, GtkXText *xtext)
{
/*   if (xtext->frozen)
      return;*/

   if ((int)xtext->old_value != (int)xtext->adj->value)
   {
      if (xtext->adj->value >= xtext->adj->upper - xtext->adj->page_size)
         xtext->scrollbar_down = TRUE;
      else
         xtext->scrollbar_down = FALSE;

      if (xtext->adj->value+1 == xtext->old_value ||
          xtext->adj->value-1 == xtext->old_value)    /* clicked an arrow? */
      {
         if (xtext->io_tag != -1)
         {
            gtk_timeout_remove (xtext->io_tag);
            xtext->io_tag = -1;
         }
         gtk_xtext_render_page (xtext, xtext->adj->value);
      } else
      {
         if (xtext->io_tag == -1)
            xtext->io_tag = gtk_timeout_add (REFRESH_TIMEOUT,
                                  (GtkFunction)gtk_xtext_adjustment_timeout,
                                          xtext);
      }
   }
   xtext->old_value = adj->value;
}

GtkWidget*
gtk_xtext_new (int indent)
{
   GtkXText *xtext;

   xtext = gtk_type_new (gtk_xtext_get_type ());
   xtext->indent = indent;
   xtext->wordwrap = FALSE;
   xtext->double_buffer = TRUE;

   return GTK_WIDGET (xtext);
}

static void
gtk_xtext_destroy (GtkObject *object)
{
   GtkXText *xtext = GTK_XTEXT (object);
   textentry *ent, *next;

   if (xtext->add_io_tag != -1)
   {
      gtk_timeout_remove (xtext->add_io_tag);
      xtext->add_io_tag = -1;
   }

   if (xtext->scroll_tag != -1)
   {
      gtk_timeout_remove (xtext->scroll_tag);
      xtext->scroll_tag = -1;
   }

   if (xtext->io_tag != -1)
   {
      gtk_timeout_remove (xtext->io_tag);
      xtext->io_tag = -1;
   }

   if (xtext->pixmap)
   {
      if (xtext->transparent)
         gtk_xtext_free_trans (xtext);
      else
         gdk_pixmap_unref (xtext->pixmap);
      xtext->pixmap = NULL;
   }

   if (xtext->font)
   {
      gdk_font_unref (xtext->font);
      xtext->font = NULL;
   }

   if (xtext->adj)
   {
      gtk_signal_disconnect_by_data (GTK_OBJECT (xtext->adj), xtext);
      gtk_object_unref (GTK_OBJECT (xtext->adj));
      xtext->adj = NULL;
   }

   if (xtext->bgc)
   {
      gdk_gc_destroy (xtext->bgc);
      xtext->bgc = NULL;
   }

   if (xtext->fgc)
   {
      gdk_gc_destroy (xtext->fgc);
      xtext->fgc = NULL;
   }

   if (xtext->hand_cursor)
   {
      gdk_cursor_destroy (xtext->hand_cursor);
      xtext->hand_cursor = NULL;
   }

   ent = xtext->text_first;
   while (ent)
   {
      next = ent->next;
      free (ent);
      ent = next;
   }
   xtext->text_first = NULL;

   if (GTK_OBJECT_CLASS (parent_class)->destroy)
      (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_xtext_realize (GtkWidget *widget)
{
   GtkXText *xtext;
   GdkWindowAttr attributes;
   GdkGCValues	val;
   int w, h;

   GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
   xtext = GTK_XTEXT (widget);

   attributes.x = widget->allocation.x;
   attributes.y = widget->allocation.y;
   attributes.width = widget->allocation.width;
   attributes.height = widget->allocation.height;
   attributes.wclass = GDK_INPUT_OUTPUT;
   attributes.window_type = GDK_WINDOW_CHILD;
   attributes.event_mask = gtk_widget_get_events (widget) | 
          GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
#ifdef MOTION_MONITOR
          | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK;
#else
          | GDK_POINTER_MOTION_MASK;
#endif

   attributes.visual = gtk_widget_get_visual (widget);
   attributes.colormap = gtk_widget_get_colormap (widget);

   widget->window = gdk_window_new (widget->parent->window, &attributes,
                       GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP);

   gdk_window_set_user_data (widget->window, widget);

   xtext->depth = gdk_window_get_visual (widget->window)->depth;

   val.subwindow_mode = GDK_INCLUDE_INFERIORS;
   val.graphics_exposures = 0;

   xtext->bgc = gdk_gc_new_with_values (widget->window, &val,
                                        GDK_GC_EXPOSURES | GDK_GC_SUBWINDOW);
   xtext->fgc = gdk_gc_new_with_values (widget->window, &val,
                                        GDK_GC_EXPOSURES | GDK_GC_SUBWINDOW);

   xtext->xbgc = ((GdkGCPrivate *)xtext->bgc)->xgc;
   xtext->xfgc = ((GdkGCPrivate *)xtext->fgc)->xgc;
   xtext->drawable = ((GdkWindowPrivate *)widget->window)->xwindow;
   xtext->display = ((GdkWindowPrivate *)widget->window)->xdisplay;

   if (xtext->fonttype != FONT_SET && xtext->xfont != NULL)
      XSetFont (xtext->display, xtext->xfgc, xtext->xfont->fid);

   XSetForeground (xtext->display, xtext->xfgc, xtext->palette[18]);
   XSetBackground (xtext->display, xtext->xfgc, xtext->palette[19]);
   XSetForeground (xtext->display, xtext->xbgc, xtext->palette[19]);

   if (xtext->transparent)
   {
      gtk_xtext_load_trans (xtext);
   } else if (xtext->pixmap)
   {
      gdk_gc_set_tile (xtext->bgc, xtext->pixmap);
      gdk_gc_set_ts_origin (xtext->bgc, 0, 0);
      gdk_gc_set_fill (xtext->bgc, GDK_TILED);
   }

   xtext->hand_cursor = gdk_cursor_new (GDK_HAND1);

   gdk_window_get_size (widget->window, &w, &h);

   XSetWindowBackgroundPixmap (xtext->display, xtext->drawable, None);
   XFillRectangle (xtext->display, xtext->drawable, xtext->xbgc, 0, 0, w, h);
 
   if (!xtext->double_buffer)
      xtext->draw_buf = xtext->drawable;

   xtext->indent = 1;
}

static void 
gtk_xtext_size_request (GtkWidget      *widget,
                        GtkRequisition *requisition)
{
   requisition->width = GTK_XTEXT (widget)->fontwidth['Z'] * 20;
   requisition->height = (GTK_XTEXT (widget)->fontsize * 10) + 3;
}

static void
gtk_xtext_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
   GtkXText *xtext = GTK_XTEXT (widget);

   if (allocation->width == widget->allocation.width &&
       allocation->height == widget->allocation.height &&
       allocation->x == widget->allocation.x &&
       allocation->y == widget->allocation.y)
      return;

   widget->allocation = *allocation;
   if (GTK_WIDGET_REALIZED (widget))
   {
      gdk_window_move_resize (widget->window,
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);
      gtk_xtext_calc_lines (xtext, FALSE);
   }
}

static gint
gtk_xtext_expose (GtkWidget *widget, GdkEventExpose *event)
{
   GtkXText *xtext = GTK_XTEXT (widget);

   gtk_xtext_render_page (xtext, xtext->adj->value);

   return FALSE;
}

static void
gtk_xtext_draw (GtkWidget *widget, GdkRectangle *area)
{
   int x, y;
   GtkXText *xtext = GTK_XTEXT (widget);
   Window childret;

   if (xtext->transparent)
   {
      XTranslateCoordinates (
         GDK_WINDOW_XDISPLAY (widget->window),
         GDK_WINDOW_XWINDOW (widget->window),
         GDK_ROOT_WINDOW (),
         0, 0,
         &x, &y,
         &childret);
      /* update transparency only if it moved */
      if (xtext->last_win_x != x ||
          xtext->last_win_y != y)
      {
         xtext->last_win_x = x;
         xtext->last_win_y = y;
         gtk_xtext_free_trans (xtext);
         gtk_xtext_load_trans (xtext);
      }
   }

   if (xtext->scrollbar_down)
      gtk_adjustment_set_value (xtext->adj, xtext->adj->upper - xtext->adj->page_size);
   gtk_xtext_render_page (xtext, xtext->adj->value);
}

static int
gtk_xtext_selection_clear (GtkXText *xtext)
{
   textentry *ent;
   int ret = 0;

   ent = xtext->text_first;
   while (ent)
   {
      if (ent->mark_start != -1)
         ret = 1;
      ent->mark_start = -1;
      ent->mark_end = -1;
      ent = ent->next;
   }

   return ret;
}

static int
find_x (GtkXText *xtext, textentry *ent, char *str, int x, int indent)
{
   int str_width;
   int len = 1;

   while (1)
   {
      str_width = gtk_xtext_text_width (xtext, str, len);
      if (str_width + indent >= x)
         return (str + len) - ent->str;
      len++;
      if (len + (str - ent->str) > ent->str_len)
         return ent->str_len;
      if (str_width + indent + 40 < x)
         len += 2;
   }
}

static int
gtk_xtext_find_x (GtkXText *xtext, int x, textentry *ent, int offset,
                  int line, int win_width, int *out_of_bounds)
{
   int indent;
   char *str;

   if (offset < 1)
      indent = ent->indent;
   else
      indent = xtext->indent;

   if (line > xtext->adj->page_size || line < 0)
      return 0;

   if (xtext->grid_offset[line] > ent->str_len)
      return 0;

   if (xtext->grid_offset[line] < 0)
      return 0;

   str = ent->str + xtext->grid_offset[line];

   if (x < indent)
   {
      *out_of_bounds = 1;
      return (str - ent->str);
   }

   *out_of_bounds = 0;

   return find_x (xtext, ent, str, x, indent);
}

static textentry *
gtk_xtext_find_char (GtkXText *xtext, int x, int y, int *off, int *out_of_bounds)
{
   textentry *ent;
   int line;
   int subline;
   int win_width;

   gdk_window_get_size (GTK_WIDGET (xtext)->window, &win_width, 0);
   win_width -= MARGIN;

   line = (y - xtext->font->descent) / xtext->fontsize;

   ent = gtk_xtext_nth (xtext, xtext->text_first, line + xtext->adj->value,
                        win_width, &subline);
   if (!ent)
      return 0;

   *off = gtk_xtext_find_x (xtext, x, ent, subline, line, win_width, out_of_bounds);

   return ent;
}

static gint
gtk_xtext_entrender_timeout (GtkXText *xtext)
{
   if (xtext->old_ent_start != xtext->last_ent_start ||
       xtext->old_ent_end != xtext->last_ent_end)
      gtk_xtext_render_ents (xtext, xtext->old_ent_start, xtext->old_ent_end,
                             xtext->adj->value, TRUE);
   gtk_xtext_render_ents (xtext, xtext->last_ent_start, xtext->last_ent_end,
                          xtext->adj->value, TRUE);
   xtext->io_tag = -1;
   return 0;
}

static void
gtk_xtext_selection_draw (GtkXText *xtext, int draw)
{
   textentry *ent;
   textentry *ent_end;
   textentry *ent_start;
   int offset_start;
   int offset_end;
   int low_x;
   int low_y;
   int high_x;
   int high_y;
   int tmp;

   if (xtext->select_start_y > xtext->select_end_y)
   {
      low_x = xtext->select_end_x;
      low_y = xtext->select_end_y;
      high_x = xtext->select_start_x;
      high_y = xtext->select_start_y;
   } else
   {
      low_x = xtext->select_start_x;
      low_y = xtext->select_start_y;
      high_x = xtext->select_end_x;
      high_y = xtext->select_end_y;
   }

   ent_start = gtk_xtext_find_char (xtext, low_x, low_y, &offset_start, &tmp);
   ent_end = gtk_xtext_find_char (xtext, high_x, high_y, &offset_end, &tmp);

   if (ent_start && !ent_end)
   {
      ent_end = xtext->text_last;
      offset_end = ent_end->str_len;
   }

   if (!ent_start || !ent_end)
   {
      if (xtext->adj->value != xtext->old_value)
         gtk_xtext_render_page (xtext, xtext->adj->value);
      return;
   }

   gtk_xtext_selection_clear (xtext);

   /* marking less than a complete line? */
   if (ent_start == ent_end)
   {
      ent_start->mark_start = MIN (offset_start, offset_end);
      ent_start->mark_end = MAX (offset_end, offset_start);
      if (offset_start == offset_end)
         ent_start->mark_end++;
   } else
   {
      ent_start->mark_start = offset_start;
      ent_start->mark_end = ent_start->str_len;

      if (offset_end != 0)
      {
         ent_end->mark_start = 0;
         ent_end->mark_end = offset_end;
      }
   }

   if (ent_start != ent_end)
   {
      ent = ent_start->next;
      while (ent && ent != ent_end)
      {
         ent->mark_start = 0;
         ent->mark_end = ent->str_len;
         ent = ent->next;
      }
   }

   /* has the selection changed? Dont render unless necessary */
   if (
      xtext->last_ent_start == ent_start &&
      xtext->last_ent_end == ent_end &&
      xtext->last_offset_start == offset_start &&
      xtext->last_offset_end == offset_end
      )
      return;

   if (draw)
   {
      if (xtext->double_buffer)
      {
         if (xtext->io_tag == -1)
            xtext->io_tag = gtk_timeout_add (REFRESH_TIMEOUT,
                                  (GtkFunction)gtk_xtext_adjustment_timeout,
                                          xtext);
      } else
      {
         if (xtext->io_tag == -1)
         {
            xtext->old_ent_start = xtext->last_ent_start;
            xtext->old_ent_end = xtext->last_ent_end;
            xtext->io_tag = gtk_timeout_add (REFRESH_TIMEOUT,
                                  (GtkFunction)gtk_xtext_entrender_timeout,
                                          xtext);
         }
     }
   }

   xtext->last_ent_start = ent_start;
   xtext->last_ent_end = ent_end;
   xtext->last_offset_start = offset_start;
   xtext->last_offset_end = offset_end;
}

static gint
gtk_xtext_scrolldown_timeout (GtkXText *xtext)
{
   int p_y, win_height;

   gdk_window_get_pointer (GTK_WIDGET (xtext)->window, 0, &p_y, 0);
   gdk_window_get_size (GTK_WIDGET (xtext)->window, 0, &win_height);

   if (p_y > win_height &&
       xtext->adj->value < (xtext->adj->upper - xtext->adj->page_size))
   {
      xtext->adj->value++;
      gtk_adjustment_changed (xtext->adj);
      gtk_xtext_render_page (xtext, xtext->adj->value);
      return 1;
   }

   xtext->scroll_tag = -1;
   return 0;
}

static gint
gtk_xtext_scrollup_timeout (GtkXText *xtext)
{
   int p_y;

   gdk_window_get_pointer (GTK_WIDGET (xtext)->window, 0, &p_y, 0);

   if (p_y < 0 &&
       xtext->adj->value > 0.0)
   {
      xtext->adj->value--;
      gtk_adjustment_changed (xtext->adj);
      gtk_xtext_render_page (xtext, xtext->adj->value);
      return 1;
   }

   xtext->scroll_tag = -1;
   return 0;
}

static void
gtk_xtext_selection_update (GtkXText *xtext, GdkEventMotion *event)
{
   int win_height;
   int moved;
   int p_y;

   gdk_window_get_size (GTK_WIDGET (xtext)->window, 0, &win_height);
   gdk_window_get_pointer (GTK_WIDGET (xtext)->window, 0, &p_y, 0);

   /* selecting past top of window, scroll up! */
   if (p_y < 0 && xtext->adj->value >= 0)
   {
      if (xtext->scroll_tag == -1)
         xtext->scroll_tag = gtk_timeout_add (100,
                              (GtkFunction)gtk_xtext_scrollup_timeout, xtext);
      return;
   }

   /* selecting past bottom of window, scroll down! */
   if (p_y > win_height &&
       xtext->adj->value < (xtext->adj->upper - xtext->adj->page_size))
   {
      if (xtext->scroll_tag == -1)
         xtext->scroll_tag = gtk_timeout_add (100,
                            (GtkFunction)gtk_xtext_scrolldown_timeout, xtext);
      return;
   }

   gtk_selection_owner_set (GTK_WIDGET (xtext), GDK_SELECTION_PRIMARY, event->time);

   moved = xtext->adj->value - xtext->select_start_adj;
   xtext->select_start_y -= (moved * xtext->fontsize);
   xtext->select_start_adj = xtext->adj->value;
   gtk_xtext_selection_draw (xtext, TRUE);
}

static char *
gtk_xtext_get_word (GtkXText *xtext, int x, int y, textentry **ret_ent, int *ret_off, int *ret_len)
{
   textentry *ent;
   int offset;
   char *str;
   char *word;
   int len;
   int out_of_bounds;

   ent = gtk_xtext_find_char (xtext, x, y, &offset, &out_of_bounds);
   if (!ent)
      return 0;

   if (out_of_bounds)
      return 0;

   if (offset == ent->str_len)
      return 0;

   if (offset < 1)
      return 0;

   offset--;

   str = ent->str + offset;

   while (!is_del(*str) && str != ent->str)
      str--;
   word = str+1;

   len = 0;
   str = word;
   while (!is_del(*str) && len != ent->str_len)
   {
      str++;
      len++;
   }

   if (ret_ent)
      *ret_ent = ent;
   if (ret_off)
      *ret_off = word - ent->str;
   if (ret_len)
      *ret_len = str - word;

   word = gtk_xtext_strip_color (word, len);

   return word;
}

static gint
gtk_xtext_leave_notify (GtkWidget *widget, GdkEventCrossing *event)
{
#ifdef MOTION_MONITOR
   GtkXText *xtext = GTK_XTEXT (widget);

   if (xtext->cursor_hand)
   {
      xtext->hilight_start = -1;
      xtext->hilight_end = -1;
      xtext->cursor_hand = FALSE;
      gdk_window_set_cursor (widget->window, 0);
      gtk_xtext_render_ents (xtext, xtext->hilight_ent, NULL, xtext->adj->value, FALSE);
   }
#endif
   return FALSE;
}

static gint
gtk_xtext_motion_notify (GtkWidget      *widget,
			                GdkEventMotion *event)
{
   GtkXText *xtext = GTK_XTEXT (widget);
   int tmp, x, y, offset, len;
   char *word;
   textentry *word_ent, *old_ent;

   gdk_window_get_pointer (widget->window, &x, &y, 0);

   if (xtext->moving_separator)
   {
      if (x < (4 * widget->allocation.width) / 5 && x > 15)
      {
         tmp = xtext->indent;
         xtext->indent = x;
         gtk_xtext_fix_indent (xtext);
         if (tmp != xtext->indent)
         {
            gtk_xtext_recalc_widths (xtext, FALSE);
            if (xtext->scrollbar_down)
               gtk_adjustment_set_value (xtext->adj, xtext->adj->upper -
                                         xtext->adj->page_size); 
            if (xtext->io_tag == -1)
               xtext->io_tag = gtk_timeout_add (REFRESH_TIMEOUT,
                                  (GtkFunction)gtk_xtext_adjustment_timeout,
                                          xtext);
         }
      }
      return FALSE;
   }

   if (xtext->button_down)
   {
      gtk_grab_add (widget);
      gdk_pointer_grab (widget->window, TRUE,
			GDK_BUTTON_RELEASE_MASK |
			GDK_BUTTON_MOTION_MASK,
		   NULL, NULL, 0);
      xtext->select_end_x = x;
      xtext->select_end_y = y;
      gtk_xtext_selection_update (xtext, event);
      return FALSE;
   }

#ifdef MOTION_MONITOR

   if (xtext->urlcheck_function == NULL)
      return FALSE;

   word = gtk_xtext_get_word (xtext, x, y, &word_ent, &offset, &len);
   if (word)
   {
      if (xtext->urlcheck_function (xtext, word) > 0)
      {
         free (word);
         if (!xtext->cursor_hand ||
             xtext->hilight_ent != word_ent ||
             xtext->hilight_start != offset ||
             xtext->hilight_end != offset + len)
         {
            xtext->cursor_hand = TRUE;
            gdk_window_set_cursor (GTK_WIDGET (xtext)->window, xtext->hand_cursor);
            old_ent = xtext->hilight_ent;
            xtext->hilight_ent = word_ent;
            xtext->hilight_start = offset;
            xtext->hilight_end = offset + len;
            gtk_xtext_render_ents (xtext, old_ent, word_ent, xtext->adj->value, FALSE);
            /*if (xtext->io_tag == -1)
               xtext->io_tag = gtk_timeout_add (REFRESH_TIMEOUT,
                                  (GtkFunction)gtk_xtext_adjustment_timeout,
                                          xtext);*/
         }
         return FALSE;
      }
      free (word);
   }

   gtk_xtext_leave_notify (widget, NULL);

#endif

   return FALSE;
}

static gint
gtk_xtext_button_release (GtkWidget      *widget,
			                 GdkEventButton *event)
{
   GtkXText *xtext = GTK_XTEXT (widget);
   char *word;

   if (xtext->moving_separator)
   {
      xtext->moving_separator = FALSE;
      if (event->x < (4 * widget->allocation.width) / 5 && event->x > 15)
      {
         xtext->indent = event->x;
      }
      gtk_xtext_fix_indent (xtext);
      gtk_xtext_recalc_widths (xtext, FALSE);
      gtk_xtext_render_page (xtext, xtext->adj->value);
      return FALSE;
   }

   if (xtext->word_or_line_select)
   {
      xtext->word_or_line_select = FALSE;
      xtext->button_down = FALSE;
      return FALSE;
   }

   if (event->button == 1)
   {
      xtext->button_down = FALSE;

      gtk_grab_remove (widget);
      gdk_pointer_ungrab (0);

      if (xtext->select_start_x == event->x &&
          xtext->select_start_y == event->y)
      {
         if (gtk_xtext_selection_clear (xtext))
            gtk_xtext_render_page (xtext, xtext->adj->value);
      } else
      {
         word = gtk_xtext_get_word (xtext, event->x, event->y, 0, 0, 0);
         if (word)
         {   
            gtk_signal_emit (GTK_OBJECT (xtext), xtext_signals[WORD_CLICK],
                             word, event);
            free (word);
            return FALSE;
         }
      }
   }

   return FALSE;
}

static gint
gtk_xtext_button_press (GtkWidget      *widget,
			               GdkEventButton *event)
{
   GtkXText *xtext = GTK_XTEXT (widget);
   textentry *ent;
   char *word;
   int line_x, x, y, offset, len;
   gfloat new_value;

   gdk_window_get_pointer (widget->window, &x, &y, 0);

   if (event->button == 3)    /* right click */
   {
      word = gtk_xtext_get_word (xtext, x, y, 0, 0, 0);
      if (word)
      {   
         gtk_signal_emit (GTK_OBJECT (xtext), xtext_signals[WORD_CLICK],
                          word, event);
         free (word);
      } else
         gtk_signal_emit (GTK_OBJECT (xtext), xtext_signals[WORD_CLICK],
                          "", event);
      return FALSE;
   }

   if (event->button == 4)    /* mouse wheel pageUp */
   {
      new_value = xtext->adj->value - xtext->adj->page_increment;
      if (new_value < xtext->adj->lower)
         new_value = xtext->adj->lower;
      gtk_adjustment_set_value (xtext->adj, new_value);
      return FALSE;
   }

   if (event->button == 5)    /* mouse wheel pageDn */
   {
      new_value = xtext->adj->value + xtext->adj->page_increment;
      if (new_value > (xtext->adj->upper - xtext->adj->page_size))
         new_value = xtext->adj->upper - xtext->adj->page_size;
      gtk_adjustment_set_value (xtext->adj, new_value);
      return FALSE;
   }

   if (event->button != 1)    /* we only want left button */
      return FALSE;

   if (event->type == GDK_2BUTTON_PRESS)     /* WORD select */
   {
      word = gtk_xtext_get_word (xtext, x, y, &ent, &offset, &len);
      if (word)
      {
         free (word);
         gtk_xtext_selection_clear (xtext);
         ent->mark_start = offset;
         ent->mark_end = offset + len;
         gtk_xtext_render_page (xtext, xtext->adj->value);
         xtext->word_or_line_select = TRUE;
         gtk_selection_owner_set (widget, GDK_SELECTION_PRIMARY, event->time);
      }

      return FALSE;
   }

   if (event->type == GDK_3BUTTON_PRESS)     /* LINE select */
   {
      word = gtk_xtext_get_word (xtext, x, y, &ent, 0, 0);
      if (word)
      {
         free (word);
         gtk_xtext_selection_clear (xtext);
         ent->mark_start = 0;
         ent->mark_end = ent->str_len;
         gtk_xtext_render_page (xtext, xtext->adj->value);
         xtext->word_or_line_select = TRUE;
         gtk_selection_owner_set (widget, GDK_SELECTION_PRIMARY, event->time);
      }

      return FALSE;
   }

   /* check if it was a separator-bar click */

   line_x = xtext->indent - ((xtext->space_width + 1) / 2);
   if (line_x == x || line_x == x+1 || line_x == x-1)
     {
       xtext->moving_separator = TRUE;
       gtk_xtext_render_page (xtext, xtext->adj->value);
       return FALSE;
     }
   

   xtext->button_down = TRUE;

   xtext->select_start_x = x;
   xtext->select_start_y = y;

   xtext->select_start_adj = xtext->adj->value;

   return FALSE;
}

/* another program has claimed the selection */

static gint
gtk_xtext_selection_kill (GtkWidget *widget, GdkEventSelection *event)
{
   if (gtk_xtext_selection_clear (GTK_XTEXT (widget)))
      gtk_xtext_render_page (GTK_XTEXT (widget), GTK_XTEXT (widget)->adj->value);
   return TRUE;
}

/* another program is asking for our selection */

static void
gtk_xtext_selection_get (GtkWidget        *widget, 
			GtkSelectionData *selection_data_ptr,
			guint             info,
			guint             time)
{
   GtkXText *xtext = GTK_XTEXT (widget);
   textentry *ent;
   char *txt;
   char *pos;
   char *stripped;
   int len;
   int first = TRUE;

   /* first find out how much we need to malloc ... */
   len = 0;
   ent = xtext->text_first;
   while (ent)
   {
      if (ent->mark_start != -1)
      {
         if (ent->mark_end - ent->mark_start > 0)
            len += (ent->mark_end - ent->mark_start) + 1;
         else
            len++;
      }
      ent = ent->next;
   }

   /* now allocate mem and copy buffer */
   pos = txt = malloc (len);
   ent = xtext->text_first;
   while (ent)
   {
      if (ent->mark_start != -1)
      {
         if (!first)
         {
            *pos = '\n';
            pos++;
         }
         first = FALSE;
         if (ent->mark_end - ent->mark_start > 0)
         {
            memcpy (pos, ent->str + ent->mark_start, ent->mark_end - ent->mark_start);
            pos += ent->mark_end - ent->mark_start;
         }
      }
      ent = ent->next;
   }
   *pos = 0;

   if (xtext->color_paste)
   {
      gtk_selection_data_set (selection_data_ptr, GDK_SELECTION_TYPE_STRING,
			                     8, txt, strlen (txt));
   } else
   {
      stripped = gtk_xtext_strip_color (txt, strlen (txt));
      gtk_selection_data_set (selection_data_ptr, GDK_SELECTION_TYPE_STRING,
			                     8, stripped, strlen (stripped));
      free (stripped);
   }

   free (txt);
}

static void
gtk_xtext_class_init (GtkXTextClass *class)
{
   GtkObjectClass *object_class;
   GtkWidgetClass *widget_class;
   GtkXTextClass *xtext_class;

   object_class = (GtkObjectClass*) class;
   widget_class = (GtkWidgetClass*) class;
   xtext_class = (GtkXTextClass*) class;

   parent_class = gtk_type_class (gtk_widget_get_type ());

   xtext_signals[WORD_CLICK] =
    gtk_signal_new ("word_click",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkXTextClass, word_click),
			           gtk_marshal_NONE__INT_POINTER,
			           GTK_TYPE_NONE, 2,
			           GTK_TYPE_INT,
			           GTK_TYPE_STRING);
   gtk_object_class_add_signals (object_class, xtext_signals, LAST_SIGNAL);

   object_class->destroy = gtk_xtext_destroy;

   widget_class->realize = gtk_xtext_realize;
   widget_class->size_request = gtk_xtext_size_request;
   widget_class->size_allocate = gtk_xtext_size_allocate;
   widget_class->button_press_event = gtk_xtext_button_press;
   widget_class->button_release_event = gtk_xtext_button_release;
   widget_class->motion_notify_event = gtk_xtext_motion_notify;
   widget_class->leave_notify_event = gtk_xtext_leave_notify;
   widget_class->draw = gtk_xtext_draw;
   widget_class->expose_event = gtk_xtext_expose;

   xtext_class->word_click = NULL;
}

guint
gtk_xtext_get_type ()
{
   static guint xtext_type = 0;

   if (!xtext_type)
   {
      GtkTypeInfo xtext_info =
      {
         "GtkXText",
         sizeof (GtkXText),
         sizeof (GtkXTextClass),
         (GtkClassInitFunc) gtk_xtext_class_init,
         (GtkObjectInitFunc) gtk_xtext_init,
         (GtkArgSetFunc) NULL,
         (GtkArgGetFunc) NULL,
      };

      xtext_type = gtk_type_unique (gtk_widget_get_type (), &xtext_info);
   }

   return xtext_type;
}

/* strip MIRC colors and other attribs. */

char *
gtk_xtext_strip_color (unsigned char *text, int len)
{
   int nc = 0;
   int i = 0;
   int col = FALSE;
   char *new_str = malloc (len + 2);

   while (len > 0)
   {
      if ((col && isdigit (*text) && nc < 2) ||
          (col && *text == ',' && nc < 3))
      {
         nc++;
         if (*text == ',')
            nc = 0;
      } else
      {
         if (col)
            col = FALSE;
         switch (*text)
         {
         case ATTR_COLOR:
            col = TRUE;
            nc = 0;
            break;
         case ATTR_BEEP:
         case ATTR_RESET:
         case ATTR_REVERSE:
         case ATTR_BOLD:
         case ATTR_UNDERLINE:
            break;
         default:
            new_str[i] = *text;
            i++;
         }
      }
      text++;
      len--;
   }

   new_str[i] = 0;

   return new_str;
}

/* gives width of a 8bit string - with no mIRC codes in it */

static int
gtk_xtext_text_width_simple (GtkXText *xtext, unsigned char *str, int len)
{
   int width = 0;

   /* check if it's a fixed width font */
   if (xtext->xfont->min_bounds.width == xtext->xfont->max_bounds.width)
   {
      return (xtext->xfont->min_bounds.width * len);
   }

   while (len)
   {
      width += xtext->fontwidth[*str];
      len--;
      str++;
   }

   return width;
}

/* gives width of a string, excluding the mIRC codes */

static int
gtk_xtext_text_width (GtkXText *xtext, unsigned char *text, int len)
{
   char *new_buf;
   int width;

   new_buf = gtk_xtext_strip_color (text, len);
   width = gdk_text_width (xtext->font, new_buf, strlen (new_buf));
   free (new_buf);

   return width;
}

/* actually draw text to screen */

static int
gtk_xtext_render_flush (GtkXText *xtext, int x, int y, char *str, int len, GC gc)
{
   int str_width;
   int dofill;

   if (xtext->dont_render)
      return 0;

   if (xtext->fonttype == FONT_1BYTE)
      str_width = gtk_xtext_text_width_simple (xtext, str, len);
   else
      str_width = gdk_text_width (xtext->font, str, len);

   if (!xtext->backcolor && xtext->pixmap)
   {
      dofill = FALSE;
      /* draw the background pixmap behind the text - CAUSES FLICKER HERE !! */
      if (!xtext->double_buffer)
      XFillRectangle (xtext->display, xtext->draw_buf, xtext->xbgc,
                      x, y - xtext->font->ascent, str_width, xtext->fontsize);
   } else
      dofill = TRUE;

   switch (xtext->fonttype)
   {
      case FONT_1BYTE:
         if (dofill)
            XDrawImageString (xtext->display, xtext->draw_buf, gc, x, y, str, len);
         else
            XDrawString (xtext->display, xtext->draw_buf, gc, x, y, str, len);
         if (xtext->bold)
            XDrawString (xtext->display, xtext->draw_buf, gc, x+1, y, str, len);
         break;

      case FONT_2BYTE:
         len /= 2;
         if (dofill)
            XDrawImageString16 (xtext->display, xtext->draw_buf,
		                          gc, x, y, (XChar2b *)str, len);
         else
            XDrawString16 (xtext->display, xtext->draw_buf,
		                     gc, x, y, (XChar2b *)str, len);
         if (xtext->bold)
            XDrawString16 (xtext->display, xtext->draw_buf,
		                     gc, x+1, y, (XChar2b *)str, len);
         break;

      case FONT_SET:
         if (dofill)
            XmbDrawImageString (xtext->display, xtext->draw_buf,
                                (XFontSet)xtext->xfont, gc, x, y, str, len);
         else
            XmbDrawString (xtext->display, xtext->draw_buf,
                           (XFontSet)xtext->xfont, gc, x, y, str, len);
         if (xtext->bold)
            XmbDrawString (xtext->display, xtext->draw_buf,
                           (XFontSet)xtext->xfont, gc, x+1, y, str, len);
   }

   if (xtext->underline)
      XDrawLine (xtext->display, xtext->draw_buf, gc, x, y+1, x + str_width, y+1);

   return str_width;
}

static void
gtk_xtext_reset (GtkXText *xtext, int mark, int attribs)
{
   if (attribs)
   {
      xtext->underline = FALSE;
      xtext->bold = FALSE;
   }
   if (!mark)
   {
      xtext->backcolor = FALSE;
      XSetForeground (xtext->display, xtext->xfgc, xtext->palette[18]);
      XSetBackground (xtext->display, xtext->xfgc, xtext->palette[19]);
   }
   xtext->col_fore = 18;
   xtext->col_back = 19;
}

/* render a single line, which WONT wrap, and parse mIRC colors */

static void
gtk_xtext_render_str (GtkXText *xtext, int y, textentry *ent, char *str, int len, int win_width, int indent, int line)
{
   GC gc;
   int i = 0, x = indent, j = 0;
   char *pstr = str;
   int col_num, tmp;
   int offset;
   int mark = FALSE;
   int hilight = FALSE;

   offset = str - ent->str;

   if (line < 255 && line > -1)
      xtext->grid_offset[line] = offset;

   gc = xtext->xfgc;       /* our foreground GC */

   if (ent->mark_start != -1 &&
       ent->mark_start <= i+offset &&
       ent->mark_end > i+offset)
   {
      XSetBackground (xtext->display, gc, xtext->palette[16]);
      XSetForeground (xtext->display, gc, xtext->palette[17]);
      xtext->backcolor = TRUE;
      mark = TRUE;
   }

#ifdef MOTION_MONITOR
   if (xtext->hilight_ent == ent &&
       xtext->hilight_start <= i+offset &&
       xtext->hilight_end > i+offset)
   {
      xtext->underline = TRUE;
/*      xtext->bold = TRUE;*/
      hilight = TRUE;
   }
#endif

   if (!xtext->double_buffer)
     /* draw background to the left of the text */
     /* fill the indent area with background gc */
     XFillRectangle (xtext->display, xtext->draw_buf, xtext->xbgc,
		     0, y - xtext->font->ascent, indent, xtext->fontsize);
  
   while (i < len)
   {

#ifdef MOTION_MONITOR
      if (xtext->hilight_ent == ent &&
          xtext->hilight_start == (i + offset))
      {
         x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);
         pstr += j;
         j = 0;
         xtext->underline = TRUE;
/*         xtext->bold = TRUE;*/
         hilight = TRUE;
      }
#endif

      if (!mark && ent->mark_start == (i + offset))
      {
         x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);
         pstr += j;
         j = 0;
         XSetBackground (xtext->display, gc, xtext->palette[16]);
         XSetForeground (xtext->display, gc, xtext->palette[17]);
         xtext->backcolor = TRUE;
         mark = TRUE;
      }

      if ( (xtext->parsing_color && isdigit (str[i]) && xtext->nc < 2) ||
           (xtext->parsing_color && str[i] == ',' && xtext->nc < 3)        )
      {
         pstr++;
         if (str[i] == ',')
         {
            xtext->parsing_backcolor = TRUE;
            if (xtext->nc)
            {
               xtext->num[xtext->nc] = 0;
               xtext->nc = 0;
               col_num = atoi (xtext->num) % 16;
               xtext->col_fore = col_num;
               if (!mark)
                  XSetForeground (xtext->display, gc, xtext->palette[col_num]);
            }
         } else
         {
            xtext->num[xtext->nc] = str[i];
            if (xtext->nc < 7)
               xtext->nc++;
         }
      } else
      {
         if (xtext->parsing_color)
         {
            xtext->parsing_color = FALSE;
            if (xtext->nc)
            {
               xtext->num[xtext->nc] = 0;
               xtext->nc = 0;
               col_num = atoi (xtext->num) % 16;
               if (xtext->parsing_backcolor)
               {
                  if (col_num == 1)
                     xtext->backcolor = FALSE;
                  else
                     xtext->backcolor = TRUE;
                  if (!mark)
                     XSetBackground (xtext->display, gc, xtext->palette[col_num]);
                  xtext->col_back = col_num;
               } else
               {
                  if (!mark)
                     XSetForeground (xtext->display, gc, xtext->palette[col_num]);
                  xtext->col_fore = col_num;
               }
               xtext->parsing_backcolor = FALSE;
            } else
            {
               /* got a \003<non-digit>... i.e. reset colors */
               x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);
               pstr += j;
               j = 0;
               gtk_xtext_reset (xtext, mark, FALSE);
            }
         }

         switch (str[i])
         {
         case '\n':
            break;   
         case ATTR_BEEP:
            break;
         case ATTR_REVERSE:
            x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);
            pstr += j+1;
            j = 0;
            tmp = xtext->col_fore;
            xtext->col_fore = xtext->col_back;
            xtext->col_back = tmp;
            if (!mark)
            {
               XSetForeground (xtext->display, gc, xtext->palette[xtext->col_fore]);
               XSetBackground (xtext->display, gc, xtext->palette[xtext->col_back]);
            }
            if (xtext->col_back != 19)
               xtext->backcolor = TRUE;
            else
               xtext->backcolor = FALSE;
            break;
         case ATTR_BOLD:
            x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);
            xtext->bold = !xtext->bold;
            pstr += j+1;
            j = 0;
            break;
         case ATTR_UNDERLINE:
            x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);
            xtext->underline = !xtext->underline;
            pstr += j+1;
            j = 0;
            break;
         case ATTR_RESET:
            x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);
            pstr += j+1;
            j = 0;
            gtk_xtext_reset (xtext, mark, !hilight);
            break;
         case ATTR_COLOR:
            x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);
            xtext->parsing_color = TRUE;
            pstr += j+1;
            j = 0;
            break;
         default:
            j++;
         }
      }
      i++;

#ifdef MOTION_MONITOR
      if (xtext->hilight_ent == ent &&
          xtext->hilight_end == (i + offset))
      {
         x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);
         pstr += j;
         j = 0;
         xtext->underline = FALSE;
/*         xtext->bold = FALSE;*/
         hilight = FALSE;
      }
#endif

      if (mark && ent->mark_end == (i + offset))
      {
         x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);
         pstr += j;
         j = 0;
         XSetBackground (xtext->display, gc, xtext->palette[xtext->col_back]);
         XSetForeground (xtext->display, gc, xtext->palette[xtext->col_fore]);
         if (xtext->col_back != 19)
            xtext->backcolor = TRUE;
         else
            xtext->backcolor = FALSE;
         mark = FALSE;
      }
   }

   if (j)
      x += gtk_xtext_render_flush (xtext, x, y, pstr, j, gc);

   if (!xtext->double_buffer)
   {
      /* draw separator now so it doesn't appear to flicker */
      gtk_xtext_draw_sep (xtext, y + 1);
      /* draw background to the right of the text */
      XFillRectangle (xtext->display, xtext->draw_buf, xtext->xbgc,
                      x, y - xtext->font->ascent, (win_width + MARGIN) - x,
                      xtext->fontsize);
   }
}

/* get the desktop/root window - thanks Eterm */

static Window desktop_window = None;

static Window
get_desktop_window (Window the_window)
{
  Atom prop, type, prop2;
  int format;
  unsigned long length, after;
  unsigned char *data;
  unsigned int nchildren;
  Window w, root, *children, parent;
  
  prop = XInternAtom(GDK_DISPLAY(), "_XROOTPMAP_ID", True);
  prop2 = XInternAtom(GDK_DISPLAY(), "_XROOTCOLOR_PIXEL", True);

  if (prop == None && prop2 == None)
    return None;

  for (w = the_window; w; w = parent) {
    if ((XQueryTree(GDK_DISPLAY(), w, &root, &parent, &children, &nchildren)) == False) 
      return None;

    if (nchildren) 
      XFree(children);

    if (prop != None) {
      XGetWindowProperty(GDK_DISPLAY(), w, prop, 0L, 1L, False, AnyPropertyType,
			 &type, &format, &length, &after, &data);
    } else if (prop2 != None) {
      XGetWindowProperty(GDK_DISPLAY(), w, prop2, 0L, 1L, False, AnyPropertyType,
			 &type, &format, &length, &after, &data);
    } else  {
      continue;
    }
    
    if (type != None) {
      return (desktop_window = w);
    }
  }
  
  return (desktop_window = None);
}

/* stolen from zvt, which was stolen from Eterm */

static Pixmap
get_pixmap_prop (Window the_window)
{
   Atom prop, type;
   int format;
   unsigned long length, after;
   unsigned char *data;
   Pixmap pix = None;

   if (desktop_window == None)
      desktop_window = get_desktop_window (the_window);
   if (desktop_window == None)
      desktop_window = GDK_ROOT_WINDOW ();

   prop = XInternAtom(GDK_DISPLAY(), "_XROOTPMAP_ID", True);
   if (prop == None)
      return None;

   XGetWindowProperty (GDK_DISPLAY(), desktop_window, prop, 0L, 1L, False,
                      AnyPropertyType, &type, &format, &length, &after,
                      &data);
   if (data)
   {
      if (type == XA_PIXMAP)
         pix = *((Pixmap *)data);

      XFree (data);
   }

   return pix;
}

#ifdef USE_IMLIB

static GdkPixmap *
create_shaded_pixmap (GtkXText *xtext, Pixmap p, int x, int y, int w, int h)
{
  GdkPixmap *pix,*pp,*tmp;
  GdkImlibColorModifier mod;
  GdkImlibImage *iim;
  GdkGC *tgc;
  int width,height,depth;

  pp = gdk_pixmap_foreign_new(p);
  gdk_window_get_geometry(pp,NULL,NULL,&width,&height,&depth);

  if (width<x+w || height<y+h || x<0 || y<0) {
    tgc = gdk_gc_new(pp);
    tmp = gdk_pixmap_new(pp,w,h,depth);
    gdk_gc_set_tile (tgc, pp);
    gdk_gc_set_fill (tgc, GDK_TILED);
    gdk_gc_set_ts_origin(tgc, -x, -y);
    gdk_draw_rectangle (tmp, tgc, TRUE, 0, 0, w, h);
    gdk_gc_destroy(tgc);

    iim = gdk_imlib_create_image_from_drawable(tmp,
					       NULL, 0, 0, w, h);
    gdk_pixmap_unref(tmp);
  } else {
    iim = gdk_imlib_create_image_from_drawable(pp,
					       NULL, x, y, w, h);
  }
  gdk_xid_table_remove (GDK_WINDOW_XWINDOW(pp));
  g_dataset_destroy (pp);
  g_free (pp);

  if(!iim)
    return NULL;

   mod.contrast = 256;
   mod.gamma = 256;

   if (xtext->tint_red == xtext->tint_blue &&
       xtext->tint_blue == xtext->tint_green)
   {
      mod.brightness = xtext->tint_red;
      gdk_imlib_set_image_modifier (iim, &mod);
   } else
   {
      mod.brightness = xtext->tint_red;
      gdk_imlib_set_image_red_modifier (iim, &mod);
      mod.brightness = xtext->tint_green;
      gdk_imlib_set_image_green_modifier (iim, &mod);
      mod.brightness = xtext->tint_blue;
      gdk_imlib_set_image_blue_modifier (iim, &mod);
   }
  gdk_imlib_render (iim, iim->rgb_width, iim->rgb_height);
  pix = gdk_imlib_move_image (iim);
  gdk_imlib_destroy_image (iim);
  
  return pix;
}

#endif

/* free transparency xtext->pixmap */

static void
gtk_xtext_free_trans (GtkXText *xtext)
{
   if (xtext->pixmap)
   {
      if (xtext->shaded)
      {
         gdk_pixmap_unref (xtext->pixmap);
      } else
      {
         gdk_xid_table_remove (GDK_WINDOW_XWINDOW (xtext->pixmap));
         g_dataset_destroy (xtext->pixmap);
         g_free (xtext->pixmap);
      }
      xtext->pixmap = NULL;
   }
}

/* grab pixmap from root window and set xtext->pixmap */

static void
gtk_xtext_load_trans (GtkXText *xtext)
{
   Pixmap rootpix;
   Window childret;
   GtkWidget *widget = GTK_WIDGET (xtext);
   int x, y;

   rootpix = get_pixmap_prop (GDK_WINDOW_XWINDOW(widget->window));
   if (rootpix == None)
   {
      if (xtext->error_function)
         xtext->error_function (
                                "Unable to get root window pixmap!\n\n"
                                "You may need to use Esetroot or Gnome\n"
                                "control-center to set your background.\n"
                               );
      xtext->transparent = FALSE;
      return;
   }

   XTranslateCoordinates (
      GDK_WINDOW_XDISPLAY (widget->window),
      GDK_WINDOW_XWINDOW (widget->window),
      GDK_ROOT_WINDOW (),
      0, 0,
      &x, &y,
      &childret);

#ifdef USE_IMLIB
   if (xtext->shaded)
   {
      int width, height;
      gdk_window_get_size(GTK_WIDGET (xtext)->window, &width, &height);
      xtext->pixmap = create_shaded_pixmap (xtext, rootpix, x, y, width, height);
      gdk_gc_set_tile (xtext->bgc, xtext->pixmap);
      gdk_gc_set_ts_origin (xtext->bgc, 0, 0);
   } else
   {
#else
#warning disabling tinted-transparency support
#endif
      xtext->pixmap = gdk_pixmap_foreign_new (rootpix);
      gdk_gc_set_tile (xtext->bgc, xtext->pixmap);
      gdk_gc_set_ts_origin (xtext->bgc, -x, -y);
#ifdef USE_IMLIB
   }
#endif
   gdk_gc_set_fill (xtext->bgc, GDK_TILED);
}

/* render a single line, which may wrap to more lines */

static int
gtk_xtext_render_line (GtkXText *xtext, textentry *ent, char *str, int len, int line, int lines_max, int subline, int indent, int str_width)
{
   int y;
   int width;
   int orig_len;
   int ret = 1;
   int tmp;

   if (len == -1)
      len = strlen (str);
   orig_len = len;

   gdk_window_get_size (GTK_WIDGET (xtext)->window, &width, 0);
   width -= MARGIN;

startrl:

   y = (xtext->fontsize * line) + xtext->font->ascent;

   if (str_width == -1)
      str_width = gtk_xtext_text_width (xtext, str, len);

   str_width += indent;

   tmp = 0;
   while (str_width > width || (!is_del(str[len]) && xtext->wordwrap))
   {
      if (str_width <= width && !tmp)
         tmp = len;
      len--;
      if (xtext->wordwrap && tmp - len > WORDWRAP_LIMIT)
      {
         len = tmp;
         str_width = gtk_xtext_text_width (xtext, str, len) + indent;
         break;
      }
      if (len == 0)
         return 1;

      /* this is quite a HACK but it speeds things up! */
      if (str_width > width + 256)
         len -= 10;
      if (str_width > width + 50)
         len -= 2;
      /* -- */

      str_width = gtk_xtext_text_width (xtext, str, len) + indent;
   }

   if (xtext->wordwrap && str[len] == ' ')
      len++;

   if (!subline)
   {
      gtk_xtext_render_str (xtext, y, ent, str, len, width, indent, line);
   } else
   {
      xtext->dont_render = TRUE;
      gtk_xtext_render_str (xtext, y, ent, str, len, width, indent, line);
      xtext->dont_render = FALSE;
      subline--;
      line--;
      ret--;
   }

   if (len != orig_len && lines_max > line+1)
   {  /* FIXME: recursion sux! */
/*      ret += gtk_xtext_render_line (xtext, ent, str + len, -1, line+1, lines_max, subline, xtext->indent, -1);*/
      ret++;
      str += len;
      len = orig_len = strlen (str);
      line++;
      indent = xtext->indent;
      str_width = -1;
      /* FIXME: gotos suck! */
      goto startrl;
   }

   return ret;
}

void
gtk_xtext_set_palette (GtkXText *xtext, GdkColor palette[])
{
   int i;

   for (i=0; i<20; i++)
      xtext->palette[i] = palette[i].pixel;

   if (GTK_WIDGET_REALIZED (xtext))
   {
      XSetForeground (xtext->display, xtext->xfgc, xtext->palette[18]);
      XSetBackground (xtext->display, xtext->xfgc, xtext->palette[19]);
      XSetForeground (xtext->display, xtext->xbgc, xtext->palette[19]);
   }
   xtext->col_fore = 18;
   xtext->col_back = 19;
}

static void
gtk_xtext_fix_indent (GtkXText *xtext)
{
  int j;
  
  /* make indent a multiple of the space width */
  
  j = 0;
  while (j < xtext->indent)
    {
      j += xtext->space_width;
    }
  xtext->indent = j;
}

static void
gtk_xtext_recalc_widths (GtkXText *xtext, int do_str_width)
{
   textentry *ent;

   /* since we have a new font, we have to recalc the text widths */
   ent = xtext->text_first;
   while (ent)
   {
      if (do_str_width)
      {
         ent->str_width = gtk_xtext_text_width (xtext, ent->str, ent->str_len);
      }
      if (ent->left_len != -1)
      {
         ent->indent = (xtext->indent - gtk_xtext_text_width (xtext, ent->str, ent->left_len)) - xtext->space_width;
         if (ent->indent < MARGIN)
            ent->indent = MARGIN;
      }
      ent = ent->next;
   }

   gtk_xtext_calc_lines (xtext, FALSE);
}

void
gtk_xtext_set_font (GtkXText *xtext, GdkFont *font, char *name)
{
   unsigned char i;

   if (xtext->font)
      gdk_font_unref (xtext->font);

   if (font)
   {
      xtext->font = font;
      gdk_font_ref (font);
   } else
      font = xtext->font = gdk_font_load (name);

   if (!font)
      font = xtext->font = gdk_font_load ("fixed");

   switch (font->type)
   {
      case GDK_FONT_FONT:
      {
         XFontStruct *xfont;
         xfont = (XFontStruct *)GDK_FONT_XFONT(font);
         xtext->fontsize = xtext->font->ascent + xtext->font->descent;
         if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
         {
            xtext->fonttype = FONT_1BYTE;
            for (i=0; i<255; i++)
            {
               xtext->fontwidth[i] = gdk_char_width (font, i);
            }
            xtext->space_width = xtext->fontwidth[' '];
         } else
         {
            xtext->fonttype = FONT_2BYTE;
            xtext->space_width = gdk_char_width (font, ' ');
         }
         break;
      }
      case GDK_FONT_FONTSET:
      {
         XFontSet fontset = (XFontSet) ((GdkFontPrivate *)font)->xfont;
         XFontSetExtents *extents = XExtentsOfFontSet(fontset);
         xtext->fontsize = extents->max_logical_extent.height;
         xtext->fonttype = FONT_SET;
         xtext->space_width = XmbTextEscapement (fontset, " ", 1);
         break;
      }
   }

   xtext->xfont = (XFontStruct *)((GdkFontPrivate*)xtext->font)->xfont;

   gtk_xtext_fix_indent (xtext);
   
   if (GTK_WIDGET_REALIZED (xtext))
   {
      if (xtext->fonttype != FONT_SET)
         XSetFont (xtext->display, xtext->xfgc, xtext->xfont->fid);

      gtk_xtext_recalc_widths (xtext, TRUE);
   }
}

void
gtk_xtext_set_background (GtkXText *xtext, GdkPixmap *pixmap, int trans, int shaded)
{
#ifndef USE_IMLIB
   shaded = FALSE;
#endif

   if (xtext->pixmap)
   {
      if (xtext->transparent)
         gtk_xtext_free_trans (xtext);
      else
         gdk_pixmap_unref (xtext->pixmap);
      xtext->pixmap = NULL;
   }

   xtext->transparent = trans;

   if (trans)
   {
      xtext->shaded = shaded;
      if (GTK_WIDGET_REALIZED (xtext))
         gtk_xtext_load_trans (xtext);
      return;
   }

   xtext->pixmap = pixmap;

   if (pixmap != 0)
   {
      gdk_pixmap_ref (pixmap);
      if (GTK_WIDGET_REALIZED (xtext))
      {
         gdk_gc_set_tile (xtext->bgc, pixmap);
         gdk_gc_set_ts_origin (xtext->bgc, 0, 0);
         gdk_gc_set_fill (xtext->bgc, GDK_TILED);
      }
   } else
   {
      if (GTK_WIDGET_REALIZED (xtext))
      {
         gdk_gc_destroy (xtext->bgc);
         xtext->bgc = gdk_gc_new (GTK_WIDGET (xtext)->window);
         xtext->xbgc = ((GdkGCPrivate *)xtext->bgc)->xgc;
         XSetForeground (xtext->display, xtext->xbgc, xtext->palette[19]);
         XSetGraphicsExposures (xtext->display, xtext->xbgc, False);
         XSetSubwindowMode (xtext->display, xtext->xbgc, IncludeInferiors);
      }
   }
}

gchar* gtk_xtext_get_chars(GtkXText *xtext)
{
    int lenght=0;
    gchar *chars;
    textentry *tentry=xtext->text_first;
    while(tentry!=NULL)
    {
        lenght+=tentry->str_len+1;
        tentry=tentry->next;
    }
    if (lenght==0) return NULL;
    chars=g_malloc (lenght+1);
    *chars = 0;

    tentry=xtext->text_first;
    while(tentry!=NULL)
    {
        strcat(chars,tentry->str);
        strcat(chars,"\n");
        tentry=tentry->next;
    }

    return chars;
}

static int
gtk_xtext_lines_taken (GtkXText *xtext, textentry *ent)
{
   int tmp, orig_len, indent, len, win_width, str_width, lines = 0;
   char *str;

   str = ent->str;
   len = orig_len = ent->str_len;
   indent = ent->indent;

   if (len < 2)
      return 1;

   win_width = GTK_WIDGET (xtext)->allocation.width - MARGIN;
   str_width = ent->str_width + indent;

   while (1)
   {
      lines++;
      if (str_width <= win_width)
         break;
      tmp = 0;
      while (str_width > win_width || (!is_del(str[len]) && xtext->wordwrap))
      {
         if (str_width <= win_width && !tmp)
            tmp = len;
         len--;
         if (xtext->wordwrap && tmp - len > WORDWRAP_LIMIT)
         {
            len = tmp;
            str_width = gtk_xtext_text_width (xtext, str, len) + indent;
            break;
         }
         if (len == 0)
            return 1;
         if (str_width > win_width + 256)  /* this might not work 100% but it */
            len -= 10;                     /* sure speeds things up ALOT!     */
         str_width = gtk_xtext_text_width (xtext, str, len) + indent;
      }

      if (len == orig_len)
         break;

      if (xtext->wordwrap && str[len] == ' ')
         len++;

      str += len;
      indent = xtext->indent;
      len = strlen (str);
      str_width = gtk_xtext_text_width (xtext, str, len) + indent;
   }
   return lines;
}

/* Calculate number of actual lines (with wraps), to set adj->lower. *
 * This should only be called when the window resizes.               */

static void
gtk_xtext_calc_lines (GtkXText *xtext, int fire_signal)
{
   textentry *ent;
   int width;
   int height;
   int lines;

   width = GTK_WIDGET (xtext)->allocation.width - MARGIN;
   height = GTK_WIDGET (xtext)->allocation.height;

   if (width < 30 || height < xtext->fontsize || width < xtext->indent + 30)
      return;

   lines = 0;
   ent = xtext->text_first;
   while (ent)
   {
      ent->lines_taken = gtk_xtext_lines_taken (xtext, ent);
      lines += ent->lines_taken;
      ent = ent->next;
   }

   xtext->last_line = -1;
   xtext->num_lines = lines;
   gtk_xtext_adjustment_set (xtext, fire_signal);
}

/* find the n-th line in the linked list, this includes wrap calculations */

static textentry *
gtk_xtext_nth (GtkXText *xtext, textentry *ent, int line, int width, int *subline)
{
   int lines = 0;

   if (xtext->last_line == line) /* take a shortcut? */
   {
      *subline = xtext->last_subline;
      return xtext->last_ent;
   }

   while (ent)
   {
      lines += ent->lines_taken;
      if (lines > line)
      {
         *subline = ent->lines_taken - (lines - line);
         return ent;
      }
      ent = ent->next;
   }
   return 0;
}

static void
gtk_xtext_draw_sep (GtkXText *xtext, int y)
{
   int x, height;
   GdkGC *light, *dark;

   if (y == -1)
   {
      y = 2;
      height = GTK_WIDGET (xtext)->allocation.height - 2;
   } else
   {
      height = xtext->fontsize;
   }

   /* draw the separator line */
   
   light = GTK_WIDGET (xtext)->style->light_gc[0];
   dark = GTK_WIDGET (xtext)->style->dark_gc[0];
   
   if (light == 0 || dark == 0)
     return;
   
   /* this should never happen, but i've seen Backtraces where it has! */
   if (((GdkGCPrivate *)light)->xgc == 0 || ((GdkGCPrivate *)dark)->xgc == 0)
     return;
   
   x = xtext->indent - ((xtext->space_width + 1) / 2);
   if (x < 1)
     return;
   
   if (xtext->moving_separator)
     XDrawLine (xtext->display, xtext->draw_buf,
		((GdkGCPrivate *)light)->xgc,
		x, y, x, height);
   else
     XDrawLine (xtext->display, xtext->draw_buf,
		((GdkGCPrivate *)dark)->xgc,
		x, y, x, height);
}

/* render 2 ents (or an inclusive range) */

static void
gtk_xtext_render_ents (GtkXText *xtext, textentry *enta, textentry *entb, int startline, int inclusive)
{
   textentry *ent, *orig_ent, *tmp_ent;
   int line;
   int lines_taken;
   int lines_max;
   int width;
   int height;
   int subline;
   int drawing = FALSE;

   if (xtext->double_buffer)
   {
      gtk_xtext_render_page (xtext, startline);
      return;
   }

   if (xtext->indent < MARGIN)
      xtext->indent = MARGIN;      /* 2 pixels is our left margin */

   gdk_window_get_size (GTK_WIDGET (xtext)->window, &width, &height);
   width -= MARGIN;

   if (width < 32 || height < xtext->fontsize || width < xtext->indent + 30)
      return;

   lines_max = (height - xtext->font->descent) / xtext->fontsize;

   subline = line = 0;
   orig_ent = xtext->text_first;

   if (startline > 0)
      orig_ent = gtk_xtext_nth (xtext, orig_ent, startline, width, &subline);

   /* check if enta is before the start of this page */
   if (inclusive)
   {
      tmp_ent = orig_ent;
      while (tmp_ent)
      {
         if (tmp_ent == enta)
            break;
         if (tmp_ent == entb)
         {
            drawing = TRUE;
            break;
         }
         tmp_ent = tmp_ent->next;
      }
   }

   xtext->last_line = startline;
   xtext->last_ent = orig_ent;
   xtext->last_subline = subline;

   line = 0;

   ent = orig_ent;
   while (ent)
   {
      if (inclusive && ent == enta)
            drawing = TRUE;

      if (drawing || ent == entb || ent == enta)
      {
         gtk_xtext_reset (xtext, FALSE, TRUE);
         lines_taken = gtk_xtext_render_line (xtext, ent, ent->str,
                                              ent->str_len, line, lines_max,
                                              subline, ent->indent,
                                              ent->str_width);
         line += ent->lines_taken;
         line -= subline;
         if (ent == orig_ent)
            subline = 0;
      } else
      {
         if (ent == orig_ent)
         {
            line -= subline;
            subline = 0;
         }
         line += ent->lines_taken;
      }

      if (inclusive && ent == entb)
         break;

      if (line >= lines_max)
         break;

      ent = ent->next;
   }

   /* draw the separator line */
   gtk_xtext_draw_sep (xtext, -1);
}

#ifdef USE_MITSHM

static void
gtk_xtext_kill_buffer (GtkXText *xtext)
{
   if (xtext->img != NULL)
   {
      XSync (xtext->display, False);
      XShmDetach (xtext->display, &xtext->shminfo);
      XDestroyImage (xtext->img);
      shmdt (xtext->shminfo.shmaddr);
   }
   XFreePixmap (xtext->display, xtext->tmp_pix);
}

static Pixmap
gtk_xtext_create_buffer (GtkXText *xtext, int width, int height)
{
   int major, minor;
   Bool pixmaps, XSh;

   XSh = XShmQueryVersion (xtext->display, &major, &minor, &pixmaps);
   if (XSh != True || pixmaps != True)
   {
/*      fprintf (stderr, "** NO MIT-SHM!\n");*/
      xtext->img = NULL;
      return XCreatePixmap (xtext->display, xtext->drawable,
                            width, height, xtext->depth);
   }

/*   fprintf (stderr, "** USING MIT-SHM!\n");*/

   xtext->img = XShmCreateImage (xtext->display,
                  DefaultVisual(xtext->display, DefaultScreen(xtext->display)),
                          xtext->depth, ZPixmap,
                          NULL, &xtext->shminfo, width, height);
/*   printf ("img: %p\n", xtext->img);*/

   /* allocate shared memory segment */
   xtext->shminfo.shmid = shmget (IPC_PRIVATE, xtext->img->bytes_per_line *
                                  xtext->img->height, IPC_CREAT|0777);
/*   printf ("shmid: 0x%x\n", xtext->shminfo.shmid);*/

   /* attach this memory to our process */
   xtext->shminfo.readOnly = False;
   xtext->shminfo.shmaddr = xtext->img->data = shmat (xtext->shminfo.shmid, 0, 0);
/*   printf ("shmaddr: %p\n", xtext->shminfo.shmaddr);*/

   XShmAttach (xtext->display, &xtext->shminfo);

   shmctl (xtext->shminfo.shmid, IPC_RMID, 0);

   return XShmCreatePixmap (xtext->display, xtext->drawable,
                            xtext->shminfo.shmaddr, &xtext->shminfo,
                            width, height, xtext->depth);
}

#endif

/* render a whole page/window, starting from 'startline' */

static void
gtk_xtext_render_page (GtkXText *xtext, int startline)
{
   textentry *ent;
   int line;
   int lines_max;
   int width;
   int height;
   int subline;

   if (xtext->indent < MARGIN)
      xtext->indent = MARGIN;      /* 2 pixels is our left margin */

   gdk_window_get_size (GTK_WIDGET (xtext)->window, &width, &height);
   width -= MARGIN;

   if (width < 32 || height < xtext->fontsize || width < xtext->indent + 30)
      return;

   lines_max = (height - xtext->font->descent) / xtext->fontsize;

   subline = line = 0;
   ent = xtext->text_first;

   if (startline > 0)
      ent = gtk_xtext_nth (xtext, ent, startline, width, &subline);

   xtext->last_line = startline;
   xtext->last_ent = ent;
   xtext->last_subline = subline;

   if (xtext->double_buffer)
   {
#ifdef USE_MITSHM
      xtext->tmp_pix = gtk_xtext_create_buffer (xtext, width+MARGIN, height);
#else
      xtext->tmp_pix = XCreatePixmap (xtext->display, xtext->drawable,
                                      width+MARGIN, height, xtext->depth);
#endif
      xtext->draw_buf = xtext->tmp_pix;
      /* render the backdrop */
      XFillRectangle (xtext->display, xtext->draw_buf, xtext->xbgc,
                      0, 0, width+MARGIN, height);
   }

   while (ent)
   {
      gtk_xtext_reset (xtext, FALSE, TRUE);
      line += gtk_xtext_render_line (xtext, ent, ent->str, ent->str_len, line, lines_max, subline, ent->indent, ent->str_width);
      subline = 0;

      if (line >= lines_max)
         break;

      ent = ent->next;
   }

   if (!xtext->double_buffer)
   {
      line = (xtext->fontsize * line);
      /* fill any space below the last line with our background GC */
      XFillRectangle (xtext->display, xtext->draw_buf, xtext->xbgc,
                      0, line, width + MARGIN, height - line);
   }

   /* draw the separator line */
   gtk_xtext_draw_sep (xtext, -1);

   if (xtext->double_buffer)
   {
      /* send our double buffer to the actual window */
#ifdef USE_MITSHM
      XShmPutImage (xtext->display, xtext->drawable, xtext->xfgc,
                    xtext->img, 0, 0, 0, 0, width + MARGIN, height, False);
      gtk_xtext_kill_buffer (xtext);
#else
      XCopyArea (xtext->display, xtext->tmp_pix, xtext->drawable, xtext->xfgc,
                 0, 0, width + MARGIN, height, 0, 0);
      XFreePixmap (xtext->display, xtext->tmp_pix);
#endif
   }
}

void
gtk_xtext_refresh (GtkXText *xtext)
{
   if (GTK_WIDGET_REALIZED (GTK_WIDGET (xtext)))
   {
      if (xtext->transparent)
      {
         gtk_xtext_free_trans (xtext);
         gtk_xtext_load_trans (xtext);
      }
      gtk_xtext_render_page (xtext, xtext->adj->value);
   }
}

/* remove the topline from the list */

static void
gtk_xtext_remove_top (GtkXText *xtext)
{
   textentry *ent;

   ent = xtext->text_first;
   xtext->num_lines -= ent->lines_taken;
   xtext->last_line = -1;
   xtext->text_first = ent->next;
   free (ent);
}

void
gtk_xtext_remove_lines (GtkXText *xtext, int lines, int refresh)
{
   textentry *next;

   while (xtext->text_first && lines)
   {
      next = xtext->text_first->next;
      free (xtext->text_first);
      xtext->text_first = next;
      lines--;
   }
   if (!xtext->text_first)
      xtext->text_last = NULL;

   if (refresh)
   {
      gtk_xtext_calc_lines (xtext, TRUE);
      gtk_xtext_refresh (xtext);
   }
}

static int
gtk_xtext_render_page_timeout (GtkXText *xtext)
{
   GtkAdjustment *adj = xtext->adj;
   gfloat val;

   if (xtext->scrollbar_down)
   {
      gtk_xtext_adjustment_set (xtext, FALSE);
      gtk_adjustment_set_value (adj, adj->upper - adj->page_size);
   } else
   {
      val = adj->value;
      gtk_xtext_adjustment_set (xtext, TRUE);
      gtk_adjustment_set_value (adj, val);
   }

   if (adj->value >= adj->upper - adj->page_size ||
       adj->value < 1)
      gtk_xtext_render_page (xtext, adj->value);

   xtext->add_io_tag = -1;

   return 0;
}

/* append a textentry to our linked list */

static void
gtk_xtext_append_entry (GtkXText *xtext, textentry *ent)
{
   ent->str_width = gtk_xtext_text_width (xtext, ent->str, ent->str_len);
   ent->mark_start = -1;
   ent->mark_end = -1;
   ent->next = NULL;

   if (ent->indent < MARGIN)
      ent->indent = MARGIN;        /* 2 pixels is the left margin */

   /* append to our linked list */
   if (xtext->text_last)
      xtext->text_last->next = ent;
   else
      xtext->text_first = ent;
   xtext->text_last = ent;

   ent->lines_taken = gtk_xtext_lines_taken (xtext, ent);
   xtext->num_lines += ent->lines_taken;

   if (xtext->max_lines > 2 && xtext->max_lines < xtext->num_lines)
   {
      gtk_xtext_remove_top (xtext);
   }

/*   if (xtext->frozen == 0 && xtext->add_io_tag == -1)*/
   if (xtext->add_io_tag == -1)
   {
      xtext->add_io_tag = gtk_timeout_add (REFRESH_TIMEOUT*2,
                              (GtkFunction)gtk_xtext_render_page_timeout,
                              xtext);
   }
}

/* the main two public functions */

void
gtk_xtext_append_indent (GtkXText *xtext,
                         char *left_text, int left_len,
                         char *right_text, int right_len)
{
   textentry *ent;
   char *str;
   int space;

   if (left_len == -1)
      left_len = strlen (left_text);

   if (right_len == -1)
      right_len = strlen (right_text);

   ent = malloc (left_len + right_len + 2 + sizeof (textentry));
   str = (char *)ent + sizeof (textentry);

   space = xtext->indent - gtk_xtext_text_width (xtext, left_text, left_len);

   memcpy (str, left_text, left_len);
   str[left_len] = ' ';
   memcpy (str + left_len + 1, right_text, right_len);
   str[left_len + 1 + right_len] = 0;

   ent->left_len = left_len;
   ent->str = str;
   ent->str_len = left_len + 1 + right_len;
   ent->indent = space - xtext->space_width;

   /* do we need to auto adjust the separator position? */
   if (ent->indent < MARGIN)
   {
      xtext->indent -= ent->indent;
      xtext->indent += MARGIN;
      if (xtext->indent > xtext->max_auto_indent)
         xtext->indent = xtext->max_auto_indent;
      gtk_xtext_fix_indent (xtext);
      gtk_xtext_recalc_widths (xtext, FALSE);
      space = xtext->indent - gtk_xtext_text_width (xtext, left_text, left_len);
      ent->indent = space - xtext->space_width;
   }

   gtk_xtext_append_entry (xtext, ent);
}

void
gtk_xtext_append (GtkXText *xtext, char *text, int len)
{
   textentry *ent;

   if (len == -1)
      len = strlen (text);

   ent = malloc (len + 1 + sizeof (textentry));
   ent->str = (char *)ent + sizeof (textentry);
   ent->str_len = len;
   if (len)
      memcpy (ent->str, text, len);
   ent->str[len] = 0;
   ent->indent = 0;
   ent->left_len = -1;

   gtk_xtext_append_entry (xtext, ent);
}

