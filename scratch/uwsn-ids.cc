#include "ns3/aqua-sim-ng-module.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h" 
#include "ns3/aqua-sim-propagation.h" 
#include "ns3/aqua-sim-channel.h"     
#include "ns3/aqua-sim-header-mac.h"  
#include "ns3/aqua-sim-address.h"     
#include "ns3/aqua-sim-application.h" 

#include <fstream>
#include <iostream>
#include <sstream>


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UwsnDataGenerationFixed");

std::ofstream g_csvFile;

class SensorDataTag : public Tag
{
  public:
    void SetNodeId(uint32_t id)
    {
        m_nodeId = id;
    }
    uint32_t GetNodeId() const
    {
        return m_nodeId;
    }
    void SetSendTime(Time t)
    {
        m_sendTime = t;
    }
    Time GetSendTime() const
    {
        return m_sendTime;
    }
    void SetReportedPos(Vector pos)
    {
        m_reportedPos = pos;
    }
    Vector GetReportedPos() const
    {
        return m_reportedPos;
    }
    void SetIsAnomaly(int anomaly)
    {
        m_isAnomaly = anomaly;
    }
    int GetIsAnomaly() const
    {
        return m_isAnomaly;
    }

    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("SensorDataTag")
                                .SetParent<Tag>()
                                .AddConstructor<SensorDataTag>()
                                .AddAttribute("NodeId",
                                              "Node ID",
                                              EmptyAttributeValue(),
                                              MakeUintegerAccessor(&SensorDataTag::m_nodeId),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("SendTime",
                                              "Packet Send Time",
                                              EmptyAttributeValue(),
                                              MakeTimeAccessor(&SensorDataTag::m_sendTime),
                                              MakeTimeChecker())
                                .AddAttribute("ReportedPos",
                                              "Reported Position",
                                              EmptyAttributeValue(),
                                              MakeVectorAccessor(&SensorDataTag::m_reportedPos),
                                              MakeVectorChecker())
                                .AddAttribute("IsAnomaly",
                                              "Is Anomaly",
                                              EmptyAttributeValue(),
                                              MakeIntegerAccessor(&SensorDataTag::m_isAnomaly),
                                              MakeIntegerChecker<int>());
        return tid;
    }
    virtual TypeId GetInstanceTypeId(void) const
    {
        return GetTypeId();
    }
    virtual uint32_t GetSerializedSize(void) const
    {
        return sizeof(m_nodeId) + sizeof(uint64_t) + (sizeof(double) * 3) + sizeof(m_isAnomaly);
    }
    virtual void Serialize(TagBuffer i) const
    {
        i.WriteU32(m_nodeId);
        i.WriteU64(m_sendTime.GetNanoSeconds());
        i.WriteDouble(m_reportedPos.x);
        i.WriteDouble(m_reportedPos.y);
        i.WriteDouble(m_reportedPos.z);
        i.WriteU32(m_isAnomaly);
    }
    virtual void Deserialize(TagBuffer i)
    {
        m_nodeId = i.ReadU32();
        m_sendTime = NanoSeconds(i.ReadU64());
        m_reportedPos.x = i.ReadDouble();
        m_reportedPos.y = i.ReadDouble();
        m_reportedPos.z = i.ReadDouble();
        m_isAnomaly = i.ReadU32();
    }
    virtual void Print(std::ostream& os) const
    {
        os << "NodeID=" << m_nodeId << ", ReportedPos=" << m_reportedPos;
    }

  private:
    uint32_t m_nodeId;
    Time m_sendTime;
    Vector m_reportedPos;
    int m_isAnomaly;
};

void
PhyRxEndTrace(Ptr<const Packet> packet, double rssi, Vector senderPos, Time propDelay)
{
    NS_LOG_INFO("PhyRxEndTrace CALLED at time " << Simulator::Now().GetSeconds());

    SensorDataTag tag;
    if (!packet->PeekPacketTag(tag))
    {
        NS_LOG_WARN("Received packet at PHY without SensorDataTag");
        return;
    }

    uint32_t nodeId = tag.GetNodeId();
    Time sendTime = tag.GetSendTime();
    Vector reportedPos = tag.GetReportedPos();
    int isAnomaly = tag.GetIsAnomaly();

    Time recvTime = Simulator::Now();

    g_csvFile << recvTime.GetSeconds() << "," << nodeId << "," << sendTime.GetSeconds() << ","
              << propDelay.GetSeconds() << "," << rssi << "," << senderPos.x << "," << senderPos.y
              << "," << senderPos.z << "," << reportedPos.x << "," << reportedPos.y << ","
              << reportedPos.z << "," << isAnomaly << "\n";

    g_csvFile.flush();
}

