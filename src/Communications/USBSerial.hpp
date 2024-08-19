#pragma once

#include <cstdint>
#include <cstring>
#include <esp_err.h>
#include <memory>
#include <tinyusb.h>

#if CONFIG_USE_TINYUSB_SERIAL
#include <esp_private/usb_phy.h>
#include <tusb.h>
#include <tusb_cdc_acm.h>
#include <tusb_console.h>
#endif
#include <functional>
#include <string>
#include <utility>

#include "Misc/Dispatch.hpp"
#include "sindarin-debug.h"

#if SIMULATOR
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <termios.h>
#include <thread>
#include <unistd.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

class AsyncPseudoSerialPort {
private:
    int master_fd, slave_fd;
    char slave_name[1024];
    std::thread read_thread;
    std::mutex mutex;
    std::condition_variable cv;
    bool stop_thread;
    std::queue<char> read_buffer;
    std::function<void(char *, size_t)> rx_callback;

public:
    AsyncPseudoSerialPort() : master_fd(-1), slave_fd(-1), stop_thread(false) {
        if (openpty(&master_fd, &slave_fd, slave_name, NULL, NULL) == -1) {
            perror("Error opening PTY");
            return;
        }

        std::cout << "Pseudo-terminal created. Slave device: " << slave_name << std::endl;

        // Log the slave device name to a file
        std::ofstream logfile("serial_device.log");
        if (logfile.is_open()) {
            logfile << slave_name << std::endl;
            logfile.close();
            std::cout << "Device name logged to serial_device.log" << std::endl;
        }

        // Start the read thread
        DispatchGetCore1BackgroundQueue()->dispatch([this] { readLoop(); });
    }

    ~AsyncPseudoSerialPort() {
        stop_thread = true;
        cv.notify_all();
        if (read_thread.joinable()) {
            read_thread.join();
        }
        if (master_fd >= 0) close(master_fd);
        if (slave_fd >= 0) close(slave_fd);
    }

    bool write(const char *data, size_t length) {
        return ::write(master_fd, data, length) == static_cast<ssize_t>(length);
    }

    const char *get_slave_name() const { return slave_name; }

    void set_rx_callback(std::function<void(char *, size_t)> callback) { rx_callback = callback; }

private:
    void readLoop() {
        char buffer[1024];
        while (!stop_thread) {
            ssize_t bytes_read = ::read(master_fd, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                std::unique_lock<std::mutex> lock(mutex);
                for (ssize_t i = 0; i < bytes_read; ++i) {
                    read_buffer.push(buffer[i]);
                }
                lock.unlock();

                if (rx_callback) {
                    rx_callback(buffer, bytes_read);
                }

                cv.notify_one();
            } else {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }
};

#endif

class USBSerial {
public:
    static auto getInstance() -> USBSerial &;
    USBSerial() = default;
    USBSerial(const USBSerial &) = delete;
    void begin();
    void initJtag();
    void lineStateChangedCallback(int itf, cdcacm_event_t *event);
    void rxLoggingCallback(int itf, cdcacm_event_t *event);
    void rxCommandCallback(int itf, cdcacm_event_t *event);
    void txSendData(const uint8_t *data, size_t length);
    void txSendLine(const std::string &str);
    void disableLogging();
    void enableLogging();

    using SerialDataListener = std::function<void(std::shared_ptr<uint8_t[]>, size_t length)>;
    void registerListener(const SerialDataListener &listener);
    void popLastListener();

private:
    enum class BootloaderState { Idle, State1, State2, State3, State4, State5, State6, State7 };
    BootloaderState currentState_{BootloaderState::Idle};
    std::vector<SerialDataListener> listeners_;

    static void cdcacmCommandCallback(int itf, cdcacm_event_t *event) {
        USBSerial::getInstance().rxCommandCallback(itf, event);
    }

#if SIMULATOR
    std::unique_ptr<AsyncPseudoSerialPort> serialPort_ = nullptr;
#endif
    void enableLoggingInternal();

    static bool loggingEnabled;
};
