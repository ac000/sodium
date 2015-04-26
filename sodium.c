/*
 * sodium.c
 *
 * DVD cover art viewer / player
 *
 * Copyright (C) 2008 - 2015	Andrew Clayton <andrew@digital-domain.net>
 *
 * License: GPLv2. See COPYING
 *
 */

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>

#include <clutter/clutter.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define SODIUM_VERSION	"007"

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

#define BUF_SIZE	4096

/* Macro to check if a given actor is an image */
#define IS_IMAGE(actor)	((isdigit(actor[0])) ? 1 : 0)

/*
 * Debug wrapper around printf(). If debug is enabled it will print the
 * message prefixed by the function name that the message originates from.
 */
#define pr_debug(fmt, ...) \
	do { \
		if (!debug) \
			break; \
		printf("%s: " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

static bool debug;		/* Default to no debug output */
static bool animation = true;	/* Animation default to enabled */
/* label to display when an image is clicked that has no associated video */
static ClutterActor *label;
static GPtrArray *files;	/* Array to hold image filenames */
static int array_pos;	/* Current position in file list array */
static int nfiles;	/* Number of files loaded into file list array */
/* How many images are currently shown on screen */
static int loaded_images;
/* Location of images passed in as argv[1] */
static char image_path[PATH_MAX];
static char *movie_list;	/* Path to movie-list mapping file */
/* Path to base directory containing videos */
static char movie_base_path[PATH_MAX];
static int window_size;	/* Size of the window */
static int image_size;	/* Size of the image */

/* Display a help/usage summary */
static void display_usage(void)
{
	printf("\nUsage: sodium image_directory size [video_directory]\n\n");
	printf("Where image_directory is the path to the location of the ");
	printf("images.\n\n");
	printf("size is the size of the window to display in pixels. It ");
	printf("must be at least 300 and also be a multiple of 300.\n\n");
	printf("video_directory is an optional directory where videos are ");
	printf("stored. Videos listed in the movie-list file should be ");
	printf("given relative to this path.\n");

	exit(EXIT_FAILURE);
}

/* Reap child pids */
static void reaper(int signo)
{
	wait(NULL);
}

static void animate_image_start(ClutterActor *actor)
{
	if (!animation)
		return;

	ClutterTransition *transition;

	clutter_actor_set_pivot_point(actor, 0.5, 1.0);
	transition = clutter_property_transition_new("rotation-angle-y");

	clutter_transition_set_from(transition, G_TYPE_DOUBLE, 0.0);
	clutter_transition_set_to(transition, G_TYPE_DOUBLE, 360.0);

	clutter_timeline_set_duration(CLUTTER_TIMELINE(transition), 5000);
	clutter_timeline_set_repeat_count(CLUTTER_TIMELINE(transition), -1);

	clutter_timeline_set_progress_mode(CLUTTER_TIMELINE (transition),
			CLUTTER_LINEAR);

	clutter_actor_add_transition(actor, "rotation", transition);
}

static void reset_image(ClutterActor *actor)
{
	if (!animation)
		return;

	clutter_actor_remove_all_transitions(actor);
	clutter_actor_set_rotation_angle(actor, CLUTTER_Y_AXIS, 0.0);
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

	dir = opendir(name);
	if (!dir) {
		fprintf(stderr, "Can't open images directory: %s\n", name);
		exit(EXIT_FAILURE);
	}

	pr_debug("Opening directory: %s\n", name);
	chdir(name);

	files = g_ptr_array_new();
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0)
			continue;
		if (is_supported_img(entry->d_name)) {
			fname = g_strdup(entry->d_name);
			pr_debug("Adding image %s to list\n", fname);
			g_ptr_array_add(files, (gpointer)fname);
			nfiles++;
		}
	}
	closedir(dir);

	g_ptr_array_sort(files, compare_string);
}

static void hide_label(void)
{
	if (CLUTTER_ACTOR_IS_VISIBLE(label))
		clutter_actor_hide(label);
}

/* Display a "No Video" message for images with no video yet */
static void create_no_video_label(ClutterActor *stage)
{
	ClutterColor actor_color = { 0xff, 0xff, 0xff, 0xff };

	label = clutter_text_new_full("Sans 32", "No Video", &actor_color);
	clutter_actor_set_position(label, image_size * 1.25, window_size / 2);
	clutter_actor_add_child(stage, label);
	clutter_actor_set_child_above_sibling(label, stage, NULL);
	clutter_actor_hide(label);
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
		int err;
		/* child */
		err = execvp(argv[0], argv);
		if (err == -1) {
			fprintf(stderr, "ERROR: Could not exec %s\n", argv[0]);
			_exit(EXIT_FAILURE);
		}
	}
}

