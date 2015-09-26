CC=gcc
CFLAGS=-Wall -g -std=c99 -O2 -fstack-protector-strong -fPIC -Wl,-z,now -pie
LIBS=`pkg-config --libs clutter-1.0 glib-2.0`
INCS=`pkg-config --cflags clutter-1.0 glib-2.0`

sodium: sodium.c
	$(CC) $(CFLAGS) sodium.c -o sodium $(INCS) $(LIBS)
	gzip -c sodium.1 > sodium.1.gz

clean:
	rm -f sodium sodium.1.gz
