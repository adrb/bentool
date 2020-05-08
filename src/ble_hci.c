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

int ble_print_events( int fd ) {

  unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
  struct hci_filter nf, of;
  socklen_t olen;
  int len = -1;

  olen = sizeof(of);
  if (getsockopt(fd, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
    printf("Could not get socket options\n");
    return -1;
  }

  hci_filter_clear(&nf);
  hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
  hci_filter_set_event(EVT_LE_META_EVENT, &nf);

  if (setsockopt(fd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
    printf("Could not set socket options\n");
    return -1;
  }

  abort_signal = 0;
  signal(SIGINT, &set_abort_signal);

  while ( !abort_signal ) {

    while ((len = read(fd, buf, sizeof(buf))) < 0) {

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

      // is it EN service?
      if ( memcmp(info->data, "\x02\x01\x1a\x03\x03\x6f\xfd", 7) )
        continue;

      printf("\n");
      printts();

      char addr[18];
      ba2str(&(info->bdaddr), addr);

      int rssi = (int8_t) ( *(info->data + info->length) );

      printf("%s, len %d, RSSI %d\n", addr, info->length, rssi );

      t_exposure_notification_data *en = (t_exposure_notification_data *) (info->data + 7);
      printf("RPI: ");
      printhex((unsigned char*)&en->rpi, 16);
      printf(" AEM: ");
      printhex((unsigned char*)&en->aem, 4);
      printf("\n");

      hexdump(info->data, info->length);

      info = (le_advertising_info *) (info->data + info->length + 1);

    }
  }

done:

  signal(SIGINT, SIG_DFL);

  setsockopt(fd, SOL_HCI, HCI_FILTER, &of, sizeof(of));

  if (len < 0 && !abort_signal )
    return -1;

return 0;
}

int xhci_open_dev( t_btdev *btdev ) {
  int fd = -1;

  if (!btdev) return -1;

  // Get first available Bluetooth device
  if (btdev->dev_id < 0) {
    btdev->dev_id = hci_get_route(NULL);
    hci_devba(btdev->dev_id, &btdev->ba);
  }

  fd = hci_open_dev(btdev->dev_id);
  if (fd < 0) {
    perror("Could not open device");
    return -1;
  }

return fd;
}

int ble_scan_en( t_btdev *btdev ) {

  int fd = -1;

  if (!btdev) return 1;

  if ( (fd = xhci_open_dev(btdev)) < 0 )
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
  if ( hci_le_set_scan_parameters(fd, 0, htobs(0x0010), htobs(0x0010),
        LE_RANDOM_ADDRESS, 0, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Set scan parameters failed");
    return 0;
  }

  /*
   * int hci_le_set_scan_enable(int dev_id, uint8_t enable, uint8_t filter_dup, int to);
   */
  if ( hci_le_set_scan_enable(fd, 0x01, 0, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Enable scan failed");
    return 1;
  }

  printf("EN BLE Scan ...\n");

  if ( ble_print_events(fd) < 0 ) {
    perror("Could not receive advertising events");
    return 1;
  }

  if ( hci_le_set_scan_enable(fd, 0x00, 0, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Disable scan failed");
    return 1;
  }

  hci_close_dev(fd);

return 0;
}

struct hci_request xble_hci_request(uint16_t ocf, int clen, void *status, void *cparam) {

  struct hci_request rq;
  memset(&rq, 0, sizeof(rq));

  rq.ogf = OGF_LE_CTL;
  rq.ocf = ocf;
  rq.cparam = cparam;
  rq.clen = clen;
  rq.rparam = status;
  rq.rlen = 1;

return rq;
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
  int fd = -1, status;

  if (!btdev) return 1;

  if ( (fd = xhci_open_dev(btdev)) < 0 )
    return -1;

  // Set BLE advertisement parameters
  le_set_advertising_parameters_cp adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.min_interval = htobs(0x0800);
  adv_params.max_interval = htobs(0x0800);
  adv_params.chan_map = 7;

  struct hci_request adv_params_rq = xble_hci_request(OCF_LE_SET_ADVERTISING_PARAMETERS,
      LE_SET_ADVERTISING_PARAMETERS_CP_SIZE, &status, &adv_params);

  if ( hci_send_req(fd, &adv_params_rq, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Failed to set advertisement parameters data.");
    goto done;
  }

  // Set BLE advertisement data.
  le_set_advertising_data_cp adv_data = set_adv_data_en(btdev);

  struct hci_request adv_data_rq = xble_hci_request(OCF_LE_SET_ADVERTISING_DATA,
    LE_SET_ADVERTISING_DATA_CP_SIZE, &status, &adv_data);

  if ( hci_send_req(fd, &adv_data_rq, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Failed to set advertising data.");
    goto done;
  }

  // Enable advertising.
  le_set_advertise_enable_cp set_advertise;
  memset(&set_advertise, 0, sizeof(set_advertise));
  set_advertise.enable = 0x01;

  struct hci_request enable_adv_rq = xble_hci_request(
    OCF_LE_SET_ADVERTISE_ENABLE,
    LE_SET_ADVERTISE_ENABLE_CP_SIZE, &status, &set_advertise);

  if ( hci_send_req(fd, &enable_adv_rq, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Failed to enable advertising.");
    goto done;
  }

  printf("EN BLE advertising ...\n");

  abort_signal = 0;
  signal(SIGINT, &set_abort_signal);

  while ( !abort_signal ) {
    usleep(5000000);
  }

  signal(SIGINT, SIG_DFL);

  // Disable advertising
  memset(&set_advertise, 0, sizeof(set_advertise));
  set_advertise.enable = 0x00;

  xble_hci_request(OCF_LE_SET_ADVERTISE_ENABLE,
    LE_SET_ADVERTISE_ENABLE_CP_SIZE, &status, &set_advertise);

  if ( hci_send_req(fd, &enable_adv_rq, HCI_REQ_TIMEOUT) < 0 ) {
    perror("Failed to enable advertising.");
    goto done;
  }

done:
  hci_close_dev(fd);

return 0;
}

