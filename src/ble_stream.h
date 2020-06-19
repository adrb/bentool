/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#ifndef __BLE_STREAM_H__
#define __BLE_STREAM_H__

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "ble_pkt.h"

typedef struct ble_pkt_stream_s {

  struct ble_pkt_stream_s *prev;
  struct ble_pkt_stream_s *next;

  ble_pkt_t *pkt_head;
  ble_pkt_t *pkt_latest;

  // stream metrics
  uint32_t pkts;
  uint64_t pkt_gap_usum;   // sum of time gaps between packets in stream in usec

  struct timeval rpa_last_change;
  uint64_t rpa_interval_us;

} ble_pkt_stream_t;

void ble_stream_free();
int ble_stream_dump(char *filename);
int ble_stream_load(char *filename);

int ble_stream_pkt_add( ble_pkt_t *new_pkt);
int ble_stream_track();

void ble_stream_print();

#endif // __BLE_STREAM_H__
