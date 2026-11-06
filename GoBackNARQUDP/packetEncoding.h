#pragma once
#include <cstdint>

// The assignment requires exactly 4 bytes in network (big-endian) byte order.
// These functions operate on the first four bytes of a UDP payload.
bool EncodeSequenceNumber(uint32_t number, uint8_t bytes[], int size);
uint32_t DecodeSequenceNumber(uint8_t bytes[4]);
