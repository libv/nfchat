/* cfgfiles.h */

#ifndef cfgfiles_h
#define cfgfiles_h


char *cfg_get_str (char *cfg, char *var, char *dest);
char *get_xdir (void);
void check_prefs_dir (void);
void load_config (void);
int save_config (void);

#define STRUCT_OFFSET_STR(type,field) \
( (unsigned int) (((char *) (&(((type *) NULL)->field)))- ((char *) NULL)) )

#define STRUCT_OFFSET_INT(type,field) \
( (unsigned int) (((int *) (&(((type *) NULL)->field)))- ((int *) NULL)) )

#define PREFS_OFFSET(field) STRUCT_OFFSET_STR(struct xchatprefs, field)
#define PREFS_OFFINT(field) STRUCT_OFFSET_INT(struct xchatprefs, field)

struct prefs
{
   char *name;
   int offset;
   int type;
};

#define TYPE_STR 1
#define TYPE_INT 2
#define TYPE_BOOL 3

#endif
