/*
 * NF-Chat: A cut down version of X-chat, cut down by _Death_
 * Largely based upon X-Chat 1.4.2 by Peter Zelezny. (www.xchat.org)
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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "xchat.h"
#include "cfgfiles.h"
#include "userlist.h"
#include "fe-gtk.h"
#include "xtext.h"

extern int handle_multiline (char *cmd);

extern void PrintText (char *text);
extern int buf_get_line (char *, char **, int *, int len);
extern void reconnect (void);

/* In the following 'b4' is *before* the text say b4 and before out loud)
   and c5 is *after* (because c5 is next after b4, get it??) --AGL */

static int
tab_nick_comp_next (char *before, char *nick, char *after, int shift)
{
   struct user *user = 0, *last = NULL;
   char buf[4096];
   GSList *list;

   list = session->userlist;
   while (list)
   {
      user = (struct user *)list->data;
      if (strcmp (user->nick, nick) == 0)
         break;
      last = user;
      list = list->next;
   }
   if (!list)
      return 0;
   if (shift)
   {
      if (last)
         snprintf (buf, 4096, "%s %s%s", before, last->nick, after);
      else
         snprintf (buf, 4096, "%s %s%s", before, nick, after);
   } else
   {
      if (list->next)
         snprintf (buf, 4096, "%s %s%s", before, ((struct user *)list->next->data)->nick, after);
      else
      {
         if (session->userlist)
            snprintf (buf, 4096, "%s %s%s", before, ((struct user *)session->userlist->data)->nick, after);
         else
            snprintf (buf, 4096, "%s %s%s", before, nick, after);
      }
   }
   gtk_entry_set_text (GTK_ENTRY (session->gui->inputgad), buf);

   return 1;
}

static void
tab_comp_find_common (char *a, char *b)
{
   int c;
   int alen = strlen (a), blen = strlen (b),
       len;

   if (blen > alen)
      len = alen;
   else
      len = blen;
   for (c = 0; c < len; c++)
   {
      if (a[c] != b[c])
      {
         a[c] = 0;
         return;
      }
   }
   a[c] = 0;
}

static void
tab_comp_cmd (void)
{
   char *text, *last = NULL, *cmd, *postfix = NULL;
   int len, i, slen;
   char buf[2048];
   char lcmd[2048];

   text = gtk_entry_get_text (GTK_ENTRY (session->gui->inputgad));

   text++;
   len = strlen (text);
   if (len == 0)
      return;
   for (i = 0; i < len; i++)
   {
      if (text[i] == ' ')
      {
         postfix = &text[i + 1];
         text[i] = 0;
         len = strlen (text);
         break;
      }
   }

   i = 0;
   while (cmds[i]->name != NULL)
   {
      cmd = cmds[i]->name;
      slen = strlen (cmd);
      if (len > slen)
      {
         i++;
         continue;
      }
      if (strncasecmp (cmd, text, len) == 0)
      {
         if (last == NULL)
         {
            last = cmd;
            snprintf (lcmd, sizeof (lcmd), "%s", last);
         } else if (last > (char *) 1)
         {
            snprintf (buf, sizeof (buf), "%s %s ", last, cmd);
            PrintText (buf);
            last = (void *) 1;
            tab_comp_find_common (lcmd, cmd);
         } else if (last == (void *) 1)
         {
            PrintText (cmd);
            tab_comp_find_common (lcmd, cmd);
         }
      }
      i++;
   }

   if (last == NULL)
      return;
   if (last == (void *) 1)
     PrintText ("\n");

   if (last > (char *) 1)
   {
      if (strlen (last) > (sizeof (buf) - 1))
         return;
      if (postfix == NULL)
         snprintf (buf, sizeof (buf), "/%s ", last);
      else
         snprintf (buf, sizeof (buf), "/%s %s", last, postfix);
      gtk_entry_set_text (GTK_ENTRY (session->gui->inputgad), buf);
      return;
   } else if (strlen (lcmd) > (sizeof (buf) - 1))
      return;
   if (postfix == NULL)
      snprintf (buf, sizeof (buf), "/%s", lcmd);
   else
      snprintf (buf, sizeof (buf), "/%s %s", lcmd, postfix);
   gtk_entry_set_text (GTK_ENTRY (session->gui->inputgad), buf);
}

/* tab completion from 1.6.4 */

