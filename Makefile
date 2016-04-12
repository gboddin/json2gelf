CC=gcc
CFLAGS=-lm -lz -Wall -I.
DEPS = JSON_checker.h
OBJ = JSON_checker.o json2gelf.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

json2gelf: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o json2gelf
