static gboolean is_supported_img(const char *name);
static void process_directory(const gchar *name);
static void load_images(ClutterActor *stage, int direction);
static void input_events_cb(ClutterActor *stage, ClutterEvent *event, 
							gpointer user_data);
static void which_image(ClutterActor *stage, int x, int y);
static void lookup_video(ClutterActor *stage, char *actor);
static void play_video(char *cmd, char *args, char *movie);
static void no_video_notice(ClutterActor *stage);