void
SinkSocketRecv(Ptr<Socket> socket)
{
}

class SensorApp : public Application
{
  public:
    SensorApp()
        : m_socket(0),
          m_peerAddress(),
          m_packetSize(100),
          m_sendInterval(Seconds(30.0)),
          m_sendEvent(),
          m_isAttacker(false),
          m_attackType(0),
          m_attackStartTime(Seconds(0)),
          m_spoofedDriftX(0.0),
          m_spoofedDriftY(0.0)
    {
        m_rand = CreateObject<UniformRandomVariable>();
    }

    virtual ~SensorApp()
    {
        m_socket = 0;
    }

    static TypeId GetTypeId(void)
    {
        static TypeId tid =
            TypeId("SensorApp").SetParent<Application>().SetGroupName("AquaSimNg").AddConstructor<
                SensorApp>();
        return tid;
    }

    
    void SetPeer(Address address)
    {
        m_peerAddress = address;
    }

    void SetInterval(Time interval)
    {
        m_sendInterval = interval;
    }

    void SetAttacker(int attackType, Time startTime)
    {
        m_isAttacker = true;
        m_attackType = attackType;
        m_attackStartTime = startTime;
        NS_LOG_INFO("Node " << GetNode()->GetId() << " is set as ATK (Type " << m_attackType
                            << ") starting at " << m_attackStartTime.GetSeconds() << "s");
    }

  protected:
    virtual void StartApplication(void)
    {
        NS_LOG_INFO("Sensor App Started at " << GetNode()->GetId());
        if (!m_socket)
        {
            
            TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);

            
            
            
            
            

            m_socket->Connect(m_peerAddress);
        }
        Time firstSend = Seconds(m_sendInterval.GetSeconds() * m_rand->GetValue(0.0, 1.0));
        Simulator::Schedule(firstSend, &SensorApp::SendPacket, this);
    }

    virtual void StopApplication(void)
    {
        Simulator::Cancel(m_sendEvent);
        if (m_socket)
        {
            m_socket->Close();
            m_socket = 0;
        }
    }

  private:
    void SendPacket(void)
    {
        Vector reportedPos = GetNode()->GetObject<MobilityModel>()->GetPosition();
        Vector realPos = reportedPos; 
        int isAnomaly = 0;
        Time now = Simulator::Now();

        
        if (m_isAttacker && now >= m_attackStartTime)
        {
            isAnomaly = 1;
            if (m_attackType == 1) 
            {
                reportedPos.x += 500.0;
                reportedPos.y += 500.0;
            }
            else if (m_attackType == 2) 
            {
                double timeSinceAttack = (now - m_attackStartTime).GetSeconds();
                double driftSpeed = 10.0;
                m_spoofedDriftX = driftSpeed * timeSinceAttack;
                reportedPos.x = realPos.x + m_spoofedDriftX;
            }
        }

        
        Ptr<Packet> packet = Create<Packet>(m_packetSize);

        SensorDataTag tag;
        tag.SetNodeId(GetNode()->GetId());
        tag.SetSendTime(now);
        tag.SetReportedPos(reportedPos);
        tag.SetIsAnomaly(isAnomaly);

        packet->AddPacketTag(tag); 

        m_socket->Send(packet);
        NS_LOG_INFO("Sensor " << GetNode()->GetId() << " SENT packet at " << now.GetSeconds());
        m_sendEvent = Simulator::Schedule(m_sendInterval, &SensorApp::SendPacket, this);
    }

    Ptr<Socket> m_socket;
    Address m_peerAddress; 
    uint32_t m_packetSize;
    Time m_sendInterval;
    EventId m_sendEvent;
    Ptr<UniformRandomVariable> m_rand;

    
    bool m_isAttacker;
    int m_attackType;
    Time m_attackStartTime;
    double m_spoofedDriftX;
    double m_spoofedDriftY;
};


