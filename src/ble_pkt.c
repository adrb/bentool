/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#include "bentool.h"

uint32_t ble_pkts_size = 0;
ble_pkt_t **ble_pkts = NULL;

void en_ga_print( en_ga_t *en ) {

  if ( !en ) return;

  printf("RPI: ");
  printhex((unsigned char*)&en->rpi, 16);
  printf(", AEM: ");
  printhex((unsigned char*)&en->aem, 4);

}

void ble_pkt_print( ble_pkt_t *pkt, int print_datadump ) {

  if ( !pkt ) return;

  print_tv( &pkt->recv_time );

  char addr[18];
  ba2str(&(pkt->ba), addr);

  printf(", BD: %s, RSSI: %d, ", addr, pkt->rssi );

  switch (pkt->data_type) {
    case BLE_ADV_INFO:
      printf("(not EN)");

      if ( print_datadump ) {
        printf("\n");
        hexdump(pkt->data.advinfo->data, pkt->data.advinfo->length);
      }
    break;
    case BLE_GA_EN:
      en_ga_print(pkt->data.ga);

      if ( print_datadump ) {
        printf("\n");
        hexdump((uint8_t*)pkt->data.ga, sizeof(en_ga_t) );
      }
    break;
  }
}

int badv_dump_csv(char *filename) {

  ble_pkt_t *pkt;

  if ( !filename ) return -1;

  FILE *f = fopen(filename, "w");

  for ( uint32_t i = 0 ; i < ble_pkts_size ; i++ ) {
    for ( pkt = ble_pkts[i] ; pkt ; pkt = pkt->ble_pkt_prev ) {

      print_busyloop();

      char addr[18];
      ba2str(&(pkt->ba), addr);

      fprintf(f, "%ld,%ld,%s,%d,",
        pkt->recv_time.tv_sec, pkt->recv_time.tv_usec,
        addr, pkt->rssi);

      switch (pkt->data_type) {

      case BLE_ADV_INFO:

        for ( int d = 0 ; d < sizeof(le_advertising_info) ; d++ )
          fprintf(f, "%02x", ((uint8_t*)pkt->data.advinfo)[d]);

        for ( int d = 0 ; d < pkt->data.advinfo->length ; d++ )
          fprintf(f, "%02x", pkt->data.advinfo->data[d]);

      break;

      case BLE_GA_EN:

        ;

        en_ga_t *ga_info = pkt->data.ga;

        for ( int d = 0 ; d < sizeof(en_ga_t) ; d++ )
          fprintf(f, "%02x", ((uint8_t*)ga_info)[d]);

      break;
      }

      fprintf(f,"\n");
    }
  }

  fflush(f);
  fclose(f);

  // clear busy loop char
  printf("\b");
  fflush(stdout);

return 0;
}

