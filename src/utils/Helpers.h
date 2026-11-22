#ifndef POCKETDECK_HELPERS_H
#define POCKETDECK_HELPERS_H

#include <Arduino.h>

namespace Helpers {
    // Format milliseconds uptime into "HH:MM:SS"
    void formatUptime(uint32_t millisVal, char* outBuf, size_t bufLen);

    // Format byte count into human-readable string (e.g., "142 KB" or "512 B")
    void formatBytes(uint32_t bytes, char* outBuf, size_t bufLen);

    // Reverse bits in a single byte (useful for NEC LSB/MSB conversion)
    uint8_t reverseBits8(uint8_t b);
}

#endif // POCKETDECK_HELPERS_H
