CC = gcc
CFLAGS = -O2 -Wall

touch-gesture-daemon: touch-gesture-daemon.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f touch-gesture-daemon
