#include <time.h>

struct user
{
   char nick[64];
   int weight;
   unsigned int op:1;
   unsigned int voice:1;
};

extern struct user *find_name (char *name);
extern void clear_user_list (void);
extern void voice_name (char *name);
extern void devoice_name (char *name);
extern void ul_op_name (char *name);
extern void deop_name (char *name);
extern int add_name (char *name);
extern int sub_name (char *name);

