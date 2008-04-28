#include <clutter/clutter.h>
#include <clutter/clutter-event.h>

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <dirent.h>


#define GRID_SIZE	9
#define ROW_SIZE	3
#define COL_SIZE	3


DIR *dir;
struct dirent *entry;
int ao = 0;

/* Doubly linked-list of image filenames */
GList *images = NULL;

static gboolean
is_supported_img(const char *name)
{
	GdkPixbufFormat *fmt = gdk_pixbuf_get_file_info(name, NULL, NULL);

	if (fmt)
		return (gdk_pixbuf_format_is_disabled(fmt) != TRUE);
  
	return FALSE;
}

static void
process_directory(const gchar *name, ClutterActor *stage)
{
	ClutterActor *img;
	GdkPixbuf *pixbuf;

	int x = 0, y = 0, c = 0, r = 0;

	if (!ao) {
		dir = opendir(name);
		printf("Opening directory: %s\n", name);
		chdir(name);
		ao = 1;
	}	
	
	while ((entry = readdir(dir)) != NULL) {
		if (is_supported_img(entry->d_name)) {
			images = g_list_append(images, entry->d_name);
			printf("Loading image: %s\n", entry->d_name);
			pixbuf = gdk_pixbuf_new_from_file_at_size(entry->d_name,								300, 300, NULL);
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

			printf("c = %d, r = %d, x = %d, y = %d\n", c, r, x, y);
			
			if (r == ROW_SIZE) 
				break;
		}
    	}
}

static void
key_release_cb (ClutterActor *stage, ClutterKeyEvent *kev, gpointer user_data)
{
	switch (clutter_key_event_symbol(kev)) {
	case CLUTTER_Down:
		printf("Got key event: %d\n", clutter_key_event_symbol(kev));
		process_directory(NULL, stage);
		break;
	}
}

int
main (int argc, char *argv[])
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
	process_directory(argv[1], stage);

	g_signal_connect(stage, "key-release-event", 
				G_CALLBACK (key_release_cb), NULL);
	
	clutter_main();

	return EXIT_SUCCESS;
}


