#pragma once

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

// Node B: accepts only in-order DATA and sends cumulative ACKs.
class gbnReceiver : public ns3::Application {
public:
    static ns3::TypeId GetTypeId();

    // main.cc configures the UDP listening port and the expected first SEQ.
    void Setup(uint16_t listeningPort, uint32_t initialSeq);
    void PrintStatistics();

private:
    void StartApplication() override;
    void StopApplication() override;

    void HandleData(ns3::Ptr<ns3::Socket> socket);
    void SendAck(uint32_t ackNum, ns3::Address destination);

    ns3::Ptr<ns3::Socket> m_socket;
    uint16_t m_listeningPort = 0;
    uint32_t m_expectedSeqNum = 0;
    uint32_t m_deliveredPackets = 0;
    uint32_t m_discardedPackets = 0;
    uint32_t m_acksSent = 0;
};