int badv_load_csv(char *filename) {

  FILE *f;
  ble_pkt_t *pkt = NULL;
  char buf[4096];
  char *tok;
  unsigned int val;
  int ret = 0, col;

  if ( !filename || badv_init() < 0 ) return -1;

  if ( !(f = fopen(filename, "r")) ) {
    perror("Coudn't load file");
    return -1;
  }

  while ( fgets(buf, sizeof(buf), f)  ) {

//    print_busyloop();

    for ( col = 0, tok = strtok(buf, ",") ; tok && *tok ; tok = strtok(NULL, ",\n"), col++ ) {

//      printf("%i: %s\n", col, tok);

      // alocate new pkt on column 0
      if ( !pkt && (pkt = calloc(1, sizeof(ble_pkt_t) )) == NULL ) {
        goto badv_load_csv_enomem;
      }

      switch ( col % 5 ) {
      case 0:
        pkt->recv_time.tv_sec = atol(tok);
      break;
      case 1:
        pkt->recv_time.tv_usec = atol(tok);
      break;
      case 2:
        str2ba(tok, &pkt->ba);
      break;
      case 3:
         pkt->rssi = atoi(tok);
      break;
      case 4:
        if ( strncmp(tok, "17166ffd", 8) ) {
          pkt->data_type = BLE_ADV_INFO;

          le_advertising_info *info_tok = (le_advertising_info*)tok;

          if ( (pkt->data.advinfo = (le_advertising_info*) malloc(sizeof(le_advertising_info)+info_tok->length)) == NULL ) {
            goto badv_load_csv_enomem;
          }

          for ( int i = 0; i < sizeof(le_advertising_info) << 1 && tok[i] ; i += 2 ) {
            val = 0;
            sscanf(tok+i, "%02x", &val);

            ((uint8_t*)pkt->data.advinfo)[i >> 1] = val & 0xff;
          }

          for ( int i = 0; i < pkt->data.advinfo->length << 1 && tok[i] ; i += 2 ) {
            val = 0;
            sscanf(tok+i, "%02x", &val);

            pkt->data.advinfo->data[i >> 1] = val & 0xff;
          }

        } else {
          pkt->data_type = BLE_GA_EN;

          if ( (pkt->data.ga = (en_ga_t*) malloc(sizeof(en_ga_t))) == NULL ) {
            goto badv_load_csv_enomem;
          }

          for ( int i = 0; i < sizeof(en_ga_t) << 1 && tok[i] ; i += 2 ) {
            val = 0;
            sscanf(tok+i, "%02x", &val);

            ((uint8_t*)pkt->data.ga)[i >> 1] = val & 0xff;
          }

        }

        // Last column, push pkt to stream
        ble_pkt_print(pkt, 0);
        printf("\n");

        if ( (ret = ble_pkt_add(pkt)) < 0 )
          goto badv_load_csv_exit;

        pkt = NULL;

      break;

      default:
        fprintf(stderr, "Unknown line format!");
        ret = -1;
        goto badv_load_csv_exit;
      }

    }
  }

badv_load_csv_exit:

  // clear busy loop char
//  printf("\b");
//  fflush(stdout);

  fclose(f);

return ret;

badv_load_csv_enomem:

  fclose(f);

  perror("Could not allocate packet");
  exit(ENOMEM);

return -1;
}

void ble_pkt_free( ble_pkt_t *pkt ) {

  switch (pkt->data_type) {
    case BLE_ADV_INFO:
      if ( pkt->data.advinfo ) free(pkt->data.advinfo);
    break;
    case BLE_GA_EN:
      if ( pkt->data.ga ) free(pkt->data.ga);
    break;
  }

  free(pkt);

}

int badv_init() {

  // deallocate packets captured during previous scan
  if ( ble_pkts ) {
    for ( uint32_t i = 0 ; i < ble_pkts_size ; i++ ) {

      ble_pkt_t *pkt = ble_pkts[i];

      while ( pkt ) {
        ble_pkt_t *ppkt = pkt->ble_pkt_prev;

        ble_pkt_free(pkt);
        pkt = ppkt;
      }
    }

    free(ble_pkts);
    ble_pkts = NULL;
  }

  if ( (ble_pkts = calloc( BLE_PKTS_BUF_INIT, sizeof(ble_pkt_t*) )) == NULL ) { 
    perror("Error while creating packet buffer");
    exit(ENOMEM);
  }

  ble_pkts_size = BLE_PKTS_BUF_INIT;

return 0;
}

