/*
 * sodium.c 
 *
 * DVD cover art viewer / player
 *
 * Copyright (C) 2008-2010 Andrew Clayton
 *
 * License: GPLv2. See COPYING
 *
 */

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>

#include <clutter/clutter.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* Image grid dimentions */
#define GRID_SIZE	9
#define ROW_SIZE	3
#define COL_SIZE	3

/* Direction of scrolling */
#define FWD		0
#define BWD		1
/* Home / END */
#define HME		2
#define END		3

/* Thresh hold for time difference in ms between scroll events */
#define SCROLL_THRESH	750

int animation = 1;	/* Animation default to enabled */
/* label to display when an image is clicked that has no associated video */
ClutterActor *label;
ClutterBehaviour *behaviour;
GPtrArray *files;	/* Array to hold image filenames */
int array_pos = 0; 	/* Current position in file list array */
int nfiles = 0;		/* Number of files loaded into file list array */
int loaded_images = 0;	/* How many images are currently shown on screen */
char image_path[255]; 	/* Location of images passed in as argv[1] */
char *movie_list; 	/* Path to movie-list mapping file */
char movie_base_path[255];	/* Path to base directory containing videos */
int window_size; 	/* Size of the window */
int image_size; 	/* Size of the image */

/* Display a help/usage summary */
static void display_usage()
{
	printf("\nUsage: sodium image_directory size [video_directory]\n\n");
	printf("Where image_directory is the path to the location of the ");
	printf("images.\n\n");
	printf("size is the size of the window to display in pixels. It ");
	printf("must be at least 300 and also be a multiple of 300.\n\n");
	printf("video_directory is an optional directory where videos are ");
	printf("stored. Videos listed in the movie-list file should be ");
	printf("given relative to this path.\n");

	exit(-1);
}

/* Reap child pids */
static void reaper(int signo)
{
	int status;

	wait(&status);
}

static gdouble on_alpha(ClutterAlpha *alpha, gpointer data)
{
	ClutterTimeline *timeline = clutter_alpha_get_timeline(alpha);

	return clutter_timeline_get_progress(timeline);
}

static void animate_image_setup()
{
	ClutterTimeline *timeline;
	ClutterAlpha *alpha;

	timeline = clutter_timeline_new(5000 /* milliseconds */);
	clutter_timeline_set_loop(timeline, TRUE);
	clutter_timeline_start(timeline);
	alpha = clutter_alpha_new_with_func(timeline, &on_alpha, NULL, NULL);
	behaviour = clutter_behaviour_rotate_new(alpha,
				CLUTTER_Y_AXIS, CLUTTER_ROTATE_CW, 0, 360);
	clutter_behaviour_rotate_set_center(
			CLUTTER_BEHAVIOUR_ROTATE(behaviour),
					image_size / 2, image_size / 2, 0);
	g_object_unref(timeline);
}

static void animate_image_start(ClutterActor *actor)
{
	if (!animation)
		return;

	clutter_behaviour_apply(behaviour, actor);
}

static void animate_image_stop(ClutterActor *actor)
{
	if (!animation)
		return;

	clutter_behaviour_remove(behaviour, actor);
}

static void reset_image(ClutterActor *actor)
{
	if (!animation)
		return;

	animate_image_stop(actor);
	clutter_actor_set_rotation(actor, CLUTTER_Y_AXIS, 0, 0, 0, 0);
}

/* Function to help sort the files list array */
static int compare_string(const void *p1, const void *p2)
{
	/* 
	 * Function used from the qsort(3) man page
	 *
	 * The actual arguments to this function are "pointers to
	 * pointers to char", but strcmp(3) arguments are "pointers
	 * to char", hence the following cast plus dereference 
	 */

	return strcmp(*(char * const *)p1, *(char * const *)p2);
}

/* Check to see if the file is a valid image */
static gboolean is_supported_img(const char *name)
{
	GdkPixbufFormat *fmt;

	fmt = gdk_pixbuf_get_file_info(name, NULL, NULL);

	if (fmt)
		return (gdk_pixbuf_format_is_disabled(fmt) != TRUE);

	return FALSE;
}

