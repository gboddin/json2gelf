CC=gcc
CFLAGS=-lm -lz -Wall -I.
DEPS = JSON_checker.h
OBJ = JSON_checker.o json2gelf.o
DESTDIR=/
INSTALL_LOCATION=$(DESTDIR)/usr

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

json2gelf: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

install: json2gelf_install

json2gelf_install:
	mkdir -p $(INSTALL_LOCATION)/bin
	cp json2gelf $(INSTALL_LOCATION)/bin
	chmod 755 $(INSTALL_LOCATION)/bin/json2gelf

.PHONY: clean

clean:
	rm -f *.o json2gelf