ble_pkt_t* ble_info2pkt( le_advertising_info *info ) {

  ble_pkt_t *pkt = NULL;

  if ( (pkt = calloc(1, sizeof(ble_pkt_t) )) == NULL ) {
    goto ble_info2pkt_enomem;
  }

  gettimeofday(&pkt->recv_time, NULL);

  // is it EN G+A service?
  if ( memcmp(info->data, "\x03\x03\x6f\xfd", 4) ) {
    pkt->data_type = BLE_ADV_INFO;

    if ( (pkt->data.advinfo = (le_advertising_info*) malloc(sizeof(le_advertising_info) + info->length)) == NULL ) {
      goto ble_info2pkt_enomem;
    }

    memcpy(pkt->data.advinfo, info, sizeof(le_advertising_info) );
    memcpy(pkt->data.advinfo->data, info->data, info->length );

  } else {
    pkt->data_type = BLE_GA_EN;

    if ( (pkt->data.ga = (en_ga_t*) malloc(sizeof(en_ga_t))) == NULL ) {
      goto ble_info2pkt_enomem;
    }

    en_ga_t *ga_info = (en_ga_t *) (info->data + 4);

    memcpy(pkt->data.ga, ga_info, sizeof(en_ga_t) );

    pkt->data.ga->uuid = btohs(ga_info->uuid);

  }

  pkt->bdaddr_type = info->bdaddr_type;
  bacpy(&pkt->ba, &info->bdaddr);
  pkt->rssi = (int8_t) ( *(info->data + info->length) );

return pkt;

ble_info2pkt_enomem:

  perror("Could not allocate packet");
  exit(ENOMEM);
}

int ble_pkt_add( ble_pkt_t *new_pkt ) {

  int64_t slot = -1;

  if ( !ble_pkts || !new_pkt ) {
    return -1;
  }

  // Search for slot with the same BD address
  for ( uint32_t i = 0 ; i < ble_pkts_size ; i++ ) {

    ble_pkt_t *pkt = ble_pkts[i];

    if ( !pkt ) {

      // Found first free slot
      if ( slot == -1 ) slot = i;

      continue;
    }

    if ( !bacmp(&pkt->ba, &new_pkt->ba) ) {

      // We have found same device
      slot = i;

      break;
    }
  }

//  printf("Slot set to : %ld\n", slot);

  // add packet to selected chain
  if ( slot != -1 ) {
    ble_pkt_t *prev_pkt = ble_pkts[slot];

    new_pkt->ble_pkt_prev = prev_pkt;

    ble_pkts[slot] = new_pkt;

    return 0;
  }

  // The for loop reached buffer end.
  // It means that buffer is full and we didn't see
  // that device address before
  if ( (ble_pkts_size * 2) > BLE_PKTS_BUF_MAX ) {
    fprintf(stderr, "Reached maximum packet buffer size\n");
    return -1;
  }

  ble_pkt_t **ble_pkts_new = (ble_pkt_t **) realloc(ble_pkts, ble_pkts_size * 2 * sizeof(ble_pkt_t*));
  if ( ble_pkts_new == NULL ) {
    perror("Error while enlarging packet buffer");
    exit(ENOMEM);
  }

  ble_pkts = ble_pkts_new;

  memset(ble_pkts+ble_pkts_size, 0, sizeof(ble_pkt_t*) * ble_pkts_size);

  ble_pkts[ble_pkts_size] = new_pkt;
  ble_pkts_size *= 2;

return 0;
}

