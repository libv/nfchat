
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
#define XP_TE_PINGREP   33
#define XP_TE_NOTICE    34
#define XP_TE_UJOIN     35
#define XP_TE_UCHANMSG  36
#define XP_TE_DPRIVMSG  37
#define XP_TE_UCHANGENICK 38
#define XP_TE_UKICK     39
#define XP_TE_UPART     40
#define XP_TE_CTCPSND   41
#define XP_TE_CTCPGEN   42
#define XP_TE_CTCPGENC  43
#define XP_TE_CHANSETKEY 44
#define XP_TE_CHANSETLIMIT 45
#define XP_TE_CHANOP    46
#define XP_TE_CHANVOICE 47
#define XP_TE_CHANBAN   48
#define XP_TE_CHANRMKEY 49
#define XP_TE_CHANRMLIMIT 50
#define XP_TE_CHANDEOP  51
#define XP_TE_CHANDEVOICE 52
#define XP_TE_CHANUNBAN 53
#define XP_TE_CHANEXEMPT 54
#define XP_TE_CHANRMEXEMPT 55
#define XP_TE_CHANINVITE 56
#define XP_TE_CHANRMINVITE 57
#define XP_TE_CHANMODEGEN 58
#define XP_TE_WHOIS1    59
#define XP_TE_WHOIS2    60
#define XP_TE_WHOIS3    61
#define XP_TE_WHOIS4    62
#define XP_TE_WHOIS4T   63
#define XP_TE_WHOIS5    64
#define XP_TE_WHOIS6    65
#define XP_TE_USERLIMIT 66
#define XP_TE_BANNED    67
#define XP_TE_INVITE    68
#define XP_TE_KEYWORD   69
#define XP_TE_MOTDSKIP  70
#define XP_TE_SERVTEXT  71
#define XP_TE_INVITED   72
#define XP_TE_USERSONCHAN 73
#define XP_TE_NICKCLASH 74
#define XP_TE_NICKFAIL  75
#define XP_TE_UKNHOST   76
#define XP_TE_CONNFAIL  77
#define XP_TE_CONNECT   78
#define XP_TE_CONNECTED 79
#define XP_TE_SCONNECT  80
#define XP_TE_DISCON    81
#define XP_TE_DELNOTIFY 83
#define XP_TE_ADDNOTIFY 84
#define XP_TE_WINTYPE   85
#define XP_TE_CHANMODES 86
#define XP_TE_RAWMODES  87
#define XP_TE_KILL      88
#define XP_TE_NOTIFYONLINE 109
#define XP_TE_NOTIFYNUMBER 110
#define XP_TE_NOTIFYEMPTY  111
#define XP_TE_NOCHILD      112
#define XP_TE_ALREADYPROCESS 113
#define XP_TE_SERVERLOOKUP 114
#define XP_TE_SERVERCONNECTED 115
#define XP_TE_SERVERERROR  116
#define XP_TE_SERVERGENMESSAGE 117
#define XP_TE_FOUNDIP      118
#define XP_TE_CTCPSEND     120
#define XP_TE_MSGSEND      121
#define XP_TE_NOTICESEND   122
#define XP_TE_WALLOPS      123
#define XP_HIGHLIGHT       124
#define XP_TE_IGNOREHEADER 125
#define XP_TE_IGNORELIST   126
#define XP_TE_IGNOREFOOTER 127
#define XP_TE_IGNOREADD    128
#define XP_TE_IGNOREREMOVE 129
#define XP_TE_RESOLVINGUSER 130
#define XP_TE_IGNOREEMPTY  131
#define XP_TE_IGNORECHANGE 132
#define XP_TE_NOTIFYOFFLINE 133
#define XP_TE_MALFORMED_FROM 134
#define XP_TE_MALFORMED_PACKET 135
#define XP_TE_PARTREASON 136
#define XP_TE_UPARTREASON 137
#define XP_TE_NEWMAIL 138
#define XP_TE_MOTD 139

#define XP_IF_SEND 140
#define XP_IF_RECV 141

#define NUM_XP		   142

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






