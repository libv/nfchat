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

extern char *g_get_home_dir (void);
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
is_channel (char *string)
{
   switch (*string)
   {
      case '#': /* normal channel */
      case '&': /* normal channel */
      case '+': /* modeless channel */
      case '!': /* some obscure IRC RFC breaking channel */
         return 1;
   }
   return 0;
}

char *
file_part (char *file)
{
   char *filepart = file;
   if (!file) return "";
   while (1)
   {
      switch (*file)
      {
      case 0:
         return (filepart);
      case '/':
         filepart = file + 1;
         break;
      }
      file++;
   }
}

void
path_part (char *file, char *path)
{
   char *filepart = file_part (file);
   *filepart = 0;
   strcpy (path, file);
}

char *
nocasestrstr (char *text, char *tofind)  /* like strstr(), but nocase */
{
   char *ret = text, *find = tofind;

   while (1)
   {
      if (*find == 0)
         return ret;
      if (*text == 0)
         return 0;
      if (toupper (*find) != toupper (*text))
      {
         ret = text + 1;
         find = tofind;
      } else
         find++;
      text++;
   }
}

char *
errorstring (int err)
{
   static char tbuf[16];
   switch (err)
   {
   case -1:
      return "";
   case 0:
      return "Remote host closed socket";
   case ECONNREFUSED:
      return "Connection refused";
   case ENETUNREACH:
   case EHOSTUNREACH:
      return "No route to host";
   case ETIMEDOUT:
      return "Connection timed out";
   case EADDRNOTAVAIL:
      return "Cannot assign that address";
   case ECONNRESET:
      return "Connection reset by peer";
   }
   sprintf (tbuf, "%d", err);
   return tbuf;
}

int
waitline (int sok, char *buf, int bufsize)
{
   int i = 0;

   while (1)
   {
      if (read (sok, &buf[i], 1) < 1)
         return -1;
      if (buf[i] == '\n')
      {
         buf[i] = 0;
         return i;
      }
      i++;
      if (i == (bufsize - 1))
         return 0;
   }
}

/* checks for "~" in a file and expands */

char *
expand_homedir (char *file)
{
   char *ret;

   if (*file == '~')
   {
      ret = malloc (strlen (file) + strlen (g_get_home_dir()) + 1);
      sprintf (ret, "%s%s", g_get_home_dir (), file + 1);
      return ret;
   }
   return strdup (file);
}

