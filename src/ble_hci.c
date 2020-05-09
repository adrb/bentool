/*
 * 
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#include "ble_hci.h"

// abort current operation in case of ctrl-c
int abort_signal = 0;
void set_abort_signal( int sig ) {

  if ( sig != SIGINT ) return;

  abort_signal = 1;
}

int xhci_dev_info(int s, int dev_id, long arg) {

  struct hci_dev_info di = { .dev_id = dev_id };
  char addr[18];

  if (ioctl(s, HCIGETDEVINFO, (void *) &di))
    return 0;

  ba2str(&di.bdaddr, addr);
  printf("\t%s\t%s\n", di.name, addr);

return 0;
}

void print_en_data( t_exposure_notification_data *en ) {

  if ( !en ) return;

  printf("RPI: ");
  printhex((unsigned char*)&en->rpi, 16);
  printf(" AEM: ");
  printhex((unsigned char*)&en->aem, 4);
  printf("\n");
}

int ble_print_events( int dd ) {

  unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
  struct hci_filter nf, of;
  socklen_t olen;
  int len = -1;

  olen = sizeof(of);
  if (getsockopt(dd, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
    printf("Could not get socket options\n");
    return -1;
  }

  hci_filter_clear(&nf);
  hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
  hci_filter_set_event(EVT_LE_META_EVENT, &nf);

  if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
    printf("Could not set socket options\n");
    return -1;
  }

  abort_signal = 0;
  signal(SIGINT, &set_abort_signal);

  while ( !abort_signal ) {

    while ((len = read(dd, buf, sizeof(buf))) < 0) {

      if ( abort_signal ) goto done;

      if (errno == EAGAIN || errno == EINTR)
        continue;

      goto done;
    }

    if ( len < HCI_EVENT_HDR_SIZE ) {
      fprintf(stderr, "HCI event partial read");
      goto done;
    }

    ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
    len -= (1 + HCI_EVENT_HDR_SIZE);

    evt_le_meta_event *meta = (void *) ptr;
    if (meta->subevent != EVT_LE_ADVERTISING_REPORT)
      continue;

    uint8_t reports_num = meta->data[0];
    le_advertising_info *info = (le_advertising_info *) (meta->data + 1);
    while ( reports_num-- ) {

      // is it EN G+A service?
      if ( memcmp(info->data, "\x02\x01\x1a\x03\x03\x6f\xfd", 7) )
        continue;

      printf("\n");
      printts();

      char addr[18];
      ba2str(&(info->bdaddr), addr);

      int rssi = (int8_t) ( *(info->data + info->length) );

      printf("%s, len %d, RSSI %d\n", addr, info->length, rssi );

      t_exposure_notification_data *en = (t_exposure_notification_data *) (info->data + 7);
      print_en_data(en);

      hexdump(info->data, info->length);

      info = (le_advertising_info *) (info->data + info->length + 1);

    }
  }

done:

  signal(SIGINT, SIG_DFL);

  setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of));

  if (len < 0 && !abort_signal )
    return -1;

return 0;
}

int xhci_open_dev( t_btdev *btdev ) {
  int dd = -1;

  if (!btdev) return -1;

  // Get first available Bluetooth device
  if (btdev->dev_id < 0) {
    btdev->dev_id = hci_get_route(NULL);
    hci_devba(btdev->dev_id, &btdev->ba);
  }

  dd = hci_open_dev(btdev->dev_id);
  if (dd < 0) {
    perror("Could not open device");
    return -1;
  }

//  hci_read_bd_addr(dd, &btdev->ba, HCI_REQ_TIMEOUT);

return dd;
}

int ble_scan_en( t_btdev *btdev ) {

  int dd = -1;

  if (!btdev) return 1;

  if ( (dd = xhci_open_dev(btdev)) < 0 )
    return -1;

  /*
   * int hci_le_set_scan_parameters(int dev_id, uint8_t type, uint16_t interval,
   *       uint16_t window, uint8_t own_type, uint8_t filter, int to);
   *
   * type:
   *   0 - passive
   *   1 - active
   * own_type:
   *   LE_PUBLIC_ADDRESS - Public Device Address
   *   LE_RANDOM_ADDRESS
   * filter:
   *   0 - accept all
   *   1 - whitelist
   * to:
   *   int - hci request timeout in ms
   */
  if ( hci_le_set_scan_parameters(dd, 0, htobs(0x0010), htobs(0x0010),
        LE_RANDOM_ADDRESS, 0, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Set scan parameters failed");
    return 0;
  }

  /*
   * int hci_le_set_scan_enable(int dev_id, uint8_t enable, uint8_t filter_dup, int to);
   */
  if ( hci_le_set_scan_enable(dd, 0x01, 0, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Enable scan failed");
    return 1;
  }

  printf("EN BLE Scan ...\n");

  if ( ble_print_events(dd) < 0 ) {
    perror("Could not receive advertising events");
    return 1;
  }

  if ( hci_le_set_scan_enable(dd, 0x00, 0, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Disable scan failed");
    return 1;
  }

  hci_close_dev(dd);

return 0;
}

int ble_randaddr( t_btdev *btdev ) {

  struct hci_request rq;
  le_set_random_address_cp cp;
  int dd = -1, status;

  if (!btdev) return 1;

  if ( (dd = xhci_open_dev(btdev)) < 0 )
    return -1;

  memset(&cp, 0, sizeof(cp));
  bacpy(&cp.bdaddr, &btdev->ba);

  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_RANDOM_ADDRESS;
  rq.cparam = &cp;
  rq.clen = LE_SET_RANDOM_ADDRESS_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  if ( hci_send_req(dd, &rq, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Can't set random address");
    return -1;
  }

  hci_close_dev(dd);

return 0;
}


// https://covid19-static.cdn-apple.com/applications/covid19/current/static/contact-tracing/pdf/ExposureNotification-BluetoothSpecificationv1.2.pdf
le_set_advertising_data_cp set_adv_data_en( t_btdev *btdev ) {

  le_set_advertising_data_cp adv_data;
  memset(&adv_data, 0, sizeof(adv_data));

  adv_data.data[0] = 0x02;
  adv_data.data[1] = 0x01;
  adv_data.data[2] = 0x1a;

  adv_data.data[3] = 0x03;
  adv_data.data[4] = 0x03;
  adv_data.data[5] = 0x6f;
  adv_data.data[6] = 0xfd;

  btdev->en_data.length = 0x17;
  btdev->en_data.type = htobs(0x16);
  btdev->en_data.uuid = htobs(0xfd6f);

  memcpy(adv_data.data + 7, (void*)&btdev->en_data, btdev->en_data.length + 1);
  adv_data.length = 7 + btdev->en_data.length + 1;

return adv_data;
}

int ble_beacon_en( t_btdev *btdev ) {

  struct hci_request rq;
  int dd = -1, status;

  if (!btdev) return 1;

  if ( (dd = xhci_open_dev(btdev)) < 0 )
    return -1;

  // Set BLE advertisement parameters
  le_set_advertising_parameters_cp adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.min_interval = htobs(0x0800);
  adv_params.max_interval = htobs(0x0800);
  adv_params.advtype = 3;   // 0 - Connectable undirected advertising, 3 - Non connectable undirected advertising
  adv_params.chan_map = 7;
  adv_params.own_bdaddr_type = LE_RANDOM_ADDRESS;

  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
  rq.cparam = &adv_params;
  rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  if ( hci_send_req(dd, &rq, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Failed to set advertisement parameters data.");
    goto done;
  }

  // Set BLE advertisement data.
  le_set_advertising_data_cp adv_data = set_adv_data_en(btdev);

  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
  rq.cparam = &adv_data;
  rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  if ( hci_send_req(dd, &rq, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Failed to set advertising data.");
    goto done;
  }

  // Enable advertising
  if ( hci_le_set_advertise_enable(dd, 0x01, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Failed to enable advertising.");
    goto done;
  }

  printf("EN BLE advertising ...\n");
  print_en_data(&btdev->en_data);

  abort_signal = 0;
  signal(SIGINT, &set_abort_signal);

  while ( !abort_signal ) {
    usleep(5000000);
  }

  signal(SIGINT, SIG_DFL);

  // Disable advertising
  if ( hci_le_set_advertise_enable(dd, 0x00, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Failed to enable advertising.");
    goto done;
  }

done:
  hci_close_dev(dd);

return 0;
}

