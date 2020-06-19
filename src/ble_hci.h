/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 * https://www.bluetooth.com/specifications/bluetooth-core-specification/
 */

#ifndef __BLE_HCI_H__
#define __BLE_HCI_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "ble_pkt.h"

#define HCI_REQ_TIMEOUT 5000

typedef struct {

  int dev_id;
  bdaddr_t ba;

  ble_ga_adv_t ga_en;

} btdev_t;

int xhci_dev_info(int s, int dev_id, long arg);
int xhci_open_dev( btdev_t *btdev );

int ble_randaddr( btdev_t *btdev );

int ble_scan( btdev_t *btdev );
int ble_beacon_ga( btdev_t *btdev );

#endif // __BLE_HCI_H__
