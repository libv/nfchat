struct notify
{
   char *name;
   GSList *server_list; 
};

struct notify_per_server
{
   struct server *server;
   time_t laston;
   time_t lastseen;
   time_t lastoff;
   int ison:1;
};