/* Read in the image file names into an array */
static void process_directory(const gchar *name)
{
	DIR *dir;
	struct dirent *entry;
	char *fname;

	if (!(dir = opendir(name))) {
		printf("Can't open images directory (%s)\n", name);
		exit(-1);
	}

	printf("Opening directory: %s\n", name);
	chdir(name);

	files = g_ptr_array_new();
	while ((entry = readdir(dir)) != NULL) {
		if (is_supported_img(entry->d_name)) {
			fname = g_strdup(entry->d_name);
			printf("Adding image %s to list\n", fname);
			g_ptr_array_add(files, (gpointer)fname);
			nfiles++;
		}
    	}
	closedir(dir);

	g_ptr_array_sort(files, compare_string);
}

/* Display a "No Video" message for images with no video yet */
static void no_video_notice(ClutterActor *stage)
{
	ClutterColor actor_color = { 0xff, 0xff, 0xff, 0xff };

	label = clutter_text_new_full("Sans 32", "No Video", &actor_color);
	clutter_actor_set_position(label, image_size,
						image_size + 0.5 * image_size);
	clutter_container_add_actor(CLUTTER_CONTAINER(stage), label);
	clutter_actor_show(label);
}

/* fork/exec the movie */
static void play_video(gchar **argv)
{
	pid_t pid;

	pid = fork();
	if (pid > 0) {
		/* parent */
		g_strfreev(argv);
	} else if (pid == 0) {
		/* child */
		execvp(argv[0], argv);
	}
}

/* Build the command and arguments to be exec'd */
static void build_exec_cmd(char *cmd, char *args, char *movie)
{
	char video_paths[1024] = "\0";
	char buf[512] = "\0";
	gchar **argv = NULL;
	gchar **videos = NULL;

	/* Build up a string that will be parsed into an argument list */
	strcpy(buf, cmd);
	strcat(buf, " ");
	strcat(buf, args);
	strcat(buf, " ");

	/* Cater for multiple space seperated video paths to be specified */
	if (strstr(movie, " ")) {
		videos = g_strsplit(movie, " ", -1);
		while (*videos != NULL) {
			/*
			 * Cater for either absolute or relative paths
			 * for videos.
			 */
			if (*videos[0] != '/') {
				strcat(video_paths, movie_base_path);
				strcat(video_paths, *videos++);
			} else {
				strcat(video_paths, *videos++);
			}

			strcat(video_paths, " ");
		}
	} else {
		if (movie[0] == '/') {
			strcpy(video_paths, movie);
		} else {
			strcpy(video_paths, movie_base_path);
			strcat(video_paths, movie);
		}
	}

	strcat(buf, video_paths);
	/* Split buf up into a vector array for passing to execvp */
	g_shell_parse_argv(buf, NULL, &argv, NULL);

	printf("Playing: (%s)\n", video_paths);
	printf("execing: %s %s %s\n", cmd, args, video_paths);
	play_video(argv);
}

/* Lookup the clicked image in the .movie-list file */
static void lookup_video(ClutterActor *stage, char *actor)
{
	char string[512];
	char image[120];
	char movie[120];
	char cmd[120];
	char args[120] = "\0";
	static FILE *fp;

	printf("Opening movie list: (%s)\n", movie_list);
	if (!(fp = fopen(movie_list, "r"))) {
		printf("Can't open movie list: (%s)\n", movie_list);
		exit(-1);
	}

	while (fgets(string, 512, fp)) {
		sscanf(string, "%119[^|]|%119[^|]|%119[^|]|%119[^\n]",
						image, movie, cmd, args);
		if (strcmp(actor, image) == 0) {
			fclose(fp);
			build_exec_cmd(cmd, args, movie);
			return;
		}

		args[0] = '\0';
	}

	/* If we get to here, we didn't find a video */
	fclose(fp);
	printf("No video for (%s)\n", actor);
	no_video_notice(stage);
}

