LIBS=`pkg-config --libs clutter-0.8 glib-2.0`
INCS=`pkg-config --cflags clutter-0.8 glib-2.0`

sodium: sodium.c sodium.h
	gcc -Wall sodium.c -o sodium $(INCS) $(LIBS)

clean:
	rm sodium

