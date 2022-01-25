#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "EPD_Waveform.h"

WaveformInfo _wf_info;

static uint16_t GetCheckSum(const uint8_t *data, uint32_t len)
{
    uint16_t result = 0;
    result = *data;
    data++;
    result += *data;
    data += 3;
    len -= 4;

    while (len)
    {
        result += *data;
        data++;
        len--;
    }
    return (0 - result);
}

uint16_t EPD_WF_Version(void)
{
    return _wf_info.Header->Version;
}

uint32_t EPD_WF_DataSize(void)
{
    return _wf_info.Header->DataSize;
}

uint8_t EPD_WF_SrcDataMode(void)
{
    return _wf_info.Header->srcDataMode;
}

uint32_t EPD_WF_Init(const uint8_t *data)
{
    uint16_t checkSum;

    _wf_info.Available = 0;
    _wf_info.Header = (const WaveformHeader *)data;

    if (_wf_info.Header->Version <= 3)
        _wf_info.TemperatureTable = (const uint8_t *)(data + sizeof(WaveformHeaderV3));
    else if (_wf_info.Header->Version == 4)
        _wf_info.TemperatureTable = (const uint8_t *)(data + sizeof(WaveformHeaderV4));
    else
        _wf_info.TemperatureTable = (const uint8_t *)(data + sizeof(WaveformHeader));

    _wf_info.TemperatureItemSize = 4 + 4 * _wf_info.Header->ModeCount;

    if (_wf_info.Header->ModeCount == 0)
        return 0;

    if (_wf_info.Header->TemperatureCount == 0)
        return 0;

    checkSum = GetCheckSum(data, _wf_info.Header->DataSize);
    if (checkSum != _wf_info.Header->CheckSum)
        return 0;

    if (_wf_info.Header->Version > MAX_WAVEFORM_VERSION) //kevint:20150413:add wf version check
        return 0;

    _wf_info.Available = 1;
    return 1;
}

uint8_t EPD_WF_GetBpp(void)
{
    if (!_wf_info.Available)
        return 0;
    return _wf_info.Header->Bpp;
}

const WaveformDataBlock *EPD_WF_GetData(int16_t inTemperature, uint8_t mode)
{
    const TemperatureItem *tempItem;
    int16_t temperature = inTemperature;

    uint32_t i;
    uint32_t offset;

    int16_t abst;
    int16_t mindiff = 256, tidx = 0;

    if (!_wf_info.Available)
        return NULL;

    if (_wf_info.Header->TemperatureCount == 0)
        return NULL;

    if (mode >= _wf_info.Header->ModeCount)
        mode = _wf_info.Header->ModeCount - 1;

    tempItem = (const TemperatureItem *)(_wf_info.TemperatureTable);

    if ((inTemperature >= _wf_info.Header->TemperatureBiasStart) &&
        (inTemperature <= _wf_info.Header->TemperatureBiasEnd))
        temperature = inTemperature + _wf_info.Header->TemperatureBiasLUT[inTemperature - _wf_info.Header->TemperatureBiasStart];
    else
        temperature = inTemperature;

    if (temperature <= tempItem->Temperautre)
    {
        offset = (&tempItem->DataAddress)[mode];
        return (const WaveformDataBlock *)((const uint8_t *)_wf_info.Header + offset);
    }

    mindiff = 256;
    for (i = 0; i < _wf_info.Header->TemperatureCount; ++i)
    {
        tempItem = (const TemperatureItem *)(_wf_info.TemperatureTable + _wf_info.TemperatureItemSize * i);

        abst = temperature - tempItem->Temperautre;
        if (abst < 0)
            abst = -abst;
        if (abst < mindiff)
        {
            mindiff = abst;
            tidx = i;
        }
    }

    tempItem = (const TemperatureItem *)(_wf_info.TemperatureTable + _wf_info.TemperatureItemSize * (tidx));

    offset = (&tempItem->DataAddress)[mode];
    return (const WaveformDataBlock *)((const uint8_t *)_wf_info.Header + offset);
}
