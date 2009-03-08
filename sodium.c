/*
 * sodium.c 
 *
 * DVD cover art viewer / player
 *
 * Copyright (C) 2008-2009 Andrew Clayton
 *
 * License: GPLv2. See COPYING
 *
 */

#include <clutter/clutter.h>
#include <clutter/clutter-event.h>

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>

#include "sodium.h"


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


/* label to display when an image is clicked that has no associated video */
ClutterActor *label;
char files[500][255];	/* Array to hold image filenames */
int array_pos = 0; 	/* Current position in file list array */
int nfiles = 0;		/* Number of files loaded into file list array */
int loaded_images = 0;	/* How many images are currently shown on screen */
char image_path[255]; 	/* Location of images passed in as argv[1] */
char movie_list[255]; 	/* Path to .movie-list mapping file */
char movie_base_path[255];	/* Path to base directory containing videos */
int window_size; 	/* Size of the window */
int image_size; 	/* Size of the image */


/* Check to see if the file is a valid image */
static gboolean
is_supported_img(const char *name)
{
	GdkPixbufFormat *fmt = gdk_pixbuf_get_file_info(name, NULL, NULL);

	if (fmt)
		return (gdk_pixbuf_format_is_disabled(fmt) != TRUE);
  
	return FALSE;
}

/* Read in the image file names into an array */
static void
process_directory(const gchar *name)
{
	DIR *dir;
	struct dirent *entry;

	dir = opendir(name);
	printf("Opening directory: %s\n", name);
	chdir(name);
	
	while ((entry = readdir(dir)) != NULL) {
		if (is_supported_img(entry->d_name)) {
			printf("Adding image %s to list\n", entry->d_name);
			strncpy(files[nfiles], entry->d_name, 254);
			nfiles++;		
		}
    	}

	closedir(dir);
}

