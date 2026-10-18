CC ?= gcc
CPPFLAGS ?= -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64
CFLAGS ?= -Og -g
LDFLAGS ?= 
LIBS := -lnewt -lslang -lcrypt

setup: main.c guess.c check-reserved-names.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o setup $^ $(LDFLAGS) $(LIBS)

check-reserved-names.c: reserved_users.txt gen-reserved-user-check.py
	./gen-reserved-user-check.py reserved_users.txt > check-reserved-names.c

.PHONY: run

run: setup
	./setup
