#include "addons/UARTBridge.h"
#include "storagemanager.h"
#include "gamepad.h"
#include <cstring>

// --- THE CACHE: Stores the last good packet ---
static UARTPacket lastValidPacket = {0xA5, 0x5A, 0, 0, 128, 128, 128, 128, 0, 0, {0,0,0,0}, 0};
static uint32_t lastPacketTime = 0;

void UARTBridge::setup() {
    uart_init(UART_ID, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_fifo_enabled(UART_ID, true);

    currentState = WAIT_SYNC_1;
    bufferIndex = 0;
}

void UARTBridge::process() {
    Gamepad* gamepad = Storage::getInstance().GetGamepad();
    if (!gamepad) return;

    // 1. Read data and update the cache ONLY if the checksum passes
    while (uart_is_readable(UART_ID)) {
        uint8_t byte = uart_getc(UART_ID);
        switch (currentState) {
            case WAIT_SYNC_1:
                if (byte == UART_SYNC_1) { buffer[0] = byte; currentState = WAIT_SYNC_2; }
                break;
            case WAIT_SYNC_2:
                if (byte == UART_SYNC_2) { buffer[1] = byte; bufferIndex = 2; currentState = READ_PAYLOAD; }
                else { 
                    currentState = (byte == UART_SYNC_1) ? WAIT_SYNC_2 : WAIT_SYNC_1; 
                    if (currentState == WAIT_SYNC_2) buffer[0] = byte; 
                }
                break;
            case READ_PAYLOAD:
                buffer[bufferIndex++] = byte;
                if (bufferIndex >= PACKET_SIZE) {
                    if (validateChecksum()) {
                        // SUCCESS! Save it to the persistent cache
                        memcpy(&lastValidPacket, buffer, PACKET_SIZE);
                        lastPacketTime = getMillis();
                    }
                    currentState = WAIT_SYNC_1;
                    bufferIndex = 0;
                }
                break;
        }
    }

    // 2. Applies the cache to the gamepad (Fixes the flickering problem and provides a failsafe if the connection is lost)
    // If we lose connection for 500ms, it will drop the inputs to prevent a "stuck" button
    if (getMillis() - lastPacketTime < 500) {
        updateGamepad(gamepad);
    }
}

bool UARTBridge::validateChecksum() {
    uint8_t checksum = 0;
    for (int i = 2; i < PACKET_SIZE - 1; i++) {
        checksum ^= buffer[i];
    }
    return checksum == buffer[PACKET_SIZE - 1];
}

void UARTBridge::updateGamepad(Gamepad* gamepad) {
    //  Strip out Bit 13 (Touchpad) so it doesn't trigger standard buttons
    uint16_t standardButtons = lastValidPacket.buttons & ~(1 << 13);
    gamepad->state.buttons |= standardButtons;

    //TOUCHPAD MAPPING (Bit 13)
    if (lastValidPacket.buttons & (1 << 13)) {
        gamepad->state.buttons |= GAMEPAD_MASK_A2; // Official GP2040-CE Touchpad Click!
    }

    // D-pad mapping
    if (lastValidPacket.dpad & 0x01) gamepad->state.dpad |= GAMEPAD_MASK_UP;
    if (lastValidPacket.dpad & 0x02) gamepad->state.dpad |= GAMEPAD_MASK_DOWN;
    if (lastValidPacket.dpad & 0x04) gamepad->state.dpad |= GAMEPAD_MASK_LEFT;
    if (lastValidPacket.dpad & 0x08) gamepad->state.dpad |= GAMEPAD_MASK_RIGHT;

    // TRUE ANALOG SCALING (8-bit to 16-bit conversion)
    gamepad->state.lx = lastValidPacket.lx * 257;
    gamepad->state.ly = lastValidPacket.ly * 257;
    gamepad->state.rx = lastValidPacket.rx * 257;
    gamepad->state.ry = lastValidPacket.ry * 257;
    
    gamepad->state.lt = lastValidPacket.lt * 257;
    gamepad->state.rt = lastValidPacket.rt * 257;

    gamepad->hasAnalogTriggers = true;

    //THE UNCHARTED 4 FIX: DualShock 4 Digital Failsafe
    // If the trigger is pulled past 10 (out of 255), slam the digital button ON
    if (lastValidPacket.lt > 10) gamepad->state.buttons |= GAMEPAD_MASK_L2;
    if (lastValidPacket.rt > 10) gamepad->state.buttons |= GAMEPAD_MASK_R2;
}