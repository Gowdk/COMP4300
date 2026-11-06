/*
    Helper class for maintaining the Go-Back-N sender window.

    State:
    1. base             - oldest DATA sequence number not yet ACKed
    2. nextSeqNum       - sequence number to use for the next NEW DATA packet
    3. outstandingCount - number of sent DATA packets still waiting for ACKs

    Sequence numbers range from 0 to 15. This class does not send packets or
    manage retransmission timers; gbnSender is responsible for those tasks.
*/

#pragma once

#include <cstdint>

#define SEQUENCE_NUMBER_SPACE 16

class SlidingWindow {
public:
    SlidingWindow();
    SlidingWindow(uint8_t windowSize, uint32_t initialSeq);

    // Reset the window using the assignment's window size and first SEQ.
    void Setup(uint8_t windowSize, uint32_t initialSeq);

    bool CanSend();
    uint32_t GetNextPacket();
    int Acknowledge(uint32_t ackNum);

    uint32_t GetBase();
    uint32_t GetNextSeqNum();
    uint8_t GetWindowSize();
    int GetOutstandingCount();
    uint32_t GetOutstandingSequence(int index);

private:
    uint32_t base;
    uint32_t nextSeqNum;
    uint8_t windowSize;
    int outstandingCount;
};
