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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "xchat.h"
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "cfgfiles.h"
#include "signals.h"
#include "util.h"
#include "xtext.h"

extern struct xchatprefs prefs;
extern void check_special_chars (char *text);
extern char **environ;
extern void PrintText (char *text);

static int pevt_build_string (char *input, char **output, int *max_arg);

/* Print Events stuff here --AGL */

/* Consider the following a NOTES file:

   The main upshot of this is:
   * Plugins can intercept text events and do what they like
   * The default text engine can be config'ed

   By default it should appear *exactly* the same (I'm working hard not to change the default style) but if you go into Settings->Edit Event Texts you can change the text's. The format is thus:

   The normal %Cx (color) and %B (bold) etc work

   $x is replaced with the data in var x (e.g. $1 is often the nick)

   $axxx is replace with a single byte of value xxx (in base 10)

   AGL (990507)
 */

/* These lists are thus:
   pntevts_text[] are the strings the user sees (WITH %x etc)
   pntevts[] are the data strings with \000 etc
   evtnames[] are the names of the events
 */

/* To add a new event:

   Think up a name (like JOIN)
   Make up a pevt_name_help struct (with a NULL at the end)
   add a struct to the end of te with,
   {"Name", help_struct, "Default text", num_of_args, NULL}
   Open up signal.h
   add one to NUM_XP
   add a #define XP_TE_NAME with a value of +1 on the last XP_
 */

/* As of 990528 this code is in BETA --AGL */

/* Internals:

   On startup ~/.xchat/printevents.conf is loaded if it doesn't exist the
   defaults are loaded. Any missing events are filled from defaults.
   Each event is parsed by pevt_build_string and a binary output is produced
   which looks like:

   (byte) value: 0 = {
   (int) numbers of bytes
   (char []) that number of byte to be memcpy'ed into the buffer
   }
   1 =
   (byte) number of varable to insert
   2 = end of buffer

   Each XP_TE_* signal is hard coded to call textsignal_handler which calls
   display_event which decodes the data

   This means that this system *should be faster* than snprintf because
   it always 'knows' that format of the string (basically is preparses much
   of the work)

   --AGL
 */

static char **pntevts_text;
static char **pntevts;

struct text_event
{
   char *name;
   char *def;
   int num_args;
};

/* *BBIIGG* struct ahead!! --AGL */

static struct text_event te[] =
{
   /* Padding for all the non-text signals */
/*000*/   {NULL, NULL, 0},
/*001*/   {NULL, NULL, 0},
/*002*/   {NULL, NULL, 0},
/*003*/   {NULL, NULL, 0},
/*004*/   {NULL, NULL, 0},
/*005*/   {NULL, NULL, 0},
/*006*/   {NULL, NULL, 0},
/*007*/   {NULL, NULL, 0},
/*008*/   {NULL, NULL, 0},
/*009*/   {NULL, NULL, 0},
/*010*/   {NULL, NULL, 0},
/*011*/   {NULL, NULL, 0},
/*012*/   {NULL, NULL, 0},
/*013*/   {NULL, NULL, 0},
/*014*/   {NULL, NULL, 0},
/*015*/   {NULL, NULL, 0},
/*016*/   {NULL, NULL, 0},
/*017*/   {NULL, NULL, 0},
/*018*/   {NULL, NULL, 0},
/*019*/   {NULL, NULL, 0},
/*020*/   {NULL, NULL, 0},

