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
make setup
```

Create the local credentials file before compiling:

```sh
cp src/config.local.example.h src/config.local.h
```

Edit `src/config.local.h` with the Wi-Fi and MQTT settings for this controller.
That file is intentionally ignored by Git.

It also defines the local installation topology: MQTT topics, every zone's RF
remote ID, blind count, and travel times, plus each controller's MAC address,
name, and owned zones. An ESP32 whose MAC is absent from this map can connect
to the network but cannot control blinds.

## Commands

```sh
make build
make flash
make monitor
make boards
make test
```

`make setup` installs the project-pinned ESP32 board support and required
libraries. Run it once on a new computer or after clearing Arduino CLI data.

`make build` compiles the normal RF-transmitting firmware but does not change
the connected controller.

`make flash` builds and installs the normal firmware. It is the only routine
command that enables actual RF control. If the serial port changes, override
it explicitly:

```sh
make flash PORT=/dev/cu.usbmodem02
```

`make monitor` opens the controller's serial console at 115200 baud, matching
the firmware's logging speed. Stop it with `Ctrl-C` before flashing.

`make boards` lists attached serial devices to help identify the controller's
port.

`make test` runs the fast host-side tests for RF profiles, queue behaviour,
and command validation. It does not require a controller.

## Safe RF dry run

Use the dry-run build to verify command handling and RF profile logs without
calling the radio write path. It is safer than using a different radio channel,
which would still broadcast.

```sh
make build-dry-run
make flash-dry-run
```

The dry-run firmware remains non-transmitting until it is replaced with a
normal `make flash` build.

`make build-dry-run` compiles the same firmware with RF writes disabled.
`make flash-dry-run` installs that non-transmitting build and runs its safe
start/stop profile self-test after boot, so it is appropriate for bench and
serial-log verification.

## Connectivity recovery

The controller treats the live MQTT connection as its service-health check;
there is no separate ICMP ping. Wi-Fi and MQTT retries begin at two seconds
and back off to one minute. After five minutes without Wi-Fi or MQTT, it resets
the Wi-Fi radio; after 30 minutes without recovery, it reboots the controller.

## Credentials

The prior source file stored network and MQTT credentials in plain text. Treat
those values as exposed: rotate them, then place replacements only in
`config.local.h`.
