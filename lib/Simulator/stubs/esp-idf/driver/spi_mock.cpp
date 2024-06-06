#include "spi_mock.hpp"

std::unordered_map<spi_host_device_t, pollingTransmitCallback> pollingTransmitCallbacks;

void setSpiCallback(spi_host_device_t host_id, pollingTransmitCallback callback) {
    pollingTransmitCallbacks[host_id] = callback;
}
