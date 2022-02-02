#include <stdlib.h>

#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

#define SPI_CS0 0
#define SPI_CS1 1
#define SPI_CS2 2
#define SPI_CS_MASK_ALL 0x7

#define SPI_HAS_TRANSACTION
#define SPI_MSBFIRST 1

class SPISettings {
public:
    SPISettings() : _clock(1000000), _bitOrder(SPI_MSBFIRST), _dataMode(SPI_MODE0) {}
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
        : _clock(clock), _bitOrder(bitOrder), _dataMode(dataMode) {}
    uint32_t _clock;
    uint8_t _bitOrder;
    uint8_t _dataMode;
};

class SPIClass {
private:
public:
    SPIClass();
    void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1);

    void beginTransaction(SPISettings settings);
    void endTransaction(void);

    uint8_t transfer(uint8_t data);

    void write(uint8_t data);
    void writeBytes(const uint8_t *data, uint32_t size);
};

extern SPIClass SPI;