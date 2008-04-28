LIBS=`pkg-config --libs clutter-0.6 glib-2.0`
INCS=`pkg-config --cflags clutter-0.6 glib-2.0`

sodium: sodium.c
	gcc -Wall sodium.c -o sodium $(INCS) $(LIBS)

clean:
	rm sodium

