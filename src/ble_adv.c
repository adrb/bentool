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

  // Temporary skip other packets that we aren't interesed with
  if ( pkt->data_type == BLE_ADV_INFO ) return;

  print_tv( &pkt->recv_time );

  char addr[18];
  ba2str(&(pkt->ba), addr);

  printf(" - BD %s, RSSI %d, ", addr, pkt->rssi );

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

  printf("\n");
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

ble_pkt_t* blescan_info2pkt( le_advertising_info *info ) {

  ble_pkt_t *pkt = NULL;

  if ( (pkt = calloc(1, sizeof(ble_pkt_t) )) == NULL ) {
    goto blescan_info2pkt_enomem;
  }

  gettimeofday(&pkt->recv_time, NULL);

  // is it EN G+A service?
  if ( memcmp(info->data, "\x03\x03\x6f\xfd", 4) ) {
    pkt->data_type = BLE_ADV_INFO;

    if ( (pkt->data.advinfo = (le_advertising_info*) malloc(sizeof(le_advertising_info) + info->length)) == NULL ) {
      goto blescan_info2pkt_enomem;
    }

    memcpy(pkt->data.advinfo, info, sizeof(le_advertising_info) );
    memcpy(pkt->data.advinfo->data, info->data, info->length );

  } else {
    pkt->data_type = BLE_GA_EN;

    if ( (pkt->data.ga = (en_ga_t*) malloc(sizeof(en_ga_t))) == NULL ) {
      goto blescan_info2pkt_enomem;
    }

    en_ga_t *ga_info = (en_ga_t *) (info->data + 4);

    memcpy(pkt->data.ga, ga_info, sizeof(en_ga_t) );

    pkt->data.ga->uuid = btohs(ga_info->uuid);

  }

  pkt->bdaddr_type = info->bdaddr_type;
  bacpy(&pkt->ba, &info->bdaddr);
  pkt->rssi = (int8_t) ( *(info->data + info->length) );

return pkt;

blescan_info2pkt_enomem:

  perror("Could not allocate packet");
  exit(ENOMEM);
}

int badv_add( le_advertising_info *info, int print_pkt ) {

  int64_t slot = -1;

  if ( !ble_pkts || !info ) {
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

    if ( !bacmp(&pkt->ba, &info->bdaddr) ) {

      // We have found same device
      slot = i;

      break;
    }
  }

//  printf("Slot set to : %ld\n", slot);

  ble_pkt_t *new_pkt = blescan_info2pkt(info);
  if ( print_pkt ) {
    ble_pkt_print(new_pkt, 0);
  }

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

int badv_track_devices() {

  ble_pkt_t *pkt, *last_pkt, *next_pkt;

  if ( !ble_pkts ) {
    fprintf(stderr, "No data to track\n");
    return -1;
  }

/*
  printf("Captured devices:\n");
  for ( uint32_t i = 0 ; i < ble_pkts_size ; i++ ) {
    ble_pkt_print(ble_pkts[i], 0);
  }
*/

  for ( uint32_t i = 0 ; i < ble_pkts_size ; i++ ) {

    // Search for last GA packet in chain
    for ( pkt = ble_pkts[i] ; pkt && pkt->data_type != BLE_GA_EN ; pkt = pkt->ble_pkt_prev ) ;

    if ( !pkt ) continue;

    last_pkt = pkt;

    // Search for changed data within current chain
    while ( pkt && pkt->ble_pkt_prev ) {

      // Skip other packets (we may analyze it in further versions)
      if ( pkt->data_type != BLE_GA_EN )
        continue;

      // Same data?
      if ( memcmp(pkt->data.ga->rpi, pkt->ble_pkt_prev->data.ga->rpi, 16) ||
            memcmp(pkt->data.ga->aem, pkt->ble_pkt_prev->data.ga->aem, 4)) {

        printf("Device has changed only EN data\n\tfrom ");
        ble_pkt_print(pkt->ble_pkt_prev, 0);
        printf("\t  to ");
        ble_pkt_print(pkt, 0);
      }

      pkt = pkt->ble_pkt_prev;
    }

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

      // Long gap?
      if ( (next_pkt->recv_time.tv_sec - last_pkt->recv_time.tv_sec) > 20 ) {
        continue;
      }

      // RSSI more or less the same?
      if ( abs(next_pkt->rssi - last_pkt->rssi) > 20 ) {
        continue;
      }

      printf("Device changed notifications\n\tfrom ");
      ble_pkt_print(last_pkt, 0);
      printf("\t  to ");
      ble_pkt_print(next_pkt, 0);

      break;
    }
  }

return 0;
}

