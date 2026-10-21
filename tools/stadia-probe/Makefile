ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is required; use bash tools/build-switch.sh -C tools/stadia-probe)
endif

CXX := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-g++
CXXFLAGS := -std=c++20 -D_DEFAULT_SOURCE -O2 -g -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
            -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE \
            -isystem $(DEVKITPRO)/libnx/include \
            -I../../mc_mitm/source/bluetooth_mitm/bluetooth
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs -L$(DEVKITPRO)/libnx/lib

all: package/switch/stadia-ble-probe.nro

build/stadia-ble-probe.elf: source/main.cpp source/managed_stadia.inc source/settled_stadia.inc source/managed_control.inc source/managed_discovery.inc source/discovery_request_probe.inc ../../mc_mitm/source/bluetooth_mitm/bluetooth/ble_scan_request_progress.hpp source/module_identity.inc source/module_build_id.hpp ../../mc_mitm/source/bluetooth_mitm/bluetooth/ble_event_progress.hpp ../../mc_mitm/source/bluetooth_mitm/bluetooth/ble_cached_scan_protocol.hpp source/discovery_evidence.hpp source/target_address.hpp source/scan_uuid.hpp source/start_gate.hpp source/control_evidence.hpp source/console_command.hpp source/sd_file_policy.hpp source/sd_file_session.hpp source/bluetooth_code_export.hpp source/connection_client.hpp source/connection_trace_client.hpp ../../mc_mitm/source/bluetooth_mitm/bluetooth/ble_connection_trace_protocol.hpp ../../mc_mitm/source/bluetooth_mitm/bluetooth/ble_connection_state.hpp ../../mc_mitm/source/bluetooth_mitm/bluetooth/stadia_ble_scan_filter.hpp ../../mc_mitm/source/bluetooth_mitm/bluetooth/ble_connection_probe_protocol.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) -lnx

build/stadia-ble-probe.nacp: Makefile
	@mkdir -p build
	nacptool --create "Stadia BLE Probe" "MissionControl contributors" "0.6.33" $@

package/switch/stadia-ble-probe.nro: build/stadia-ble-probe.elf build/stadia-ble-probe.nacp
	@mkdir -p package/switch
	elf2nro $< $@ --nacp=build/stadia-ble-probe.nacp

.PHONY: all