unsigned char *
strip_color (unsigned char *text)
{
   int nc = 0;
   int i = 0;
   int col = 0;
   int len = strlen (text);
   unsigned char *new_str = malloc (len + 2);

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
            col = 0;
         switch (*text)
         {
         case '\003':/*ATTR_COLOR:*/
            col = 1;
            nc = 0;
            break;
         case '\007':/*ATTR_BEEP:*/
         case '\017':/*ATTR_RESET:*/
         case '\026':/*ATTR_REVERSE:*/
         case '\002':/*ATTR_BOLD:*/
         case '\037':/*ATTR_UNDERLINE:*/
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

#ifdef USING_LINUX

static void
get_cpu_info (int *mhz, int *cpus)
{
   char buf[256];
   int fh;

   *mhz = 0;
   *cpus = 0;

   fh = open ("/proc/cpuinfo", O_RDONLY);  /* linux 2.2+ only */
   if (fh == -1)
   {
      *cpus = 1;
      return;
   }

   while (1)
   {
      if (waitline (fh, buf, sizeof buf) < 0)
         break;
      if (!strncmp (buf, "cycle frequency [Hz]\t:", 22))    /* alpha */
      {
         *mhz = atoi (buf + 23) / 1048576;
      }
      else if (!strncmp (buf, "cpu MHz\t\t:", 10))          /* i386 */
      {
         *mhz = atof (buf + 11) + 0.5;
      }
      else if (!strncmp (buf, "clock\t\t:", 8))             /* PPC */
      {
         *mhz = atoi (buf + 9);
      }
      else if (!strncmp (buf, "processor\t", 10))
      {
         (*cpus)++;
      }
   }
   close (fh);
   if (!*cpus)
      *cpus = 1;
}

#endif

char *
get_cpu_str (int color)
{
#ifdef USING_LINUX
   int mhz, cpus;
#endif
   struct utsname un;
   static char *buf = NULL;
   static char *color_buf;

   if (buf)
   {
      return (color) ? color_buf : buf;
   }

   color_buf = malloc (128);

   uname (&un);

#ifdef USING_LINUX
   get_cpu_info (&mhz, &cpus);
   if (mhz)
      snprintf (color_buf, 128,
               (cpus == 1) ? "%s \00310%s \0032[\00310%s\0032/\00310%dMHz\0032]"
                           : "%s \00310%s \0032[\00310%s\0032/\00310%dMHz\0032/\00310SMP\0032]",
               un.sysname, un.release, un.machine, mhz);
   else
#endif
      snprintf (color_buf, 128, "%s \00310%s \0032[\00310%s\0032]",
                un.sysname, un.release, un.machine);

   buf = strip_color (color_buf);

   return (color) ? color_buf : buf;
}

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

#ifdef HAVE_FNMATCH

#define _GNU_SOURCE
#include <fnmatch.h>

#ifdef FNM_CASEFOLD

int
match (unsigned char *mask, unsigned char *string)
{
   if (fnmatch (mask, string, FNM_CASEFOLD) == 0)
      return 1;

   return 0;
}

#else

int
match (unsigned char *mask, unsigned char *string)
{
   int ret = 0;

   mask = strdup (mask);
   string = strdup (string);

   tolowerStr (mask);      /* hack to make it case-insensitive */
   tolowerStr (string);

   if (fnmatch (mask, string, 0) == 0)
      ret = 1;

   free (mask);
   free (string);

   return ret;
}

#endif

#else

int
match (unsigned char *mask, unsigned char *string)
{
   unsigned char *m = mask, *n = string,
       *ma = NULL, *na = NULL;
   int just = 0;
   unsigned char *pm = NULL, *pn = NULL;
   int lp_c = 0, count = 0, last_count = 0;

   while (1)
   {

      switch (*m)
      {
      case '*':
         goto ultimate_lameness;
         break;
      case '%':
         goto ultimate_lameness2;
         break;
      case '?':
         m++;
         if (!*n++)
            return 0;
         break;
      case 0:
         if (!*n)
            return 1;
      case '\\':
         if (((*m == '\\') && (m[1] == '*')) || (m[1] == '?'))
            m++;
      default:
         if (tolower (*m) != tolower (*n))
            return 0;
         else
         {
            count++;
            m++;
            n++;
         }
      }
   }

   while (1)
   {

      switch (*m)
      {
      case '*':
       ultimate_lameness:
         ma = ++m;
         na = n;
         just = 1;
         last_count = count;
         break;
      case '%':
       ultimate_lameness2:
         pm = ++m;
         pn = n;
         lp_c = count;
         if (*n == ' ')
            pm = NULL;
         break;
      case '?':
         m++;
         if (!*n++)
            return 0;
         break;
      case 0:
         if (!*n || just)
            return 1;
      case '\\':
         if (((*m == '\\') && (m[1] == '*')) || (m[1] == '?'))
            m++;
      default:
         just = 0;
         if (tolower (*m) != tolower (*n))
         {
            if (!*n)
               return 0;
            if (pm)
            {
               m = pm;
               n = ++pn;
               count = lp_c;
               if (*n == ' ')
                  pm = NULL;
            } else if (ma)
            {
               m = ma;
               n = ++na;
               if (!*n)
                  return 0;
               count = last_count;
            } else
               return 0;
         } else
         {
            count++;
            m++;
            n++;
         }
      }
   }
}

#endif

int
for_files (char *dirname, char *mask, void callback (char *file))
{
   DIR *dir;
   struct dirent *ent;
   char *buf;
   int i = 0;

   dir = opendir (dirname);
   if (dir)
   {
      while ((ent = readdir (dir)))
      {
         if (strcmp (ent->d_name, ".") && strcmp (ent->d_name, ".."))
         {
            if (match (mask, ent->d_name))
            {
               i++;
               buf = malloc (strlen (dirname) + strlen (ent->d_name) + 2);
               sprintf (buf, "%s/%s", dirname, ent->d_name);
               callback (buf);
               free (buf);
            }
         }
      }
      closedir (dir);
   }
   return i;
}

/*void
toupperStr (char *str)
{
   while (*str)
   {
      *str = toupper (*str);
      str++;
   }
}*/

void
tolowerStr (char *str)
{
   while (*str)
   {
      *str = tolower (*str);
      str++;
   }
}

/* thanks BitchX */

char *country(char *hostname)
{
typedef struct _domain {
char *code;
char *country;
} Domain;
static Domain domain[] = {
	{"AD", "Andorra" },
	{"AE", "United Arab Emirates" },
	{"AF", "Afghanistan" },
	{"AG", "Antigua and Barbuda" },
	{"AI", "Anguilla" },
	{"AL", "Albania" },
	{"AM", "Armenia" },
	{"AN", "Netherlands Antilles" },
	{"AO", "Angola" },
	{"AQ", "Antarctica" },
	{"AR", "Argentina" },
	{"AS", "American Samoa" },
	{"AT", "Austria" },
	{"AU", "Australia" },
	{"AW", "Aruba" },
	{"AZ", "Azerbaijan" },
	{"BA", "Bosnia and Herzegovina" },
	{"BB", "Barbados" },
	{"BD", "Bangladesh" },
	{"BE", "Belgium" },
	{"BF", "Burkina Faso" },
	{"BG", "Bulgaria" },
	{"BH", "Bahrain" },
	{"BI", "Burundi" },
	{"BJ", "Benin" },
	{"BM", "Bermuda" },
	{"BN", "Brunei Darussalam" },
	{"BO", "Bolivia" },
	{"BR", "Brazil" },
	{"BS", "Bahamas" },
	{"BT", "Bhutan" },
	{"BV", "Bouvet Island" },
	{"BW", "Botswana" },
	{"BY", "Belarus" },
	{"BZ", "Belize" },
	{"CA", "Canada" },
	{"CC", "Cocos Islands" },
	{"CF", "Central African Republic" },
	{"CG", "Congo" },
	{"CH", "Switzerland" },
	{"CI", "Cote D'ivoire" },
	{"CK", "Cook Islands" },
	{"CL", "Chile" },
	{"CM", "Cameroon" },
	{"CN", "China" },
	{"CO", "Colombia" },
	{"CR", "Costa Rica" },
	{"CS", "Former Czechoslovakia" },
	{"CU", "Cuba" },
	{"CV", "Cape Verde" },
	{"CX", "Christmas Island" },
	{"CY", "Cyprus" },
	{"CZ", "Czech Republic" },
	{"DE", "Germany" },
	{"DJ", "Djibouti" },
	{"DK", "Denmark" },
	{"DM", "Dominica" },
	{"DO", "Dominican Republic" },
	{"DZ", "Algeria" },
	{"EC", "Ecuador" },
	{"EE", "Estonia" },
	{"EG", "Egypt" },
	{"EH", "Western Sahara" },
	{"ER", "Eritrea" },
	{"ES", "Spain" },
	{"ET", "Ethiopia" },
	{"FI", "Finland" },
	{"FJ", "Fiji" },
	{"FK", "Falkland Islands" },
	{"FM", "Micronesia" },
	{"FO", "Faroe Islands" },
	{"FR", "France" },
	{"FX", "France, Metropolitan" },
	{"GA", "Gabon" },
	{"GB", "Great Britain" },
	{"GD", "Grenada" },
	{"GE", "Georgia" },
	{"GF", "French Guiana" },
	{"GH", "Ghana" },
	{"GI", "Gibraltar" },
	{"GL", "Greenland" },
	{"GM", "Gambia" },
	{"GN", "Guinea" },
	{"GP", "Guadeloupe" },
	{"GQ", "Equatorial Guinea" },
	{"GR", "Greece" },
	{"GS", "S. Georgia and S. Sandwich Isles." },
	{"GT", "Guatemala" },
	{"GU", "Guam" },
	{"GW", "Guinea-Bissau" },
	{"GY", "Guyana" },
	{"HK", "Hong Kong" },
	{"HM", "Heard and McDonald Islands" },
	{"HN", "Honduras" },
	{"HR", "Croatia" },
	{"HT", "Haiti" },
	{"HU", "Hungary" },
	{"ID", "Indonesia" },
	{"IE", "Ireland" },
	{"IL", "Israel" },
	{"IN", "India" },
	{"IO", "British Indian Ocean Territory" },
	{"IQ", "Iraq" },
	{"IR", "Iran" },
	{"IS", "Iceland" },
	{"IT", "Italy" },
	{"JM", "Jamaica" },
	{"JO", "Jordan" },
	{"JP", "Japan" },
	{"KE", "Kenya" },
	{"KG", "Kyrgyzstan" },
	{"KH", "Cambodia" },
	{"KI", "Kiribati" },
	{"KM", "Comoros" },
	{"KN", "St. Kitts and Nevis" },
	{"KP", "North Korea" },
	{"KR", "South Korea" },
	{"KW", "Kuwait" },
	{"KY", "Cayman Islands" },
	{"KZ", "Kazakhstan" },
	{"LA", "Laos" },
	{"LB", "Lebanon" },
	{"LC", "Saint Lucia" },
	{"LI", "Liechtenstein" },
	{"LK", "Sri Lanka" },
	{"LR", "Liberia" },
	{"LS", "Lesotho" },
	{"LT", "Lithuania" },
	{"LU", "Luxembourg" },
	{"LV", "Latvia" },
	{"LY", "Libya" },
	{"MA", "Morocco" },
	{"MC", "Monaco" },
	{"MD", "Moldova" },
	{"MG", "Madagascar" },
	{"MH", "Marshall Islands" },
	{"MK", "Macedonia" },
	{"ML", "Mali" },
	{"MM", "Myanmar" },
	{"MN", "Mongolia" },
	{"MO", "Macau" },
	{"MP", "Northern Mariana Islands" },
	{"MQ", "Martinique" },
	{"MR", "Mauritania" },
	{"MS", "Montserrat" },
	{"MT", "Malta" },
	{"MU", "Mauritius" },
	{"MV", "Maldives" },
	{"MW", "Malawi" },
	{"MX", "Mexico" },
	{"MY", "Malaysia" },
	{"MZ", "Mozambique" },
	{"NA", "Namibia" },
	{"NC", "New Caledonia" },
	{"NE", "Niger" },
	{"NF", "Norfolk Island" },
	{"NG", "Nigeria" },
	{"NI", "Nicaragua" },
	{"NL", "Netherlands" },
	{"NO", "Norway" },
	{"NP", "Nepal" },
	{"NR", "Nauru" },
	{"NT", "Neutral Zone" },
	{"NU", "Niue" },
	{"NZ", "New Zealand" },
	{"OM", "Oman" },
	{"PA", "Panama" },
	{"PE", "Peru" },
	{"PF", "French Polynesia" },
	{"PG", "Papua New Guinea" },
	{"PH", "Philippines" },
	{"PK", "Pakistan" },
	{"PL", "Poland" },
	{"PM", "St. Pierre and Miquelon" },
	{"PN", "Pitcairn" },
	{"PR", "Puerto Rico" },
	{"PT", "Portugal" },
	{"PW", "Palau" },
	{"PY", "Paraguay" },
	{"QA", "Qatar" },
	{"RE", "Reunion" },
	{"RO", "Romania" },
	{"RU", "Russian Federation" },
	{"RW", "Rwanda" },
	{"SA", "Saudi Arabia" },
	{"Sb", "Solomon Islands" },
	{"SC", "Seychelles" },
	{"SD", "Sudan" },
	{"SE", "Sweden" },
	{"SG", "Singapore" },
	{"SH", "St. Helena" },
	{"SI", "Slovenia" },
	{"SJ", "Svalbard and Jan Mayen Islands" },
	{"SK", "Slovak Republic" },
	{"SL", "Sierra Leone" },
	{"SM", "San Marino" },
	{"SN", "Senegal" },
	{"SO", "Somalia" },
	{"SR", "Suriname" },
	{"ST", "Sao Tome and Principe" },
	{"SU", "Former USSR" },
	{"SV", "El Salvador" },
	{"SY", "Syria" },
	{"SZ", "Swaziland" },
	{"TC", "Turks and Caicos Islands" },
	{"TD", "Chad" },
	{"TF", "French Southern Territories" },
	{"TG", "Togo" },
	{"TH", "Thailand" },
	{"TJ", "Tajikistan" },
	{"TK", "Tokelau" },
	{"TM", "Turkmenistan" },
	{"TN", "Tunisia" },
	{"TO", "Tonga" },
	{"TP", "East Timor" },
	{"TR", "Turkey" },
	{"TT", "Trinidad and Tobago" },
	{"TV", "Tuvalu" },
	{"TW", "Taiwan" },
	{"TZ", "Tanzania" },
	{"UA", "Ukraine" },
	{"UG", "Uganda" },
	{"UK", "United Kingdom" },
	{"UM", "US Minor Outlying Islands" },
	{"US", "United States of America" },
	{"UY", "Uruguay" },
	{"UZ", "Uzbekistan" },
	{"VA", "Vatican City State" },
	{"VC", "St. Vincent and the grenadines" },
	{"VE", "Venezuela" },
	{"VG", "British Virgin Islands" },
	{"VI", "US Virgin Islands" },
	{"VN", "Vietnam" },
	{"VU", "Vanuatu" },
	{"WF", "Wallis and Futuna Islands" },
	{"WS", "Samoa" },
	{"YE", "Yemen" },
	{"YT", "Mayotte" },
	{"YU", "Yugoslavia" },
	{"ZA", "South Africa" },
	{"ZM", "Zambia" },
	{"ZR", "Zaire" },
	{"ZW", "Zimbabwe" },
	{"COM", "Internic Commercial" },
	{"EDU", "Educational Institution" },
	{"GOV", "Government" },
	{"INT", "International" },
	{"MIL", "Military" },
	{"NET", "Internic Network" },
	{"ORG", "Internic Non-Profit Organization" },
	{"RPA", "Old School ARPAnet" },
	{"ATO", "Nato Fiel" },
	{"MED", "United States Medical" },
	{"ARPA", "Reverse DNS" },
	{NULL, NULL}
};
char *p;
int i = 0;
	if (!hostname || !*hostname || isdigit(hostname[strlen(hostname)-1]))
		return "unknown";
	if ((p = strrchr(hostname, '.')))
		p++;
	else
		p = hostname;
	for (i = 0; domain[i].code; i++)
		if (!strcasecmp(p, domain[i].code))
			return domain[i].country;
	return "unknown";
}


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

unsigned char tolowertab[] =
{0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
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
 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

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