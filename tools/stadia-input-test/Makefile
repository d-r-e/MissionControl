ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is required; build with bash tools/build-switch.sh -C tools/stadia-input-test)
endif

CXX := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-g++
CXXFLAGS := -std=c++20 -D_DEFAULT_SOURCE -O2 -g -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
            -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE \
            -isystem $(DEVKITPRO)/libnx/include
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs -L$(DEVKITPRO)/libnx/lib

all: package/switch/stadia-input-test.nro

build/stadia-input-test.elf: source/main.cpp source/model.hpp source/ui.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) -lnx

build/stadia-input-test.nacp: Makefile
	@mkdir -p build
	nacptool --create "Stadia Input Lab" "MissionControl contributors" "1.0.1" $@

package/switch/stadia-input-test.nro: build/stadia-input-test.elf build/stadia-input-test.nacp
	@mkdir -p package/switch
	elf2nro $< $@ --nacp=build/stadia-input-test.nacp

.PHONY: all