/* Build the command and arguments to be exec'd */
static void build_exec_cmd(char *cmd, char *args, char *movie)
{
	char video_paths[BUF_SIZE] = "\0";
	char buf[BUF_SIZE] = "\0";
	gchar **argv = NULL;

	/* Build up a string that will be parsed into an argument list */
	snprintf(buf, BUF_SIZE, "%s %s ", cmd, args);

	/* Cater for multiple space separated video paths to be specified */
	if (strstr(movie, " ")) {
		gchar **videos = NULL;

		videos = g_strsplit(movie, " ", 0);
		while (*videos != NULL) {
			/*
			 * Cater for either absolute or relative paths
			 * for videos.
			 */
			if (*videos[0] != '/')
				strncat(video_paths, movie_base_path,
					BUF_SIZE - strlen(video_paths));

			strncat(video_paths, *videos++,
					BUF_SIZE - strlen(video_paths));
			strncat(video_paths, " ",
					BUF_SIZE - strlen(video_paths));
		}
	} else {
		if (movie[0] != '/')
			strncat(video_paths, movie_base_path,
					BUF_SIZE - strlen(video_paths));

		strncat(video_paths, movie,
					BUF_SIZE - strlen(video_paths));
	}

	strncat(buf, video_paths, BUF_SIZE - strlen(buf));
	/* Split buf up into a vector array for passing to execvp */
	g_shell_parse_argv(buf, NULL, &argv, NULL);

	pr_debug("Playing: (%s)\n", video_paths);
	pr_debug("execing: %s %s %s\n", cmd, args, video_paths);
	play_video(argv);
}

/*
 * Lookup the clicked image in the .movie-list file.
 *
 * Reads in the movie file list and splits each line up into its
 * fields as:
 *
 *	fields[0] : image
 *	fields[1] : movie
 *	fields[2] : command
 *	fields[3] : command arguments
 */
static void lookup_video(ClutterActor *stage, char *actor)
{
	char buf[BUF_SIZE];
	FILE *fp;

	pr_debug("Opening movie list: (%s)\n", movie_list);
	fp = fopen(movie_list, "re");
	if (!fp) {
		fprintf(stderr, "Can't open movie list: (%s)\n", movie_list);
		exit(EXIT_FAILURE);
	}

	while (fgets(buf, BUF_SIZE, fp)) {
		char **fields = g_strsplit(buf, "|", 0);
		if (strcmp(actor, fields[0]) == 0) {
			build_exec_cmd(fields[2], fields[3], fields[1]);
			g_strfreev(fields);
			goto out;
		}
		g_strfreev(fields);
	}

	/* If we get to here, we didn't find a video */
	pr_debug("No video for (%s)\n", actor);
	clutter_actor_show(label);

out:
	fclose(fp);
}

static void lookup_image(ClutterActor *stage, int img_no)
{
	if (img_no <= loaded_images) {
		char *image = g_ptr_array_index(files,
				array_pos - loaded_images + img_no - 1);
		lookup_video(stage, image);
	} else {
		pr_debug("No image at (%d)\n", img_no);
	}
}

