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
#include "fe.h"

extern struct xchatprefs prefs;
extern GSList *sess_list;

extern int notify_isnotify (struct session *sess, char *name);

int add_name (struct session *sess, char *name, char *hostname);
int sub_name (struct session *sess, char *name);


int
userlist_add_hostname (struct session *sess, char *nick, char *hostname, char *realname, char *servername)
{
   struct user *user;

   user = find_name (sess, nick);
   if (user && !user->hostname)
   {
      user->hostname = strdup (hostname);
      if (!user->realname)
         user->realname = strdup (realname);
      if (!user->servername)
         user->servername = strdup (servername);
      return 1;
   }
   return 0;
}

int
nick_cmp_az_ops (struct user *user1, struct user *user2)
{
   if (user1->op && !user2->op)
      return -1;
   if (!user1->op && user2->op)
      return +1;

   if (user1->voice && !user2->voice)
      return -1;
   if (!user1->voice && user2->voice)
      return +1;

   return strcasecmp (user1->nick, user2->nick);
}

int
nick_cmp (struct user *user1, struct user *user2)
{
   switch (prefs.userlist_sort)
   {
      case 0:
         return nick_cmp_az_ops (user1, user2);
      case 1:
         return strcasecmp (user1->nick, user2->nick);
      case 2:
         if (nick_cmp_az_ops (user1, user2) < 0)
            return 1;
         else
            return -1;
      case 3:
         if (strcasecmp (user1->nick, user2->nick) < 0)
            return 1;
         else
            return -1;
      default:
         return 1;
   }
}

void
clear_user_list (struct session *sess)
{
   struct user *user;
   GSList *list = sess->userlist;

   fe_userlist_clear (sess);
   while (list)
   {
      user = (struct user *)list->data;
      if (user->realname)
         free (user->realname);
      if (user->hostname)
         free (user->hostname);
      if (user->servername);
         free (user->servername);
      free (user);
      list = g_slist_remove (list, user);
   }
   sess->userlist = 0;
   sess->ops = 0;
   sess->total = 0;
}

struct user *
find_name (struct session *sess, char *name)
{
   struct user *user;
   GSList *list;

   list = sess->userlist;
   while (list)
   {
      user = (struct user *)list -> data;
      if (!strcasecmp (user->nick, name))
         return user;
      list = list->next;
   }
   return FALSE;
}

struct user *
find_name_global (struct server *serv, char *name)
{
   struct user *user;
   session *sess;
   GSList *list = sess_list;
   while (list)
   {
      sess = (session *)list -> data;
      if (sess->server == serv)
      {
         user = find_name (sess, name);
         if (user)
            return user;
      }
      list = list -> next;
   }
   return 0;
}

void
update_entry (struct session *sess, struct user *user)
{
   int row;

   sess->userlist = g_slist_remove (sess->userlist, user);
   row = userlist_insertname_sorted (sess, user);

   fe_userlist_move (sess, user, row);
   fe_userlist_numbers (sess);
}

void
ul_op_name (struct session *sess, char *name)
{
   struct user *user = find_name (sess, name);
   if (user && !user->op)
   {
      sess->ops++;
      user->op = TRUE;
      update_entry (sess, user);
   }
}

void
deop_name (struct session *sess, char *name)
{
   struct user *user = find_name (sess, name);
   if (user && user->op)
   {
      sess->ops--;
      user->op = FALSE;
      update_entry (sess, user);
   }
}

void
voice_name (struct session *sess, char *name)
{
   struct user *user = find_name (sess, name);
   if (user)
   {
      user->voice = TRUE;
      update_entry (sess, user);
   }
}

void
devoice_name (struct session *sess, char *name)
{
   struct user *user = find_name (sess, name);
   if (user)
   {
      user->voice = FALSE;
      update_entry (sess, user);
   }
}

void
change_nick (struct session *sess, char *oldname, char *newname)
{
   struct user *user = find_name (sess, oldname);
   if (user)
   {
      strcpy (user->nick, newname);
      update_entry (sess, user);
   }
}

int
sub_name (struct session *sess, char *name)
{
   struct user *user;

   user = find_name (sess, name);
   if (!user)
      return FALSE;

   if (user->op)
      sess->ops--;
   sess->total--;
   fe_userlist_numbers (sess);
   fe_userlist_remove (sess, user);

   if (user->realname)
      free (user->realname);
   if (user->hostname)
      free (user->hostname);
   if (user->servername)
      free (user->servername);
   free (user);
   sess->userlist = g_slist_remove (sess->userlist, user);

   return TRUE;
}

int
userlist_insertname_sorted (struct session *sess, struct user *newuser)
{
   int row = 0;
   struct user *user;
   GSList *list = sess->userlist;

   while (list)
   {
      user = (struct user *)list -> data;
      if (nick_cmp (newuser, user) < 1)
      {
         sess->userlist = g_slist_insert (sess->userlist, newuser, row);
         return row;
      }
      row++;
      list = list -> next;
   }
   sess->userlist = g_slist_append (sess->userlist, newuser);
   return -1;
}

int
add_name (struct session *sess, char *name, char *hostname)
{
   struct user *user;
   int row;

   user = malloc (sizeof (struct user));
   memset (user, 0, sizeof (struct user));

   /* filter out UnrealIRCD stuff */
   while (*name == '~' || *name == '*' || *name == '%' || *name == '.')
      name++;

   if (*name == '@')
   {
      name++;
      user->op = TRUE;
      sess->ops++;
   }
   sess->total++;
   if (*name == '+')
   {
      name++;
      user->voice = TRUE;
   }
   if (hostname)
      user->hostname = strdup (hostname);
   strcpy (user->nick, name);
   row = userlist_insertname_sorted (sess, user);

   fe_userlist_insert (sess, user, row);
   fe_userlist_numbers (sess);

   return row;
}

void
update_all_of (char *name)
{
   struct user *user;
   struct session *sess;
   GSList *list = sess_list;
   while (list)
   {
      sess = (struct session *) list->data;
      user = find_name (sess, name);
      if (user)
      {
         update_entry (sess, user);
      }
      list = list->next;
   }
}