static void lookup_image(ClutterActor *stage, int img_no)
{
	char *image;

	if (img_no <= loaded_images) {
		image = g_ptr_array_index(files,
					array_pos - loaded_images +
								img_no - 1);
		lookup_video(stage, image);
	} else {
		printf("No image at (%d)\n", img_no);
	}
}

/* Determine which image on the grid was clicked. */
static int which_image(ClutterActor *stage, int x, int y)
{
	int img_no = 0;

	printf("array_pos = %d, loaded_images = %d\n", array_pos,
								loaded_images);

	/*
	 * Images are layed out in a 3 x 3 grid from left to right
	 * top to bottom. I.e image 1 is the top left image and image 9
	 * is the bottom right image.
	 */
	if (x >= 0 && x <= image_size && y >= 0 && y <= image_size) {
		img_no = 1;
		printf("Image 1\n");
	} else if (x >= image_size + 1 && x <= image_size * 2 &&
						y >= 0 && y <= image_size) {
		img_no = 2;
		printf("Image 2\n");
	} else if (x >= image_size * 2 + 1 && x <= image_size * 3 &&
						y >= 0 && y <= image_size) {
		img_no = 3;
		printf("Image 3\n");
	} else if (x >= 0 && x <= image_size && y >= image_size + 1 &&
							y <= image_size * 2) {
		img_no = 4;
		printf("Image 4\n");
	} else if (x >= image_size + 1 && x <= image_size * 2 &&
				y >= image_size + 1 && y <= image_size * 2) {
		img_no = 5;
		printf("Image 5\n");
	} else if (x >= image_size * 2 + 1 && x <= image_size * 3 &&
				y >= image_size + 1 && y <= image_size * 2) {
		img_no = 6;
		printf("Image 6\n");
	} else if (x >= 0 && x <= image_size && y >= image_size * 2 + 1 &&
							y <= image_size * 3) {
		img_no = 7;
		printf("Image 7\n");
	} else if (x >= image_size + 1 && x <= image_size * 2 &&
			y >= image_size * 2 + 1 && y <= image_size * 3) {
		img_no = 8;
		printf("Image 8\n");
	} else if (x >= image_size * 2 + 1 && x <= image_size * 3 &&
			y >= image_size * 2 + 1 && y <= image_size * 3) {
		img_no = 9;
		printf("Image 9\n");
	}

	return img_no;
}

/* Load images onto stage */
static void load_images(ClutterActor *stage, int direction)
{
        ClutterActor *img;
	int i; 
	int x = 0;
	int y = 0;
	int c = 0;		/* column */
	int r = 0;		/* row */
	char image_name[5];	/* i_NN */

	if (direction == BWD || direction == END) {
		if (array_pos <= GRID_SIZE || direction == END) {
			/*
			 * If we are at the first screen of images. Loop round
			 * to the end of the images. If we have exactly
			 * nfiles / GRID_SIZE images then goto the end of the
			 * array - GRID_SIZE images. Otherwise goto the end of
			 * the array - the number of images left over below
			 * GRID_SIZE.
			 */
			if (nfiles % GRID_SIZE == 0)
				array_pos = nfiles - GRID_SIZE;
			else 
				array_pos = nfiles - (nfiles % GRID_SIZE);
		} else {
			/*
			 * If we are at the end of the images and there is not
			 * a full grid of images to display, we need to only
			 * go back GRID_SIZE + the last number of images.
			 * Otherwise we go back 2 * GRID_SIZE of images.
			 */
			if (array_pos == nfiles && nfiles % GRID_SIZE != 0)
				array_pos -= GRID_SIZE + (nfiles % GRID_SIZE);
			else
				array_pos -= GRID_SIZE * 2;
		}
	} else if ((direction == FWD && array_pos >= nfiles) ||
							direction == HME) {
		/* Loop round to beginning of images */
		array_pos = 0;
	}

	/* How many images are on screen */	
	loaded_images = 0;

	/*
	 * Remove images from the stage before loading new ones,
	 * this avoids a memory leak by freeing the textures
	 * and clears the stage so when there are less than
	 * GRID_SIZE images to view you don't get the previous
	 * images still vivisble.
	 */
	clutter_group_remove_all(CLUTTER_GROUP(stage));

	for (i = array_pos; i < (array_pos + GRID_SIZE); i++) {
		if (r == ROW_SIZE || i == nfiles)
                        break;

		printf("Loading image: %s\n",
					(char *)g_ptr_array_index(files, i));
		img = clutter_texture_new_from_file(g_ptr_array_index(
							files, i), NULL);
		clutter_actor_set_size(img, image_size, image_size);
		clutter_actor_set_position(img, x, y);
		clutter_container_add_actor(CLUTTER_CONTAINER(stage), img);
		clutter_actor_show(img);
		sprintf(image_name, "i_%.2d", loaded_images + 1);
		clutter_actor_set_name(img, image_name);

		/* Allow the actor to emit events. */
		clutter_actor_set_reactive(img, TRUE);

		c += 1;
		if (c == COL_SIZE) {
			x = 0;
			y += image_size;
			c = 0;
			r += 1;
		} else {
			x += image_size;
		}

		array_pos++;
		loaded_images++;
		/*printf("c = %d, r = %d, x = %d, y = %d, array_pos = %d\n",
						c, r, x, y, array_pos);*/
        }
}

