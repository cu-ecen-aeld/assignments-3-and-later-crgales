CROSS_COMPILE ?=

CC = ${CROSS_COMPILE}gcc

OBJS = writer.o

TGT = writer

all: ${TGT}

%.o : %.c
	${CC} ${CFLAGS} -c $<

${TGT}: ${OBJS}
	${CC} ${OBJS} -o ${TGT}

clean:
	rm -rf ${OBJS} ${TGT}