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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "xchat.h"
#include "cfgfiles.h"
#include "userlist.h"
#include "fe-gtk.h"
#include "gtkutil.h"
#include "xtext.h"

extern GtkWidget *main_window, *main_book;
extern int waitline (int sok, char *buf, int bufsize);
extern int handle_multiline (session_t *sess, char *cmd, int history, int nocommand);

extern GSList *sess_list;
extern struct commands cmds[1];

extern void PrintText (session_t *sess, unsigned char *text);
extern int buf_get_line (char *, char **, int *, int len);
extern void history_up (GtkWidget * widget, struct history *his);
extern void history_down (GtkWidget * widget, struct history *his);
extern int tab_nick_comp (GtkWidget * t, int shift);
extern void nick_comp_chng (GtkWidget * t, int updown);
extern void replace_handle (GtkWidget * wid);

/***************** Key Binding Code ******************/

/* NOTES:

   To add a new action:
   1) inc KEY_MAX_ACTIONS
   2) write the function at the bottom of this file (with all the others)
   FIXME: Write about calling and returning
   3) Add it to key_actions

   --AGL

 */

/* Remember that the *number* of actions is this *plus* 1 --AGL */
#define KEY_MAX_ACTIONS 9
/* These are cp'ed from history.c --AGL */
#define STATE_SHIFT     GDK_SHIFT_MASK
#define	STATE_ALT	GDK_MOD1_MASK
#define STATE_CTRL	GDK_CONTROL_MASK

struct key_binding
{
   int keyval;                  /* GDK keynumber */
   char *keyname;               /* String with the name of the function */
   int action;                  /* Index into key_actions */
   int mod;                     /* Flags of STATE_* above */
   char *data1, *data2;         /* Pointers to strings, these must be freed */
   struct key_binding *next;
};

struct key_action
{
   int (*handler) (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
               session_t * sess);
   char *name;
   char *help;
};

int key_load_kbs ();
void key_load_defaults ();

static int key_action_handle_command (GtkWidget * wid, GdkEventKey * evt, char *d1,
      char *d2, session_t *sess);
static int key_action_page_switch (GtkWidget * wid, GdkEventKey * evt, char *d1,
char *d2, session_t *sess);
int key_action_insert (GtkWidget * wid, GdkEventKey * evt, char *d1,
      char *d2, session_t *sess);
static int key_action_scroll_page (GtkWidget * wid, GdkEventKey * evt, char *d1,
      char *d2, session_t *sess);
static int key_action_set_buffer (GtkWidget * wid, GdkEventKey * evt, char *d1,
      char *d2, session_t *sess);
static int key_action_history_up (GtkWidget * wid, GdkEventKey * evt, char *d1,
      char *d2, session_t *sess);
static int key_action_history_down (GtkWidget * wid, GdkEventKey * evt, char *d1,
      char *d2, session_t *sess);
static int key_action_tab_comp (GtkWidget * wid, GdkEventKey * evt, char *d1,
      char *d2, session_t *sess);
static int key_action_comp_chng (GtkWidget * wid, GdkEventKey * evt, char *d1,
      char *d2, session_t *sess);
static int key_action_replace (GtkWidget * wid, GdkEventKey * evt, char *d1,
      char *d2, session_t *sess);

static struct key_binding *keys_root = NULL;