int
main(int argc, char* argv[])
{
    
    int runType = 0;
    uint32_t seed = 1;
    std::string csvFileName = "uwsn_data_default.csv";
    double simTime = 2000.0; 
    uint32_t numSensorNodes = 30;

    LogComponentEnable("UwsnDataGenerationFixed", LOG_LEVEL_INFO);
    
    

    

    CommandLine cmd;
    cmd.AddValue("runType", "Loại kịch bản (0: Bth, 1: Jump, 2: Drift)", runType);
    cmd.AddValue("seed", "Giá trị seed cho RNG", seed);
    cmd.AddValue("csvFile", "Tên file CSV output", csvFileName);
    cmd.AddValue("simTime", "Thời gian mô phỏng (s)", simTime);
    cmd.AddValue("numNodes", "Số lượng nút cảm biến", numSensorNodes);
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(runType);

    g_csvFile.open(csvFileName);
    g_csvFile << "RecvTime,NodeID,SendTime,PropDelay,RSSI,Real_X,Real_Y,Real_Z,Reported_X,"
                 "Reported_Y,Reported_Z,Is_Anomaly\n";
    NS_LOG_INFO("Bắt đầu mô phỏng Kịch bản " << runType << ". Output: " << csvFileName);

    
    NodeContainer sinkNode;
    sinkNode.Create(1);
    NodeContainer sensorNodes;
    sensorNodes.Create(numSensorNodes);
    NodeContainer allNodes = NodeContainer(sinkNode, sensorNodes);

    
    
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> sinkAllocator = CreateObject<ListPositionAllocator>();
    sinkAllocator->Add(Vector(500.0, 500.0, 950.0));
    mobility.SetPositionAllocator(sinkAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sinkNode);

    
    
    Ptr<RandomBoxPositionAllocator> sensorAllocator = CreateObject<RandomBoxPositionAllocator>();
    sensorAllocator->SetAttribute("X",
                                  PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
                                      "Min", DoubleValue(0.0), "Max", DoubleValue(1000.0))));
    sensorAllocator->SetAttribute("Y",
                                  PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
                                      "Min", DoubleValue(0.0), "Max", DoubleValue(1000.0))));
    
    sensorAllocator->SetAttribute("Z",
                                  PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
                                      "Min", DoubleValue(0.0), "Max", DoubleValue(900.0))));

    
    mobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed",
        PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
            "Min", DoubleValue(0.5),
            "Max", DoubleValue(2.0))),
        "Pause",
        PointerValue(CreateObjectWithAttributes<ConstantRandomVariable>(
            "Constant", DoubleValue(5.0))),
        "PositionAllocator", 
        PointerValue(sensorAllocator));

    
    mobility.SetPositionAllocator(sensorAllocator);

    
    mobility.Install(sensorNodes);

    
    AquaSimChannelHelper channel = AquaSimChannelHelper::Default();
    
    channel.SetPropagation("ns3::AquaSimRangePropagation");

    AquaSimHelper asHelper = AquaSimHelper::Default();
    asHelper.SetChannel(channel.Create());

    
    asHelper.SetPhy("ns3::AquaSimPhyCmn", "PT", DoubleValue(20.0)); 

    
    asHelper.SetMac("ns3::AquaSimAloha",
                    "AckOn",
                    IntegerValue(0),
                    "MinBackoff",
                    DoubleValue(0.0),
                    "MaxBackoff",
                    DoubleValue(1.5));

    asHelper.SetRouting("ns3::AquaSimRoutingDummy"); 

    
    NetDeviceContainer sinkDevice;
    Ptr<AquaSimNetDevice> sinkDev = CreateObject<AquaSimNetDevice>();
    sinkDevice.Add(asHelper.Create(sinkNode.Get(0), sinkDev));
    sinkDev->GetPhy()->SetTransRange(1500.0); 

    NetDeviceContainer sensorDevices;
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    {
        Ptr<AquaSimNetDevice> dev = CreateObject<AquaSimNetDevice>();
        sensorDevices.Add(asHelper.Create(sensorNodes.Get(i), dev));
        dev->GetPhy()->SetTransRange(1500.0); 
    }

    
    
    PacketSocketHelper socketHelper;
    socketHelper.Install(allNodes);

    

    
    PacketSocketAddress sinkPktAddress;
    
    
    sinkPktAddress.SetAllDevices(); 
    sinkPktAddress.SetProtocol(0); 

    
    Ptr<Node> sink = sinkNode.Get(0);
    TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
    Ptr<Socket> sinkSocket = Socket::CreateSocket(sink, tid);
    sinkSocket->Bind(sinkPktAddress);
    
    sinkSocket->SetRecvCallback(MakeCallback(&SinkSocketRecv));


    
    

    Time sendInterval = Seconds(30.0);
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    {
        Ptr<SensorApp> app = CreateObject<SensorApp>();
        
        app->SetPeer(Address(sinkPktAddress));
        app->SetInterval(sendInterval);

        if (runType > 0 && i < 5)
        {
            app->SetAttacker(runType, Seconds(500.0));
        }

        sensorNodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(simTime));
    }

    
    sinkDev->GetPhy()->TraceConnectWithoutContext("RxEnd", MakeCallback(&PhyRxEndTrace));

    
    NS_LOG_INFO("Start simulating...");
    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();
    Simulator::Destroy();

    g_csvFile.close();
    NS_LOG_INFO("Finish simulating, log saved into" << csvFileName);

    return 0;
}