/* Process keyboard/mouse events */
static void input_events_cb(ClutterActor *stage, ClutterEvent *event,
							gpointer user_data)
{
	static int cur_toggle = 1;	/* Cursor is defaulted to visible */
	static int pset = 0;		/* previous scroll event time */
	int setd = 0;			/* scroll event time difference */
	gfloat x = 0.0;
	gfloat y = 0.0;
	int img_no;
	char *image;
	ClutterActor *actor;
	static ClutterActor *last_actor;

	switch (event->type) {
	case CLUTTER_KEY_PRESS: {
		guint sym = clutter_event_get_key_symbol(event);

		switch (sym) {
		case CLUTTER_Page_Up:
		case CLUTTER_Up:
			load_images(stage, BWD);
			break;
		case CLUTTER_Page_Down:
		case CLUTTER_Down:
			load_images(stage, FWD);
			break;
		case CLUTTER_Home:
			load_images(stage, HME);
			break;
		case CLUTTER_End:
			load_images(stage, END);
			break;
		case CLUTTER_c:
			if (!cur_toggle) {
				g_object_set(stage, "cursor-visible", TRUE,
									NULL);
				cur_toggle = 1;
			} else {
				g_object_set(stage, "cursor-visible", FALSE,
									NULL);
				cur_toggle = 0;
			}
			break;
		case CLUTTER_1:
		case CLUTTER_2:
		case CLUTTER_3:
		case CLUTTER_4:
		case CLUTTER_5:
		case CLUTTER_6:
		case CLUTTER_7:
		case CLUTTER_8:
		case CLUTTER_9:
			img_no = sym - 48; /* 1 is sym(49) */
			if (img_no <= loaded_images) {
				image = g_ptr_array_index(files,
					array_pos - loaded_images +
								img_no - 1);
				lookup_video(stage, image);
			} else {
				printf("No image at (%d)\n", img_no);
			}
			break;
		case CLUTTER_Escape:
		case CLUTTER_q:
		case 269025110: /* Exit button on Hauppauge Nova-T remote */
			clutter_main_quit();
			break;
		}
	}
	case CLUTTER_SCROLL:
		/*
		 * Limit the amount of scroll events we act on to one event in
		 * SCROLL_TRESH milliseconds. This avoids over scrolling
		 * through the images, particulary on a trackpad where it's
		 * hard to judge what constitutes a single scroll event.
		 */
		setd = clutter_event_get_time(event) - pset;
		printf("pset = (%d), setd = (%d)\n", pset, setd);

		if (event->scroll.direction == CLUTTER_SCROLL_UP &&
							setd > SCROLL_THRESH) {
			printf("Scroll Up @ (%u)\n",
						clutter_event_get_time(event));
			load_images(stage, BWD);
		} else if (event->scroll.direction == CLUTTER_SCROLL_DOWN &&
							setd > SCROLL_THRESH) {
			printf("Scroll Down @ (%u)\n",
				clutter_event_get_time(event));
			load_images(stage, FWD);
		}
		pset = clutter_event_get_time(event);
		break;
	case CLUTTER_BUTTON_PRESS:
		clutter_actor_hide(label);
		break;
	case CLUTTER_BUTTON_RELEASE:
		actor = clutter_event_get_source(event);
		if (strncmp(clutter_actor_get_name(actor), "i_", 2) == 0)
			reset_image(actor);
		clutter_event_get_coords(event, &x, &y);
		printf("Clicked image at (%.0f, %.0f)\n", x, y);
		img_no = which_image(stage, x, y);
		lookup_image(stage, img_no);
		break;
	case CLUTTER_ENTER:
		actor = clutter_event_get_source(event);
		printf("%s has focus\n", clutter_actor_get_name(actor));
		/* Don't animate the stage! */
		if (strncmp(clutter_actor_get_name(actor), "i_", 2) == 0) {
			reset_image(last_actor);
			animate_image_start(actor);
		}
		break;
	case CLUTTER_LEAVE:
		actor = clutter_event_get_source(event);
		printf("%s lost focus\n", clutter_actor_get_name(actor));
		if (strncmp(clutter_actor_get_name(actor), "i_", 2) == 0)
			last_actor = actor;
		break;
	default:
		break;
	}
}

