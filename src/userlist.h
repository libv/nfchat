#include <time.h>

struct user
{
   char nick[64];
   char *hostname;
   char *realname;
   char *servername;
   time_t lasttalk;
   int weight;
   unsigned int op:1;
   unsigned int voice:1;
};

extern int userlist_insertname_sorted (session_t *sess, struct user *newuser);
extern int userlist_add_hostname (session_t *sess, char *nick, char *hostname, char *realname, char *servername);
extern struct user *find_name (session_t *sess, char *name);
extern void update_user_list (session_t *sess);
extern void clear_user_list (session_t *sess);
extern void sort_namelist (session_t *sess);
extern void voice_name (session_t *sess, char *name);
extern void devoice_name (session_t *sess, char *name);
extern void ul_op_name (session_t *sess, char *name);
extern void deop_name (session_t *sess, char *name);
extern int add_name (session_t *sess, char *name, char *hostname);
extern int sub_name (session_t *sess, char *name);
