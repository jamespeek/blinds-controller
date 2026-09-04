# Blinds controller firmware

## Why this exists

Hunter Douglas blinds can be controlled by remotes that use infrared and a
2.4 GHz RF protocol. This project makes the RF remote protocol available to
Home Assistant through an ESP32-S2, MQTT, and an nRF24L01+-compatible radio.
The RF payload and channel-hopping approach are based on this useful
[Arduino Forum investigation](https://forum.arduino.cc/t/nrf24l01-hack-on-hunter-douglas-shades/676940?page=2).

One installation can use multiple controllers. Each ESP32 is identified by
its Wi-Fi MAC address and is configured to own specific zones, so controllers
can be placed near the blinds they target while sharing the same MQTT topics.

## Hardware used

Each controller uses:

- An ESP32-S2 Mini development board with 4 MB flash, 2 MB PSRAM, and USB-C.
  Build it as an **ESP32S2 Dev Module**.
- An nRF24L01+-compatible 2.4 GHz radio module with a suitable 3.3 V breakout
  adapter for transmitting blind commands.
- A USB data cable for power, programming, and serial monitoring.

Specific items used in my installation:

- [DIANN ESP32 S2 Mini V1.0.0](https://www.amazon.com.au/dp/B0DHVBB6P2)
- [nRF24L01+ PA+LNA radio module with SMA antenna and 8-pin breakout adapter](https://www.amazon.com.au/dp/B0CDV8J2WF)

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

Copy and edit [`src/config.local.example.h`](src/config.local.example.h) as
`src/config.local.h` with the Wi-Fi and MQTT settings for this controller.
The local file is intentionally ignored by Git because it contains credentials.

It also defines the local installation topology: MQTT topics, every zone's RF
remote ID, blind count, and travel times, plus each controller's MAC address,
name, and owned zones. An ESP32 whose MAC is absent from this map can connect
to the network but cannot control blinds.

## Home Assistant

Configure each blind as an MQTT cover. Replace the example name, ID, zone, and
blind number to match a zone in your local configuration, based on
[`src/config.local.example.h`](src/config.local.example.h):

```yaml
mqtt:
  cover:
    - name: "Office top blind"
      unique_id: blind_front_1
      device_class: shade
      command_topic: "blinds/front/1/set"
      state_topic: "blinds/front/1/state"
      position_topic: "blinds/front/1/position"
      set_position_topic: "blinds/front/1/set"
```

Because the controller estimates position from configured travel times rather
than receiving blind feedback, mark the cover as assumed state:

```yaml
homeassistant:
  customize:
    cover.office_top_blind:
      assumed_state: true
```

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

## RF reliability tuning

RF timing is still being tuned for reliable reception across the installation.
Movement starts currently use two redundant send profiles, while STOP uses one
short channel-hop gesture to avoid a repeated opposite-direction command being
interpreted as a new movement. The resend count, timing, and train parameters
are grouped in `src/config.h`. Test any adjustment with `make flash-dry-run`
first, then make a small observed live-RF change rather than increasing every
retry at once.

## Connectivity recovery

The controller treats the live MQTT connection as its service-health check;
there is no separate ICMP ping. Wi-Fi and MQTT retries begin at two seconds
and back off to one minute. After five minutes without Wi-Fi or MQTT, it resets
the Wi-Fi radio; after 30 minutes without recovery, it reboots the controller.
