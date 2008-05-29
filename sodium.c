#include <clutter/clutter.h>
#include <clutter/clutter-event.h>

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

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


/* Array to hold image filenames */
char files[500][255];
int array_pos = 0;
int nfiles = 0;
int fullscreen = 0;

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
}

/* Load images onto stage */
static void
load_images(ClutterActor *stage, int direction)
{
        ClutterActor *img;
        GdkPixbuf *pixbuf;
	
	int i; 
	int x = 0, y = 0, c = 0, r = 0;

	if (direction == BWD) {
		if (array_pos < (GRID_SIZE * 2)) {
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
	
	/* 
 	 * Remove images from the stage before loading new ones, 
 	 * this avoids a memory leak by freeing the textures 
 	 * and clears the stage so when there are less than
 	 * GRID_SIZE images to view you don't get the previous 
 	 * images still vivisble
 	 */
	clutter_group_remove_all(CLUTTER_GROUP (stage));
	for (i = array_pos; i < (array_pos + GRID_SIZE); i++) {
		if (r == ROW_SIZE || i == nfiles)
                        break;

		printf("Loading image: %s\n", files[i]);
		pixbuf = gdk_pixbuf_new_from_file_at_size(files[i],  
								300, 300, NULL);
		img = clutter_texture_new_from_pixbuf(pixbuf);
		/* Free the pixbuf to avoid a memory leak */
		g_object_unref(pixbuf);
		clutter_actor_set_position(img, x, y);
		clutter_group_add(CLUTTER_GROUP(stage), img);
		clutter_actor_show(img);

		c += 1;
		if (c == COL_SIZE) {
			x = 0;
			y += 300;
			c = 0;
			r += 1;
		} else {
			x += 300;
		}

		array_pos++;
		/*printf("c = %d, r = %d, x = %d, y = %d, array_pos = %d\n", 
						c, r, x, y, array_pos);*/
        }
}

/* Process keyboard/mouse events */
static void
input_events_cb(ClutterActor *stage, ClutterEvent *event, gpointer user_data)
{
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
	default:
		break;
	}
}

int
main(int argc, char *argv[])
{
	ClutterActor *stage;
	ClutterColor stage_clr = { 0x00, 0x00, 0x00, 0xff };
	const gchar *stage_title = { "sodium - DVD Cover Art Viewer" };

	if (argc != 2) {
      		printf("\n    usage: %s image_directory\n\n", argv[0]);
		exit(1);
	}

	clutter_init(&argc, &argv);

	stage = clutter_stage_get_default();
	clutter_actor_set_size(stage, 900, 900);
	clutter_stage_set_color(CLUTTER_STAGE (stage), &stage_clr);
	clutter_stage_set_title(CLUTTER_STAGE (stage), stage_title);
	/*g_object_set(stage, "cursor-visible", FALSE, NULL);*/
	clutter_actor_show_all(stage);

	process_directory(argv[1]);
	load_images(stage, FWD);
	
	/* Handle keyboard/mouse events */
	g_signal_connect(stage, "event", G_CALLBACK (input_events_cb), NULL);

	clutter_main();

	return EXIT_SUCCESS;
}