unsigned char tolowertab[] =
        { 0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
        0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
        0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
        0x1e, 0x1f,
        ' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
        '*', '+', ',', '-', '.', '/',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        ':', ';', '<', '=', '>', '?',
        '@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
        'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
        't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
        '_',
        '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
        'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
        't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
        0x7f,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
        0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
        0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
        0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
        0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
        0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
        0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
        0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
        0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
        0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
        0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
        0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
        0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
#define tolower(c) (tolowertab[(unsigned char)(c)])

static int
tab_nick_comp (int shift)
{
  struct user *user = NULL, *match_user = NULL;
  char *text, not_nick_chars[16] = "";
  int len, slen, first = 0, i, j, match_count = 0, match_pos = 0;
  char buf[2048], nick_buf[2048] = {0}, *b4 = NULL, *c5 = NULL, *match_text = NULL,
		       match_char = -1, *nick = NULL, *current_nick = NULL;
  GSList *list = NULL, *match_list = NULL, *first_match = NULL;
  
  text = gtk_entry_get_text (GTK_ENTRY (session->gui->inputgad));
  
  len = strlen (text);
  if (!strchr (text, ' '))
    {
      if (text[0] == '/')
	{
	  tab_comp_cmd ();
	  return 0;
	}
    }
  
  /* Is the text more than just a nick? */
  
  sprintf(not_nick_chars, " .?:");
  
  if (strcspn (text, not_nick_chars) != strlen (text))
    {
      j = GTK_EDITABLE (session->gui->inputgad)->current_pos;
      
      /* !! FIXME !! */
      if (len - j < 0)
	return 0;
      
      b4 = (char *) malloc (len + 1);
      c5 = (char *) malloc (len + 1);
      memmove (c5, &text[j], len - j);
      c5[len - j] = 0;
      memcpy (b4, text, len + 1);
      
      for (i = j - 1; i > -1; i--)
	{
	  if (b4[i] == ' ')
	    {
	      b4[i] = 0;
	      break;
	    }
	  b4[i] = 0;
	}
      memmove (text, &text[i + 1], (j - i) + 1);
      text[(j - i) - 1] = 0;
      
      if (tab_nick_comp_next (b4, text, c5, shift))
	{
	  free (b4);
	  free (c5);
	  return 1;
	}
      first = 0;
    } else
      first = 1;
  
  len = strlen (text);
  
  if (text[0] == 0)
    return -1;
  
  list = session->userlist;
  
  /* make a list of matches */
  while (list)
    {
      user = (struct user *) list->data;
      slen = strlen (user->nick);
      if (len > slen)
	{
	  list = list->next;
	  continue;
	}
      if (strncasecmp (user->nick, text, len) == 0)
	match_list = g_slist_prepend (match_list, user->nick);
      list = list->next;
    }
  match_list = g_slist_reverse (match_list); /* faster then _append */
  match_count = g_slist_length (match_list);
  
  /* no matches, return */
  if (match_count == 0)
    {
      if (!first)
	{
	  free (b4);
	  free (c5);
	}
      return 0;
    }
  first_match = match_list;
  match_pos = len;
  
  /* if we have more then 1 match, we want to act like readline and grab common chars */
  if (match_count > 1)
    {
      while (1)
	{
	  while (match_list)
	    {
	      current_nick = (char *) malloc (strlen ((char *) match_list->data) + 1);
	      strcpy (current_nick, (char *) match_list->data);
	      if (match_char == -1)
		{
		  match_char = current_nick[match_pos];
		  match_list = g_slist_next (match_list);
		  free (current_nick);
		  continue;
		}
	      if (tolower (current_nick[match_pos]) != tolower (match_char))
		{
		  match_text = (char *) malloc (match_pos);
		  current_nick[match_pos] = '\0';
		  strcpy (match_text, current_nick);
		  free (current_nick);
		  match_pos = -1;
		  break;
		}
	      match_list = g_slist_next (match_list);
	      free (current_nick);
	    }
	  
	  if (match_pos == -1)
	    break;
	  
	  match_list = first_match;
	  match_char = -1;
	  ++match_pos;
	}
      
      match_list = first_match;
    } else
      match_user = (struct user *) match_list->data;
  
  /* no match, if we found more common chars among matches, display them in entry */
  if (match_user == NULL)
    {
      while (match_list)
	{
	  nick = (char *) match_list->data;
	  sprintf (nick_buf, "%s%s ", nick_buf, nick);
	  match_list = g_slist_next (match_list);
	}
      PrintText (nick_buf);
      
      if (first)
	snprintf (buf, sizeof (buf), "%s", match_text);
      else
	{
	  snprintf (buf, sizeof (buf), "%s %s%s", b4, match_text, c5);
	  GTK_EDITABLE (session->gui->inputgad)->current_pos = strlen (b4) + strlen (match_text);
	  free (b4);
	  free (c5);
	}
      free (match_text);
    } else
      {
	if (first)
	  snprintf (buf, sizeof (buf), "%s: ", match_user->nick);
	else
	  {
	    snprintf (buf, sizeof (buf), "%s %s%s", b4, match_user->nick, c5);
	    GTK_EDITABLE (session->gui->inputgad)->current_pos = strlen (b4) + strlen (match_user->nick);
	    free (b4);
	    free (c5);
	  }
      }
  gtk_entry_set_text (GTK_ENTRY (session->gui->inputgad), buf);
  
  return 0;
}

/* handles PageUp/Down keys */
static int
key_action_scroll_page (gboolean up)
{
   int value, end;
   GtkAdjustment *adj;

   adj = GTK_RANGE (session->gui->vscrollbar)->adjustment;
   if (up)                   /* arrow up */
     {
       value = adj->value - adj->page_size;
       if (value < 0)
	 value = 0;
     } else
       {                         /* arrow down */
         end = adj->upper - adj->lower - adj->page_size;
         value = adj->value + adj->page_size;
         if (value > end)
	   value = end;
       }
   gtk_adjustment_set_value (adj, value);
   return 0;
}

static int
key_action_tab_comp (gboolean shift)
{
   if (shift)
   {
      if (tab_nick_comp (1) == -1)
         return 1;
      else
         return 2;
   } else
   {
      if (tab_nick_comp (0) == -1)
         return 1;
      else
         return 2;
   }
}

static int
key_action_scroll_userlist (gboolean up)
{
   int value, end;
   GtkAdjustment *adj;

   adj = (GtkAdjustment *) gtk_clist_get_vadjustment ((GtkCList *)session->gui->namelistgad);

   if (adj)
     {
       if (up)                   /* PageUp */
	 {
	   value = adj->value - adj->page_size;
	   if (value < 0)
	     value = 0;
	 } else
	   {                         /* PageDown */
	     end = adj->upper - adj->lower - adj->page_size;
	     value = adj->value + adj->page_size;
	     if (value > end)
	       value = end;
	   }
       gtk_adjustment_set_value (adj, value);
     }
   return 0;
}

#define STATE_SHIFT     GDK_SHIFT_MASK
#define	STATE_ALT	GDK_MOD1_MASK
#define STATE_CTRL	GDK_CONTROL_MASK

int
key_handle_key_press (GtkWidget * wid, GdkEventKey * evt, void *blah)
{
   int mod;
   
   mod = evt->state & (STATE_CTRL | STATE_ALT | STATE_SHIFT);

   if (!mod)
     switch (evt->keyval)
       {
       case 65293: /* Enter - check if server->connected */
	 if (!server->connected)
	   reconnect ();
	 break;
       case 65362: /* Arrow up */
	 gtk_signal_emit_stop_by_name (GTK_OBJECT (wid), "key_press_event");
	 key_action_scroll_page (True);
	 break;
       case 65364: /* Arrow down */
	 gtk_signal_emit_stop_by_name (GTK_OBJECT (wid), "key_press_event");
	 key_action_scroll_page (False);
	 break;
       case 65289: /* Tab */
	 gtk_signal_emit_stop_by_name (GTK_OBJECT (wid), "key_press_event");
	 key_action_tab_comp (False);
	 break;
       case 65421: /* keypad enter */
	 gtk_signal_emit_stop_by_name (GTK_OBJECT (wid), "key_press_event");
	 break; 
       case 65365: /* Pgup */
	 gtk_signal_emit_stop_by_name (GTK_OBJECT (wid), "key_press_event");
	 key_action_scroll_userlist (True);
	 break;
       case 65366: /* PgDn */
	 gtk_signal_emit_stop_by_name (GTK_OBJECT (wid), "key_press_event");
	 key_action_scroll_userlist (False);
	 break;
       }
   else if ((mod == STATE_SHIFT) && (evt->keyval == 65289))
     {
       gtk_signal_emit_stop_by_name (GTK_OBJECT (wid), "key_press_event");
       key_action_tab_comp (True);
     }
   else if ((mod == STATE_CTRL) || (mod == STATE_ALT))
     gtk_signal_emit_stop_by_name (GTK_OBJECT (wid), "key_press_event");

   switch (evt->keyval) /* huge fall through */
     {
     case 65307: /*esc key */

     case 65470: /* F1 */
     case 65471:
     case 65472:
     case 65473:
     case 65474:
     case 65475:
     case 65476:
     case 65477:
     case 65478:
     case 65479:
     case 65480:
     case 65481: /* F12 */

     case 65377: /* PRntscr */
     case 65300: /* Scroll lock */
     case 65229: /* Pause */
     case 65507: /* left ctrl */
     case 65508: /* right ctrl */
     case 65513: /* left alt */
       gtk_signal_emit_stop_by_name (GTK_OBJECT (wid), "key_press_event");
       break;
     }
   return 1;
}
