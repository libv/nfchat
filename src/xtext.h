#ifndef __XTEXT_H__
#define __XTEXT_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#ifdef USE_MITSHM

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#endif

#define GTK_XTEXT(obj) ((GtkXText*)obj)

#define FONT_1BYTE 0
#define FONT_2BYTE 1
#define FONT_SET 2

#define ATTR_BOLD '\002'
#define ATTR_COLOR '\003'
#define ATTR_BEEP '\007'
#define ATTR_RESET '\017'
#define ATTR_REVERSE '\026'
#define ATTR_ESCAPE '\033'
#define ATTR_UNDERLINE '\037'

typedef struct _GtkXText         GtkXText;
typedef struct _GtkXTextClass    GtkXTextClass;

typedef struct textentry
{
   struct textentry *next;
   char *str;
   int str_width;
   short str_len;
   short mark_start;
   short mark_end;
   short indent;
   short lines_taken;
   short left_len;
} textentry;

struct _GtkXText
{
   GtkWidget widget;

   GtkAdjustment *adj;
   gfloat old_value;    /* last known adj->value */
   GdkPixmap *pixmap;   /* 0 = use palette[19] */
   GdkCursor *hand_cursor;

   int last_win_x;
   int last_win_y;
   int last_win_h;
   int last_win_w;

   int tint_red;
   int tint_green;
   int tint_blue;

   GdkGC *bgc;          /* backing pixmap */
   GdkGC *fgc;          /* text foreground color */
   gulong palette[20];

   textentry *text_first;
   textentry *text_last;

   gint io_tag;         /* for delayed refresh events */
   gint add_io_tag;     /* "" when adding new text */
   gint scroll_tag;     /* marking-scroll timeout */

   GC xfgc;             /* this stuff is repeated, but its the X11 pointers */
   GC xbgc;
   Drawable drawable;
   Drawable draw_buf;
   Display *display;
   XFontStruct *xfont;

   GdkFont *font;
   int fontsize;
   int fonttype;
   guint16 fontwidth[256];  /* each char's width, only for FONT_1BYTE type */
   int space_width;     /* width (pixels) of the space " " character */

   int indent;          /* position of separator (pixels) from left */
   int max_auto_indent;

   int select_start_adj;   /* the adj->value when the selection started */
   int select_start_x;
   int select_start_y;
   int select_end_x;
   int select_end_y;

   textentry *last_ent_start; /* this basically describes the last rendered */
   textentry *last_ent_end;   /* selection. */
   int last_offset_start;
   int last_offset_end;

   textentry *old_ent_start;
   textentry *old_ent_end;

   int num_lines;
   int max_lines;

   int last_subline;
   int last_line;
   textentry *last_ent;

   int col_fore;
   int col_back;

   int depth;           /* gdk window depth */

/*   int frozen;*/

   char num[8];   /* for parsing mirc color */
   int nc;        /* offset into xtext->num */

   textentry *hilight_ent;
   int hilight_start;
   int hilight_end;

   short grid_offset[256];

   unsigned int auto_indent:1;
   unsigned int moving_separator:1;
   unsigned int scrollbar_down:1;
   unsigned int word_or_line_select:1;
   unsigned int color_paste:1;
   unsigned int parsing_backcolor:1;
   unsigned int parsing_color:1;
   unsigned int backcolor:1;
   unsigned int button_down:1;
   unsigned int bold:1;
   unsigned int underline:1;
   unsigned int reverse:1;
   unsigned int transparent:1;
   unsigned int shaded:1;
   unsigned int dont_render:1;
   unsigned int cursor_hand:1;
};

struct _GtkXTextClass
{
   GtkWidgetClass parent_class;
   void (* word_click) (GtkXText *xtext, char *word, GdkEventButton *event);
};

GtkWidget* gtk_xtext_new         (int indent);
guint      gtk_xtext_get_type    (void);
void       gtk_xtext_append_indent (GtkXText *xtext,
                                    char *left_text, int left_len,
                                    char *right_text, int right_len);
void       gtk_xtext_set_font    (GtkXText *xtext, GdkFont *font, char *name);
void       gtk_xtext_set_background (GtkXText *xtext, GdkPixmap *pixmap, int trans, int shaded);
void       gtk_xtext_set_palette (GtkXText *xtext, GdkColor palette[]);
char*      gtk_xtext_strip_color (unsigned char *text, int len);

#endif

