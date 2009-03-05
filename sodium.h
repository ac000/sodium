static gboolean is_supported_img(const char *name);
static void process_directory(const gchar *name);
static void load_images(ClutterActor *stage, int direction);
static void input_events_cb(ClutterActor *stage, ClutterEvent *event, 
							gpointer user_data);
static void play_video(int x, int y);