static struct key_action key_actions[KEY_MAX_ACTIONS + 1] =
{
   {key_action_handle_command, "Run Command", "The \002Run Command\002 action runs the data in Data 1 as if it has been typed into the entry box where you pressed the key sequence. Thus it can contain text (which will be sent to the channel/person), commands or user commands. When run all \002\\n\002 charactors in Data 1 are used to deliminate seperate commands so it is possible to run more than one command. If you want a \002\\\002 in the actual text run then enter \002\\\\\002"},
       {key_action_page_switch, "Change Page", "The \002Change Page\002 command switchs between pages in the notebook. Set Data 1 to the page you want to switch to. If Data 2 is set to anything then the switch will be relative to the current position"},
   {key_action_insert, "Insert in Buffer", "The \002Insert in Buffer\002 command will insert the contents of Data 1 into the entry where the key sequence was pressed at the current cursor position"},
   {key_action_scroll_page, "Scroll Page", "The \002Scroll Page\002 command scrolls the text widget up or down one page. If Data 1 is set to anything the page scrolls up, else it scrolls down"},
   {key_action_set_buffer, "Set Buffer", "The \002Set Buffer\002 command sets the entry where the key sequence was entered to the contents of Data 1"},
   {key_action_history_up, "Last Command", "The \002Last Command\002 command sets the entry to contain the last command entered - the same as pressing up in a shell"},
   {key_action_history_down, "Next Command", "The \002Last Command\002 command sets the entry to contain the next command entered - the same as pressing down in a shell"},
   {key_action_tab_comp, "Complete nick/command", "This command changes the text in the entry to finish an incomplete nickname or command. If Data 1 is set then double-tabbing in a string will select the last nick, not the next"},
   {key_action_comp_chng, "Change Selected Nick", "This command scrolls up and down through the list of nicks. If Data 1 is set to anything then if scrolls up else if scrolls down"},
   {key_action_replace, "Check For Replace", "This command checks the last work entered in the entry against the replace list and replaces it if it finds a match"},
};

void
key_init ()
{
   keys_root = NULL;
   if (key_load_kbs () == 1)
   {
      key_load_defaults ();
      if (key_load_kbs () == 1)
         fprintf(stderr, "Error: There was an error loading key bindings configuration");
   }
}

/* Ok, here are the NOTES

   key_handle_key_press now handles all the key presses and history_keypress is now defunct. It goes thru the linked list keys_root and finds a matching key. It runs the action func and switches on these values:
   0) Return
   1) Find next
   2) stop signal and return

   * history_keypress is now dead (and gone)
   * key_handle_key_press now takes its role
   * All the possible actions are in a struct called key_actions (in fkeys.c)
   * it is made of {function, name, desc}
   * key bindings can pass 2 *text* strings to the handler. If more options are nee
   ded a format can be put on one of these options
   * key actions are passed {
   the entry widget
   the Gdk event
   data 1
   data 2
   session struct
   }
   * key bindings are stored in a linked list of key_binding structs
   * which looks like {
   int keyval;  GDK keynumber
   char *keyname;  String with the name of the function 
   int action;  Index into key_actions 
   int mod; Flags of STATE_* above 
   char *data1, *data2;  Pointers to strings, these must be freed 
   struct key_binding *next;
   }
   * remember that is (data1 || data2) != NULL then they need to be free()'ed

   --AGL

 */

int
key_handle_key_press (GtkWidget * wid, GdkEventKey * evt, session_t *sess)
{
   struct key_binding *kb, *last = NULL;
   int keyval = evt->keyval;
   int mod, n;

   mod = evt->state & (STATE_CTRL | STATE_ALT | STATE_SHIFT);
   kb = keys_root;
   while (kb)
   {
      if (kb->keyval == keyval && kb->mod == mod)
      {
         if (kb->action < 0 || kb->action > KEY_MAX_ACTIONS)
            return 0;

         /* Bump this binding to the top of the list */
         if (last != NULL)
         {
            last->next = kb->next;
            kb->next = keys_root;
            keys_root = kb;
         }
         /* Run the function */
         n = key_actions[kb->action].handler (wid, evt, kb->data1,
                     kb->data2, sess);
         switch (n)
         {
         case 0:
            return 1;
         case 2:
            gtk_signal_emit_stop_by_name (GTK_OBJECT (wid),
                   "key_press_event");
            return 1;
         }
      }
      last = kb;
      kb = kb->next;
   }
   return 1;
}

/* ***** GUI code here ******************* */

