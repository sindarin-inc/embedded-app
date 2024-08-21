#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>
#include <sys/select.h>
#include <unistd.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

class AsyncPseudoSerialPort {
private:
    int masterFd, slaveFd;
    char slaveName[1024];
    bool stopThread;
    std::function<void(char *, size_t)> rxCallback;

public:
    AsyncPseudoSerialPort() : masterFd(-1), slaveFd(-1), stopThread(false) {
        if (openpty(&masterFd, &slaveFd, slaveName, NULL, NULL) == -1) {
            log_e("Error opening PTY");
            return;
        }

        log_i("Pseudo-terminal created. Slave device: %s", slaveName);

        // Start the read thread
        xTaskCreate(
            [](void *param) {
            auto *self = static_cast<AsyncPseudoSerialPort *>(param);
            self->readLoop();
            vTaskDelete(nullptr);
            },
            "AsyncPseudoSerialPort", 4096, this, 1, nullptr);
    }

    ~AsyncPseudoSerialPort() {
        stopThread = true;
        if (masterFd >= 0) {
            close(masterFd);
        }
        if (slaveFd >= 0) {
            close(slaveFd);
        }
    }

    bool write(const char *data, size_t length) {
        ssize_t written = 0;
        // For some reason, sometimes it doesn't write the whole buffer
        do {
            written += ::write(masterFd, data + written, length - written);
            if (written < 0) {
                log_e("Error writing to PTY");
                return false;
            }
        } while (written != static_cast<ssize_t>(length));
        return true;
    }

    const char *getSlaveName() const { return slaveName; }

    void setRxCallback(std::function<void(char *, size_t)> callback) { rxCallback = callback; }

private:
    void readLoop() {
        char buffer[1024];
        while (!stopThread) {
            auto bytesRead = read(masterFd, buffer, sizeof(buffer));
            if (bytesRead > 0) {
                if (rxCallback) {
                    rxCallback(buffer, bytesRead);
                }
            } else {
                // We tried using select but kept getting EINTR, so we're just going to sleep for a
                // bit
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }
    }
};