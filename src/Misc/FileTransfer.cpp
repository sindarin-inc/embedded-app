#include "FileTransfer.hpp"

#include <cstring>

void FileTransfer::init(uart_port_t uart_num, int tx_pin, int rx_pin) {
    uart_num_ = uart_num;
    tx_pin_ = tx_pin;
    rx_pin_ = rx_pin;

    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(uart_num_, &uart_config);
    uart_set_pin(uart_num_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(uart_num_, BUF_SIZE * 2, 0, 0, NULL, 0);
}

uint8_t FileTransfer::calculateChecksum(const uint8_t *data, size_t length) {
    uint8_t checksum = CHECKSUM_INIT;
    for (size_t i = 0; i < length; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

void FileTransfer::startTransfer() {
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    while (1) {
        int len = uart_read_bytes(uart_num_, data, CONTROL_SEQUENCE_LEN, 20 / portTICK_RATE_MS);
        if (len > 0 && strncmp((char *)data, CONTROL_SEQUENCE, CONTROL_SEQUENCE_LEN) == 0) {
            ESP_LOGI(TAG, "File transfer start detected");

            // Read the file name
            len = uart_read_bytes(uart_num_, data, BUF_SIZE, 20 / portTICK_RATE_MS);
            if (len <= 0) continue;
            data[len] = '\0';
            char *file_name = strtok((char *)data, "\n");

            // Read the file size
            len = uart_read_bytes(uart_num_, data, BUF_SIZE, 20 / portTICK_RATE_MS);
            if (len <= 0) continue;
            data[len] = '\0';
            char *file_size_str = strtok((char *)data, "\n");
            int file_size = atoi(file_size_str);

            ESP_LOGI(TAG, "File Name: %s", file_name);
            ESP_LOGI(TAG, "File Size: %d bytes", file_size);

            SFile fd = fileSystem_.open(StringFormat("/%s", file_name), "w", true);
            if (fd < 0) {
                ESP_LOGE(TAG, "Failed to open file for writing");
                continue;
            }

            int bytes_received = 0;
            uint8_t checksum = CHECKSUM_INIT;
            while (bytes_received < file_size) {
                len = uart_read_bytes(uart_num_, data, BUF_SIZE, 20 / portTICK_RATE_MS);
                if (len > 0) {
                    fd.write(data, len);
                    checksum = calculateChecksum(data, len);
                    bytes_received += len;
                }
            }

            close(fd);

            // Read and verify checksum
            len = uart_read_bytes(uart_num_, data, 1, 20 / portTICK_RATE_MS);
            if (len == 1 && data[0] == checksum) {
                ESP_LOGI(TAG, "File received and saved successfully: %s", file_name);
            } else {
                ESP_LOGE(TAG, "Checksum verification failed");
                // Unimplemented
                // fileSystem_.unlink(file_name); // Remove the corrupted file
            }
        }
    }
}

void FileTransfer::taskFunction(void *arg) {
    FileTransfer *instance = static_cast<FileTransfer *>(arg);
    instance->startTransfer();
}