void
key_load_defaults ()
{
   char def[] =
   /* This is the default config */
   "A\nminus\nChange Page\nD1:-1\nD2:Relative\n\n"
   "A\n9\nChange Page\nD1:9\nD2!\n\n"
   "A\n8\nChange Page\nD1:8\nD2!\n\n"
   "A\n7\nChange Page\nD1:7\nD2!\n\n"
   "A\n6\nChange Page\nD1:6\nD2!\n\n"
   "A\n5\nChange Page\nD1:5\nD2!\n\n"
   "A\n4\nChange Page\nD1:4\nD2!\n\n"
   "A\n3\nChange Page\nD1:3\nD2!\n\n"
   "A\n2\nChange Page\nD1:2\nD2!\n\n"
   "A\n1\nChange Page\nD1:1\nD2!\n\n"
   "C\no\nInsert in Buffer\nD1:%O\nD2!\n\n"
   "C\nb\nInsert in Buffer\nD1:%B\nD2!\n\n"
   "C\nk\nInsert in Buffer\nD1:%C\nD2!\n\n"
   "S\nNext\nChange Selected Nick\nD1!\nD2!\n\n"
   "S\nPrior\nChange Selected Nick\nD1!\nD2!\n\n"
   "None\nNext\nScroll Page\nD1!\nD2!\n\n"
   "None\nPrior\nScroll Page\nD1:Up\nD2!\n\n"
   "None\nDown\nNext Command\nD1!\nD2!\n\n"
   "None\nUp\nLast Command\nD1!\nD2!\n\n"
   "None\nTab\nComplete nick/command\nD1!\nD2!\n\n"
   "None\nspace\nCheck For Replace\nD1!\nD2!\n\n"
   "None\nReturn\nCheck For Replace\nD1!\nD2!\n\n"
   "A\nequal\nChange Page\nD1:1\nD2:Relative\n\n"
   "C\nTab\nComplete nick/command\nD1:Up\nD2!\n\n";
   char buf[512];
   int fd;

   snprintf (buf, 512, "%s/keybindings.conf", get_xdir ());
   fd = open (buf, O_CREAT | O_TRUNC | O_WRONLY | OFLAGS, 0x180);
   if (fd < 0)
      /* ???!!! */
      return;

   write (fd, def, strlen (def));
   close (fd);
}

/* I just know this is going to be a nasty parse, if you think it's bugged
   it almost certainly is so contact the XChat dev team --AGL */

inline int
key_load_kbs_helper_mod (char *in, int *out)
{
   int n, len, mod = 0;

   /* First strip off the fluff */
   while (in[0] == ' ' || in[0] == '\t')
      in++;
   len = strlen (in);
   while (in[len] == ' ' || in[len] == '\t')
   {
      in[len] = 0;
      len--;
   }

   if (strcmp (in, "None") == 0)
   {
      *out = 0;
      return 0;
   }
   for (n = 0; n < len; n++)
   {
      switch (in[n])
      {
      case 'C':
         mod |= STATE_CTRL;
         break;
      case 'A':
         mod |= STATE_ALT;
         break;
      case 'S':
         mod |= STATE_SHIFT;
         break;
      default:
         return 1;
      }
   }

   *out = mod;
   return 0;
}

/* These are just local defines to keep me sane --AGL */

#define KBSTATE_MOD 0
#define KBSTATE_KEY 1
#define KBSTATE_ACT 2
#define KBSTATE_DT1 3
#define KBSTATE_DT2 4

/* *** Warning, Warning! - massive function ahead! --AGL */

