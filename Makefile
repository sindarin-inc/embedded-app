.PHONY: sim
sim:
	pio run -e simulator -t upload

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

.PHONY: build
build: check_idf
	idf.py flash

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
