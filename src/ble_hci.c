/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#include "bentool.h"

// abort current operation in case of ctrl-c
int abort_signal = 0;
void set_abort_signal( int sig ) {

  if ( sig != SIGINT ) return;

  abort_signal = 1;
}

int xhci_open_dev( btdev_t *btdev ) {
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

int xhci_dev_info(int s, int dev_id, long arg) {

  struct hci_dev_info di = { .dev_id = dev_id };
  char addr[18];

  if (ioctl(s, HCIGETDEVINFO, (void *) &di))
    return 0;

  ba2str(&di.bdaddr, addr);
  printf("\t%s\t%s\n", di.name, addr);

return 0;
}

void print_dev_info( btdev_t *btdev ) {

  char addr[18];

  if ( !btdev ) return;

  ba2str(&btdev->ba, addr);
  printf("Random BA: %s, ", addr);

  ble_ga_adv_print(&btdev->ga_en);

  printf("\n");
}

int ble_scan_events( int dd ) {

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

      ble_pkt_t *new_pkt = ble_info2pkt(info);
      if ( !new_pkt || ble_stream_pkt_add(new_pkt) < 0 ) {
        abort_signal = 1;
        break;
      }

      if ( new_pkt->data_type == BLE_GA_EN ) {
        ble_pkt_print(new_pkt, 0);
        printf("\n");
      }

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

int ble_scan( btdev_t *btdev ) {

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

  ble_stream_free();

  printf("Scanning for Bluetooth Advertisement packets...\n");

  if ( ble_scan_events(dd) < 0 ) {
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

int ble_randaddr( btdev_t *btdev ) {

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
le_set_advertising_data_cp set_adv_data_en( btdev_t *btdev ) {

  le_set_advertising_data_cp adv_data;
  memset(&adv_data, 0, sizeof(adv_data));

  adv_data.data[0] = 0x03;
  adv_data.data[1] = 0x03;
  adv_data.data[2] = 0x6f;
  adv_data.data[3] = 0xfd;

  btdev->ga_en.length = 0x17;
  btdev->ga_en.type = htobs(0x16);
  btdev->ga_en.uuid = htobs(0xfd6f);

  memcpy(adv_data.data + 4, (void*)&btdev->ga_en, btdev->ga_en.length + 1);
  adv_data.length = 4 + btdev->ga_en.length + 1;

return adv_data;
}

int ble_beacon_ga( btdev_t *btdev ) {

  struct hci_request rq;
  int dd = -1, status;

  if (!btdev) return 1;

  if ( (dd = xhci_open_dev(btdev)) < 0 || ble_randaddr(btdev) < 0 )
    return -1;

  // Set BLE advertisement parameters
  le_set_advertising_parameters_cp adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.min_interval = htobs(0x0140);  // 1 = 0.625ms (16 = 10ms), see BLE HCI spec
  adv_params.max_interval = htobs(0x0280);  // 400ms, BT Core 5.1 defines upper limit to 10.24sec (0x4000)
  adv_params.advtype = 2;   // 0 - Connectable undirected advertising, 2 - Scannable undirected advertising, 3 - Non connectable undirected advertising
  adv_params.chan_map = 7;
  adv_params.own_bdaddr_type = LE_RANDOM_ADDRESS; // Use random address

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

  printf("Advertising G+A notifications...\n");

  print_dev_info(btdev);

  abort_signal = 0;
  signal(SIGINT, &set_abort_signal);

  while ( !abort_signal ) {
    usleep(1000000);
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

