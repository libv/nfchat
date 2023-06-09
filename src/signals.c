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

#define	SIGNALS_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "xchat.h"
#include "util.h"
#include "signals.h"

int current_signal;
GSList *sigroots[NUM_XP];
void *signal_data;

extern void printevent_setup ();

void
signal_setup ()
{
   memset (sigroots, 0, NUM_XP * sizeof (void *));
   printevent_setup ();
}

/* static void
printchars (char *a)
{
  int i, size = strlen(a);

  for(i = 0; i <= size; i++)
    fprintf(stderr, "char %d:%c\n", a[i], a[i]);
  fprintf(stderr, "end of string\n");
  }*/

int
fire_signal (int s, char *a, char *b, char *c, char *d, char e)
{
   GSList *cur;
   struct xp_signal *sig;
   int flag = 0;
   /*   fprintf (stderr, "-int: %d\n", s);
   if (s != 79 && s != 80)
   fprintf (stderr, "char * %s, char * %s, char * %s, char * %s, char %c\n",a, b, c, d, e);*/
   /* if (s == 78)
      printchars(a);*/
   cur = sigroots[s];
   current_signal = s;
   while (cur) {
      sig = cur->data;

      signal_data = sig->data;
      if (sig->callback (a, b, c, d, e))
	 flag = 1;
      
      cur = cur->next;
   }

   return flag;
}