int
key_load_kbs (void)
{
   char *buf, *ibuf;
   struct stat st;
   struct key_binding *kb = NULL, *last = NULL;
   int fd, len, pnt = 0, state = 0, n;

   buf = malloc (1000);
   snprintf (buf, 1000, "%s/keybindings.conf", get_xdir ());
   fd = open (buf, O_RDONLY | OFLAGS);
   free (buf);

   if (fd < 0)
      return 1;
   if (fstat (fd, &st) != 0)
      return 1;
   ibuf = malloc (st.st_size);
   read (fd, ibuf, st.st_size);
   close (fd);

   while (buf_get_line (ibuf, &buf, &pnt, st.st_size))
   {
      if (buf[0] == '#')
         continue;
      if (strlen (buf) == 0)
         continue;

      switch (state)
      {
      case KBSTATE_MOD:
         kb = (struct key_binding *) malloc (sizeof (struct key_binding));
         if (key_load_kbs_helper_mod (buf, &kb->mod))
            goto corrupt_file;
         state = KBSTATE_KEY;
         continue;
      case KBSTATE_KEY:
         /* First strip off the fluff */
         while (buf[0] == ' ' || buf[0] == '\t')
            buf++;
         len = strlen (buf);
         while (buf[len] == ' ' || buf[len] == '\t')
         {
            buf[len] = 0;
            len--;
         }

         n = gdk_keyval_from_name (buf);
         if (n == 0)
         {
            /* Unknown keyname, abort */
            if (last)
               last->next = NULL;
            free (ibuf);
            fprintf (stderr, "Error: Unknown keyname %s in key bindings config file\nLoad aborted, please fix ~/.nfchat/keybindings.conf\n", buf);
            return 2;
         }
         kb->keyname = gdk_keyval_name (n);
         kb->keyval = n;

         state = KBSTATE_ACT;
         continue;
      case KBSTATE_ACT:
         /* First strip off the fluff */
         while (buf[0] == ' ' || buf[0] == '\t')
            buf++;
         len = strlen (buf);
         while (buf[len] == ' ' || buf[len] == '\t')
         {
            buf[len] = 0;
            len--;
         }

         for (n = 0; n < KEY_MAX_ACTIONS + 1; n++)
         {
            if (strcmp (key_actions[n].name, buf) == 0)
            {
               kb->action = n;
               break;
            }
         }

         if (n == KEY_MAX_ACTIONS + 1)
         {
            if (last)
               last->next = NULL;
            free (ibuf);
            fprintf (stderr, "Error: Unknown action %s in key bindings config file\nLoad aborted, Please fix ~/.nfchat/keybindings.conf\n", buf);
            return 3;
         }
         state = KBSTATE_DT1;
         continue;
      case KBSTATE_DT1:
      case KBSTATE_DT2:
         if (state == KBSTATE_DT1)
            kb->data1 = kb->data2 = NULL;

         while (buf[0] == ' ' || buf[0] == '\t')
            buf++;

         if (buf[0] != 'D')
         {
            free (ibuf);
            fprintf (stderr, "Error: Expecting Data line (beginning Dx{:|!}) but got:\n%s\n\nLoad aborted, Please fix ~/.nfchat/keybindings.nf\n", buf);
            return 4;
         }
         switch (buf[1])
         {
         case '1':
            if (state != KBSTATE_DT1)
               goto corrupt_file;
            break;
         case '2':
            if (state != KBSTATE_DT2)
               goto corrupt_file;
            break;
         default:
            goto corrupt_file;
         }

         if (buf[2] == ':')
         {
            len = strlen (buf);
            /* Add one for the NULL, subtract 3 for the "Dx:" */
            len++;
            len -= 3;
            if (state == KBSTATE_DT1)
            {
               kb->data1 = malloc (len);
               memcpy (kb->data1, &buf[3], len);
            } else
            {
               kb->data2 = malloc (len);
               memcpy (kb->data2, &buf[3], len);
            }
         } else if (buf[2] == '!')
         {
            if (state == KBSTATE_DT1)
               kb->data1 = NULL;
            else
               kb->data2 = NULL;
         }
         if (state == KBSTATE_DT1)
         {
            state = KBSTATE_DT2;
            continue;
         } else
         {
            if (last)
               last->next = kb;
            else
               keys_root = kb;
            last = kb;

            state = KBSTATE_MOD;
         }

         continue;
      }
   }
   free (ibuf);
   if (last)
      last->next = NULL;
   return 0;

 corrupt_file:
   if (getenv ("XCHAT_DEBUG"))
      abort ();
   fprintf (stderr, "Error: Key bindings config file is corrupt, load aborted\nPlease fix ~/.nfchat/keybindings.conf\n");
   return 5;
}

/* ***** Key actions start here *********** */

/* See the NOTES above --AGL */

/* "Run command" */

int
key_action_handle_command (GtkWidget * wid, GdkEventKey * evt, char *d1,
       char *d2, session_t *sess)
{
   int ii, oi, len;
   char out[2048], d = 0;

   if (!d1)
      return 0;

   len = strlen (d1);

   /* Replace each "\n" substring with '\n' */
   for (ii = oi = 0; ii < len; ii++)
   {
      d = d1[ii];
      if (d == '\\')
      {
         ii++;
         d = d1[ii];
         if (d == 'n')
            out[oi++] = '\n';
         else if (d == '\\')
            out[oi++] = '\\';
         else
         {
            out[oi++] = '\\';
            out[oi++] = d;
         }
         continue;
      }
      out[oi++] = d;
   }
   out[oi] = 0;

   handle_multiline (sess, out, 0, 0);
   return 0;
}

