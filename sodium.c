#include <clutter/clutter.h>
#include <clutter/clutter-event.h>

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
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


/* Array to hold image filenames */
char files[500][255];
int array_pos = 0;
int nfiles = 0;

/* Check to see if the file is a valid image */
static gboolean
is_supported_img(const char *name)
{
	GdkPixbufFormat *fmt = gdk_pixbuf_get_file_info(name, NULL, NULL);

	if (fmt)
		return (gdk_pixbuf_format_is_disabled(fmt) != TRUE);
  
	return FALSE;
}

/* Read in the image file names into a linked-list */
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
		if (array_pos < (GRID_SIZE * 2))
			return;
		else
			array_pos -= (GRID_SIZE * 2);
	} else {
		if (array_pos >= nfiles)
			return;
	}

	for (i = array_pos; i < (array_pos + 9); i++) {
		printf("Loading image: %s\n", files[i]);
		pixbuf = gdk_pixbuf_new_from_file_at_size(files[i],  
								300, 300, NULL);
		img = clutter_texture_new_from_pixbuf(pixbuf);
		clutter_actor_set_position(img, x, y);
		clutter_group_add(CLUTTER_GROUP(stage), img);
		clutter_actor_show(img);

		c += 1;
		if (c == COL_SIZE) {
			x = 0;
			y += 300;
			c = 0;
			r += 1;
		} else
			x += 300;

		array_pos++;
		/*printf("c = %d, r = %d, x = %d, y = %d, array_pos = %d\n", 
						c, r, x, y, array_pos);*/

		if (r == ROW_SIZE)
			break;
        }
}

/* Process key events */
static void
key_release_cb(ClutterActor *stage, ClutterKeyEvent *kev, gpointer user_data)
{
	switch (clutter_key_event_symbol(kev)) {
	case CLUTTER_Down:
		printf("Got key event: %d\n", clutter_key_event_symbol(kev));
		load_images(stage, FWD);
		break;
	case CLUTTER_Up:
		printf("Got key event: %d\n", clutter_key_event_symbol(kev));
		load_images(stage, BWD);
		break;
	case CLUTTER_q:
		printf("Got key event: %d\n", clutter_key_event_symbol(kev));
		clutter_main_quit ();
		break;
	}
}

int
main(int argc, char *argv[])
{
	ClutterActor *stage;
	ClutterColor stage_clr = { 0x00, 0x00, 0x00, 0xff };

	if (argc != 2) {
      		printf("\n    usage: %s image_directory\n\n", argv[0]);
		exit(1);
	}

	clutter_init(&argc, &argv);

	stage = clutter_stage_get_default();
	clutter_actor_set_size(stage, 900, 900);
	clutter_stage_set_color(CLUTTER_STAGE (stage), &stage_clr);

	clutter_actor_show_all(stage);

	process_directory(argv[1]);
	load_images(stage, FWD);

	g_signal_connect(stage, "key-release-event", 
				G_CALLBACK (key_release_cb), NULL);
	
	clutter_main();

	return EXIT_SUCCESS;
}


