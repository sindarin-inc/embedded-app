
#include <Spiram/Memory.hpp>
#include <cmath>
#include <cstring>

#include "sindarin-debug.h"

#define SPIRAM_SIZE (4 * 1024 * 1024) // 4 MB
#define BUFFER_SIZE 4096              // 4 KB
#define TEST_ITERATIONS 100000

uint8_t writeBuffer[BUFFER_SIZE];
uint8_t readBuffer[BUFFER_SIZE];

void RunSpiRamBenchmark() {
    DisableCore0Wdt();

    // Allocate a buffer in SPIRAM
    uint8_t *spiRamBuffer = (uint8_t *)SpiramMalloc(SPIRAM_SIZE);
    if (!spiRamBuffer) {
        log_e("Failed to allocate SPIRAM buffer.");
        return;
    }
    // Fill writeBuffer with random data
    for (int i = 0; i < BUFFER_SIZE; i++) {
        writeBuffer[i] = random() % 256;
    }

    log_i("SPIRAM write benchmark started.");
    uint32_t writeStartTime = Millis();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        int addr = random() % (SPIRAM_SIZE - BUFFER_SIZE);
        memcpy(spiRamBuffer + addr, writeBuffer, BUFFER_SIZE);
    }
    uint32_t writeEndTime = Millis();
    uint32_t writeElapsedTime = writeEndTime - writeStartTime;
    double writeThroughput =
        (double(TEST_ITERATIONS) * BUFFER_SIZE * 1000) / (double(writeElapsedTime) * 1024 * 1024);
    log_i("SPIRAM write benchmark completed. Throughput: %.2f MB/s", writeThroughput);

    // Read benchmark
    log_i("SPIRAM read benchmark started.");
    uint32_t readStartTime = Millis();

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        int addr = random() % (SPIRAM_SIZE - BUFFER_SIZE);
        memcpy(readBuffer, spiRamBuffer + addr, BUFFER_SIZE);
    }

    uint32_t readEndTime = Millis();
    uint32_t readElapsedTime = readEndTime - readStartTime;
    double readThroughput =
        (double(TEST_ITERATIONS) * BUFFER_SIZE * 1000) / (double(readElapsedTime) * 1024 * 1024);
    log_i("SPIRAM read benchmark completed. Throughput: %.2f MB/s", readThroughput);

    SpiramFree(spiRamBuffer);

    EnableCore0Wdt();
}
