SOURCES = \
	TConTestMain.cpp \
	../../lib/swtcon/EPD_Driver.cpp \
	../../lib/swtcon/EPD_TCON.cpp \
	../../lib/swtcon/EPD_Waveform.cpp \
	../../lib/swtcon/waveform_data.cpp

TConTest: $(SOURCES)
	clang -I ../../lib/swtcon $(SOURCES) -std=c++17 -Dregister= -o $@

.PHONY: clean
clean:
	rm -f TConTest
