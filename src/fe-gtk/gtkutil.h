void gtkutil_destroy (GtkWidget * igad, GtkWidget * dgad);
void gtkutil_destroy_window (GtkWidget * igad, struct session *sess);
GtkWidget *gtkutil_simpledialog (char *msg);
GtkWidget *gtkutil_clist_new (int columns, char *titles[],
          GtkWidget * box, int policy,
                              void *select_callback, gpointer select_userdata,
              void *unselect_callback,
           gpointer unselect_userdata,
                  int selection_mode);
int gtkutil_clist_selection (GtkWidget * clist);
void gtkutil_null_this_var (GtkWidget * unused, GtkWidget ** dialog);

