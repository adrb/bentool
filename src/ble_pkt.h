/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#ifndef __BLE_ADV_H__
#define __BLE_ADV_H__

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

typedef struct {

  uint8_t length;   // 0x17
  uint8_t type;     // 0x16
  uint16_t uuid;    // 0xfd6f

  uint8_t rpi[16];
  uint8_t aem[4];

} __attribute__ ((packed)) ble_ga_adv_t;

typedef enum {

  BLE_ADV_INFO,
  BLE_GA_EN,

} ble_pkt_data_type;

typedef struct ble_pkt_s {

  struct ble_pkt_s *older;  // packet received before that packet
  struct ble_pkt_s *newer;  // packet received after that packet

  uint8_t bdaddr_type;
  bdaddr_t ba;
  int rssi;
  struct timeval recv_time;

  ble_pkt_data_type data_type;

  union {
    ble_ga_adv_t *ga;
    le_advertising_info *advinfo;
//    uint8_t raw[255];
  } data;

} ble_pkt_t;

void ble_ga_adv_print( ble_ga_adv_t *en );
void ble_pkt_print( ble_pkt_t *pkt, int print_datadump );
void ble_pkt_free( ble_pkt_t *pkt );

ble_pkt_t* ble_info2pkt( le_advertising_info *info );

#endif // __BLE_ADV_H__