// Keep in mind that it process data in reverse order (from newest packet to oldest)
int badv_track_devices() {

  ble_pkt_t *pkt, *last_pkt, *next_pkt;
  int merges = 0;

  if ( !ble_pkts ) {
    fprintf(stderr, "No data to track\n");
    return -1;
  }

  for ( uint32_t i = 0 ; i < ble_pkts_size ; i++ ) {

    // Search for last GA packet in chain
    for ( pkt = ble_pkts[i] ; pkt && pkt->data_type != BLE_GA_EN ; pkt = pkt->ble_pkt_prev ) ;

    if ( !pkt ) continue;

    last_pkt = pkt;

    // Try to gues to what data, device switched with new EN interval
    // (match last packet with first packet belonging to next chain)
    for ( uint32_t j = 0 ; j < ble_pkts_size ; j++ ) {

      if ( i == j ) continue;

      // Search for earliest GA packet in chain
      next_pkt = NULL;
      for ( pkt = ble_pkts[j] ; pkt ; pkt = pkt->ble_pkt_prev ) {
        if ( pkt->data_type == BLE_GA_EN )
          next_pkt = pkt;
      }

      // No GA packets in this chain
      if ( !next_pkt ) continue;

      // Next packet before our device last packet?
      if ( next_pkt->recv_time.tv_sec < last_pkt->recv_time.tv_sec ) {
        continue;
      }

      // Too long gap?
      if ( (next_pkt->recv_time.tv_sec - last_pkt->recv_time.tv_sec) > 11 ) {
        continue;
      }

      // RSSI more or less the same?
      if ( abs(next_pkt->rssi - last_pkt->rssi) > 20 ) {
        continue;
      }

      // If it's the same device, merge old chain to newer chain
      for ( pkt = ble_pkts[j] ; pkt && pkt->ble_pkt_prev ; pkt = pkt->ble_pkt_prev ) ; // go to first packet in chain

      pkt->ble_pkt_prev = ble_pkts[i];
      ble_pkts[i] = NULL;

      merges++;

      break;
    }
  }

return merges;
}

// Keep in mind that it process data in reverse order (from newest packet to oldest)
void badv_print() {

  ble_pkt_t *tail_pkt, *head_pkt, *pkt;
  double time_sum;
  uint32_t pkts_num;

  for ( uint32_t i = 0 ; i < ble_pkts_size ; i++ ) {

    // Print changes for chain
    for ( pkt = ble_pkts[i], tail_pkt = NULL ; pkt ; pkt = pkt->ble_pkt_prev ) {

      if ( pkt->data_type != BLE_GA_EN ) continue;

      if ( !tail_pkt ) {
        // Save lately received packet from notification stream for further comparison
        tail_pkt = pkt;
        head_pkt = pkt;

        // average gap
        time_sum = 0.0;
        pkts_num = 1;
        continue;
      }

      // Compare head with previously caught packet
      if ( memcmp(pkt->data.ga->rpi, head_pkt->data.ga->rpi, 16) ||
              memcmp(pkt->data.ga->aem, head_pkt->data.ga->aem, 4) ||
              bacmp(&pkt->ba, &head_pkt->ba) ||
              !pkt->ble_pkt_prev // we are at begining of whole device stream
         ) {

        if ( !pkt->ble_pkt_prev ) {
          // Move head to first packet in the stream
          head_pkt = pkt;
        }

        printf("Device %d - advertisment stream detected:\n", i);
        printf("\tbroadcast period %.3lf seconds, average gap between packets %.3lf seconds\n",
          (tail_pkt->recv_time.tv_sec + (tail_pkt->recv_time.tv_usec - head_pkt->recv_time.tv_usec)/1000000.0 - head_pkt->recv_time.tv_sec),
          ( (double)time_sum / (double)pkts_num ));
        printf("\thead ");
        ble_pkt_print(head_pkt, 0);
        printf("\n\ttail ");
        ble_pkt_print(tail_pkt, 0);
        printf("\n");

        // We are already at start of whole stream
        if ( pkt->ble_pkt_prev ) {
          printf("\ttail of previous stream (gap %.3lf seconds)\n\t     ",
            (head_pkt->recv_time.tv_sec + (head_pkt->recv_time.tv_usec - pkt->recv_time.tv_usec)/1000000.0 - pkt->recv_time.tv_sec));
          ble_pkt_print(pkt, 0);
          printf("\n");
        }

        tail_pkt = pkt; // Mark changed data tail
      }

      time_sum += (head_pkt->recv_time.tv_sec + (head_pkt->recv_time.tv_usec - pkt->recv_time.tv_usec)/1000000.0 - pkt->recv_time.tv_sec);
      pkts_num++;

      // Move stream head
      head_pkt = pkt;
    }
  }
}

