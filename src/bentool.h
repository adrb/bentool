/*
 * 
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#ifndef __BENTOOL_H__
#define __BENTOOL_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <time.h>

#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "utils.h"
#include "ble_hci.h"
#include "ble_adv.h"

#endif // __BENTOOL_H__
