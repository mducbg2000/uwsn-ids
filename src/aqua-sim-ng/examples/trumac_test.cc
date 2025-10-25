#include "ns3/aqua-sim-ng-module.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h" // Thêm thư viện Applications
#include "ns3/aqua-sim-propagation.h" // Thêm header cho Propagation
#include "ns3/aqua-sim-channel.h"     // Thêm header cho Channel
#include "ns3/aqua-sim-header-mac.h"  // Thêm header cho MAC
#include "ns3/aqua-sim-address.h"     // Thêm header cho Address
#include "ns3/aqua-sim-application.h" // Thêm header cho Application (cho SensorApp)

#include <fstream>
#include <iostream>
#include <sstream>

/*
 * ===================================================================
 * Kịch bản mô phỏng sinh dữ liệu cho UWSN IDS
 * * PHIÊN BẢN ĐÃ SỬA LỖI cho Aqua-Sim-NG 1.1 (Dùng Tracing)
 * * Tác giả: Đối tác lập trình (AI)
 * ===================================================================
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UwsnDataGenerationFixed");

// Biến toàn cục để ghi file CSV
std::ofstream g_csvFile;

// --- SỬA LỖI: Bước 1: Tạo một Tag tùy chỉnh để mang dữ liệu ---
class SensorDataTag : public Tag
{
  public:
    // ... (Các hàm Get/Set) ...
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

    // ... (Các hàm bắt buộc của NS3 Tag) ...
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
        i.WriteU32(m_isAnomaly); // Dùng U32 cho int
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

// --- SỬA LỖI: Bước 2: Xóa SinkApp (Không cần nữa) ---

// --- SỬA LỖI: Bước 3: Hàm Trace Sink (Hàm này sẽ ghi CSV) ---
// Hàm này được gọi bởi Lớp Vật lý (PHY) của Sink
void
PhyRxEndTrace(Ptr<const Packet> packet, double rssi, Vector senderPos, Time propDelay)
{
    // SỬA LỖI (KHÔNG CÓ DỮ LIỆU): Thêm log để kiểm tra
    NS_LOG_INFO("PhyRxEndTrace CALLED at time " << Simulator::Now().GetSeconds());

    // --- 1. Đọc Tag tùy chỉnh từ gói tin ---
    SensorDataTag tag;
    if (!packet->PeekPacketTag(tag))
    {
        NS_LOG_WARN("Received packet at PHY without SensorDataTag");
        return;
    }

    // --- 2. Lấy dữ liệu từ Tag (Dữ liệu báo cáo) ---
    uint32_t nodeId = tag.GetNodeId();
    Time sendTime = tag.GetSendTime();
    Vector reportedPos = tag.GetReportedPos();
    int isAnomaly = tag.GetIsAnomaly();

    // --- 3. Lấy dữ liệu từ tham số Trace (Bằng chứng vật lý) ---
    // (rssi, senderPos, propDelay đã được cung cấp)
    Time recvTime = Simulator::Now();

    // --- 4. Ghi ra file CSV ---
    g_csvFile << recvTime.GetSeconds() << "," << nodeId << "," << sendTime.GetSeconds() << ","
              << propDelay.GetSeconds() << "," << rssi << "," << senderPos.x << "," << senderPos.y
              << "," << senderPos.z << "," << reportedPos.x << "," << reportedPos.y << ","
              << reportedPos.z << "," << isAnomaly << "\n";

    // SỬA LỖI (KHÔNG CÓ DỮ LIỆU): Thêm flush() để đảm bảo dữ liệu được ghi
    g_csvFile.flush();
}

// --- Ứng dụng tại Nút Cảm biến (Gửi tin + Giả mạo) ---
// SỬA LỖI: Dùng PacketSocket
class SensorApp : public Application
{
  public:
    SensorApp()
        : m_socket(0),
          m_peerAddress(), // SỬA LỖI: Dùng Address
          m_packetSize(100), // Kích thước gói tin (bytes), Tag không chiếm dụng
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

    // SỬA LỖI: Nhận Address (L2) thay vì Ipv4Address (L3)
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
            // SỬA LỖI: Dùng PacketSocketFactory
            TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);

            // SỬA LỖI (KHÔNG CÓ DỮ LIỆU): Xóa lệnh Bind()
            // if (m_socket->Bind() == -1)
            // {
            //     NS_FATAL_ERROR("Failed to bind socket");
            // }

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
        Vector realPos = reportedPos; // Lưu vị trí thật
        int isAnomaly = 0;
        Time now = Simulator::Now();

        // --- Logic Tấn công (Nếu được kích hoạt) ---
        if (m_isAttacker && now >= m_attackStartTime)
        {
            isAnomaly = 1;
            if (m_attackType == 1) // Kịch bản 1: "Jump" Attack
            {
                reportedPos.x += 500.0;
                reportedPos.y += 500.0;
            }
            else if (m_attackType == 2) // Kịch bản 2: "Drift" Attack
            {
                double timeSinceAttack = (now - m_attackStartTime).GetSeconds();
                double driftSpeed = 10.0;
                m_spoofedDriftX = driftSpeed * timeSinceAttack;
                reportedPos.x = realPos.x + m_spoofedDriftX;
            }
        }

        // SỬA LỖI: Bước 4: Gửi dữ liệu bằng Tag, không dùng Payload
        Ptr<Packet> packet = Create<Packet>(m_packetSize);

        SensorDataTag tag;
        tag.SetNodeId(GetNode()->GetId());
        tag.SetSendTime(now);
        tag.SetReportedPos(reportedPos);
        tag.SetIsAnomaly(isAnomaly);

        packet->AddPacketTag(tag); // Thêm tag vào gói tin

        m_socket->Send(packet);
        NS_LOG_INFO("Sensor " << GetNode()->GetId() << " SENT packet at " << now.GetSeconds());
        m_sendEvent = Simulator::Schedule(m_sendInterval, &SensorApp::SendPacket, this);
    }

    Ptr<Socket> m_socket;
    Address m_peerAddress; // SỬA LỖI: Đổi sang Address
    uint32_t m_packetSize;
    Time m_sendInterval;
    EventId m_sendEvent;
    Ptr<UniformRandomVariable> m_rand;

    // Attack parameters
    bool m_isAttacker;
    int m_attackType;
    Time m_attackStartTime;
    double m_spoofedDriftX;
    double m_spoofedDriftY;
};

// --- Hàm Main ---
int
main(int argc, char* argv[])
{
    // SỬA LỖI: Khai báo biến trước khi sử dụng
    int runType = 0;
    uint32_t seed = 1;
    std::string csvFileName = "uwsn_data_default.csv";
    double simTime = 2000.0; // Giây
    uint32_t numSensorNodes = 30;

    LogComponentEnable("UwsnDataGenerationFixed", LOG_LEVEL_INFO);
    LogComponentEnable("SensorApp", LOG_LEVEL_INFO); // Thêm log cho SensorApp

    // SỬA LỖI: Xóa dòng Packet::RegisterPacketTag

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

    // --- 1. Tạo Nodes ---
    NodeContainer sinkNode;
    sinkNode.Create(1);
    NodeContainer sensorNodes;
    sensorNodes.Create(numSensorNodes);
    NodeContainer allNodes = NodeContainer(sinkNode, sensorNodes);

    // --- 2. Cài đặt Mobility ---
    // (Phần Mobility của bạn đã đúng, giữ nguyên)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> sinkAllocator = CreateObject<ListPositionAllocator>();
    sinkAllocator->Add(Vector(500.0, 500.0, 950.0));
    mobility.SetPositionAllocator(sinkAllocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sinkNode);

    // SỬA LỖI RUNTIME: Dùng "RandomBoxPositionAllocator" 3D
    // 1. Tạo PositionAllocator 3D (cho cả vị trí ban đầu VÀ waypoints)
    Ptr<RandomBoxPositionAllocator> sensorAllocator = CreateObject<RandomBoxPositionAllocator>();
    sensorAllocator->SetAttribute("X",
                                  PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
                                      "Min", DoubleValue(0.0), "Max", DoubleValue(1000.0))));
    sensorAllocator->SetAttribute("Y",
                                  PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
                                      "Min", DoubleValue(0.0), "Max", DoubleValue(1000.0))));
    // SỬA LỖI (BIÊN DỊCH): Sửa lỗi đánh máy DValue -> DoubleValue
    sensorAllocator->SetAttribute("Z",
                                  PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
                                      "Min", DoubleValue(0.0), "Max", DoubleValue(900.0))));

    // 2. Đặt MobilityModel VÀ gán PositionAllocator (cho waypoints) cho nó
    mobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed",
        PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
            "Min", DoubleValue(0.5),
            "Max", DoubleValue(2.0))),
        "Pause",
        PointerValue(CreateObjectWithAttributes<ConstantRandomVariable>(
            "Constant", DoubleValue(5.0))),
        "PositionAllocator", // SỬA LỖI: Gán bộ cấp phát cho Waypoints
        PointerValue(sensorAllocator));

    // 3. Gán bộ cấp phát cho MobilityHelper (cho vị trí ban đầu)
    mobility.SetPositionAllocator(sensorAllocator);

    // 4. Cài đặt mobility
    mobility.Install(sensorNodes);

    // --- 3. Cài đặt Aqua-Sim-NG Stack (SỬA LỖI theo broadcastMAC_example.cc) ---
    AquaSimChannelHelper channel = AquaSimChannelHelper::Default();
    // SỬA LỖI RUNTIME: Dùng "Range" propagation
    channel.SetPropagation("ns3::AquaSimRangePropagation");

    AquaSimHelper asHelper = AquaSimHelper::Default();
    asHelper.SetChannel(channel.Create());

    // SỬA LỖI (KHÔNG CÓ DỮ LIỆU): Thêm công suất phát (PT)
    asHelper.SetPhy("ns3::AquaSimPhyCmn", "PT", DoubleValue(20.0)); // 20.0 Watts

    // SỬA LỖI (KHÔNG CÓ DỮ LIỆU): Thêm Backoff cho Aloha, giống hệt ví dụ
    asHelper.SetMac("ns3::AquaSimAloha",
                    "AckOn",
                    IntegerValue(0),
                    "MinBackoff",
                    DoubleValue(0.0),
                    "MaxBackoff",
                    DoubleValue(1.5));

    asHelper.SetRouting("ns3::AquaSimRoutingDummy"); // Cần thiết

    // SỬA LỖI: Dùng vòng lặp Create() thủ công
    NetDeviceContainer sinkDevice;
    Ptr<AquaSimNetDevice> sinkDev = CreateObject<AquaSimNetDevice>();
    sinkDevice.Add(asHelper.Create(sinkNode.Get(0), sinkDev));
    sinkDev->GetPhy()->SetTransRange(1500.0); // SỬA LỖI: Đặt tầm phát sóng

    NetDeviceContainer sensorDevices;
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    {
        Ptr<AquaSimNetDevice> dev = CreateObject<AquaSimNetDevice>();
        sensorDevices.Add(asHelper.Create(sensorNodes.Get(i), dev));
        dev->GetPhy()->SetTransRange(1500.0); // SỬA LỖI: Đặt tầm phát sóng
    }

    // --- 4. Cài đặt Internet Stack (SỬA LỖI) ---
    // SỬA LỖI: Dùng PacketSocketHelper thay vì InternetStack
    PacketSocketHelper socketHelper;
    socketHelper.Install(allNodes);

    // SỬA LỖI: Bỏ Ipv4AddressHelper

    // --- 5. Cài đặt Ứng dụng ---
    // SỬA LỖI (KHÔNG CÓ DỮ LIỆU): Tạo PacketSocketAddress đầy đủ
    PacketSocketAddress sinkPktAddress;
    sinkPktAddress.SetPhysicalAddress(sinkDevice.Get(0)->GetAddress());
    sinkPktAddress.SetProtocol(0); // Giao thức 0 = bất kỳ

    // SỬA LỖI: Xóa SinkApp

    Time sendInterval = Seconds(30.0);
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    {
        Ptr<SensorApp> app = CreateObject<SensorApp>();
        // SỬA LỖI: Truyền Address L2 đầy đủ
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

    // --- SỬA LỖI: Bước 5: Kết nối Trace Sink ---
    sinkDev->GetPhy()->TraceConnectWithoutContext("RxEnd", MakeCallback(&PhyRxEndTrace));

    // --- 6. Chạy mô phỏng ---
    NS_LOG_INFO("Bắt đầu mô phỏng...");
    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();
    Simulator::Destroy();

    g_csvFile.close();
    NS_LOG_INFO("Mô phỏng kết thúc. Dữ liệu đã được lưu vào " << csvFileName);

    return 0;
}

