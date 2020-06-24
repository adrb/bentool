/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

char *hrbytes(char *buf, unsigned int buflen, long long bytes);
void hexdump(unsigned char *data , int datalen);
void printhex(unsigned char *data , int datalen);
void hex2raw( uint8_t *dest, char *src, int len);
void print_tv( struct timeval *tv );
uint64_t tvusec( struct timeval *tv );

void print_busyloop();

#endif // __UTILS_H__
