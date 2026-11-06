/*
    Responsibilities:
        Receive DATA packets over UDP
        Track the next expected sequence number
        Discard duplicate / out-of-order DATA
        Send a cumulative ACK after EVERY received DATA packet
*/

#include "gbnReceiver.h"
#include "packetEncoding.h"

#include <iostream>

using namespace ns3;

NS_OBJECT_ENSURE_REGISTERED(gbnReceiver);

// Register the receiver with ns-3, so CreateObject<gbnReceiver>() works.
TypeId gbnReceiver::GetTypeId() {
    static TypeId tid = TypeId("ns3::gbnReceiver")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<gbnReceiver>();
    return tid;
}

// Set the UDP port and the FIRST sequence number the receiver must accept.
void gbnReceiver::Setup(uint16_t listeningPort, uint32_t initialSeq) {
    m_listeningPort = listeningPort;
    m_expectedSeqNum = initialSeq;
}

// Start listening for UDP DATA packets when ns-3 starts this application.
void gbnReceiver::StartApplication() {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());

    if (m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_listeningPort)) < 0) {
        NS_ABORT_MSG("Receiver could not bind its UDP socket");
    }

    // ns-3 calls HandleData() when the receiving socket has packets available.
    m_socket->SetRecvCallback(MakeCallback(&gbnReceiver::HandleData, this));
}

// Process incoming DATA datagrams in the order ns-3 gives them to us.
void gbnReceiver::HandleData(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet;

    while ((packet = socket->RecvFrom(from))) {
        // DATA has 4 bytes for SEQ followed by exactly 1000 payload bytes.
        if (packet->GetSize() != 1004) {
            std::cerr << "Receiver: ignored DATA of size " << packet->GetSize() << "\n";
            continue;
        }

        uint8_t bytes[1004];
        packet->CopyData(bytes, 1004);
        uint32_t seq = DecodeSequenceNumber(bytes);

        if (seq == m_expectedSeqNum) {
            // Accept exactly one copy of the next expected DATA packet.
            m_deliveredPackets++;
            m_expectedSeqNum = (m_expectedSeqNum + 1) % 16;

        } else {
            // Go-Back-N receiver DOES NOT buffer packets received out of order.
            m_discardedPackets++;

        }

        // ACK the last packet that was successfully delivered in order.
        // If we expected 0, the last in-order packet was 15.
        uint32_t ackNum = (m_expectedSeqNum + 15) % 16;
        SendAck(ackNum, from);
    }
}

// Reply to the original UDP sender. ACK payload is EXACTLY four bytes.
void gbnReceiver::SendAck(uint32_t ackNum, Address destination) {
    uint8_t ackBytes[4];
    
    if(!EncodeSequenceNumber(ackNum, ackBytes, sizeof(ackBytes))){
        std::cerr << "Tried to encode a sequence number into a byte array of size " 
        << sizeof(ackBytes) 
        << "\n";
        return;
    }

    Ptr<Packet> ack = Create<Packet>(ackBytes, 4);
    int result = m_socket->SendTo(ack, 0, destination);

    if (result == 4) {
        m_acksSent++;
    } else {
        std::cerr << "Receiver could not send ACK " << ackNum << "\n";
    }
}

// Clean up
void gbnReceiver::StopApplication() {
    if (m_socket) {
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket->Close();
        m_socket = nullptr;
    }
}

// Print receiver-side counts. Assumes application is done
void gbnReceiver::PrintStatistics() {
    std::cout << "\n===== RECEIVER STATISTICS =====\n";
    std::cout << "Delivered DATA packets: " << m_deliveredPackets << "\n";
    std::cout << "Discarded duplicate/out-of-order DATA: " << m_discardedPackets << "\n";
    std::cout << "ACK transmissions: " << m_acksSent << "\n";
}
