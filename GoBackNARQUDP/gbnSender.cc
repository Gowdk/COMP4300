/*
    Responsibilities:
        Maintain SlidingWindow state
        Transmit DATA packets containing a 4-byte SEQ and 1000-byte payload
        Process cumulative ACKs
        Use ONE retransmission timer for the oldest outstanding DATA
        On timeout, retransmit ALL unacknowledged DATA (Go-Back-N)
*/

#include "gbnSender.h"
#include "packetEncoding.h"

#include <iostream>

using namespace ns3;

NS_OBJECT_ENSURE_REGISTERED(gbnSender);

// Register the sender class so ns-3's CreateObject<gbnSender>() can use it.
TypeId gbnSender::GetTypeId() {
    static TypeId tid = TypeId("ns3::gbnSender")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<gbnSender>();
    return tid;
}

// Receive the simulation parameters chosen in main.cc.
void gbnSender::Setup(Ipv4Address receiverAddress, uint16_t receiverPort,
                      uint8_t windowSize, uint32_t initialSeq, uint32_t numPackets,
                      std::string givenName, double timeoutMs) {
    m_receiverAddress = receiverAddress;
    m_receiverPort = receiverPort;
    m_totalPackets = numPackets;
    m_givenName = givenName;
    m_timeout = MilliSeconds(timeoutMs);
    
    // Set up the Sliding Window 
    window = SlidingWindow(windowSize, initialSeq);
}

// Create our UDP socket and begin filling the sender window with new DATA.
void gbnSender::StartApplication() {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());

    // Error checking
    if (m_socket->Bind() < 0) {
        NS_ABORT_MSG("Sender could not bind its UDP socket");
    }
    if (m_socket->Connect(InetSocketAddress(m_receiverAddress, m_receiverPort)) < 0) {
        NS_ABORT_MSG("Sender could not connect its UDP socket");
    }

    // When our sending socket gets data, invoke HandleAck()
    m_socket->SetRecvCallback(MakeCallback(&gbnSender::HandleAck, this));

    // Sender-observed completion time starts when first DATA is transmitted.
    m_startTime = Simulator::Now();
    SendAvailable();

    // Only one timer exists, covering the OLDEST outstanding DATA packet.
    if (window.GetOutstandingCount() > 0) {
        StartTimer();
    }
}

// Fill the GBN window with NEW packets until full or until 100 were sent.
// This does not wait for individual ACKs between new transmissions.
void gbnSender::SendAvailable() {

    // While the window is not full/more data needs to be sent, fill the window with packets.
    while (window.CanSend() && m_newPacketsSent < m_totalPackets) {
        uint32_t seq = window.GetNextSeqNum();

        // Do not advance window state unless the UDP socket accepts the DATA.
        if (!SendData(seq, false)) {
            std::cerr << "Sender: failed to send new DATA " << seq << "\n";
            return;
        }


        window.GetNextPacket();
        m_newPacketsSent++;
    }
}

// Construct and transmit one DATA datagram (new or retransmitted).
// First four bytes are the SEQ in big-endian; bytes 4 --> 1003 are name + '0'.
// RETRANSMISSIONS must NOT call window.GetNextPacket().
bool gbnSender::SendData(uint32_t seq, bool retransmission) {
    uint8_t bytes[1004];

    // Encode the sequence number into BIG ENDIAN
    if(!EncodeSequenceNumber(seq, bytes, sizeof(bytes))){
        std::cerr << "Tried to encode a sequence number into a byte array of size " 
        << sizeof(bytes) 
        << "\n";
        return false;
    };

    /*  From the Assignment Specifications:
        "For DATA packets, place the 4-byte sequence number at the beginning of 
        the UDP payload, followed by the application data. 
        For ACK packets, the UDP payload should contain only the 4-byte ACK number. 
        Do not modify UDP packet headers. A separate custom Header class is not required."
    */

    // Write all bytes to '0'
    for (int i = 4; i < 1004; i++) {
        bytes[i] = '0';
    }

    // Overwrite the bytes corresponding to the name. 
    for (uint32_t i = 0; i < m_givenName.size() && i < 1000; i++) {
        // Translate each char to its underlying byte value, save to byte
        bytes[4 + i] = static_cast<uint8_t>(m_givenName[i]);
    }

    // Create the packet and send it into the socket.
    Ptr<Packet> packet = Create<Packet>(bytes, 1004);
    int resultSize = m_socket->Send(packet);

    // result must be of size 1004
    if (resultSize != 1004) {
        return false;
    }

    // Keeping track of stats
    m_totalTransmissions++;
    if (retransmission) {
        m_retransmissions++;
    }

    return true;
}

