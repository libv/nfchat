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

/* signal.c by Adam Langley */

#define	SIGNALS_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "xchat.h"
#include "util.h"
#include "signals.h"
#include "fe.h"

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

int
fire_signal (int s, char *a, char *b, char *c, char *d, char e)
{
   GSList *cur;
   struct xp_signal *sig;
   int flag = 0;
   fprintf(stderr, "signal: %d\n", s);
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

