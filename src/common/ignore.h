
struct ignore
{
   unsigned char *mask;
   unsigned int priv:1;
   unsigned int chan:1;
   unsigned int ctcp:1;
   unsigned int noti:1;
   unsigned int invi:1;
   unsigned int unignore:1;
   unsigned int no_save:1;
};

extern int ignore_add (char *mask, int priv, int noti, int chan, int ctcp, int mode, int unignore, int no_save);
extern void ignore_showlist (struct session *sess);
extern int ignore_del (char *mask, struct ignore *ig);
extern int ignore_check (char *mask, int priv, int noti, int chan, int ctcp, int mode);
extern void ignore_load (void);
extern void ignore_save (void);
extern void ignore_gui_open (void);
extern void ignore_gui_update (int level);
