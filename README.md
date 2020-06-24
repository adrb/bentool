
Bluetooth Exposure Notification beacons manipulation tool for Linux.

## What is it?

Tool which demonstrates practical replay attack against new Bluetooth based
contact tracing G+A protocol. It can also track notification changes as long
tracked device (smartfone) remains at receiver's range all the time or
you somehow take over device IRK key.

If you never heard of that before, please read about it [here](https://github.com/DP-3T/documents/blob/master/DP3T%20-%20Simplified%20Three%20Page%20Brief.pdf) and [here](https://www.apple.com/covid19/contacttracing/).

## Why?

I'm not inherently against contact-tracing protocols, furthermore I firmly
believe that it may help combat the COVID19 virus.

That being said you should also be aware that hardware commonly used today
wasn't desinged to that kind of operations. That leads us to situation where,
none of the Bluetooth-based protocols is completely secure.

You can read on some of threads [here](https://www.eff.org/deeplinks/2020/04/apple-and-googles-covid-19-exposure-notification-api-questions-and-answers) and [here](https://eprint.iacr.org/2020/399.pdf).

## What it can do?

- Broadcast customized G+A exposure notification advertisments
- Capture and dump notifications broadcasted by nearby devices
- Track devices using exposure notifications

## How device tracking works?

Basically it uses two metods. Assuming that tracked device is never going
out of receiver's range it can classify traffic to device based on factors like :

  - when devices changes broadcasted data
  - how frequent it sends data
  - when it has been received

Second technique is based on tracking device BT address, broadcasted along with
EN data. Contrary to what is written in [specification](https://covid19-static.cdn-apple.com/applications/covid19/current/static/contact-tracing/pdf/ExposureNotification-BluetoothSpecificationv1.2.pdf), devices uses Resolvable Random Private addreses with fixed interval changes.

So what does that means? It means that if you pair your phone with another device,
that devices can get your Identity Resolution key (IRK), which allows to track
your changing BT address.

### How to get the IRK key?

The easiest way is to pair your device with Windows or Linux machine.

Then on windows download [psexec](https://docs.microsoft.com/en-us/sysinternals/downloads/psexec) and run regedit as System account:

```
PsExec.exe -s -i reg export HKLM\SYSTEM\CurrentControlSet\Services\BTHPORT\Parameters\Keys c:\bt_keys.reg /Y
find "IRK" "C:\bt_keys.reg"
```

For Linux use command similar to :

```
# sed -ne '/IdentityResolvingKey/ { n ; p }' /var/lib/bluetooth/*/*/info
```

## Prerequisites

Linux system and cheap USB dongle with support for Bluetooth 4.0 or never.

Install some non-standard packages:

  - Debian like systems
```
# apt install gcc make libreadline-dev libbluetooth-dev libssl-dev
```

## Compilation

```
$ make
```

## Usage:

Please keep in mind that it's work in progress, so expect major changes
with every commit. The best you can do is run the program and type "help"
then hit enter key:

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

Check if device random private address is resolvable against IRK key :

```
> resolve_rpa 4a:a0:d4:ff:c8:57 e2270523033eb8f92204cba9ea221cf3
Resolved
```

Adding captured IRK key to list :

```
> bonding some_name --irk e2270523033eb8f92204cba9ea221cf3
Name: some_name, IRK: e2270523033eb8f92204cba9ea221cf3
```

Scanning for EN data broadcasted by other devices (Ctrl-C to stop):

```
> scan
Scanning for Bluetooth Advertisement packets...
2020-06-13 14:49:35.705 - BD 01:02:03:04:05:06, RSSI -39, RPI: 12340000000000000000000000000000, AEM: 12345678
^C> 
```

Exporting scan result to CSV file, so you can load it at later time in pristine state :

```
> track --dump /tmp/bentool.csv
```

Process scanned data :

```
> track
```

Commands history is saved to .bthistory file if it exists.

## Author:
<adrian.brzezinski at adrb.pl>
