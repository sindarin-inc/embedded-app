// Mocked stuff
#include <functional>
#include <unordered_map>

#include "driver/spi_master.h"

using pollingTransmitCallback =
    std::function<esp_err_t(spi_device_handle_t handle, spi_transaction_t *trans_desc)>;

extern std::unordered_map<spi_host_device_t, pollingTransmitCallback> pollingTransmitCallbacks;

void setSpiCallback(spi_host_device_t host_id, pollingTransmitCallback callback);
