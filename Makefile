all: build

.PHONY: sim
sim:
	pio run -e simulator -t upload

.PHONY: lldb
lldb:
	pio run -e simulator && lldb .pio/build/simulator/program

.PHONY: test
test:
	pio run -e tests -t upload

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
	idf.py flash

.PHONY: monitor
monitor: check_idf
	# Try to find the right serial port and use --no-reset by default
	@if [[ "$$(uname)" == "Linux" ]]; then \
		PORT=$$(udevadm info --query=all --name=/dev/ttyUSB* | grep 'ID_MODEL_FROM_DATABASE=USB JTAG/serial debug unit' -B4 | grep 'DEVNAME' | cut -d'=' -f2) ; \
	elif [[ "$$(uname)" == "Darwin" ]]; then \
		PORT=$$(ls /dev/cu.usb* | head -n1) ; \
	else \
		echo "Unsupported operating system." ; \
		exit 1 ; \
	fi ; \
	if [[ -z "$$PORT" ]]; then \
		echo "Error: Could not find a serial port with description 'USB JTAG/serial debug unit'." ; \
		exit 1 ; \
	fi ; \
	idf.py monitor --no-reset --port $$PORT

.PHONY: clean
clean:
	rm -fr build

.PHONY: clang-tidy
clang-tidy: check_idf
	@if ! command -v clang-tidy &> /dev/null || [[ $$(which clang-tidy) != *"esp-clang"* ]]; then \
		echo "clang-tidy not found. Installing it now:" ; \
		idf_tools.py install esp-clang ; \
		pip install --upgrade pyclang ; \
		source $$IDF_PATH/export.sh ; \
	fi
	idf.py clang-check --run-clang-tidy-py scripts/run-clang-tidy.py --run-clang-tidy-options "-fix -header-filter=embedded-app/src/.*" --exclude-paths managed_components --exclude-paths components
