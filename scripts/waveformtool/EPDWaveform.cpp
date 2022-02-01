#include "EPDWaveform.hpp"

WaveformInfo _wf_info;

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

    _wf_info.available = 0;
    _wf_info.header = (const WaveformHeader *)data;

    if (_wf_info.header->version <= 3) {
        _wf_info.temperatureTable = (const uint8_t *)(data + sizeof(WaveformHeaderV3));
    } else if (_wf_info.header->version == 4) {
        _wf_info.temperatureTable = (const uint8_t *)(data + sizeof(WaveformHeaderV4));
    } else {
        _wf_info.temperatureTable = (const uint8_t *)(data + sizeof(WaveformHeader));
    }

    _wf_info.temperatureItemSize = 4 + 4 * _wf_info.header->modeCount;

    if (_wf_info.header->modeCount == 0) {
        return 0;
    }

    if (_wf_info.header->temperatureCount == 0) {
        return 0;
    }

    checkSum = getCheckSum(data, _wf_info.header->dataSize);
    if (checkSum != _wf_info.header->checkSum) {
        return 0;
    }

    if (_wf_info.header->version > MAX_WAVEFORM_VERSION) {
        return 0;
    }

    _wf_info.available = 1;
    return 1;
}
