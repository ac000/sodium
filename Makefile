LIBS=`pkg-config --libs clutter-1.0 glib-2.0`
INCS=`pkg-config --cflags clutter-1.0 glib-2.0`

sodium: sodium.c sodium.h
	gcc -Wall -O2 sodium.c -o sodium $(INCS) $(LIBS)

clean:
	rm sodium