/* Load images onto stage */
static void
load_images(ClutterActor *stage, int direction)
{
        ClutterActor *img;
	
	int i; 
	int x = 0, y = 0, c = 0, r = 0;

	if (direction == BWD) {
		if (array_pos <= GRID_SIZE) {
			return;
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
	} else if (direction == FWD) {
		if (array_pos >= nfiles)
			return;
	} else if (direction == HME) {
		/* Goto begining of images */
		array_pos = 0;
	} else if (direction == END) {
		/* Goto end of images */
		if (nfiles % GRID_SIZE != 0)
			array_pos = nfiles - (nfiles % GRID_SIZE);
		else
			array_pos = nfiles - GRID_SIZE;
	}
	
	/* How many images are on screen */	
	loaded_images = 0;

	/* 
 	 * Remove images from the stage before loading new ones, 
 	 * this avoids a memory leak by freeing the textures 
 	 * and clears the stage so when there are less than
 	 * GRID_SIZE images to view you don't get the previous 
 	 * images still vivisble
 	 */
	clutter_group_remove_all(CLUTTER_GROUP(stage));

	for (i = array_pos; i < (array_pos + GRID_SIZE); i++) {
		if (r == ROW_SIZE || i == nfiles)
                        break;

		printf("Loading image: %s\n", files[i]);
		img = clutter_texture_new_from_file(files[i], NULL);
		clutter_actor_set_size(img, image_size, image_size);
		clutter_actor_set_position(img, x, y);
		clutter_container_add_actor(CLUTTER_CONTAINER(stage), img);
		clutter_actor_show(img);

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
static void
input_events_cb(ClutterActor *stage, ClutterEvent *event, gpointer user_data)
{
	/* Cursor is defaulted to visible */
	static int cur_toggle = 1;
	int x = 0;
	int y = 0;

	switch (event->type) {
	case CLUTTER_KEY_PRESS: {
			guint sym = clutter_key_event_symbol((ClutterKeyEvent*)
								 event);
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
		case CLUTTER_Escape:
		case CLUTTER_q:
			clutter_main_quit();
			break;
		}
	}
	case CLUTTER_SCROLL:
		if (event->scroll.direction == CLUTTER_SCROLL_UP)
			load_images(stage, BWD);
		else if (event->scroll.direction == CLUTTER_SCROLL_DOWN)
			load_images(stage, FWD);
		break;
	case CLUTTER_BUTTON_PRESS:
		clutter_actor_hide(label);
		break;
	case CLUTTER_BUTTON_RELEASE:
		clutter_event_get_coords(event, &x, &y);
		printf("Clicked image at (%d, %d)\n", x, y);
		which_image(stage, x, y);
		break;
	default:
		break;
	}
}

/* Determine which image on the grid was clicked. */
static void
which_image(ClutterActor *stage, int x, int y)
{
	char *image;
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

	if (img_no <= loaded_images) {
		image = files[array_pos - loaded_images + img_no - 1];
		lookup_video(stage, image);
	} else {
		printf("No image at (%d)\n", img_no);
	}
}
 
/* Lookup the clicked image in the .movie-list file */
static void
lookup_video(ClutterActor *stage, char *actor)
{
	char string[255];
	char image[120];
	char movie[120];
	char cmd[20];
	char args[20];

	static FILE *fp;

	printf("Opening movie list: (%s)\n", movie_list);
	if (! (fp = fopen(movie_list, "r"))) {
		printf("Can't open movie list: (%s)\n", movie_list);
		return;
	}
	
	while (fgets(string, 254, fp)) {
		sscanf(string, "%119[^|]|%119[^|]|%19[^|]|%19s",
						image, movie, cmd, args);
		if (strcmp(actor, image) == 0) {
			fclose(fp);
			play_video(cmd, args, movie);
			return;
		}	
	}

	/* We get to here, we didn't find a video */
	fclose(fp);
	printf("No video for (%s)\n", actor);
	no_video_notice(stage);
}

/* fork/exec the movie */
static void
play_video(char *cmd, char *args, char *movie)
{
	pid_t pid;
	int status;
	char movie_path[255];
	
	/* Cater for either absolute or relative paths for videos */
	if (movie[0] == '/') {
		strncpy(movie_path, movie, 254);
	} else {
		strncpy(movie_path, movie_base_path, 130);
		strncat(movie_path, movie, 120);
	}
	
	
	pid = fork();

	if (pid > 0) {
		/* parent */
		waitpid(-1, &status, 0);
	} else if (pid == 0) {
		/* child */
		printf("Playing: (%s)\n", movie_path);
		printf("execing: %s %s %s\n", cmd, args, movie_path);
		execlp(cmd, cmd, args, movie_path, NULL);
	}		
}

/* Display a "No Video" message for images with no video yet */
static void
no_video_notice(ClutterActor *stage)
{
	ClutterColor actor_color = { 0xff, 0xff, 0xff, 0xff };

	label = clutter_label_new_full("Sans 36", "No Video", &actor_color);
	clutter_actor_set_size(label, 500, 500);
	clutter_actor_set_position(label, 342, 400);
	clutter_container_add_actor(CLUTTER_CONTAINER(stage), label);
	clutter_actor_show(label);
}

/* Setup the window and image size dimensions */
static void
set_dimensions(char *size)
{
	window_size = atoi(size);
		
	if (window_size < 300 || window_size % 300 != 0) 
		display_usage();

	image_size = window_size / ROW_SIZE;

	printf("Setting window size to = (%dx%d)\n", window_size, window_size);
	printf("Setting image size to  = (%dx%d)\n", image_size, image_size); 
}

/* Display a help/usage summary */
static void
display_usage()
{
	printf("\nUsage: sodium image_directory size [video_directory]\n\n");
	printf("Where image_directory is the path to the location of the ");
	printf("images.\n\n");
	printf("size is the size of the window to display in pixels. It ");
	printf("must be at least 300 and also be a multiple of 300.\n\n");
	printf("video_directory is an optional directory where videos are ");
	printf("stored. Videos listed in the .movie-list file should be ");
	printf("given relative to this path.\n");
	
	exit(1);
}

int
main(int argc, char *argv[])
{
	ClutterActor *stage;
	ClutterColor stage_clr = { 0x00, 0x00, 0x00, 0xff };
	const gchar *stage_title = { "sodium - DVD Cover Art Viewer / Player" };

	if (argc < 3) 
		display_usage();

	set_dimensions(argv[2]);

	/* Setup the video path if supplied */
	if (argc == 4) {
		strncpy(movie_base_path, argv[3], 120);
		strcat(movie_base_path, "/");
	} 

	strncpy(image_path, argv[1], 240);

	strcpy(movie_list, image_path);
	strcat(movie_list, "/.movie-list");

	clutter_init(&argc, &argv);

	stage = clutter_stage_get_default();
	clutter_actor_set_size(stage, window_size, window_size);
	clutter_stage_set_color(CLUTTER_STAGE(stage), &stage_clr);
	clutter_stage_set_title(CLUTTER_STAGE(stage), stage_title);
	g_object_set(stage, "cursor-visible", TRUE, NULL);
	clutter_actor_show(stage);
	
	process_directory(image_path);
	load_images(stage, FWD);
	
	/* Handle keyboard/mouse events */
	g_signal_connect(stage, "event", G_CALLBACK(input_events_cb), NULL);

	clutter_main();

	return EXIT_SUCCESS;
}

