#include "slidingWindow.h"
#include <stdexcept>

// Default constructor: ns-3 creates gbnSender before main.cc calls Setup().
// The real student-ID window size and initial sequence are set in Setup().
SlidingWindow::SlidingWindow() {
    base = 0;
    nextSeqNum = 0;
    windowSize = 5;
    outstandingCount = 0;
}

// This constructor is handy when testing the helper without ns-3.
SlidingWindow::SlidingWindow(uint8_t windowSize, uint32_t initialSeq) {
    Setup(windowSize, initialSeq);
}

// Set the window size and the starting sequence number for BOTH counters.
// Starting both at the same sequence means no DATA is initially outstanding.
void SlidingWindow::Setup(uint8_t windowSize, uint32_t initialSeq) {
    if (windowSize < 1 || windowSize >= SEQUENCE_NUMBER_SPACE) {
        throw std::invalid_argument("Window size must be between 1 and 15");
    }
    if (initialSeq >= SEQUENCE_NUMBER_SPACE) {
        throw std::invalid_argument("Initial sequence number must be between 0 to 15");
    }

    base = initialSeq;
    nextSeqNum = initialSeq;
    this->windowSize = windowSize;
    outstandingCount = 0;
}

// Can we transmit another NEW packet without exceeding the sender window?
bool SlidingWindow::CanSend() {
    return outstandingCount < windowSize;
}

// Reserve a sequence number for a NEW transmission (not a retransmission).
uint32_t SlidingWindow::GetNextPacket() {
    if (!CanSend()) {
        throw std::logic_error("Sliding window is full");
    }

    uint32_t seq = nextSeqNum;
    nextSeqNum = (nextSeqNum + 1) % SEQUENCE_NUMBER_SPACE;
    outstandingCount++;
    return seq;
}

// ACK(n) means that all outstanding DATA through sequence n were received.
// Return the number of newly acknowledged packets. Zero means duplicate,
// outdated, invalid, or outside the current sender window.
int SlidingWindow::Acknowledge(uint32_t ackNum) {
    if (ackNum >= SEQUENCE_NUMBER_SPACE || outstandingCount == 0) {
        return 0;
    }

    // Modular distance handles cases such as base=14 and ACK=0.
    int distance = (ackNum + SEQUENCE_NUMBER_SPACE - base) % SEQUENCE_NUMBER_SPACE;
    if (distance >= (outstandingCount)) {
        return 0;
    }

    int acknowledged = static_cast<int>(distance) + 1;
    base = (ackNum + 1) % SEQUENCE_NUMBER_SPACE;
    outstandingCount -= acknowledged;
    return acknowledged;
}

// Getters
uint32_t SlidingWindow::GetBase() { return base; }
uint32_t SlidingWindow::GetNextSeqNum() { return nextSeqNum; }
uint8_t SlidingWindow::GetWindowSize() { return windowSize; }
int SlidingWindow::GetOutstandingCount() { return outstandingCount; }

// Return the SEQ of the index-th outstanding packet WITHOUT advancing state.
// For example, base=14, index=2 gives SEQ=0.
uint32_t SlidingWindow::GetOutstandingSequence(int index) {
    if (index < 0 || index >= outstandingCount) {
        throw std::out_of_range("Invalid outstanding packet index");
    }
    return (base + index) % SEQUENCE_NUMBER_SPACE;
}
