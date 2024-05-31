#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <driver/uart.h>
#include <esp_log.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>

#if SIMULATOR
#include "Storage/PosixFileSystem.hpp"
#else
#include "Storage/LittleFSFileSystem.hpp"
#endif

#define UART_TX_PIN 43 // 49
#define UART_RX_PIN 44 // 50

class FileTransfer {
public:
    FileTransfer(FileSystem &fileSystem) : fileSystem_(fileSystem){};
    void init(uart_port_t uart_num, int tx_pin, int rx_pin);
    void startTransfer();
    static void taskFunction(void *arg); // Static task function

private:
    uart_port_t uart_num_;
    int tx_pin_;
    int rx_pin_;
    FileSystem &fileSystem_;
    uint8_t calculateChecksum(const uint8_t *data, size_t length);
    static constexpr const char *TAG = "FileTransfer";
    static constexpr int BUF_SIZE = 1024;
    static constexpr int CONTROL_SEQUENCE_LEN = 20;
    static constexpr uint8_t CHECKSUM_INIT = 0;
    static constexpr const char *CONTROL_SEQUENCE = "FILE_TRANSFER_START";
};

#endif // FILE_TRANSFER_H