   /* Now we get down to business */

/*021*/   {"Join", "-%C10-%C11>%O$t%B$1%B%C has joined.", 1},
/*022*/   {"Channel Action", "%C13*%O$t$1 $2%O", 2},
/*023*/   {"Channel Message", "%C2<%O$1%C2>%O$t$2%O", 2},
/*024*/   {"Private Message", "%C12*%C13$1%C12*$t%O$2%O", 2},
/*025*/   {"Change Nick", "-%C10-%C11-%O$t$1 is now known as $2.", 2},
/*026*/   {"New Topic", "-%C10-%C11-%O$t$1 has changed the topic to: $2%O", 3},
/*027*/   {"Topic", "-%C10-%C11-%O$tTopic is %C11$1%O", 1},
/*028*/   {"Kick", "<%C10-%C11-%O$t$1 has kicked $2 ($3%O)", 3},
/*029*/   {"Part", "<%C10-%C11-%O$t$1%C has left.", 1},
/*030*/   {"Quit", "<%C10-%C11-%O$t$1 has quit %C14(%O$2%O%C14)%O", 2},
/*031*/   {"Part with Reason", "<%C10-%C11-%O$t$1%C has left %C14(%O$2%C14)%O", 2},
/*032*/   {"Notice", "%C12-%C13$1%C12-%O$t$2%O", 2},
/*033*/   {"Message Send", "%C3>%O$1%C3<%O$t$2%O", 2},
/*034*/   {"Your Message", "%C6<%O$1%C6>%O$t$2%O", 2},
/*035*/   {"Away Line", "-%C10-%C11-%O$t%C12[%O$1%C12] %Cis away %C14(%O$2%O%C14)", 2},
/*036*/   {"Your Nick Changing", "-%C10-%C11-%O$tYou are now known as $2.", 2},
/*037*/   {"You're Kicked", "-%C10-%C11-%O$tYou have been kicked by $2 ($3%O)", 3},
/*038*/   {"Channel Set Key", "-%C10-%C11-%O$t$1 sets keyword to $2.", 2},
/*039*/   {"Channel Set Limit", "-%C10-%C11-%O$t$1 sets limit to $2.", 2},
/*040*/   {"Channel Operator", "-%C10-%C11-%O$t$1 gives operator status to $2.", 2},
/*041*/   {"Channel Voice", "-%C10-%C11-%O$t$1 gives voice to $2.", 2},
/*042*/   {"Channel Ban", "-%C10-%C11-%O$t$1 sets ban on $2.", 2},
/*043*/   {"Channel Remove Keyword", "-%C10-%C11-%O$t$1 removes keyword.", 1},
/*044*/   {"Channel Remove Limit", "-%C10-%C11-%O$t$1 removes user limit.", 1},
/*045*/   {"Channel DeOp", "-%C10-%C11-%O$t$1 removes operator status from $2.", 2},
/*046*/   {"Channel DeVoice", "-%C10-%C11-%O$t$1 removes voice from $2.", 2},
/*047*/   {"Channel UnBan", "-%C10-%C11-%O$t$1 removes ban on $2.", 2},
/*048*/   {"Channel Exempt", "-%C10-%C11-%O$t$1 sets exempt on $2.", 2},
/*049*/   {"Channel Remove Exempt", "-%C10-%C11-%O$t$1 removes exempt on $2.", 2},
/*050*/   {"Channel INVITE", "-%C10-%C11-%O$t$1 sets invite on $2.", 2},
/*051*/   {"Channel Remove INVITE", "-%C10-%C11-%O$t$1 removes invite on $2.", 2},
/*052*/   {"Channel Mode Generic", "-%C10-%C11-%O$t$1 sets mode $2$3 $4", 4},
/*053*/   {"User Limit", "-%C10-%C11-%O$tCannot join%C11 %B$1 %O(User limit reached).", 1},
/*054*/   {"Banned", "-%C10-%C11-%O$tCannot join%C11 %B$1 %O(You are banned).", 1},
/*055*/   {"Invite", "-%C10-%C11-%O$tCannot join%C11 %B$1 %O(Channel is invite only).", 1},
/*056*/   {"Keyword", "-%C10-%C11-%O$tCannot join%C11 %B$1 %O(Requires keyword).", 1},
/*057*/   {"MOTD Skipped", "-%C10-%C11-%O$tMOTD Skipped.", 0},
/*058*/   {"Server Text", "-%C10-%C11-%O$t$1%O", 1},
/*059*/   {"Nick Clash", "-%C10-%C11-%O$t$1 already in use. Retrying with $2..", 2},
/*060*/   {"Nick Failed", "-%C10-%C11-%O$tNickname already in use. Use /NICK to try another.", 0},
/*061*/   {"Unknown Host", "-%C10-%C11-%O$tUnknown host. Maybe you misspelled it?", 0},
/*062*/   {"Connection Failed", "-%C10-%C11-%O$tConnection failed. Error: $1", 1},
/*063*/   {"Connecting", "-%C10-%C11-%O$tConnecting to %C11$1 %C14(%C11$2%C14)%C port %C11$3%C..", 3},
/*064*/   {"Connected", "-%C10-%C11-%O$tConnected. Now logging in..", 0},
/*065*/   {"Stop Connection", "-%C10-%C11-%O$tStopped previous connection attempt (pid=$1)", 1},
/*066*/   {"Disconnected", "-%C10-%C11-%O$tDisconnected. ($1).", 1},
/*067*/   {"Killed", "-%C10-%C11-%O$tYou have been killed by $1 ($2%O)", 2},
/*068*/   {"Server Lookup", "-%C10-%C11-%O$tLooking up %C11$1%C..", 1},
/*069*/   {"Server Connected", "-%C10-%C11-%O$tConnected.", 0},
/*070*/   {"Server Error", "-%C10-%C11-%O$t$1%O", 1},
/*071*/   {"Server Generic Message", "-%C10-%C11-%O$t$1%O", 1},
/*072*/   {NULL, NULL, 0},  /* XP_HIGHLIGHT */
/*073*/   {"Motd", "-%C10-%C11-%O$t$1%O", 1},
/*074*/   {NULL, NULL, 0},	/* XP_IF_SEND */
/*075*/   {NULL, NULL, 0},	/* XP_IF_RECV */
};

