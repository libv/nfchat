void gtkutil_destroy (GtkWidget * igad, GtkWidget * dgad);
GtkWidget *gtkutil_clist_new (int columns, char *titles[],
          GtkWidget * box, int policy,
                              void *select_callback, gpointer select_userdata,
              void *unselect_callback,
           gpointer unselect_userdata,
                  int selection_mode);
int gtkutil_clist_selection (GtkWidget * clist);