/* Load images onto stage */
static void load_images(ClutterActor *stage, int direction)
{
	static ClutterActor *images = NULL;
	int i;
	int x = 0;
	int y = 0;
	int c = 0;		/* column */
	int r = 0;		/* row */
	char image_name[3];	/* 1..99 + \0 */

	if (!images) {
		images = clutter_actor_new();
		clutter_actor_add_child(stage, images);
	}

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
	clutter_actor_remove_all_children(images);

	for (i = array_pos; i < (array_pos + GRID_SIZE); i++) {
		if (r == ROW_SIZE || i == nfiles)
			break;
		GdkPixbuf *pixbuf;
		ClutterContent *image;
		ClutterActor *ibox;

		pr_debug("Loading image: %s\n",
			(char *)g_ptr_array_index(files, i));
		pixbuf = gdk_pixbuf_new_from_file(
				(char *)g_ptr_array_index(files, i), NULL);
		image = clutter_image_new();
		clutter_image_set_data(CLUTTER_IMAGE(image),
				gdk_pixbuf_get_pixels(pixbuf),
				gdk_pixbuf_get_has_alpha(pixbuf)
					? COGL_PIXEL_FORMAT_RGBA_8888
					: COGL_PIXEL_FORMAT_RGB_888,
				gdk_pixbuf_get_width(pixbuf),
				gdk_pixbuf_get_height(pixbuf),
				gdk_pixbuf_get_rowstride(pixbuf),
				NULL);
		g_object_unref(pixbuf);
		ibox = clutter_actor_new();
		clutter_actor_set_content(ibox, image);
		g_object_unref(image);
		clutter_actor_set_size(ibox, image_size, image_size);
		clutter_actor_set_position(ibox, x, y);
		clutter_actor_add_child(images, ibox);
		clutter_actor_show(ibox);
		snprintf(image_name, sizeof(image_name), "%d",
				loaded_images + 1);
		clutter_actor_set_name(ibox, image_name);

		/* Allow the actor to emit events. */
		clutter_actor_set_reactive(ibox, TRUE);

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
	static int pset = 0;		/* previous scroll event time */
	int setd = 0;			/* scroll event time difference */
	int img_no;
	ClutterActor *actor = clutter_event_get_source(event);
	static ClutterActor *last_actor;

	switch (event->type) {
	case CLUTTER_KEY_PRESS: {
		guint sym = clutter_event_get_key_symbol(event);

		hide_label();
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
		case CLUTTER_c: {
			static bool cur_enabled = true;

			if (!cur_enabled) {
				g_object_set(stage, "cursor-visible", TRUE,
						NULL);
				cur_enabled = true;
			} else {
				g_object_set(stage, "cursor-visible", FALSE,
						NULL);
				cur_enabled = false;
			}
			break;
		}
		case CLUTTER_1...CLUTTER_9:
			img_no = sym - 48; /* 1 is sym(49) */
			if (img_no <= loaded_images) {
				char *image = g_ptr_array_index(files,
					array_pos - loaded_images +
					img_no - 1);
				lookup_video(stage, image);
			} else {
				pr_debug("No image at (%d)\n", img_no);
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
		pr_debug("pset = (%d), setd = (%d)\n", pset, setd);

		if (event->scroll.direction == CLUTTER_SCROLL_UP &&
				setd > SCROLL_THRESH) {
			pr_debug("Scroll Up @ (%u)\n",
				clutter_event_get_time(event));
			load_images(stage, BWD);
		} else if (event->scroll.direction == CLUTTER_SCROLL_DOWN &&
				setd > SCROLL_THRESH) {
			pr_debug("Scroll Down @ (%u)\n",
				clutter_event_get_time(event));
			load_images(stage, FWD);
		}
		pset = clutter_event_get_time(event);
		break;
	case CLUTTER_BUTTON_PRESS:
		hide_label();
		break;
	case CLUTTER_BUTTON_RELEASE:
		if (IS_IMAGE(clutter_actor_get_name(actor))) {
			reset_image(actor);
			img_no = atoi(clutter_actor_get_name(actor));
			lookup_image(stage, img_no);
		}
		break;
	case CLUTTER_ENTER:
		pr_debug("%s has focus\n", clutter_actor_get_name(actor));
		/* Don't animate the stage! */
		if (IS_IMAGE(clutter_actor_get_name(actor))) {
			reset_image(last_actor);
			animate_image_start(actor);
		}
		break;
	case CLUTTER_LEAVE:
		pr_debug("%s lost focus\n", clutter_actor_get_name(actor));
		if (IS_IMAGE(clutter_actor_get_name(actor)))
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
		fprintf(stderr, "Unable to set the path for the movie-list.\n");
		fprintf(stderr, "Please ensure $HOME is set.\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * Malloc enough space for the movie-list path
	 * $HOME + /.config/sodium/movie-list
	 */
	movie_list = malloc(strlen(home) + 27);
	strcpy(movie_list, home);
	strcat(movie_list, "/.config/sodium/movie-list");
	pr_debug("Movie list path: %s\n", movie_list);
}

/* Setup the window and image size dimensions */
static void set_dimensions(char *size)
{
	window_size = atoi(size);

	if (window_size < 300 || window_size % 300 != 0)
		display_usage();

	image_size = window_size / ROW_SIZE;

	pr_debug("Setting window size to (%dx%d)\n", window_size, window_size);
	pr_debug("Setting image size to  (%dx%d)\n", image_size, image_size);
}

int main(int argc, char *argv[])
{
	ClutterActor *stage;
	ClutterColor stage_clr = { 0x00, 0x00, 0x00, 0xff };
	const gchar *stage_title =
				{ "sodium - DVD Cover Art Viewer / Player" };
	struct sigaction action;
	char *e_animation;
	char *e_debug;
	int ci;

	if (argc < 3)
		display_usage();

	e_debug = getenv("SODIUM_DEBUG");
	if (e_debug)
		debug = atoi(e_debug);
	if (!debug) {
		/* Make stderr point to /dev/null */
		int fd = open("/dev/null", O_WRONLY);
		dup2(fd, STDERR_FILENO);
		close(fd);
	}

	e_animation = getenv("SODIUM_ANIMATION");
	if (e_animation)
		animation = atoi(e_animation);

	/* Setup signal handler to reap child pids */
	sigemptyset(&action.sa_mask);
	action.sa_handler = reaper;
	action.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &action, NULL);

	set_dimensions(argv[2]);

	/* Setup the video path if supplied */
	if (argc == 4)
		snprintf(movie_base_path, sizeof(movie_base_path), "%s/",
			argv[3]);

	snprintf(image_path, sizeof(image_path), "%s", argv[1]);

	get_movie_list_path(getenv("HOME"));

	ci = clutter_init(&argc, &argv);
	if (ci < 0) {
		perror("clutter_init");
		exit(EXIT_FAILURE);
	}

	stage = clutter_stage_new();
	clutter_actor_set_size(stage, window_size, window_size);
	clutter_actor_set_background_color(stage, &stage_clr);
	clutter_stage_set_title(CLUTTER_STAGE(stage), stage_title);
	g_object_set(stage, "cursor-visible", TRUE, NULL);
	clutter_actor_set_name(stage, "stage");
	clutter_actor_show(stage);

	process_directory(image_path);
	load_images(stage, FWD);

	create_no_video_label(stage);

	/* Handle keyboard/mouse events */
	g_signal_connect(stage, "event", G_CALLBACK(input_events_cb), NULL);

	clutter_main();

	free(movie_list);

	exit(EXIT_SUCCESS);
}
