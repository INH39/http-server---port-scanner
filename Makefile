
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread

SERVER = server
PORT_SCANNER = port_scanner
BLACKLIST_TEST = blacklist_test

all: $(SERVER) $(PORT_SCANNER) $(BLACKLIST_TEST)

$(SERVER): server.c
	$(CC) $(CFLAGS) -o $@ $<

$(PORT_SCANNER): port_scanner.c
	$(CC) $(CFLAGS) -o $@ $<

$(BLACKLIST_TEST): blacklist_test.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(SERVER) $(PORT_SCANNER) $(BLACKLIST_TEST)

.PHONY: all clean