static int
key_action_page_switch (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
                 session_t *sess)
{
   int len, i, num;

   if (!d1)
      return 1;
   if (!main_window)
      return 1;

   len = strlen (d1);
   if (!len)
      return 1;

   for (i = 0; i < len; i++)
   {
      if (d1[i] < '0' || d1[i] > '9')
      {
         if (i == 0 && (d1[i] == '+' || d1[i] == '-'))
            continue;
         else
            return 1;
      }
   }

   num = atoi (d1);
   if (!d2)
      num--;
   if (!d2 || d2[0] == 0)
      gtk_notebook_set_page (GTK_NOTEBOOK (main_book), num);
   else
   {
      len = g_list_length ( ((GtkNotebook *) main_book)->children);
      i = gtk_notebook_get_current_page (GTK_NOTEBOOK (main_book)) + num;
      if (i >= len)
         i = 0;
      if (i < 0)
	 i = len - 1;
      gtk_notebook_set_page (GTK_NOTEBOOK (main_book), i);
   }
   return 0;
}

int
key_action_insert (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
                 session_t *sess)
{
   int tmp_pos;

   if (!d1)
      return 1;

   tmp_pos = GTK_EDITABLE (wid)->current_pos;
   gtk_editable_insert_text (GTK_EDITABLE (wid), d1, strlen (d1), &tmp_pos);
   GTK_EDITABLE (wid)->current_pos = tmp_pos;
   return 2;
}

