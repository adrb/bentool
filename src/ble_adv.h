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

#define BLE_PKTS_BUF_INIT 64
#define BLE_PKTS_BUF_MAX  (BLE_PKTS_BUF_INIT << 6)

typedef struct {

  uint8_t length;   // 0x17
  uint8_t type;     // 0x16
  uint16_t uuid;    // 0xfd6f

  uint8_t rpi[16];
  uint8_t aem[4];

} __attribute__ ((packed)) en_ga_t;

typedef enum {

  BLE_ADV_INFO,
  BLE_GA_EN,

} ble_pkt_data_type;

typedef struct ble_pkt_s {

  struct ble_pkt_s *ble_pkt_prev;  // previous packet from the same sender

  uint8_t bdaddr_type;
  bdaddr_t ba;
  int rssi;
  struct timeval recv_time;

  ble_pkt_data_type data_type;

  union {
    en_ga_t *ga;
    le_advertising_info *advinfo;
//    uint8_t raw[255];
  } data;

} ble_pkt_t;

int badv_init();
int badv_add( le_advertising_info *info, int print_pkt );
int badv_track_devices();

void en_ga_print( en_ga_t *en );
void ble_pkt_print( ble_pkt_t *pkt, int print_datadump );

#endif // __BLE_ADV_H__

