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

#include "utils.h"

#define HCI_REQ_TIMEOUT 10000

#pragma pack(1)
typedef struct {

  uint8_t length;   // 0x17
  uint8_t type;     // 0x16
  uint16_t uuid;    // 0xfd6f

  uint8_t rpi[16];
  uint8_t aem[4];

} t_exposure_notification_data;
#pragma pack()

typedef struct {

  int dev_id;
  bdaddr_t ba;

  t_exposure_notification_data en_data;

} t_btdev;

int xhci_dev_info(int s, int dev_id, long arg);

int ble_randaddr( t_btdev *btdev );

int ble_scan_en( t_btdev *btdev );
int ble_beacon_en( t_btdev *btdev );

#endif // __BLE_HCI_H__
