typedef int (*socket_callback) (void *user_data, int sok);

struct socketeventRec
{
   socket_callback callback;
   void *userdata;
   int sok;
   int tag;
   int rread:1;
   int wwrite:1;
   int eexcept:1;
};

typedef struct socketeventRec socketevent;
