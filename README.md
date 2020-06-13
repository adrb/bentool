
Bluetooth Exposure Notification beacons manipulation tool for Linux.

## What is it?

Tool which demonstrates practical replay attack against new Bluetooth based
contact tracing G+A protocol.

If you never heard of that before, please read about it [here](https://github.com/DP-3T/documents/blob/master/DP3T%20-%20Simplified%20Three%20Page%20Brief.pdf) and [here](https://www.apple.com/covid19/contacttracing/).

## Why?

I'm not inherently against contact-tracing protocols, furthermore I firmly
believe that it may help combat the COVID19 virus.

That being said you should also be aware that hardware commonly used today
wasn't desinged to that kind of operations. That leads us to situation where,
none of the Bluetooth-based protocols is completely secure.

You can read on some of threads [here](https://www.eff.org/deeplinks/2020/04/apple-and-googles-covid-19-exposure-notification-api-questions-and-answers) and [here](https://eprint.iacr.org/2020/399.pdf).

## Prerequisites

Linux system and cheap USB dongle with support for Bluetooth 4.0 version
or never.

## Compilation

You may need to install some non-standard packages:

  - Debian like systems: gcc make libreadline-dev libbluetooth-dev

```
$ make
```

## Usage:

Please keep in mind that it's work in progress, so expect seurious changes
with every commit. The best you can do is run the program and type "help":

```
# ./bentool
> help
```

Sending customized EN data (Ctrl-C to stop):

```
# ./bentool
> dev hci0
> ga_rpi 12340000000000000000000000000000
G+A RPI: 12340000000000000000000000000000
> ga_aem 12345678
G+A AEM: 12345678
> lerandaddr 01:02:03:04:05:06
Random BA: 01:02:03:04:05:06
> beacon
Advertising G+A notifications...
```

Scanning for EN data broadcasted by other devices (Ctrl-C to stop):

```
# ./bentool
> scan
Scanning for Bluetooth Advertisement packets...
2020-06-13 14:49:35.705 - BD 00:19:86:00:04:FD, RSSI -39, RPI: 12340000000000000000000000000000, AEM: 12345678
^C> track
```

Commands history is saved to .bthistory file if it exists.

## Author:
<adrian.brzezinski at adrb.pl>
