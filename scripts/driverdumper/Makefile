SOURCES = \
	SPI.cpp \
	Arduino.cpp

SOURCES_NEW_DRIVER = \
	main-new.cpp \
	../../src/Displays/EinkET013TT1.cpp

SOURCES_OLD_DRIVER = \
	main-old.cpp \
	./swtcon/EPD_Driver.cpp \
	./swtcon/EPD_TCON.cpp \
	./swtcon/EPD_Waveform.cpp \
	./swtcon/SWTCON.cpp \
	./swtcon/waveform_data.cpp

HEADERS = \
	SPI.h \
	Arduino.h \
	esp32/clk.h

INCLUDES = \
	-I . \
	-I ../../src/

BINARY = driverdumper

driverdumper-new: $(SOURCES) $(SOURCES_NEW_DRIVER) $(HEADERS)
	clang++ $(INCLUDES) $(SOURCES) $(SOURCES_NEW_DRIVER) -DEINKET013TT1=1 -std=c++17 -Wno-gnu-designator -o $@

driverdumper-old: $(SOURCES) $(SOURCES_OLD_DRIVER) $(HEADERS)
	clang++ $(INCLUDES) -I ./swtcon $(SOURCES) $(SOURCES_OLD_DRIVER) -std=c++17 -Wno-gnu-designator -Dregister= -o $@

all: driverdumper-new driverdumper-old

.PHONY: clean
clean:
	rm -f driverdumper-new driverdumper-old
