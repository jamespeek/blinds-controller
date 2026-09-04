# Blinds controller firmware

Arduino firmware for the ESP32-S2 blind controller. The target board is
**ESP32S2 Dev Module** with USB CDC enabled and **Upload Mode: Internal USB**
(`esp32:esp32:esp32s2:CDCOnBoot=cdc,UploadMode=cdc`). The currently connected
device is available at `/dev/cu.usbmodem01`.

## Hardware used

Each controller uses:

- A [DIANN ESP32 S2 Mini V1.0.0](https://www.amazon.com.au/dp/B0DHVBB6P2),
  based on the ESP32-S2FN4R2 and fitted with 4 MB flash, 2 MB PSRAM, and a
  USB-C port. Build it as an **ESP32S2 Dev Module**.
- An [nRF24L01+ PA+LNA 2.4 GHz radio module with SMA antenna and 8-pin
  breakout adapter](https://www.amazon.com.au/dp/B0CDV8J2WF) for transmitting
  blind commands.
- A USB data cable for power, programming, and serial monitoring.

The radio is connected to the ESP32-S2 Mini over SPI as follows:

| Radio signal | ESP32-S2 GPIO | Wire colour |
| --- | ---: | --- |
| CE | 9 | Purple |
| CSN / CS | 11 | Blue |
| SCK | 12 | Green |
| MOSI | 18 | Yellow |
| MISO | 16 | Orange |

The radio module itself requires 3.0–3.6 V (3.3 V recommended). The supplied
breakout adapter includes a 3.3 V regulator; follow its input-voltage label
rather than applying 5 V directly to the radio. Connect the ESP32 and radio
grounds, and check the breakout's pin labels before wiring it.

## One-time setup

Install the command-line tool, then install the project-pinned board core and
libraries:

```sh
brew install arduino-cli
make firmware-setup
```

Create the local credentials file before compiling:

```sh
cp firmware/config.local.example.h firmware/config.local.h
```

Edit `config.local.h` with the Wi-Fi and MQTT settings for this controller.
That file is intentionally ignored by Git.

It also defines the local installation topology: MQTT topics, every zone's RF
remote ID, blind count, and travel times, plus each controller's MAC address,
name, and owned zones. An ESP32 whose MAC is absent from this map can connect
to the network but cannot control blinds.

## Everyday commands

```sh
make firmware-build
make firmware-flash
make firmware-monitor
make firmware-boards
make firmware-test
```

`firmware-flash` is the only command that changes firmware on the device. If
the serial port changes, override it explicitly:

```sh
make firmware-flash PORT=/dev/cu.usbmodem02
```

## Safe RF dry run

Use the dry-run build to verify command handling and RF profile logs without
calling the radio write path. It is safer than using a different radio channel,
which would still broadcast.

```sh
make firmware-build-dry-run
make firmware-flash-dry-run
```

The dry-run firmware remains non-transmitting until it is replaced with a
normal `make firmware-flash` build.

The monitor uses a baud rate of 115200, which matches `Serial.begin(115200)`
in the sketch.

## Connectivity recovery

The controller treats the live MQTT connection as its service-health check;
there is no separate ICMP ping. Wi-Fi and MQTT retries begin at two seconds
and back off to one minute. After five minutes without Wi-Fi or MQTT, it resets
the Wi-Fi radio; after 30 minutes without recovery, it reboots the controller.

## Credentials

The prior source file stored network and MQTT credentials in plain text. Treat
those values as exposed: rotate them, then place replacements only in
`config.local.h`.