static int
text_event (int i)
{
   if (te[i].name == NULL)
      return 0;
   else
      return 1;
}

static void
pevent_load_defaults ()
{
   int i, len;

   for (i = 0; i < NUM_XP; i++)
   {
      if (!text_event (i))
         continue;
      len = strlen (te[i].def);
      len++;
      if (pntevts_text[i])
         free (pntevts_text[i]);
      pntevts_text[i] = malloc (len);
      memcpy (pntevts_text[i], te[i].def, len);
   }
}

static void
pevent_make_pntevts ()
{
  int i, m;
  
  for (i = 0; i < NUM_XP; i++)
    {
      if (!text_event (i))
	continue;
      if (pntevts[i] != NULL)
	free (pntevts[i]);
      if (pevt_build_string (pntevts_text[i], &(pntevts[i]), &m) != 0)
	{
	  fprintf (stderr, "NF-CHAT Error: Default event text from  %sfailed to build!\n", te[i].name);
	  abort ();
	}
      check_special_chars (pntevts[i]);
    }
}

static void
pevent_check_all_loaded ()
{
   int i, len;

   for (i = 0; i < NUM_XP; i++)
   {
      if (!text_event (i))
         continue;
      if (pntevts_text[i] == NULL)
      {

	printf("%s\n", te[i].name);

	len = strlen (te[i].def) + 1;
	pntevts_text[i] = malloc (len);
	memcpy (pntevts_text[i], te[i].def, len);
      }
   }
}

void
load_text_events ()
{
   /* I don't free these as the only time when we don't need them
      is once XChat has exit(2)'ed, so why bother ?? --AGL */

   pntevts_text = malloc (sizeof (char *) * (NUM_XP));
   memset (pntevts_text, 0, sizeof (char *) * (NUM_XP));
   pntevts = malloc (sizeof (char *) * (NUM_XP));
   memset (pntevts, 0, sizeof (char *) * (NUM_XP));

   pevent_load_defaults ();
   pevent_check_all_loaded ();
   pevent_make_pntevts ();
}

static void
display_event (char *i, int numargs, char **args)
{
   int len, oi, ii, *ar;
   char o[4096], d, a, done_all = FALSE;

   oi = ii = len = d = a = 0;
 
   if (i == NULL)
      return;

   while (done_all == FALSE)
   {
      d = i[ii++];
      switch (d)
      {
      case 0:
         memcpy (&len, &(i[ii]), sizeof (int));
         ii += sizeof (int);
         if (oi + len > sizeof (o))
         {
            fprintf (stderr, "NF-CHAT Error: display_event: Overflow in display_event (%s)\n", i);
            return;
         }
         memcpy (&(o[oi]), &(i[ii]), len);
         oi += len;
         ii += len;
         break;
      case 1:
         a = i[ii++];
         if (a > numargs)
         {
            fprintf (stderr, "NF-CHAT Error: display_event: arg > numargs (%d %d %s)\n", a, numargs, i);
	     break;
         }
         ar = (int *) args[(int) a];
         if (ar == NULL)
         {
            printf ("NF-CHAT Error: Error args[a] == NULL in display_event\n");
            abort ();
         }
         len = strlen ((char *) ar);
         memcpy (&o[oi], ar, len);
         oi += len;
         break;
      case 2:
         o[oi++] = '\n';
         o[oi++] = 0;
         done_all = TRUE;
         continue;
      case 3:
	o[oi++] = '\t';
	 break;
      }
   }
   o[oi] = 0;
   if (*o == '\n')
      return;
   PrintText (o);
}

