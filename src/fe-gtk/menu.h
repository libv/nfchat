
typedef void (*menucallback) (void *wid, void *data);

#define M_MENU 0
#define M_NEWMENURIGHT 6
#define M_MENUD 5
#define M_NEWMENU 1
#define M_END 2
#define M_SEP 3
#define M_MENUTOG 4

struct popup
{
   char cmd[256];
   char name[82];
};

struct mymenu
{
   int type;
   char *text;
   menucallback callback;
   int state;
   int activate;
};
