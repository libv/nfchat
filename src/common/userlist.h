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

extern int userlist_insertname_sorted (struct session *sess, struct user *newuser);
extern int userlist_add_hostname (struct session *sess, char *nick, char *hostname, char *realname, char *servername);
extern struct user *find_name (struct session *sess, char *name);
extern struct user *find_name_global (struct server *serv, char *name);
extern void update_user_list (struct session *sess);
extern void clear_user_list (struct session *sess);
extern void sort_namelist (struct session *sess);
extern void voice_name (struct session *sess, char *name);
extern void devoice_name (struct session *sess, char *name);
extern void ul_op_name (struct session *sess, char *name);
extern void deop_name (struct session *sess, char *name);
extern int add_name (struct session *sess, char *name, char *hostname);
extern int sub_name (struct session *sess, char *name);

