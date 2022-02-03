SOURCES = \
	main.cpp \
	EPDWaveform.cpp

HEADERS = \
	EPDWaveform.hpp \
	Waveforms.hpp

waveformtool: $(SOURCES) $(HEADERS)
	clang++ $(SOURCES) -std=c++17 -o $@

.PHONY: clean
clean:
	rm -f waveformtool
