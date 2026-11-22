#include "Helpers.h"
#include <stdio.h>

namespace Helpers {

void formatUptime(uint32_t millisVal, char* outBuf, size_t bufLen) {
    uint32_t totalSec = millisVal / 1000UL;
    uint32_t hours = totalSec / 3600UL;
    uint32_t mins = (totalSec % 3600UL) / 60UL;
    uint32_t secs = totalSec % 60UL;
    snprintf(outBuf, bufLen, "%02lu:%02lu:%02lu",
             (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
}

void formatBytes(uint32_t bytes, char* outBuf, size_t bufLen) {
    if (bytes >= 1024UL * 1024UL) {
        snprintf(outBuf, bufLen, "%lu.%luMB",
                 (unsigned long)(bytes / (1024UL * 1024UL)),
                 (unsigned long)((bytes % (1024UL * 1024UL)) * 10UL / (1024UL * 1024UL)));
    } else if (bytes >= 1024UL) {
        snprintf(outBuf, bufLen, "%luKB", (unsigned long)(bytes / 1024UL));
    } else {
        snprintf(outBuf, bufLen, "%luB", (unsigned long)bytes);
    }
}

uint8_t reverseBits8(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

} // namespace Helpers