/* Set the path for the movie-list mapping file */
static void get_movie_list_path(char *home)
{
	if (!home) {
		printf("Unable to set the path for the movie-list.\n");
		printf("Please ensure $HOME is set.\n");
		exit(-1);
	}

	/*
	 * Malloc enough space for the movie-list path
	 * $HOME + /.config/sodium/movie-list
	 */
	movie_list = malloc(strlen(home) + 27);
	strcpy(movie_list, home);
	strcat(movie_list, "/.config/sodium/movie-list");
	printf("Movie list path: %s\n", movie_list);
}

/* Setup the window and image size dimensions */
static void set_dimensions(char *size)
{
	window_size = atoi(size);

	if (window_size < 300 || window_size % 300 != 0)
		display_usage();

	image_size = window_size / ROW_SIZE;

	printf("Setting window size to (%dx%d)\n", window_size, window_size);
	printf("Setting image size to  (%dx%d)\n", image_size, image_size);
}

int main(int argc, char *argv[])
{
	ClutterActor *stage;
	ClutterColor stage_clr = { 0x00, 0x00, 0x00, 0xff };
	const gchar *stage_title =
				{ "sodium - DVD Cover Art Viewer / Player" };
	struct sigaction action;
	char *e_animation;

	if (argc < 3)
		display_usage();

	if ((e_animation = getenv("SODIUM_ANIMATION")))
		animation = atoi(e_animation);

	/* Setup signal handler to reap child pids */
	memset(&action, 0, sizeof(&action));
	sigemptyset(&action.sa_mask);
	action.sa_handler = reaper;
	action.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &action, NULL);

	set_dimensions(argv[2]);

	/* Setup the video path if supplied */
	if (argc == 4) {
		strncpy(movie_base_path, argv[3], 120);
		strcat(movie_base_path, "/");
	}

	strncpy(image_path, argv[1], 240);
	get_movie_list_path(getenv("HOME"));

	clutter_init(&argc, &argv);

	stage = clutter_stage_get_default();
	clutter_actor_set_size(stage, window_size, window_size);
	clutter_stage_set_color(CLUTTER_STAGE(stage), &stage_clr);
	clutter_stage_set_title(CLUTTER_STAGE(stage), stage_title);
	g_object_set(stage, "cursor-visible", TRUE, NULL);
	clutter_actor_set_name(stage, "stage");
	clutter_actor_show(stage);

	process_directory(image_path);
	load_images(stage, FWD);

	/* Handle keyboard/mouse events */
	g_signal_connect(stage, "event", G_CALLBACK(input_events_cb), NULL);

	if (animation)
		animate_image_setup();

	clutter_main();

	free(movie_list);

	exit(0);
}
