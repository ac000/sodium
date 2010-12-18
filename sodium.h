/* 
 * sodium.h
 *
 * Function definitions for sodium.c
 *
 * Copyright (C) 2008-2010 Andrew Clayton
 *
 * License: GPLv2. See COPYING
 *
 */

static gdouble on_alpha(ClutterAlpha *alpha, gpointer data);
static void animate_image_setup();
static void animate_image_start(ClutterActor *actor);
static void animate_image_stop(ClutterActor *actor);
static void reset_image(ClutterActor *actor);
static gboolean is_supported_img(const char *name);
static void process_directory(const gchar *name);
static void load_images(ClutterActor *stage, int direction);
static void input_events_cb(ClutterActor *stage, ClutterEvent *event,
							gpointer user_data);
static int which_image(ClutterActor *stage, int x, int y);
static void lookup_image(ClutterActor *stage, int img_no);
static void lookup_video(ClutterActor *stage, char *actor);
static void build_exec_cmd(char *cmd, char *args, char *movie);
static void play_video(gchar **argv);
static void no_video_notice(ClutterActor *stage);
static void get_movie_list_path(char *home);
static void set_dimensions(char *size);
static void display_usage();
static int compare_string(const void *p1, const void *p2);
static void reaper(int signo);
