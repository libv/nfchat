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

#undef toupper
#define toupper(c) (touppertab[(unsigned char)(c)])

#define strcasecmp      rfc_casecmp
#define strncasecmp     rfc_ncasecmp


extern int rfc_casecmp (char *, char *);
extern int rfc_ncasecmp (char *, char *, int);
extern unsigned char touppertab[];
extern int buf_get_line (char *, char **, int *, int len);


