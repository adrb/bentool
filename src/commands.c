/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#include "commands.h"
#include "bentool.h"

btdev_t btdev = {
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
      if ( !(col % 1) ) printf("\n");
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
    memset(&btdev.bda, 0, sizeof(bdaddr_t));
  }

  btdev.dev_id = hci_devid(argv[1]);
  if (btdev.dev_id < 0) {
    perror("Invalid device");
    return 1;
  }

  if ( hci_devba(btdev.dev_id, &btdev.bda) < 0) {
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

  str2ba(argv[1], &btdev.bda);

print_ba:

  ba2str(&btdev.bda, addr);
  printf("Random BDA: %s\n", addr);

return 0;
}

int cmd_resolve_rpa( int argc, char **argv) {

  char addr[18];
  uint8_t key[16];
  bdaddr_t bda;

  CHECK_ARGS_NUM(2);

  memset(key, 0, 16);

  str2ba(argv[1], &bda);
  hex2raw(key, argv[2], 16);

  if ( ble_resolve_rpa(&bda, key) ) {
    printf("Mismatch\n");
  } else {
    printf("Resolved\n");
  }

return 0;
}

int cmd_beacon( int argc, char **argv) {

  CHECK_ARGS_NUM(0);

return ble_beacon_ga(&btdev);
}

int cmd_bonding( int argc, char **argv) {

  char addr[18];
  ble_bonding_t *bk;

  CHECK_ARGS_MAXNUM(3);

  if ( argc == 4 ) {

    bk = calloc(1, sizeof(ble_bonding_t) );
    if (!bk) {
      perror("Can't allocate new bonding");
      return -1;
    }

    bk->name = strdup(argv[1]);

    if ( !strcmp(argv[2], "--bda" ) ) {
      str2ba(argv[3], &bk->bda_public);
    }

    if ( !strcmp(argv[2], "--irk" ) ) {
      hex2raw(bk->irk, argv[3], 16);
    }

    ble_bonding_add(bk);
  }

  ble_bonding_print();

return 0;
}

int cmd_scan( int argc, char **argv) {

  CHECK_ARGS_NUM(0);

return ble_scan(&btdev);
}

int cmd_track( int argc, char **argv) {

  CHECK_ARGS_MAXNUM(2);

  if ( argc > 1 ) {

    if ( !strcmp(argv[1], "--load" ) ) {
      return ble_stream_load(argv[2]);
    }

    if ( !strcmp(argv[1], "--dump" ) ) {
      return ble_stream_dump(argv[2]);
    }

    fprintf(stderr, "Unknown option\n");
    return -1;
  }

  // Loop until we merge all possible devices
  while ( ble_stream_track() > 0 ) ;

  ble_stream_print();

return 0;
}

int cmd_ga_rpi( int argc, char **argv) {

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

    btdev.ga_en.rpi[i >> 1] = val & 0xff;
  }

print_rpi:
  printf("G+A RPI: ");
  printhex((void*)&btdev.ga_en.rpi, 16);
  printf("\n");

return 0;
}

int cmd_ga_aem( int argc, char **argv) {

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

    btdev.ga_en.aem[i >> 1] = val & 0xff;
  }

print_aem:
  printf("G+A AEM: ");
  printhex((void*)&btdev.ga_en.aem, 4);
  printf("\n");

return 0;
}

command_t commands[] = {
  {
    .cmd = cmd_ga_rpi,
    .name = "ga_rpi",
    .desc = "[RPI]\n\n"
      "\tDisplay or set advertised G+A RPI\n",
  },
  {
    .cmd = cmd_ga_aem,
    .name = "ga_aem",
    .desc = "[AEM]\n\n"
      "\tDisplay or set advertised G+A AEM\n",
  },
  {
    .cmd = cmd_scan,
    .name = "scan",
    .desc = "\n\n"
      "\tScan for exposure notification beacons (Ctrl-C to stop)\n",
  },
  {
    .cmd = cmd_beacon,
    .name = "beacon",
    .desc = "\n\n"
      "\tAdvertise exposure notification beacons (Ctrl-C to stop)\n",
  },
  {
    .cmd = cmd_bonding,
    .name = "bonding",
    .desc = "[NAME] [--bda BDADDR |--irk IRK]\n\n"
      "\tSet or list bonding info\n\n"
      "\tBDADDR - Bluetooth device address\n"
      "\tIRK    - 128-bit Identity Resolving Key\n",
  },

  {
    .cmd = cmd_track,
    .name = "track",
    .desc = "[--dump|--load CSVFILE]\n\n"
      "\tAnalyze scanned advertisements and try to track devices\n"
      "\tExecute 'scan' first\n\n"
      "\tCSVFILE - Dump or load scan results to/from this CSV file\n",
    },
  {
    .cmd = cmd_lerandaddr,
    .name = "lerandaddr",
    .desc = "[BDADDR]\n\n"
      "\tDisplay or set BLE Random Address\n",
  },
  {
    .cmd = cmd_resolve_rpa,
    .name = "resolve_rpa",
    .desc = "[BDADDR] [IRK]\n\n"
      "\tValidate Random Private Address against 128bit IRK key\n",
  },
  {
    .cmd = cmd_dev,
    .name = "dev",
    .desc = "[hciX]\n\n"
    "\tList bluetooth devices or select HCI device\n"
    "\tIf device is not specified, further commands will\n"
    "\tbe sent to the first available Bluetooth device\n",
  },
  { .cmd = cmd_help,
    .name = "help",
    .desc = "[COMMAND]\n\n"
    "\tList available commands\n\n"
    "\tCOMMAND\tprint detailed description about COMMAND\n",
  },
  { cmd_help, "?", "Synonym for 'help'" },
  { cmd_quit, "quit", "Exit the program" },
  { (tf_command *)NULL, (char *)NULL, (char *)NULL }
};

