#include <algorithm>
#include <iostream>
#include <string>

#include "EPDWaveform.hpp"
#include "Waveforms.hpp"

#define log_d(format, ...) printf(format "\n", ##__VA_ARGS__)
// Separating these in case we want to write directly to a file at some point
#define out(format, ...) printf(format, ##__VA_ARGS__)

auto StringFromFrameBits(uint8_t bits) -> const char * {
    bits &= 0x3;
    switch (bits) {
    case 0x3:
        return "BLACK";
    case 0x2:
        return "WHITE";
    case 0:
        return "NOCHG";
    default:
        return "WAT??";
    }
}

auto StringFromMode(uint8_t mode) -> const char * {
    switch (mode) {
    case 0:
        return "CLEAR";
    case 1:
        return "FULL";
    case 2:
        return "PARTIAL";
    case 3:
        return "AU4";
    default:
        return "UNKNOWN";
    }
}

auto ToUpperUnderscore(const char *string) -> const char * {
    // This leaks, but yolo
    auto *str = new std::string(string);
    std::replace(str->begin(), str->end(), ' ', '_');
    std::replace(str->begin(), str->end(), '-', '_');
    std::transform(str->begin(), str->end(), str->begin(), ::toupper);
    return str->c_str();
}

auto main() -> int {
    auto numWaveforms = sizeof(waveforms) / sizeof(char *);
    // log_d("Found %lu waveforms", numWaveforms);

    for (int i = 0; i < numWaveforms; i++) {
        auto ret = EpdWfInit(waveforms[i]);
        if (ret == 0) {
            std::cerr << "Failed to initialize waveform";
            return 1;
        }
        auto name = waveformNames[i];
        // log_d("Processing waveform \"%s\"", name);

        for (int j = 0; j < _wf_info.header->temperatureCount; ++j) {
            auto tempItem = (const TemperatureItem *)(_wf_info.temperatureTable +
                                                      _wf_info.temperatureItemSize * j);
            for (int mode = 0; mode < _wf_info.header->modeCount; mode++) {
                auto offset = (&tempItem->dataAddress)[mode];
                auto waveform =
                    (const WaveformDataBlock *)((const uint8_t *)_wf_info.header + offset);

                out("const EinkET013TT1Waveform %s_WAVEFORM_%s = {\n", ToUpperUnderscore(name),
                    StringFromMode(mode));
                out("    \"%s (%s)\",\n", name, StringFromMode(mode));
                out("    %d,\n", waveform->frameCount);
                out("    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{\n");
                for (int i = 0; i < waveform->frameCount; i++) {
                    auto frame = (uint8_t *)(&waveform->data + 2 * i);
                    out("        Frame{");
                    auto val = (frame[0] & 0x3);
                    out("bb : %s, ", StringFromFrameBits(frame[0]));
                    out("bw : %s, ", StringFromFrameBits(frame[0] >> 2));
                    out("wb : %s, ", StringFromFrameBits(frame[1]));
                    out("ww : %s},\n", StringFromFrameBits(frame[1] >> 2));
                }
                out("    },\n");
                out("};\n\n");
            }
        }
    }

    return 0;
}
