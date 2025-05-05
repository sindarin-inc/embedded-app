all: build

# Use all but 1 core for building
NUM_CORES := $(shell nproc)
NUM_CORES := $(shell echo $$(( $(NUM_CORES) - 1 )))
NUM_CORES := $(shell [ $(NUM_CORES) -lt 1 ] && echo 1 || echo $(NUM_CORES))

MAKE := nice make -j$(NUM_CORES)
CMAKE_FLAGS = -DCMAKE_BUILD_TYPE=Debug
# This can be useful for testing the GCC build (used by CI)
# CMAKE_FLAGS = -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/opt/homebrew/opt/gcc/bin/gcc-14 -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/gcc/bin/g++-14

# Set the TARGET to glasses-debug if unset
TARGET ?= glasses-debug

# For the M5Stack targets:
# You'll also need to uncomment the M5GFX component in src/CMakeLists.txt and also clone M5GFX:
#   clone -b john/atom-display git@github.com:sindarin-inc/M5GFX.git components/M5GFX
# If we end up wanting to use these targets more, we could add M5GFX as a submodule.

# List of valid targets
VALID_TARGETS := glasses-release glasses-debug v2pod-release tester m5stack-atom-display m5stack-module-display waveshare-6inhd sharp-27in p4-function-ev p4-function-ev-r0

# Verify TARGET is valid
ifeq ($(filter $(TARGET),$(VALID_TARGETS)),)
    $(error Unsupported TARGET: $(TARGET). Supported targets are: $(VALID_TARGETS))
endif

# Special handling for P4 targets that need additional sdkconfig defaults
P4_TARGETS := p4-function-ev p4-function-ev-r0
ifneq ($(filter $(TARGET),$(P4_TARGETS)),)
    SDKCONFIG_DEFAULTS := sdkconfig.defaults.esp32-p4;sdkconfig.defaults.$(TARGET)
else ifeq ($(TARGET),waveshare-6inhd)
    SDKCONFIG_DEFAULTS := sdkconfig.defaults.$(TARGET)
else ifeq ($(TARGET),sharp-27in)
    SDKCONFIG_DEFAULTS := sdkconfig.defaults.$(TARGET)
else
    SDKCONFIG_DEFAULTS := sdkconfig.defaults;sdkconfig.defaults.$(TARGET)
endif

# Set build profile
IDF_PROFILE = -B build-$(TARGET) -DSDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" -DBUILD_TARGET="$(TARGET)"

# Optional variables for the Simulator builds
CMAKE_SIM_DISPLAY_OPTIONS := $(if $(DISPLAY_WIDTH),-DDISPLAY_WIDTH=$(DISPLAY_WIDTH)) $(if $(DISPLAY_HEIGHT),-DDISPLAY_HEIGHT=$(DISPLAY_HEIGHT)) $(if $(DISPLAY_SCALE),-DDISPLAY_SCALE=$(DISPLAY_SCALE)) $(if $(SIM_LAYOUT_DEBUG),-DSIM_LAYOUT_DEBUG=$(SIM_LAYOUT_DEBUG)) $(if $(DISPLAY_TYPE),-DDISPLAY_TYPE=$(DISPLAY_TYPE)) $(if $(TTF_SCREEN_RES_PER_INCH),-DTTF_SCREEN_RES_PER_INCH=$(TTF_SCREEN_RES_PER_INCH))

.PHONY: sim-build
sim-build:
	mkdir -p lib/Simulator/build
	cd lib/Simulator/build && cmake $(CMAKE_FLAGS) -DSIM_SCALE=2 $(CMAKE_SIM_DISPLAY_OPTIONS) .. && $(MAKE)

.PHONY: sim-run
sim: sim-build
	./lib/Simulator/build/simulator

.PHONY: sim8-build
sim8-build:
	mkdir -p lib/Simulator/build8
	cd lib/Simulator/build8 && cmake $(CMAKE_FLAGS) -DDISPLAY_TYPE=DISPLAY_SIM_8BIT -DDISPLAY_WIDTH=600 -DDISPLAY_HEIGHT=600 $(CMAKE_SIM_DISPLAY_OPTIONS) .. && $(MAKE)

.PHONY: sim-run8
sim8: sim8-build
	./lib/Simulator/build8/simulator

.PHONY: sim16-build
sim16-build:
	mkdir -p lib/Simulator/build16
	cd lib/Simulator/build16 && cmake $(CMAKE_FLAGS) -DDISPLAY_TYPE=DISPLAY_SIM_16BIT -DDISPLAY_WIDTH=600 -DDISPLAY_HEIGHT=600 $(CMAKE_SIM_DISPLAY_OPTIONS) .. && $(MAKE)

.PHONY: sim-run16
sim16: sim16-build
	./lib/Simulator/build16/simulator

.PHONY: sim24-build
sim24-build:
	mkdir -p lib/Simulator/build24
	cd lib/Simulator/build24 && cmake $(CMAKE_FLAGS) -DDISPLAY_TYPE=DISPLAY_SIM_24BIT -DDISPLAY_WIDTH=600 -DDISPLAY_HEIGHT=600 $(CMAKE_SIM_DISPLAY_OPTIONS) .. && $(MAKE)

.PHONY: sim-run24
sim24: sim24-build
	./lib/Simulator/build24/simulator

