#include "userlist.h"

int fe_args (int argc, char *argv[]);
void fe_init (void);
void fe_main (void);
void fe_exit (void);
int fe_timeout_add (int interval, void *callback, void *userdata);
void fe_timeout_remove (int tag);
void fe_new_window (struct session *sess);
void fe_message (char *msg, int wait);
int fe_input_add (int sok, int read, int write, int ex, void *func, void *data);
void fe_input_remove (int tag);
struct session *fe_new_window_popup (char *target, struct server *serv);
void fe_set_topic (struct session *sess, char *topic);
void fe_set_hilight (struct session *sess);
void fe_text_clear (struct session *sess);
void fe_close_window (struct session *sess);
void fe_progressbar_start (struct session *sess);
void fe_progressbar_end (struct session *sess);
void fe_print_text (struct session *sess, char *text);
void fe_userlist_insert (struct session *sess, struct user *newuser, int row);
void fe_userlist_remove (struct session *sess, struct user *user);
void fe_userlist_move (struct session *sess, struct user *user, int new_row);
void fe_userlist_numbers (struct session *sess);
void fe_userlist_clear (struct session *sess);
void fe_userlist_hide (struct session *sess);
void fe_clear_channel (struct session *sess);
void fe_session_callback (struct session *sess);
void fe_buttons_update (struct session *sess);
void fe_set_channel (struct session *sess);
void fe_set_title (struct session *sess);
void fe_set_nick (struct server *serv, char *newnick);
void fe_beep (void);
char *fe_buffer_get (session *sess);
