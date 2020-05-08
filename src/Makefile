# 
# slave makefile
# --------------
#
# Copyright (C) 2020 by Adrian Brzezinski <adrian.brzezinski at adrb.pl>
#

CC=gcc
OBJS = $(shell find . -iname "*.c" -type f -print | sed -e "/xdr\// d; s/.//;s/\///")

all: $(OBJS)
	$(CC) ${CFLAGS} ${OBJS} -o ${OUT_NAME}
	mv ${OUT_NAME} ../

clean:
	-rm  *.o

