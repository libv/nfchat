#define THEME_GROUP 1
#define THEME_GROUP_EXPANDED 2
#define THEME_SERVER 3
#define THEME_SERVERLIST_LOGO 4
#define THEME_OP_ICON 5
#define THEME_VOICE_ICON 6

#include <sys/stat.h>
#include <unistd.h>

struct theme_entry
{
  char name[256];
  char path[256];
  struct stat fstat;
  int is_local;
};
