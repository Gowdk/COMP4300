// ns-3 libraries
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Receiver/Sender header files
#include "gbnReceiver.h"
#include "gbnSender.h"

// Standard C++ libraries
#include <iostream>
#include <cstdint>

using namespace ns3;

#define STUDENT_NUM 781237
#define STUDENT_NAME "Keenan"

/*
    main.cc creates the simulation environment. The two applications handle
    DATA, ACKs, the sliding window, and GBN retransmissions themselves.
*/
int main(int argc, char* argv[]) {

    // P("DATA Loss") and P("ACK loss")
    double dataLoss = 0.10;
    double ackLoss = 0.10;

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataLoss", "DATA packet loss probability (0 to 1)", dataLoss);
    cmd.AddValue("ackLoss", "ACK packet loss probability (0 to 1)", ackLoss);
    cmd.Parse(argc, argv);

    if (dataLoss < 0.0 || dataLoss > 1.0 || ackLoss < 0.0 || ackLoss > 1.0) {
        std::cerr << "Loss probabilities must be between 0 and 1.\n";
        return 1;
    }

    // Compute the assignment's student-specific parameters once in main.cc.
    uint8_t windowSize = 5 + (STUDENT_NUM % 5);
    uint32_t initialSeq = (STUDENT_NUM / 10) % 10;
    uint32_t numPackets = 100;
    double timeoutMs = 100.0;

    std::cout << "Window size: " << static_cast<int>(windowSize) << "\n";
    std::cout << "Initial SEQ: " << initialSeq << "\n";
    std::cout << "DATA loss: " << dataLoss << ", ACK loss: " << ackLoss << "\n";

    // Create the Sender and Receiver nodes.
    NodeContainer nodes;
    nodes.Create(2);

    Ptr<Node> n1 = nodes.Get(0);
    Ptr<Node> n2 = nodes.Get(1);

    // Create the bidirectional point-to-point link.
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("20ms"));
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install UDP/IP protocol stacks and assign IPv4 addresses.
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // DATA loss occurs when packets ARRIVE at the receiver interface.
    Ptr<RateErrorModel> dataError = CreateObject<RateErrorModel>();
    dataError->SetAttribute("ErrorRate", DoubleValue(dataLoss));
    dataError->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(dataError));

    // ACK loss occurs when ACK packets ARRIVE at the sender interface.
    Ptr<RateErrorModel> ackError = CreateObject<RateErrorModel>();
    ackError->SetAttribute("ErrorRate", DoubleValue(ackLoss));
    ackError->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    devices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(ackError));

    std::cout << "Sender: " << interfaces.GetAddress(0) << "\n";
    std::cout << "Receiver: " << interfaces.GetAddress(1) << "\n";

    // Record DATA and ACK traffic on both network interfaces.
    // Files such as gbn-0-0.pcap / gbn-1-0.pcap can be opened in Wireshark.
    p2p.EnablePcapAll("gbn");

    // Create the receiver on node B. Start it FIRST so port 8500 is ready.
    Ptr<gbnReceiver> receiver = CreateObject<gbnReceiver>();
    receiver->Setup(8500, initialSeq);
    n2->AddApplication(receiver);
    receiver->SetStartTime(Seconds(0.5));
    receiver->SetStopTime(Seconds(120.0));

    // Create the sender on node A. Pass the receiver IP and assignment data.
    Ptr<gbnSender> sender = CreateObject<gbnSender>();
    sender->Setup(interfaces.GetAddress(1), 8500, windowSize,
                  initialSeq, numPackets, STUDENT_NAME, timeoutMs);
    n1->AddApplication(sender);
    sender->SetStartTime(Seconds(1.0));
    sender->SetStopTime(Seconds(120.0));

    // The simulation must never run beyond 120 simulated seconds.
    Simulator::Stop(Seconds(120.0));
    Simulator::Run();

    // These are end-of-simulation reports, not communication between apps.
    sender->PrintStatistics();
    receiver->PrintStatistics();

    Simulator::Destroy();
    return 0;
}
