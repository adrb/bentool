/*
 * 
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#include "utils.h"

char *hrbytes(char *buf, unsigned int buflen, long long bytes) {

  double unit = 1024;
  char *units = { "KMGTPE" };

  if ( bytes < unit ) {
    snprintf(buf, buflen, "%lld B", bytes);
    return buf;
  }

  int exp = (int)(logl(bytes) / logl(unit));

  if ( exp > strlen(units) ) {
    snprintf(buf, buflen, "(too big value)");
  } else {
    snprintf(buf, buflen, "%.1f %ciB", (float)(bytes / powl(unit, exp)), units[exp-1]);
  }

return buf;
}

void hexdump( unsigned char *data , int datalen ) {

  int len = 0;
  int col;

  if ( !data || !datalen ) return;

  while( len < datalen ) {
    
    printf("%04x  ",len);

    for ( col = 0; len+col < datalen && col < 16 ; col++ ) {
      printf("%02x ",data[len+col]);
      if ( col == 7 ) printf("- ");
    }

    for ( ; col < 16 ; col++ ) {
      printf("   ");
      if ( col == 7 ) printf("- ");
    }

    for ( col = 0; len+col < datalen && col < 16 ; col++ )
      if ( data[len+col] > 31 && data[len+col] < 127 )
        printf("%c",data[len+col]);
      else
        printf(".");

    if ( col < 16 ) break;

    len += 16;
    printf("\n");
  }

  printf("\n");
}

void printhex(unsigned char *data , int datalen) {

  int i;

  for ( i = 0 ; i < datalen ; i++ )
    printf("%02x", data[i]);

}

void printts() {

  time_t t = time(NULL);

  struct tm *tm = localtime(&t);
  printf("%s", asctime(tm));

}