// A received ACK may acknowledge one or several outstanding DATA packets.
// We slide the window, refill it, and restart the single base-packet timer.
void gbnSender::HandleAck(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet;

    // Recv the entire ACK packet, then handle it
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() != 4) {
            std::cerr << "Sender: ignored ACK with invalid size\n";
            continue;
        }

        uint8_t ackBytes[4];
        packet->CopyData(ackBytes, 4);
        uint32_t ackNum = DecodeSequenceNumber(ackBytes);

        int newlyAcknowledged = window.Acknowledge(ackNum);
        if (newlyAcknowledged == 0) {
            // Duplicate, stale, or invalid ACK; do not restart timer.
            continue;
        }
        // Cumulative acknowledgement!
        m_packetsAcknowledged += newlyAcknowledged;

        // A cumulative ACK means the oldest outstanding DATA may change, so cancel timer
        if (m_timeoutEvent.IsPending()) {
            Simulator::Cancel(m_timeoutEvent);
        }

        // If we've acknowledged all packets we're done!
        if (m_packetsAcknowledged == m_totalPackets) {
            // All NEW application DATA packets have been cumulatively ACKed.
            m_completionTime = Simulator::Now() - m_startTime;
            m_finished = true;
            return;
        }

        // We may have more space now that we've potentially ACK'd some outstanding packets.
        SendAvailable();

        // If we end up sending more, start the timer!
        if (window.GetOutstandingCount() > 0) {
            StartTimer();
        }
    }
}

// Schedule the timeout for the oldest outstanding DATA packet.
// ns-3 EventId provides a SIMULATION timer.
void gbnSender::StartTimer() {
    if (m_timeoutEvent.IsPending()) {
        Simulator::Cancel(m_timeoutEvent);
    }

    // 
    if (window.GetOutstandingCount() > 0) {
        m_timeoutEvent = Simulator::Schedule(
            m_timeout, &gbnSender::HandleTimeout, this
        );
    }
}

// No ACK advanced our window before the timeout. go back to base and
// retransmit EVERY outstanding DATA packet, in its original sequence order.
void gbnSender::HandleTimeout() {
    // There's a chance that we've caught up to all oustanding packets, or finished the sim
    if (window.GetOutstandingCount() == 0 || m_finished) {
        return;
    }

    m_timeouts++;

    // Send retransmissions!
    for (int i = 0; i < window.GetOutstandingCount(); i++) {
        uint32_t seq = window.GetOutstandingSequence(i);
        if (!SendData(seq, true)) {
            std::cerr << "Sender: failed retransmission of DATA " << seq << "\n";
        }
    }

    // Start the timer again for the still-unacknowledged base packet.
    StartTimer();
}

// Cancel the timer and close our socket when this application stops.
void gbnSender::StopApplication() {
    if (m_timeoutEvent.IsPending()) {
        Simulator::Cancel(m_timeoutEvent);
    }
    if (m_socket) {
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket->Close();
        m_socket = nullptr;
    }
}

// Calculate sender-OBSERVED goodput only after all 100 packets were ACKed.
// Application payload = 1000 bytes per packet; SEQ/UDP/IP/ACK overhead and
// retransmitted data are NOT included in the numerator.
void gbnSender::PrintStatistics() {
    std::cout << "\n===== SENDER STATISTICS =====\n";
    std::cout << "Total DATA transmissions: " << m_totalTransmissions << "\n";
    std::cout << "Retransmissions: " << m_retransmissions << "\n";
    std::cout << "Timeouts: " << m_timeouts << "\n";
    std::cout << "Cumulatively ACKed DATA packets: " << m_packetsAcknowledged << " / "
              << m_totalPackets << "\n";

    if (m_finished) {
        double seconds = m_completionTime.GetSeconds();
        double goodputMbps = 0.0;
        if (seconds > 0.0) {
            // Total Megabytes = packets * packet size * 8 bits
            // Recall goodput to be the "usefull application data", not inclusive of 
            // the 4 bytes for the sequence number
            goodputMbps = (m_packetsAcknowledged * 1000.0 * 8.0) / (seconds * 1000000.0);
        }
        double goodputPercentage = (goodputMbps / 10.0) * 100.0;

        std::cout << "Transfer complete: yes\n";
        std::cout << "Sender-observed completion time (s): " << seconds << "\n";
        std::cout << "Goodput (Mbps): " << goodputMbps << "\n";
        std::cout << "Goodput percentage (%): " << goodputPercentage << "\n";
    } else {
        // Transfer was incomplete.
        std::cout << "Transfer complete: no (120-second limit reached)\n";
        std::cout << "Sender-observed completion time (s): N/A\n";
        std::cout << "Goodput (Mbps): N/A - incomplete transfer\n";
        std::cout << "Goodput percentage (%): N/A - incomplete transfer\n";
    }
}
