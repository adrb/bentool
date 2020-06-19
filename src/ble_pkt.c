/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#include "bentool.h"

void ble_ga_adv_print( ble_ga_adv_t *en ) {

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
      ble_ga_adv_print(pkt->data.ga);

      if ( print_datadump ) {
        printf("\n");
        hexdump((uint8_t*)pkt->data.ga, sizeof(ble_ga_adv_t) );
      }
    break;
  }
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

    if ( (pkt->data.ga = (ble_ga_adv_t*) malloc(sizeof(ble_ga_adv_t))) == NULL ) {
      goto ble_info2pkt_enomem;
    }

    ble_ga_adv_t *ga_info = (ble_ga_adv_t *) (info->data + 4);

    memcpy(pkt->data.ga, ga_info, sizeof(ble_ga_adv_t) );

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