static int
pevt_build_string (char *input, char **output, int *max_arg)
{
   struct pevt_stage1 *s = NULL, *base = NULL,
              *last = NULL, *next;
   int clen;
   char o[4096], d, *obuf, *i;
   int oi, ii, ti, max = -1, len, x;

   len = strlen (input);
   i = malloc (len + 1);
   memcpy (i, input, len + 1);
   check_special_chars (i);

   len = strlen (i);

   clen = oi = ii = ti = 0;

   for (;;)
   {
      if (ii == len)
         break;
      d = i[ii++];
      if (d != '$')
      {
         o[oi++] = d;
         continue;
      }
      if (i[ii] == '$')
      {
         o[oi++] = '$';
         continue;
      }
      if (oi > 0)
      {
         s = (struct pevt_stage1 *) malloc (sizeof (struct pevt_stage1));
         if (base == NULL)
            base = s;
         if (last != NULL)
            last->next = s;
         last = s;
         s->next = NULL;
         s->data = malloc (oi + sizeof (int) + 1);
         s->len = oi + sizeof (int) + 1;
         clen += oi + sizeof (int) + 1;
         s->data[0] = 0;
         memcpy (&(s->data[1]), &oi, sizeof (int));
         memcpy (&(s->data[1 + sizeof (int)]), o, oi);
         oi = 0;
      }
      if (ii == len)
      {
         fprintf (stderr, "NF-CHAT Error: pevt_build_string: String ends with a $");
         return 1;
      }
      d = i[ii++];
      if (d == 'a')
      {                         /* Hex value */
         x = 0;
         if (ii == len)
            goto a_len_error;
         d = i[ii++];
         d -= '0';
         x = d * 100;
         if (ii == len)
            goto a_len_error;
         d = i[ii++];
         d -= '0';
         x += d * 10;
         if (ii == len)
            goto a_len_error;
         d = i[ii++];
         d -= '0';
         x += d;
         if (x > 255)
            goto a_range_error;
         o[oi++] = x;
         continue;

       a_len_error:
         fprintf (stderr, "NF-CHAT Error: pevt_build_string: String ends in $a");
         return 1;
       a_range_error:
         fprintf (stderr, "NF-CHAT Error: pevt_build_string: $a value is greater then 255");
         return 1;
      }
      if (d == 't') {
	 /* Tab - if tabnicks is set then write '\t' else ' ' */
	 /*s = g_new (struct pevt_stage1, 1);*/
    s = (struct pevt_stage1 *) malloc (sizeof (struct pevt_stage1));
	 if (base == NULL)
	   base = s;
	 if (last != NULL)
	   last->next = s;
	 last = s;
	 s->next = NULL;
	 s->data = malloc (1);
	 s->len = 1;
	 clen += 1;
	 s->data[0] = 3;
	 
	 continue;
      }
      if (d < '0' || d > '9')
      {
         fprintf (stderr, "NF-CHAT Error: invalid argument $%c\n", d);
         return 1;
      }
      d -= '0';
      if (max < d)
         max = d;
      s = (struct pevt_stage1 *) malloc (sizeof (struct pevt_stage1));
      if (base == NULL)
         base = s;
      if (last != NULL)
         last->next = s;
      last = s;
      s->next = NULL;
      s->data = malloc (2);
      s->len = 2;
      clen += 2;
      s->data[0] = 1;
      s->data[1] = d - 1;
   }
   if (oi > 0)
   {
      s = (struct pevt_stage1 *) malloc (sizeof (struct pevt_stage1));
      if (base == NULL)
         base = s;
      if (last != NULL)
         last->next = s;
      last = s;
      s->next = NULL;
      s->data = malloc (oi + sizeof (int) + 1);
      s->len = oi + sizeof (int) + 1;
      clen += oi + sizeof (int) + 1;
      s->data[0] = 0;
      memcpy (&(s->data[1]), &oi, sizeof (int));
      memcpy (&(s->data[1 + sizeof (int)]), o, oi);
      oi = 0;
   }
   s = (struct pevt_stage1 *) malloc (sizeof (struct pevt_stage1));
   if (base == NULL)
      base = s;
   if (last != NULL)
      last->next = s;
   last = s;
   s->next = NULL;
   s->data = malloc (1);
   s->len = 1;
   clen += 1;
   s->data[0] = 2;

   oi = 0;
   s = base;
   obuf = malloc (clen);
   while (s)
   {
      next = s->next;
      memcpy (&obuf[oi], s->data, s->len);
      oi += s->len;
      free (s->data);
      free (s);
      s = next;
   }

   free (i);

   if (max_arg)
      *max_arg = max;
   if (output)
      *output = obuf;

   return 0;
}

static int
textsignal_handler (void *b, void *c, void *d, void *e, char f)
{
   /* This handler *MUST* always be the last in the chain
      because it doesn't call the next handler
    */

   char *args[8];
   int numargs, i;

   if (!text_event (current_signal))
   {
      printf ("error, textsignal_handler passed non TE signal (%d)\n", current_signal);
      abort ();
   }
   numargs = te[current_signal].num_args;
   i = 0;

   args[0] = b;
   args[1] = c;
   args[2] = d;
   args[3] = e;

   display_event (pntevts[(int) current_signal], numargs, args);
   return 0;
}

void
printevent_setup ()
{
   int evt = 0;
   struct xp_signal *sig;

   sig = (struct xp_signal *) malloc (sizeof (struct xp_signal));

   sig->signal = -1;
   sig->callback = XP_CALLBACK (textsignal_handler);
   
   for (evt = 0; evt < NUM_XP; evt++)
   {
      if (!text_event (evt))
         continue;
      g_assert (!sigroots[evt]);
      sigroots[evt] = (struct xp_signal *)g_slist_prepend ((void *)sigroots[evt], sig);
   }
}
