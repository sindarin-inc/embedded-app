#include "EPDWaveform.hpp"

#include <iostream>

WaveformInfo wfInfo;

static auto getCheckSum(const uint8_t *data, uint32_t len) -> uint16_t {
    uint16_t result = 0;
    result = *data;
    data++;
    result += *data;
    data += 3;
    len -= 4;

    while (len) {
        result += *data;
        data++;
        len--;
    }
    return (0 - result);
}

auto EpdWfInit(const uint8_t *data) -> uint32_t {
    uint16_t checkSum;

    wfInfo.available = 0;
    wfInfo.header = (const WaveformHeader *)data;

    if (wfInfo.header->version <= 3) {
        wfInfo.temperatureTable = (const uint8_t *)(data + sizeof(WaveformHeaderV3));
    } else if (wfInfo.header->version == 4) {
        wfInfo.temperatureTable = (const uint8_t *)(data + sizeof(WaveformHeaderV4));
    } else {
        wfInfo.temperatureTable = (const uint8_t *)(data + sizeof(WaveformHeader));
    }

    wfInfo.temperatureItemSize = 4 + 4 * wfInfo.header->modeCount;

    if (wfInfo.header->modeCount == 0) {
        return 0;
    }

    if (wfInfo.header->temperatureCount == 0) {
        return 0;
    }

    checkSum = getCheckSum(data, wfInfo.header->dataSize);
    if (checkSum != wfInfo.header->checkSum) {
        std::cout << "CheckSum error. Calculated: " << std::hex << checkSum
                  << " Expected: " << wfInfo.header->checkSum << std::endl;
        return 0;
    }

    if (wfInfo.header->version > MAX_WAVEFORM_VERSION) {
        return 0;
    }

    wfInfo.available = 1;
    return 1;
}
