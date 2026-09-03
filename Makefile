CC = gcc
CFLAGS = -O2 -Wall

touch-gesture-daemon: touch-gesture-daemon.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: test
test:
	node --test test/gesture-model.js test/gesture-flow.js test/bump-manifest.js test/release-config.js

clean:
	rm -f touch-gesture-daemon

