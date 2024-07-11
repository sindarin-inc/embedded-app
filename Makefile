all: build

# Use all but 1 core for building
NUM_CORES := $(shell nproc)
NUM_CORES := $(shell echo $$(( $(NUM_CORES) - 1 )))
NUM_CORES := $(shell [ $(NUM_CORES) -lt 1 ] && echo 1 || echo $(NUM_CORES))

MAKE := nice make -j$(NUM_CORES)
CMAKE_FLAGS = -DCMAKE_BUILD_TYPE=Debug

.PHONY: sim-build
sim-build:
	mkdir -p lib/Simulator/build
	cd lib/Simulator/build && cmake $(CMAKE_FLAGS) .. && $(MAKE)

.PHONY: sim-run
sim: sim-build
	./lib/Simulator/build/simulator

.PHONY: sim8-build
sim8-build:
	mkdir -p lib/Simulator/build8
	cd lib/Simulator/build8 && cmake $(CMAKE_FLAGS) -DENABLE_8BIT_DISPLAY=ON .. && $(MAKE)

.PHONY: sim-run8
sim8: sim8-build
	./lib/Simulator/build8/simulator

.PHONY: lldb
lldb: sim
	lldb lib/Simulator/build/simulator

.PHONY: lldb8
lldb8: sim8
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

.PHONY: epub-render-build
epub-render-build:
	mkdir -p scripts/epub-render/build
	cd scripts/epub-render/build && cmake $(CMAKE_FLAGS) .. && $(MAKE)

.PHONY: epub-render
epub-render: epub-render-build
	./scripts/epub-render/build/epub-render

.PHONY: check_idf
check_idf:
	@if ! command -v idf.py &> /dev/null ; then \
		echo "idf.py not found. Searching your history for a possible command:" ; \
		cat ~/.zsh_history | awk -F';' '{print $$2}' | grep '^source .*idf.*export.sh' | tail -n 1 ; \
		exit 1 ; \
	fi

# Only runs if build/compile_commands.json isn't available
.PHONY: configure
configure: check_idf
	@if [[ ! -f build/compile_commands.json ]]; then \
		idf.py set-target esp32s3 ; \
	fi

.PHONY: build
build: check_idf configure
	idf.py build

.PHONY: flash
flash: build
	if [ -e /dev/cu.usbmodemsolreader1 ]; then \
		echo "PROGRAM" | tee /dev/cu.usbmodemsolreader1; \
		sleep 2; \
	fi
	@if [[ "$$(uname)" == "Linux" ]]; then \
		PORT=$$(udevadm info --query=all --name=/dev/ttyUSB* | grep 'ID_MODEL_FROM_DATABASE=Sol Reader' -B4 | grep 'DEVNAME' | cut -d'=' -f2) ; \
		echo "PROGRAM" | tee $$PORT ; \
		sleep 2;\
	fi
	idf.py flash

.PHONY: monitor
monitor: check_idf
	# Turn logging on on the new USB port if it exists.
	if [ -e /dev/cu.usbmodemsolreader1 ]; then \
		echo "LOGGING_ON" | tee /dev/cu.usbmodemsolreader1; \
		sleep 2; \
	fi
	@if [[ "$$(uname)" == "Linux" ]]; then \
		PORT=$$(udevadm info --query=all --name=/dev/ttyUSB* | grep 'ID_MODEL_FROM_DATABASE=Sol Reader' -B4 | grep 'DEVNAME' | cut -d'=' -f2) ; \
	elif [[ "$$(uname)" == "Darwin" ]]; then \
		PORT=$$(ls /dev/cu.usbmodemsolreader3 | head -n1) ; \
	else \
		echo "Unsupported operating system." ; \
		exit 1 ; \
	fi ; \
	if [[ -z "$$PORT" ]]; then \
		echo "Error: Could not find a serial port with description 'Sol Reader'." ; \
		exit 1 ; \
	fi ; \
	idf.py monitor --no-reset --port $$PORT

.PHONY: clean
clean:
	rm -fr build
	rm -fr lib/Simulator/build
	rm -fr lib/Simulator/build8
	rm -fr scripts/epub-render/build
	rm -fr test/CatchTests/build

.PHONY: clang-tidy
clang-tidy: check_idf
	@if ! command -v clang-tidy &> /dev/null || [[ $$(which clang-tidy) != *"esp-clang"* ]]; then \
		echo "clang-tidy not found. Installing it now:" ; \
		idf_tools.py install esp-clang ; \
		pip install --upgrade pyclang ; \
		echo "Clang-tidy should now be installed. Please reload your IDF environment with:" ; \
		echo ". $$IDF_PATH/export.sh" ; \
		exit 1 ; \
	fi
	IDF_TOOLCHAIN=clang idf.py -B build-clang clang-check --run-clang-tidy-py scripts/run-clang-tidy.py --run-clang-tidy-options "-fix -header-filter=embedded-app/src/.*" --exclude-paths managed_components --exclude-paths components
	find src -iname "*.hpp" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.c" | xargs clang-format -i
