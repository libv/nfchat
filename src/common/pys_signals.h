/* XChat
   This file was generated by sigextract.py by Adam Langley
   agl@linuxpower.org

   It should *not* be edited by hand!

   AGL
*/

struct signalmapping {
   char *name;
   int num;
};

static struct signalmapping signal_mapping[] = {
   {"XP_USERCOMMAND", 0},
   {"XP_PRIVMSG", 1},
   {"XP_CHANACTION", 2},
   {"XP_CHANMSG", 3},
   {"XP_CHANGENICK", 4},
   {"XP_JOIN", 5},
   {"XP_CHANSETKEY", 6},
   {"XP_CHANSETLIMIT", 7},
   {"XP_CHANOP", 8},
   {"XP_CHANVOICE", 9},
   {"XP_CHANBAN", 10},
   {"XP_CHANRMKEY", 11},
   {"XP_CHANRMLIMIT", 12},
   {"XP_CHANDEOP", 13},
   {"XP_CHANDEVOICE", 14},
   {"XP_CHANUNBAN", 15},
   {"XP_CHANEXEMPT", 16},
   {"XP_CHANRMEXEMPT", 17},
   {"XP_CHANINVITE", 18},
   {"XP_CHANRMINVITE", 19},
   {"XP_INBOUND", 20},
   {"XP_TE_JOIN", 21},
   {"XP_TE_CHANACTION", 22},
   {"XP_TE_CHANMSG", 23},
   {"XP_TE_PRIVMSG", 24},
   {"XP_TE_CHANGENICK", 25},
   {"XP_TE_NEWTOPIC", 26},
   {"XP_TE_TOPIC", 27},
   {"XP_TE_KICK", 28},
   {"XP_TE_PART", 29},
   {"XP_TE_CHANDATE", 30},
   {"XP_TE_TOPICDATE", 31},
   {"XP_TE_QUIT", 32},
   {"XP_TE_PINGREP", 33},
   {"XP_TE_NOTICE", 34},
   {"XP_TE_UJOIN", 35},
   {"XP_TE_UCHANMSG", 36},
   {"XP_TE_DPRIVMSG", 37},
   {"XP_TE_UCHANGENICK", 38},
   {"XP_TE_UKICK", 39},
   {"XP_TE_UPART", 40},
   {"XP_TE_CTCPSND", 41},
   {"XP_TE_CTCPGEN", 42},
   {"XP_TE_CTCPGENC", 43},
   {"XP_TE_CHANSETKEY", 44},
   {"XP_TE_CHANSETLIMIT", 45},
   {"XP_TE_CHANOP", 46},
   {"XP_TE_CHANVOICE", 47},
   {"XP_TE_CHANBAN", 48},
   {"XP_TE_CHANRMKEY", 49},
   {"XP_TE_CHANRMLIMIT", 50},
   {"XP_TE_CHANDEOP", 51},
   {"XP_TE_CHANDEVOICE", 52},
   {"XP_TE_CHANUNBAN", 53},
   {"XP_TE_CHANEXEMPT", 54},
   {"XP_TE_CHANRMEXEMPT", 55},
   {"XP_TE_CHANINVITE", 56},
   {"XP_TE_CHANRMINVITE", 57},
   {"XP_TE_CHANMODEGEN", 58},
   {"XP_TE_WHOIS1", 59},
   {"XP_TE_WHOIS2", 60},
   {"XP_TE_WHOIS3", 61},
   {"XP_TE_WHOIS4", 62},
   {"XP_TE_WHOIS4T", 63},
   {"XP_TE_WHOIS5", 64},
   {"XP_TE_WHOIS6", 65},
   {"XP_TE_USERLIMIT", 66},
   {"XP_TE_BANNED", 67},
   {"XP_TE_INVITE", 68},
   {"XP_TE_KEYWORD", 69},
   {"XP_TE_MOTDSKIP", 70},
   {"XP_TE_SERVTEXT", 71},
   {"XP_TE_INVITED", 72},
   {"XP_TE_USERSONCHAN", 73},
   {"XP_TE_NICKCLASH", 74},
   {"XP_TE_NICKFAIL", 75},
   {"XP_TE_UKNHOST", 76},
   {"XP_TE_CONNFAIL", 77},
   {"XP_TE_CONNECT", 78},
   {"XP_TE_CONNECTED", 79},
   {"XP_TE_SCONNECT", 80},
   {"XP_TE_DISCON", 81},
   {"XP_TE_NODCC", 82},
   {"XP_TE_DELNOTIFY", 83},
   {"XP_TE_ADDNOTIFY", 84},
   {"XP_TE_WINTYPE", 85},
   {"XP_TE_CHANMODES", 86},
   {"XP_TE_RAWMODES", 87},
   {"XP_TE_KILL", 88},
   {"XP_TE_DCCSTALL", 89},
   {"XP_TE_DCCTOUT", 90},
   {"XP_TE_DCCCHATF", 91},
   {"XP_TE_DCCFILEERR", 92},
   {"XP_TE_DCCRECVERR", 93},
   {"XP_TE_DCCRECVCOMP", 94},
   {"XP_TE_DCCCONFAIL", 95},
   {"XP_TE_DCCCON", 96},
   {"XP_TE_DCCSENDFAIL", 97},
   {"XP_TE_DCCSENDCOMP", 98},
   {"XP_TE_DCCOFFER", 99},
   {"XP_TE_DCCABORT", 100},
   {"XP_TE_DCCIVAL", 101},
   {"XP_TE_DCCCHATREOFFER", 102},
   {"XP_TE_DCCCHATOFFERING", 103},
   {"XP_TE_DCCDRAWOFFER", 104},
   {"XP_TE_DCCCHATOFFER", 105},
   {"XP_TE_DCCRESUMEREQUEST", 106},
   {"XP_TE_DCCSENDOFFER", 107},
   {"XP_TE_DCCGENERICOFFER", 108},
   {"XP_TE_NOTIFYONLINE", 109},
   {"XP_TE_NOTIFYNUMBER", 110},
   {"XP_TE_NOTIFYEMPTY", 111},
   {"XP_TE_NOCHILD", 112},
   {"XP_TE_ALREADYPROCESS", 113},
   {"XP_TE_SERVERLOOKUP", 114},
   {"XP_TE_SERVERCONNECTED", 115},
   {"XP_TE_SERVERERROR", 116},
   {"XP_TE_SERVERGENMESSAGE", 117},
   {"XP_TE_FOUNDIP", 118},
   {"XP_TE_DCCRENAME", 119},
   {"XP_TE_CTCPSEND", 120},
   {"XP_TE_MSGSEND", 121},
   {"XP_TE_NOTICESEND", 122},
   {"XP_TE_WALLOPS", 123},
   {"XP_HIGHLIGHT", 124},
   {"XP_TE_IGNOREHEADER", 125},
   {"XP_TE_IGNORELIST", 126},
   {"XP_TE_IGNOREFOOTER", 127},
   {"XP_TE_IGNOREADD", 128},
   {"XP_TE_IGNOREREMOVE", 129},
   {"XP_TE_RESOLVINGUSER", 130},
   {"XP_TE_IGNOREEMPTY", 131},
   {"XP_TE_IGNORECHANGE", 132},
   {"XP_TE_NOTIFYOFFLINE", 133},
   {"XP_TE_MALFORMED_FROM", 134},
   {"XP_TE_MALFORMED_PACKET", 135},
   {"XP_IF_SEND", 136},
   {"XP_IF_RECV", 137},
   {NULL, 0}
};


#define NUM_SIGNALS_LOOKUP  136