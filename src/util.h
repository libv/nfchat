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

#undef toupper
#define toupper(c) (touppertab[(unsigned char)(c)])

#define strcasecmp      rfc_casecmp
#define strncasecmp     rfc_ncasecmp


extern int rfc_casecmp (char *, char *);
extern int rfc_ncasecmp (char *, char *, int);
extern unsigned char touppertab[];
extern int buf_get_line (char *, char **, int *, int len);