/* handles PageUp/Down keys */
int
key_action_scroll_page (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
                 session_t *sess)
{
   int value, end;
   GtkAdjustment *adj;
   int up = 0;

   if (d1 && d1[0] != 0)
      up++;

   if (sess)
   {
      adj = GTK_RANGE (sess->gui->vscrollbar)->adjustment;
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

int
key_action_set_buffer (GtkWidget * wid, GdkEventKey * evt, char *d1, char *d2,
                 session_t *sess)
{
   if (!d1)
      return 1;
   if (d1[0] == 0)
      return 1;

   gtk_entry_set_text (GTK_ENTRY (wid), d1);
   return 2;
}

int
key_action_history_up (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
                 session_t *sess)
{
   history_up (wid, &sess->history);
   return 2;
}

int
key_action_history_down (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
			 session_t *sess)
{
   history_down (wid, &sess->history);
   return 2;
}

int
key_action_tab_comp (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
                 session_t *sess)
{
   if (d1 && d1[0])
   {
      if (tab_nick_comp (wid, 1) == -1)
         return 1;
      else
         return 2;
   } else
   {
      if (tab_nick_comp (wid, 0) == -1)
         return 1;
      else
         return 2;
   }
}

int
key_action_comp_chng (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
                 session_t *sess)
{
   if (d1 && d1[0] != 0)
      nick_comp_chng (wid, 1);
   else
      nick_comp_chng (wid, 0);

   return 2;
}

int
key_action_replace (GtkWidget * wid, GdkEventKey * ent, char *d1, char *d2,
                 session_t *sess)
{
   replace_handle (wid);
   return 1;
}


/* -------- */


#define STATE_SHIFT     GDK_SHIFT_MASK
#define	STATE_ALT	GDK_MOD1_MASK
#define STATE_CTRL	GDK_CONTROL_MASK

void
replace_handle (GtkWidget * t)
{
   char *text, *postfix_pnt;
   char word[140];
   char postfix[140];
   int c, len, xlen;

   text = gtk_entry_get_text (GTK_ENTRY (t));

   len = strlen (text);
   for (c = len - 1; c > 0; c--)
   {
      if (text[c] == ' ')
         break;
   }
   if (text[c] == ' ')
      c++;
   xlen = c;
   if (len - c >= (sizeof (word) - 12))
      return;
   memcpy (word, &text[c], len - c);
   word[len - c] = 0;
   len = strlen (word);
   if (word[0] == '\'' && word[len] == '\'')
      return;
   postfix_pnt = NULL;
   for (c = 0; c < len; c++)
   {
      if (word[c] == '\'')
      {
         postfix_pnt = &word[c + 1];
         word[c] = 0;
         break;
      }
   }

   if (postfix_pnt != NULL)
   {
      if (strlen (postfix_pnt) > sizeof (postfix) - 12)
         return;
      strcpy (postfix, postfix_pnt);
   }

}

session_t *
find_session_from_inputgad (GtkWidget * w)
{
   GSList *list = sess_list;
   session_t *sess = 0;

   /* First find the session from the widget */
   while (list)
   {
      sess = (session_t *) list->data;
      if (sess->gui->inputgad == w)
         break;
      list = list->next;
   }
   if (!list)
   {
      /*Erm, we didn't find ANY valid session, HELP! */
      return NULL;
   }
   return sess;
}

int
text_is_more_than_just_nick (char *tx)
{
   int c, len = strlen (tx);

   for (c = 0; c < len; c++)
   {
      if (tx[c] == ' ' || tx[c] == ':' || tx[c] == '.' || tx[c] == '?')
         return 1;
   }
   return 0;
}

int
nick_comp_get_nick (char *tx, char *n)
{
   int c, len = strlen (tx);

   for (c = 0; c < len; c++)
   {
      if (tx[c] == ':')
      {
         n[c] = 0;
         return 0;
      }
      if (tx[c] == ' ' || tx[c] == '.' || tx[c] == 0)
         return -1;
      n[c] = tx[c];
   }
   return -1;
}

void
nick_comp_chng (GtkWidget * t, int updown)
{
   session_t *sess;
   struct user *user, *last = NULL;
   char *text, nick[64];
   int len, slen;
   GSList *list;

   sess = find_session_from_inputgad (t);
   if (sess == NULL)
      return;

   text = gtk_entry_get_text (GTK_ENTRY (t));

   if (nick_comp_get_nick (text, nick) == -1)
      return;
   len = strlen (nick);

   list = sess->userlist;
   while (list)
   {
      user = (struct user *) list->data;
      slen = strlen (user->nick);
      if (len != slen)
      {
         last = user;
         list = list->next;
         continue;
      }
      if (strncasecmp (user->nick, nick, len) == 0)
      {
         if (updown == 0)
         {
            if (list->next == NULL)
               return;
            user->weight--;
            ((struct user *)list->next->data)->weight++;
            snprintf (nick, sizeof (nick), "%s: ", ((struct user *)list->next->data)->nick);
         } else
         {
            if (last == NULL)
               return;
            user->weight--;
            last->weight++;
            snprintf (nick, sizeof (nick), "%s: ", last->nick);
         }
         gtk_entry_set_text (GTK_ENTRY (t), nick);
         return;
      }
      last = user;
      list = list->next;
   }
}

void
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

void
tab_comp_cmd (GtkWidget * t)
{
   char *text, *last = NULL, *cmd, *postfix = NULL;
   int len, i, slen;
   session_t *sess;
   char buf[2048];
   char lcmd[2048];

   text = gtk_entry_get_text (GTK_ENTRY (t));

   sess = find_session_from_inputgad (t);

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
   while (cmds[i].name != NULL)
   {
      cmd = cmds[i].name;
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
            PrintText (sess, buf);
            /*PrintText (sess, last);
            PrintText (sess, "\t");
            PrintText (sess, cmd);
            PrintText (sess, "\t");*/
            last = (void *) 1;
            tab_comp_find_common (lcmd, cmd);
         } else if (last == (void *) 1)
         {
            PrintText (sess, cmd);
            /*PrintText (sess, cmd);
            PrintText (sess, "\t");*/
            tab_comp_find_common (lcmd, cmd);
         }
      }
      i++;
   }

   if (last == NULL)
      return;
   if (last == (void *) 1)
      PrintText (sess, "\n");

   if (last > (char *) 1)
   {
      if (strlen (last) > (sizeof (buf) - 1))
         return;
      if (postfix == NULL)
         snprintf (buf, sizeof (buf), "/%s ", last);
      else
         snprintf (buf, sizeof (buf), "/%s %s", last, postfix);
      gtk_entry_set_text (GTK_ENTRY (t), buf);
      return;
   } else if (strlen (lcmd) > (sizeof (buf) - 1))
      return;
   if (postfix == NULL)
      snprintf (buf, sizeof (buf), "/%s", lcmd);
   else
      snprintf (buf, sizeof (buf), "/%s %s", lcmd, postfix);
   gtk_entry_set_text (GTK_ENTRY (t), buf);
}

