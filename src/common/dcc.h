/* dcc.h */

#ifndef dcc_h
#define dcc_h


#define STAT_QUEUED 0
#define STAT_ACTIVE 1
#define STAT_FAILED 2
#define STAT_DONE 3
#define STAT_CONNECTING 4
#define STAT_ABORTED 5

#define TYPE_SEND 0
#define TYPE_RECV 1
#define TYPE_CHATRECV 2
#define TYPE_CHATSEND 3

void dcc_get (struct DCC *dcc);
void dcc_check_timeouts (void);
void dcc_send_filereq (struct session *sess, char *nick);
void dcc_close (struct DCC *dcc, int stat, int destroy);
void dcc_notify_kill (struct server *serv);
struct sockaddr_in *dcc_write_chat (char *nick, char *text);
void dcc_send (struct session *sess, char *tbuf, char *to, char *file);
void dcc_connect (struct session *sess, struct DCC *dcc);
struct DCC *find_dcc (char *nick, char *file, int type);
void dcc_get_nick (struct session *sess, char *nick);
struct DCC *new_dcc (void);
void dcc_chat (struct session *sess, char *nick);
void handle_dcc (struct session *sess, char *outbuf, char *nick, char *word[], char *word_eol[]);
void dcc_show_list (struct session *sess, char *outbuf);
void open_dcc_recv_window (void);
void open_dcc_send_window (void);
void open_dcc_chat_window (void);

struct dcc_chat
{
   char linebuf[1024];
   int pos;
};

#endif
