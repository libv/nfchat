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

#ifndef SIGNAL_H
#define SIGNAL_H

#define XP_CALLBACK(x)	( (int (*) (void *, void *, void *, void *, char) ) x )

#define XP_USERCOMMAND	0
#define XP_PRIVMSG	1
#define XP_CHANACTION	2
#define XP_CHANMSG	3
#define XP_CHANGENICK	4
#define XP_JOIN		5
#define XP_CHANSETKEY	6
#define XP_CHANSETLIMIT	7
#define XP_CHANOP	8
#define XP_CHANVOICE	9
#define XP_CHANBAN	10
#define XP_CHANRMKEY	11
#define XP_CHANRMLIMIT	12
#define XP_CHANDEOP	13
#define XP_CHANDEVOICE	14
#define XP_CHANUNBAN	15
#define XP_CHANEXEMPT   16
#define XP_CHANRMEXEMPT 17
#define XP_CHANINVITE   18
#define XP_CHANRMINVITE 19
#define XP_INBOUND	20

#define XP_TE_JOIN       21
#define XP_TE_CHANACTION 22
#define XP_TE_CHANMSG   23
#define XP_TE_PRIVMSG   24
#define XP_TE_CHANGENICK 25
#define XP_TE_NEWTOPIC  26
#define XP_TE_TOPIC     27
#define XP_TE_KICK      28
#define XP_TE_PART      29
#define XP_TE_QUIT      30
#define XP_TE_PARTREASON 31
#define XP_TE_NOTICE    32
#define XP_TE_MSGSEND      33
#define XP_TE_UCHANMSG  34
#define XP_TE_AWAY    35
#define XP_TE_UCHANGENICK 36
#define XP_TE_UKICK     37
#define XP_TE_CHANSETKEY 38
#define XP_TE_CHANSETLIMIT 39
#define XP_TE_CHANOP    40
#define XP_TE_CHANVOICE 41
#define XP_TE_CHANBAN   42
#define XP_TE_CHANRMKEY 43
#define XP_TE_CHANRMLIMIT 44
#define XP_TE_CHANDEOP  45
#define XP_TE_CHANDEVOICE 46
#define XP_TE_CHANUNBAN 47
#define XP_TE_CHANEXEMPT 48
#define XP_TE_CHANRMEXEMPT 49
#define XP_TE_CHANINVITE 50
#define XP_TE_CHANRMINVITE 51
#define XP_TE_CHANMODEGEN 52
#define XP_TE_USERLIMIT 53
#define XP_TE_BANNED    54
#define XP_TE_INVITE    55
#define XP_TE_KEYWORD   56
#define XP_TE_MOTDSKIP  57
#define XP_TE_SERVTEXT  58
#define XP_TE_NICKCLASH 59
#define XP_TE_NICKFAIL  60
#define XP_TE_UKNHOST   61
#define XP_TE_CONNFAIL  62
#define XP_TE_CONNECT   63
#define XP_TE_CONNECTED 64
#define XP_TE_SCONNECT  65
#define XP_TE_DISCON    66
#define XP_TE_KILL      67
#define XP_TE_SERVERLOOKUP 68
#define XP_TE_SERVERCONNECTED 69
#define XP_TE_SERVERERROR  70
#define XP_TE_SERVERGENMESSAGE 71
#define XP_HIGHLIGHT       72
#define XP_TE_MOTD 73
#define XP_IF_SEND 74
#define XP_IF_RECV 75

#define NUM_XP		   76

extern int current_signal;

struct xp_signal
{
   int signal;
   int (*callback) (void *, void *, void *, void *, char);
   /* These aren't used, but needed to keep compatibility --AGL */
   void *next, *last;
   void *data;
   void *padding;
};

struct pevt_stage1
{
   int len;
   char *data;
   struct pevt_stage1 *next;
};

#ifndef SIGNALS_C
extern struct xp_signal *sigroots[NUM_XP];
extern int fire_signal (int, void *, void *, void *, void *, char);
#endif

#endif /* SIGNAL_H */