/* In the following 'b4' is *before* the text (just say b4 and before out loud)
   and c5 is *after* (because c5 is next after b4, get it??) --AGL */

int
tab_nick_comp_next (session_t *sess, GtkWidget * wid, char *b4,
      char *nick, char *c5, int shift)
{
   struct user *user = 0, *last = NULL;
   char buf[4096];
   GSList *list;

   list = sess->userlist;
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
         snprintf (buf, 4096, "%s %s%s", b4, last->nick, c5);
      else
         snprintf (buf, 4096, "%s %s%s", b4, nick, c5);
   } else
   {
      if (list->next)
         snprintf (buf, 4096, "%s %s%s", b4, ((struct user *)list->next->data)->nick, c5);
      else
      {
         if (sess->userlist)
            snprintf (buf, 4096, "%s %s%s", b4, ((struct user *)sess->userlist->data)->nick, c5);
         else
            snprintf (buf, 4096, "%s %s%s", b4, nick, c5);
      }
   }
   gtk_entry_set_text (GTK_ENTRY (wid), buf);

   return 1;
}

int
tab_nick_comp (GtkWidget * t, int shift)
{
   session_t *sess;
   struct user *user, *best = NULL;
   char *text;
   int len, slen, highest = -10000, first = 0,
       i, j;
   char buf[16384], *b4 = NULL, *c5 = NULL;
   GSList *list;

   text = gtk_entry_get_text (GTK_ENTRY (t));

   sess = find_session_from_inputgad (t);
   
   if (sess == NULL)
      return 0;

   len = strlen (text);
   if (!strchr (text, ' '))
   {
      if (text[0] == '/')
      {
         tab_comp_cmd (t);
         return 0;
      }
   }
   if (text_is_more_than_just_nick (text))
   {
      j = GTK_EDITABLE (t)->current_pos;
      b4 = (char *) malloc (len + 1);
      c5 = (char *) malloc (len + 1);
      memcpy (c5, &text[j], len - j);
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
      memcpy (text, &text[i + 1], (j - i) + 1);
      text[(j - i) - 1] = 0;

      if (tab_nick_comp_next (sess, t, b4, text, c5, shift))
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

   list = sess->userlist;
   while (list)
   {
      user = (struct user *)list->data;
      slen = strlen (user->nick);
      if (len > slen)
      {
         list = list->next;
         continue;
      }
      if (strncasecmp (user->nick, text, len) == 0)
      {
         if (user->weight > highest)
         {
            best = user;
            highest = user->weight;
         }
      }
      list = list->next;
   }
   if (best == NULL)
      return 0;

   if (first)
      snprintf (buf, sizeof (buf), "%s: ", best->nick);
   else
   {
      snprintf (buf, sizeof (buf), "%s %s%s", b4, best->nick, c5);
      GTK_EDITABLE (t)->current_pos = strlen (b4) + strlen (best->nick);
      free (b4);
      free (c5);
   }
   gtk_entry_set_text (GTK_ENTRY (t), buf);
   return 0;
}

void
history_down (GtkWidget * widget, struct history *his)
{
   his->pos++;
   if (his->pos == HISTORY_SIZE)
      his->pos = 0;
   if (his->lines[his->pos])
   {
      gtk_entry_set_text (GTK_ENTRY (widget), his->lines[his->pos]);
   } else
   {
      if (his->pos != 0)
         his->pos--;
      else
         his->pos = (HISTORY_SIZE - 1);
   }
}

void
history_up (GtkWidget * widget, struct history *his)
{
   if (his->pos == 0)
      his->pos = (HISTORY_SIZE - 1);
   else
      his->pos--;
   if (his->lines[his->pos])
   {
      gtk_entry_set_text (GTK_ENTRY (widget), his->lines[his->pos]);
   } else
   {
      if (his->pos == (HISTORY_SIZE - 1))
         his->pos = 0;
      else
         his->pos++;
   }
}
