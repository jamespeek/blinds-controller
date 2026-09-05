ARDUINO_CLI ?= arduino-cli
ARDUINO_CONFIG := $(CURDIR)/arduino-cli.yaml
SKETCH := $(CURDIR)/src
FQBN := esp32:esp32:esp32s2:CDCOnBoot=cdc,UploadMode=cdc
PORT ?= /dev/cu.usbmodem01
BUILD_DIR := $(CURDIR)/.arduino-build
CAPTURE_CHANNEL ?= 52
ESP32_CORE_VERSION := 3.3.7
RF24_VERSION := 1.4.11
PUBSUBCLIENT_VERSION := 2.8.0
RELEASE_VERSION := $(strip $(shell tr -d '\n' < VERSION))
GIT_COMMIT := $(shell git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)
GIT_DIRTY := $(shell test -z "$$(git status --porcelain 2>/dev/null)" || echo -dirty)
FIRMWARE_VERSION ?= $(RELEASE_VERSION)+$(GIT_COMMIT)$(GIT_DIRTY)
FIRMWARE_VERSION_FLAG := -DFIRMWARE_VERSION=\"$(FIRMWARE_VERSION)\"

.PHONY: setup build build-dry-run capture-build channel-capture-build flash flash-dry-run capture-flash channel-capture-flash monitor boards test

setup:
	@command -v $(ARDUINO_CLI) >/dev/null || { echo "Arduino CLI is required. Install it with: brew install arduino-cli"; exit 1; }
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) core update-index
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) core install esp32:esp32@$(ESP32_CORE_VERSION)
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) lib install "RF24@$(RF24_VERSION)"
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) lib install "PubSubClient@$(PUBSUBCLIENT_VERSION)"

build:
	@test -f $(SKETCH)/config.local.h || { echo "Missing config.local.h. Copy config.local.example.h and fill it in."; exit 1; }
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) compile --clean --fqbn $(FQBN) --build-property compiler.cpp.extra_flags=$(FIRMWARE_VERSION_FLAG) --build-path $(BUILD_DIR) $(SKETCH)

build-dry-run:
	@test -f $(SKETCH)/config.local.h || { echo "Missing config.local.h. Copy config.local.example.h and fill it in."; exit 1; }
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) compile --clean --fqbn $(FQBN) --build-property compiler.cpp.extra_flags="-DRF_DRY_RUN=1 $(FIRMWARE_VERSION_FLAG)" --build-path $(BUILD_DIR)-dry-run $(SKETCH)

capture-build:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) compile --clean --fqbn $(FQBN) --build-path $(BUILD_DIR)-capture tools/rf_capture

capture-flash: capture-build
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) upload --fqbn $(FQBN) --port $(PORT) --build-path $(BUILD_DIR)-capture tools/rf_capture

channel-capture-build:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) compile --clean --fqbn $(FQBN) --build-property compiler.cpp.extra_flags=-DCAPTURE_CHANNEL=$(CAPTURE_CHANNEL) --build-path $(BUILD_DIR)-channel-capture-$(CAPTURE_CHANNEL) tools/rf_channel_capture

channel-capture-flash: channel-capture-build
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) upload --fqbn $(FQBN) --port $(PORT) --build-path $(BUILD_DIR)-channel-capture-$(CAPTURE_CHANNEL) tools/rf_channel_capture

flash: build
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) upload --fqbn $(FQBN) --port $(PORT) --build-path $(BUILD_DIR) $(SKETCH)

flash-dry-run: build-dry-run
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) upload --fqbn $(FQBN) --port $(PORT) --build-path $(BUILD_DIR)-dry-run $(SKETCH)

test:
	c++ -std=c++17 -Wall -Wextra -Werror tests/test_rf_profiles.cpp -o /tmp/blinds-rf-profile-tests
	/tmp/blinds-rf-profile-tests
	c++ -std=c++17 -Wall -Wextra -Werror tests/test_rf_gesture.cpp -o /tmp/blinds-rf-gesture-tests
	/tmp/blinds-rf-gesture-tests
	c++ -std=c++17 -Wall -Wextra -Werror tests/test_rf_remote_frame.cpp -o /tmp/blinds-rf-remote-frame-tests
	/tmp/blinds-rf-remote-frame-tests
	c++ -std=c++17 -Wall -Wextra -Werror tests/test_remote_motion.cpp -o /tmp/blinds-remote-motion-tests
	/tmp/blinds-remote-motion-tests
	c++ -std=c++17 -Wall -Wextra -Werror -Itests/fakes tests/test_queue.cpp src/queue.cpp -o /tmp/blinds-queue-tests
	/tmp/blinds-queue-tests
	c++ -std=c++17 -Wall -Wextra -Werror tests/test_command_validation.cpp -o /tmp/blinds-command-validation-tests
	/tmp/blinds-command-validation-tests
	c++ -std=c++17 -Wall -Wextra -Werror -Itests/fakes tests/test_post_stop_guard.cpp -o /tmp/blinds-post-stop-guard-tests
	/tmp/blinds-post-stop-guard-tests
	c++ -std=c++17 -Wall -Wextra -Werror tests/test_rf_command_gate.cpp -o /tmp/blinds-rf-command-gate-tests
	/tmp/blinds-rf-command-gate-tests
	c++ -std=c++17 -Wall -Wextra -Werror tests/test_stop_retry.cpp -o /tmp/blinds-stop-retry-tests
	/tmp/blinds-stop-retry-tests

monitor:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) monitor --port $(PORT) --config baudrate=115200

boards:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) board list