.PHONY: lldb
lldb: sim-build
	lldb lib/Simulator/build/simulator

.PHONY: lldb8
lldb8: sim8-build
	lldb lib/Simulator/build8/simulator

.PHONY: test-build
test-build:
	mkdir -p test/CatchTests/build
	cd test/CatchTests/build && cmake $(CMAKE_FLAGS) .. && $(MAKE)

.PHONY: test
test: test-build
	./test/CatchTests/build/tests
	./test/CatchTests/build/tests '[epub]'
	./test/CatchTests/build/tests '[content-sync]'
	./test/CatchTests/build/tests '[telemetry]'

.PHONY: test-content-sync
test-content-sync: test-build
	./test/CatchTests/build/tests '[content-sync]'

.PHONY: test-lldb
test-lldb: test-build
	lldb ./test/CatchTests/build/tests

.PHONY: test-epub-lldb
test-epub-lldb: test-build
	lldb ./test/CatchTests/build/tests '[epub]'

.PHONY: epub-render-build
epub-render-build:
	mkdir -p scripts/epub-render/build
	cd scripts/epub-render/build && cmake $(CMAKE_FLAGS) .. && $(MAKE)

.PHONY: epub-render
epub-render: epub-render-build
	./scripts/epub-render/build/epub-render

.PHONY: check_idf
check_idf:
	@if ! idf.py --version >/dev/null 2>&1; then \
		echo "idf.py not found. Searching your history for a possible command:" ; \
		cat ~/.zsh_history | awk -F';' '{print $$2}' | grep '^source .*idf.*export.sh' | tail -n 1 ; \
		exit 1 ; \
	fi

# Only runs if build/compile_commands.json isn't available
.PHONY: configure
configure: check_idf
	@if [[ ! -f build/compile_commands.json ]]; then \
		idf.py $(IDF_PROFILE) set-target esp32s3 ; \
	fi

.PHONY: menuconfig
menuconfig: check_idf
	idf.py $(IDF_PROFILE) menuconfig

.PHONY: save-defconfig
save-defconfig: check_idf
	idf.py $(IDF_PROFILE) save-defconfig

.PHONY: size-components
size-components: check_idf
	idf.py $(IDF_PROFILE) size-components

.PHONY: size-files
size-files: check_idf
	idf.py $(IDF_PROFILE) size-files

.PHONY: size
size: check_idf
	idf.py $(IDF_PROFILE) size

.PHONY: build
build: check_idf
	idf.py $(IDF_PROFILE) build

.PHONY: flash
flash: build
ifeq ($(TARGET),p4-function-ev)
	idf.py $(IDF_PROFILE) flash
else ifeq ($(TARGET),p4-function-ev-r0)
	idf.py $(IDF_PROFILE) flash
else
	idf.py $(IDF_PROFILE) flash --port $$(python3 scripts/port_tool.py find_port programming)
endif

.PHONY: monitor
monitor: check_idf
ifeq ($(TARGET),p4-function-ev)
	idf.py $(IDF_PROFILE) monitor
else ifeq ($(TARGET),p4-function-ev-r0)
	idf.py $(IDF_PROFILE) monitor
else
	idf.py $(IDF_PROFILE) monitor --no-reset --port $$(python3 scripts/port_tool.py find_port logging)
endif

.PHONY: monitor-jtag
monitor-jtag: check_idf
	idf.py $(IDF_PROFILE) monitor --no-reset

.PHONY: monitor-devboard
monitor-devboard: check_idf
	idf.py $(IDF_PROFILE) monitor --no-reset --port $$(python3 scripts/port_tool.py find_port devboard)

.PHONY: ios-framework
ios-framework:
	cd lib/Simulator/CommonIos && make build

.PHONY: clean
clean:
	rm -fr build*
	rm -fr lib/Simulator/build*
	rm -fr scripts/epub-render/build
	rm -fr test/CatchTests/build
	rm -fr test/CatchTests/Images/TestRun/*diff.png
	rm -fr test/CatchTests/Images/TestRun/GylphTests/*diff.png
	rm -fr lib/Simulator/CommonIos/build*

.PHONY: clang-tidy
clang-tidy: check_idf
	@if ! clang-tidy --version >/dev/null 2>&1 || [[ $$(which clang-tidy) != *"esp-clang"* ]]; then \
		echo "clang-tidy not found. Installing it now:" ; \
		idf_tools.py install esp-clang ; \
		pip install --upgrade pyclang ; \
		echo "Clang-tidy should now be installed. Please reload your IDF environment with:" ; \
		echo ". $$IDF_PATH/export.sh" ; \
		exit 1 ; \
	fi
	IDF_TOOLCHAIN=clang idf.py -B build-clang clang-check --run-clang-tidy-py scripts/run-clang-tidy.py --run-clang-tidy-options "-fix -header-filter=embedded-app/src/.*" --exclude-paths managed_components --exclude-paths components
	find src -iname "*.hpp" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.c" | xargs clang-format -i

.PHONY: clang-format
clang-format: check_idf
	find src -iname "*.hpp" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.c" | xargs clang-format -i

program-mode:
	python3 scripts/port_tool.py find_port programming

unpair-remote:
	esptool.py erase_region 0x9000 0x5000
