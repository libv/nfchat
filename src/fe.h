#include "userlist.h"

int fe_args (int argc, char *argv[]);
void fe_init (void);
void fe_main (void);
void fe_exit (void);
int fe_timeout_add (int interval, void *callback, void *userdata);
void fe_timeout_remove (int tag);
void fe_new_window (void);
int fe_input_add (int sok, int read, int write, int ex, void *func);
void fe_input_remove (int tag);
void fe_set_topic (char *topic);
void fe_progressbar_start (void);
void fe_progressbar_end (void);
void fe_clear_channel (void);
void fe_session_callback (void);
void fe_set_channel (void);
void fe_set_title (void);
void fe_set_nick (char *newnick);
