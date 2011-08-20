LIBS=`pkg-config --libs clutter-1.0 glib-2.0`
INCS=`pkg-config --cflags clutter-1.0 glib-2.0`

sodium: sodium.c
	gcc -Wall -O2 sodium.c -o sodium $(INCS) $(LIBS)
	gzip -c sodium.1 > sodium.1.gz

clean:
	rm sodium sodium.1.gz

