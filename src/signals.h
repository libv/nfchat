
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
#define XP_TE_CHANDATE  30
#define XP_TE_TOPICDATE 31
#define XP_TE_QUIT      32
#define XP_TE_PARTREASON 33
#define XP_TE_NOTICE    34
#define XP_TE_MSGSEND      35
#define XP_TE_UCHANMSG  36
#define XP_TE_AWAY    37
#define XP_TE_UCHANGENICK 38
#define XP_TE_UKICK     39
#define XP_TE_CHANSETKEY 40
#define XP_TE_CHANSETLIMIT 41
#define XP_TE_CHANOP    42
#define XP_TE_CHANVOICE 43
#define XP_TE_CHANBAN   44
#define XP_TE_CHANRMKEY 45
#define XP_TE_CHANRMLIMIT 46
#define XP_TE_CHANDEOP  47
#define XP_TE_CHANDEVOICE 48
#define XP_TE_CHANUNBAN 49
#define XP_TE_CHANEXEMPT 50
#define XP_TE_CHANRMEXEMPT 51
#define XP_TE_CHANINVITE 52
#define XP_TE_CHANRMINVITE 53
#define XP_TE_CHANMODEGEN 54
#define XP_TE_USERLIMIT 55
#define XP_TE_BANNED    56
#define XP_TE_INVITE    57
#define XP_TE_KEYWORD   58
#define XP_TE_MOTDSKIP  59
#define XP_TE_SERVTEXT  60
#define XP_TE_USERSONCHAN  61
#define XP_TE_NICKCLASH 62
#define XP_TE_NICKFAIL  63
#define XP_TE_UKNHOST   64
#define XP_TE_CONNFAIL  65
#define XP_TE_CONNECT   66
#define XP_TE_CONNECTED 67
#define XP_TE_SCONNECT  68
#define XP_TE_DISCON    69
#define XP_TE_CHANMODES 70
#define XP_TE_RAWMODES  71
#define XP_TE_KILL      72
#define XP_TE_SERVERLOOKUP 73
#define XP_TE_SERVERCONNECTED 74
#define XP_TE_SERVERERROR  75
#define XP_TE_SERVERGENMESSAGE 76
#define XP_HIGHLIGHT       77
#define XP_TE_MOTD 78
#define XP_TE_CTCPGEN   79
#define XP_TE_CTCPGENC  80
#define XP_IF_SEND 81
#define XP_IF_RECV 82

#define NUM_XP		   83

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






