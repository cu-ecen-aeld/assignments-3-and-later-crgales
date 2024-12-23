CC = ${CROSS_COMPILE}gcc
CFLAGS = -Wall -Wextra -O2
TARGET = aesdsocket
SRCS = aesdsocket.c
OBJS = ${SRCS:.c=.o}

all: ${TARGET}
default: all

${TARGET}: ${OBJS}
	${CC} ${CFLAGS} -o ${TARGET} ${OBJS}

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f ${TARGET} ${OBJS}

.PHONY: all clean