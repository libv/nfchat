struct setup
{
   GtkWidget *settings_window;
   GtkWidget *nu_color;
   GtkWidget *bt_color;

   GtkWidget *font_normal;
   GtkWidget *font_bold;

   GtkWidget *dialog_font_normal;
   GtkWidget *dialog_font_bold;

   GtkWidget *entry_bluestring;
   GtkWidget *entry_doubleclickuser;
   GtkWidget *entry_max_lines;
   GtkWidget *entry_quit;
   GtkWidget *entry_away;
   GtkWidget *entry_timeout;
   GtkWidget *entry_dccdir;
   GtkWidget *entry_dcctimeout;
   GtkWidget *entry_dccstalltimeout;
   GtkWidget *entry_permissions;
   GtkWidget *entry_dcc_blocksize;
   GtkWidget *entry_dnsprogram;
   GtkWidget *entry_recon_delay;

   GtkWidget *entry_sounddir;
   GtkWidget *entry_soundcmd;

   GtkWidget *entry_mainw_left;
   GtkWidget *entry_mainw_top;
   GtkWidget *entry_mainw_width;
   GtkWidget *entry_mainw_height;

   GtkWidget *entry_hostname;

   GtkWidget *background;
   GtkWidget *background_dialog;

   GtkWidget *check_transparent;
   GtkWidget *check_tint;
   GtkWidget *check_detecthost;
   GtkWidget *check_detectip;
   GtkWidget *check_ip;

   GtkWidget *entry_trans_file;

   GtkWidget *dialog_check_transparent;
   GtkWidget *dialog_check_tint;

   GtkWidget *cancel_button;

   GtkWidget *logmask_entry;

   struct xchatprefs prefs;
};
