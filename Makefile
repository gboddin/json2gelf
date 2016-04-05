TARGET = json2gelf
LIBS = -lm -lz
CC = gcc
CFLAGS = -g -Wall

default: json2gelf

json2gelf:
	${CC} ${TARGET}.c ${CFLAGS} ${LIBS}  -o ${TARGET}

clean:
	rm -f json2gelf.o json2gelf
