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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/utsname.h>
#include "util.h"
#include "../config.h"
#ifdef SOCKS
#include <socks.h>
#endif

#ifndef HAVE_SNPRINTF
#define snprintf g_snprintf 
#endif

/* extern char *g_get_home_dir (void); */
extern int g_snprintf ( char *str, size_t n,
                                const char *format, ... );


#ifdef USE_DEBUG

#define malloc malloc
#define realloc realloc
#define free free
#undef strdup
#define strdup strdup

int current_mem_usage;

struct mem_block
{
   void *buf;
   int size;
   struct mem_block *next;
};

struct mem_block *mroot = NULL;


void *
xchat_malloc (int size)
{
   void *ret;
   struct mem_block *new;

   current_mem_usage += size;
   ret = malloc (size);
   if (!ret)
   {
      printf ("Out of memory! (%d)\n", current_mem_usage);
      exit (255);
   }

   new = malloc (sizeof (struct mem_block));
   new->buf = ret;
   new->size = size;
   new->next = mroot;
   mroot = new;

   printf ("Malloc'ed %d bytes, now \033[35m%d\033[m\n", size, current_mem_usage);

   return ret;
}

void *
xchat_realloc (char *old, int len)
{
   char *ret;

   ret = xchat_malloc (len);
   if (ret)
   {
      strcpy (ret, old);
      xchat_free (old);
   }
   return ret;
}

void *
xchat_strdup (char *str)
{
   void *ret;
   struct mem_block *new;
   int size;

   size = strlen (str) + 1;
   current_mem_usage += size;
   ret = malloc (size);
   if (!ret)
   {
      printf ("Out of memory! (%d)\n", current_mem_usage);
      exit (255);
   }
   strcpy (ret, str);

   new = malloc (sizeof (struct mem_block));
   new->buf = ret;
   new->size = size;
   new->next = mroot;
   mroot = new;

   printf ("strdup (\"%-.40s\") size: %d, total: \033[35m%d\033[m\n", str, size, current_mem_usage);

   return ret;
}

void
xchat_free (void *buf)
{
   struct mem_block *cur, *last;

   last = NULL;
   cur = mroot;
   while (cur)
   {
      if (buf == cur->buf)
         break;
      last = cur;
      cur = cur->next;
   }
   if (cur == NULL)
   {
      printf ("\033[31mTried to free unknown block %lx!\033[m\n", (unsigned long)buf);
      /*      abort(); */
      free (buf);
      free (cur);
      return;
   }
   current_mem_usage -= cur->size;
   printf ("Free'ed %d bytes, usage now \033[35m%d\033[m\n", cur->size, current_mem_usage);
   if (last)
      last->next = cur->next;
   else
      mroot = cur->next;
   free (cur);
}

#endif /* MEMORY_DEBUG */

int
buf_get_line (char *ibuf, char **buf, int *position, int len)
{
   int pos = *position, spos = pos;

   if (pos == len)
      return 0;

   while (ibuf[pos++] != '\n')
   {
      if (pos == len)
         return 0;
   }
   pos--;
   ibuf[pos] = 0;
   *buf = &ibuf[spos];
   pos++;
   *position = pos;
   return 1;
}

/* thanks BitchX */

/************************************************************************
 *    This technique was borrowed in part from the source code to 
 *    ircd-hybrid-5.3 to implement case-insensitive string matches which
 *    are fully compliant with Section 2.2 of RFC 1459, the copyright
 *    of that code being (C) 1990 Jarkko Oikarinen and under the GPL.
 *    
 *    A special thanks goes to Mr. Okarinen for being the one person who
 *    seems to have ever noticed this section in the original RFC and
 *    written code for it.  Shame on all the rest of you (myself included).
 *    
 *        --+ Dagmar d'Surreal
 */


int
rfc_casecmp (char *s1, char *s2)
{
   register unsigned char *str1 = (unsigned char *) s1;
   register unsigned char *str2 = (unsigned char *) s2;
   register int res;

   while ((res = toupper (*str1) - toupper (*str2)) == 0)
   {
      if (*str1 == '\0')
         return 0;
      str1++;
      str2++;
   }
   return (res);
}

int
rfc_ncasecmp (char *str1, char *str2, int n)
{
   register unsigned char *s1 = (unsigned char *) str1;
   register unsigned char *s2 = (unsigned char *) str2;
   register int res;

   while ((res = toupper (*s1) - toupper (*s2)) == 0)
   {
      s1++;
      s2++;
      n--;
      if (n == 0 || (*s1 == '\0' && *s2 == '\0'))
         return 0;
   }
   return (res);
}

unsigned char touppertab[] =
{0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
 0x1e, 0x1f,
 ' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
 '*', '+', ',', '-', '.', '/',
 '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
 ':', ';', '<', '=', '>', '?',
 '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
 0x5f,
 '`', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
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
 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};
