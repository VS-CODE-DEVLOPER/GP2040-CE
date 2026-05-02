#ifndef UART_BRIDGE_H_
#define UART_BRIDGE_H_

#include "gpaddon.h"
#include "gamepad.h"
#include "storagemanager.h"

#include "hardware/uart.h"
#include "hardware/gpio.h"
#include <string>

// UART1 on GP4 (TX) and GP5 (RX)
#define UART_ID uart1
static constexpr uint UART_TX_PIN = 4;
static constexpr uint UART_RX_PIN = 5;
static constexpr uint UART_BAUD_RATE = 921600;

static constexpr uint8_t UART_SYNC_1 = 0xA5;
static constexpr uint8_t UART_SYNC_2 = 0x5A;
static constexpr uint8_t PACKET_SIZE = 16;

struct __attribute__((packed)) UARTPacket {
    uint8_t sync1;
    uint8_t sync2;
    uint16_t buttons;
    uint8_t dpad;
    uint8_t lx;
    uint8_t ly;
    uint8_t rx;
    uint8_t ry;
    uint8_t lt;
    uint8_t rt;
    uint8_t aux[4];
    uint8_t checksum;
};

class UARTBridge : public GPAddon {
public:
    UARTBridge() = default;

    bool available() override { return true; }
    std::string name() override { return "UARTBridge"; }

    void setup() override;
    void preprocess() override {}
    void process() override;
    void postprocess(bool) override {}
    void reinit() override { setup(); }

private:
    enum State { WAIT_SYNC_1, WAIT_SYNC_2, READ_PAYLOAD };
    State currentState = WAIT_SYNC_1;
    uint8_t buffer[PACKET_SIZE];
    uint8_t bufferIndex = 0;

    bool validateChecksum();
    void updateGamepad(Gamepad* gamepad);
};

#endif
