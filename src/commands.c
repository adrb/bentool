/*
 * 
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "commands.h"

t_btdev btdev = {
  .dev_id = -1,
};

#define CHECK_ARGS_NUM(num) \
  { \
    if ( argc != (num+1) ) { \
      fprintf(stderr,"%s: Wrong arguments number\n", argv[0]); \
      return -1; \
    } \
  }

#define CHECK_ARGS_MAXNUM(num) \
  { \
    if ( argc > (num+1) ) { \
      fprintf(stderr,"%s: Too many arguments\n", argv[0]); \
      return -1; \
    } \
  }


int cmd_quit( int argc, char **argv) {

  exit(0);
}

int cmd_help( int argc, char **argv) {
  int i, col;

  CHECK_ARGS_MAXNUM(1);

  if ( argc == 1 ) {
    printf("List of available commands printed below.\n" \
        "Use 'help command' to display detailed description\n\n");
  }

  for (i = 0, col = 0; commands[i].name; i++) {

    if ( argc > 1 ) {
      if ( strcmp(argv[1], commands[i].name) == 0 ) {
        printf("%s\t%s", commands[i].name, commands[i].desc);
        break;
      }
    } else {
      printf("%s\t\t", commands[i].name);
      
      col++;
      if ( !(col % 5) ) printf("\n");
    }
  }

  printf("\n");
return 0;
}

int cmd_dev( int argc, char **argv) {

  CHECK_ARGS_MAXNUM(1);

  if ( argc == 1 ) {
    hci_for_each_dev(HCI_UP, xhci_dev_info, 0);
    return 0;
  }

  if ( btdev.dev_id != -1 ) {
    memset(&btdev.ba, 0, sizeof(bdaddr_t));
  }

  btdev.dev_id = hci_devid(argv[1]);
  if (btdev.dev_id < 0) {
    perror("Invalid device");
    return 1;
  }

  if ( hci_devba(btdev.dev_id, &btdev.ba) < 0) {
    perror("Device is not available");
    return 1;
  }

return 0;
}

int cmd_lerandaddr( int argc, char **argv) {

  char addr[18];
  int dd = -1;

  CHECK_ARGS_MAXNUM(1);

  // Read address if user didn't select device
  if ( (dd = xhci_open_dev(&btdev)) < 0 )
    return -1;
  hci_close_dev(dd);

  if ( argc == 1 ) {
    goto print_ba;
  }

  str2ba(argv[1], &btdev.ba);

  ble_randaddr(&btdev);

print_ba:

  ba2str(&btdev.ba, addr);
  printf("Random BA: %s\n", addr);

return 0;
}

int cmd_beacon( int argc, char **argv) {

  CHECK_ARGS_NUM(0);

return ble_beacon_en(&btdev);
}

int cmd_scan( int argc, char **argv) {

  CHECK_ARGS_NUM(0);

return ble_scan_en(&btdev);
}

int cmd_rpi( int argc, char **argv) {

  int len, i;

  CHECK_ARGS_MAXNUM(1);

  if ( argc == 1 ) {
    goto print_rpi;
  }

  len = strlen(argv[1]);
  if ( len != 32 ) {
    fprintf(stderr, "Wrong RPI format\n");
    return 1;
  }

  for ( i = 0; i < len ; i += 2 ) {
    unsigned int val = 0;
    sscanf(argv[1]+i, "%02x", &val);

    btdev.en_data.rpi[i >> 1] = val & 0xff;
  }

print_rpi:
  printf("RPI: ");
  printhex((void*)&btdev.en_data.rpi, 16);
  printf("\n");

return 0;
}

int cmd_aem( int argc, char **argv) {

  int len, i;

  CHECK_ARGS_MAXNUM(1);

  if ( argc == 1 ) {
    goto print_aem;
  }

  len = strlen(argv[1]);
  if ( len != 8 ) {
    fprintf(stderr, "Wrong AEM format\n");
    return 1;
  }

  for ( i = 0; i < len ; i += 2 ) {
    unsigned int val = 0;
    sscanf(argv[1]+i, "%02x", &val);

    btdev.en_data.aem[i >> 1] = val & 0xff;
  }

print_aem:
  printf("AEM: ");
  printhex((void*)&btdev.en_data.aem, 4);
  printf("\n");

return 0;
}

t_command commands[] = {
  { cmd_rpi, "rpi",
    "[RPI]\n\n\tDisplay or set advertised RPI\n"
  },
  { cmd_aem, "aem",
    "[AEM]\n\n\tDisplay or set advertised AEM\n"
  },
  { cmd_scan, "scan",
    "\n\n\tScan for exposure notification beacons\n"
  },
  { cmd_beacon, "beacon",
    "\n\n\tAdvertise exposure notification beacons\n"
  },
  { cmd_lerandaddr, "lerandaddr",
    "[BDADDR]\n\n\tDisplay or set BLE Random Address\n"
  },
  { cmd_dev, "dev",
    "[hciX]\n\n" \
    "\tList bluetooth devices or select HCI device\n" \
    "\tIf device is not specified, further commands will\n" \
    "\tbe sent to the first available Bluetooth device\n"
  },
  { cmd_help, "help",
    "[COMMAND]\n\n" \
    "\tList available commands\n\n" \
    "\tCOMMAND\tprint detailed description about COMMAND\n"
  },
  { cmd_help, "?", "Synonym for 'help'" },
  { cmd_quit, "quit", "Exit the program" },
  { (tf_command *)NULL, (char *)NULL, (char *)NULL }
};

