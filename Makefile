ARDUINO_CLI ?= arduino-cli
ARDUINO_CONFIG := $(CURDIR)/arduino-cli.yaml
SKETCH := $(CURDIR)/firmware
FQBN := esp32:esp32:esp32s2:CDCOnBoot=cdc,UploadMode=cdc
PORT ?= /dev/cu.usbmodem01
BUILD_DIR := $(CURDIR)/.arduino-build
ESP32_CORE_VERSION := 3.3.7
RF24_VERSION := 1.4.11
PUBSUBCLIENT_VERSION := 2.8.0

.PHONY: firmware-setup firmware-build firmware-build-dry-run firmware-flash firmware-flash-dry-run firmware-monitor firmware-boards firmware-test

firmware-setup:
	@command -v $(ARDUINO_CLI) >/dev/null || { echo "Arduino CLI is required. Install it with: brew install arduino-cli"; exit 1; }
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) core update-index
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) core install esp32:esp32@$(ESP32_CORE_VERSION)
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) lib install "RF24@$(RF24_VERSION)"
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) lib install "PubSubClient@$(PUBSUBCLIENT_VERSION)"

firmware-build:
	@test -f $(SKETCH)/config.local.h || { echo "Missing config.local.h. Copy config.local.example.h and fill it in."; exit 1; }
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) compile --clean --fqbn $(FQBN) --build-path $(BUILD_DIR) $(SKETCH)

firmware-build-dry-run:
	@test -f $(SKETCH)/config.local.h || { echo "Missing config.local.h. Copy config.local.example.h and fill it in."; exit 1; }
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) compile --clean --fqbn $(FQBN) --build-property compiler.cpp.extra_flags=-DRF_DRY_RUN=1 --build-path $(BUILD_DIR)-dry-run $(SKETCH)

firmware-flash: firmware-build
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) upload --fqbn $(FQBN) --port $(PORT) --build-path $(BUILD_DIR) $(SKETCH)

firmware-flash-dry-run: firmware-build-dry-run
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) upload --fqbn $(FQBN) --port $(PORT) --build-path $(BUILD_DIR)-dry-run $(SKETCH)

firmware-test:
	c++ -std=c++17 -Wall -Wextra -Werror tests/test_rf_profiles.cpp -o /tmp/blinds-rf-profile-tests
	/tmp/blinds-rf-profile-tests
	c++ -std=c++17 -Wall -Wextra -Werror -Itests/fakes tests/test_queue.cpp firmware/queue.cpp -o /tmp/blinds-queue-tests
	/tmp/blinds-queue-tests
	c++ -std=c++17 -Wall -Wextra -Werror tests/test_command_validation.cpp -o /tmp/blinds-command-validation-tests
	/tmp/blinds-command-validation-tests

firmware-monitor:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) monitor --port $(PORT) --config baudrate=115200

firmware-boards:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) board list
