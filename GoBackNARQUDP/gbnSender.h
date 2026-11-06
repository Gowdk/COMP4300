#pragma once

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

#include "slidingWindow.h"
#include <string>

// Node A: uses the SlidingWindow helper and a single GBN retransmission timer.
class gbnSender : public ns3::Application {
public:
    static ns3::TypeId GetTypeId();

    void Setup(ns3::Ipv4Address receiverAddress, uint16_t receiverPort,
               uint8_t windowSize, uint32_t initialSeq, uint32_t numPackets,
               std::string givenName, double timeoutMs);
    void PrintStatistics();

private:
    void StartApplication() override;
    void StopApplication() override;

    void SendAvailable();
    bool SendData(uint32_t seq, bool retransmission);
    void HandleAck(ns3::Ptr<ns3::Socket> socket);
    void StartTimer();
    void HandleTimeout();

    SlidingWindow window;

    ns3::Ptr<ns3::Socket> m_socket;
    ns3::Ipv4Address m_receiverAddress;
    uint16_t m_receiverPort = 0;
    ns3::EventId m_timeoutEvent;
    ns3::Time m_timeout = ns3::MilliSeconds(100);
    ns3::Time m_startTime = ns3::Seconds(0);
    ns3::Time m_completionTime = ns3::Seconds(0);

    uint32_t m_totalPackets = 100;
    uint32_t m_newPacketsSent = 0;
    uint32_t m_packetsAcknowledged = 0;
    uint32_t m_totalTransmissions = 0;
    uint32_t m_retransmissions = 0;
    uint32_t m_timeouts = 0;

    std::string m_givenName;
    
    bool m_finished = false;
};
