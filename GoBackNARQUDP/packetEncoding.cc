#include "packetEncoding.h"

// Shift the high-order bits into the first byte, then the next byte.
// This works the same way on little-endian and big-endian computers.
bool EncodeSequenceNumber(uint32_t number, uint8_t bytes[], int size) {
    if(size < 4){
        return false;
    }
    bytes[0] = (number >> 24) & 0xff;
    bytes[1] = (number >> 16) & 0xff;
    bytes[2] = (number >> 8) & 0xff;
    bytes[3] = number & 0xff;

    return true;
}

// Rebuild the 32-bit integer from the four network-order bytes.
uint32_t DecodeSequenceNumber(uint8_t bytes[4]) {
    uint32_t number = 0;
    // 1st byte
    number |= static_cast<uint32_t>(bytes[0]) << 24;
    
    // 2nd byte
    number |= static_cast<uint32_t>(bytes[1]) << 16;

    // 3rd byte
    number |= static_cast<uint32_t>(bytes[2]) << 8;

    // 4th byte
    number |= static_cast<uint32_t>(bytes[3]);
    return number;
}
