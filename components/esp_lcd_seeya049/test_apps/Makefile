IDF_PROFILE = -B build-esp32p4 -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32p4"

build:
	idf.py $(IDF_PROFILE) build

run: build
	idf.py $(IDF_PROFILE) flash monitor

clean:
	rm -rf build
