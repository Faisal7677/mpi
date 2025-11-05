#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mpi-research-application.h"
#include "ns3/mpi-research-helper.h"
#include <iostream>
#include <vector>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TorusScenario");

/**
 * 2D/3D Torus Network Scenario for MPI Research
 *
 * Implements torus network topologies commonly used in HPC systems
 * with wrap-around connections in multiple dimensions.
 */
class TorusScenario {
public:
    TorusScenario(uint32_t dimX, uint32_t dimY, uint32_t dimZ = 1);
    ~TorusScenario();

    void RunSimulation(Time duration = Seconds(10.0));
    void ConfigureTorus(uint32_t dimX, uint32_t dimY, uint32_t dimZ);
    void InstallMpiApplications();
    void ScheduleCollectiveOperations();
    void CollectResults();

private:
    void Create2DTorusTopology(uint32_t dimX, uint32_t dimY);
    void Create3DTorusTopology(uint32_t dimX, uint32_t dimY, uint32_t dimZ);
    void SetupIpAddressing();
    void SetupRouting();
    void ConnectTorusNodes(uint32_t node1, uint32_t node2, DataRate dataRate, Time delay);

    uint32_t m_dimX, m_dimY, m_dimZ;
    bool m_is3D;

    NodeContainer m_allNodes;
    NetDeviceContainer m_allDevices;
    std::vector<NetDeviceContainer> m_links;

    ApplicationContainer m_mpiApps;
    Ptr<MpiResearchHelper> m_mpiHelper;
};

TorusScenario::TorusScenario(uint32_t dimX, uint32_t dimY, uint32_t dimZ)
    : m_dimX(dimX), m_dimY(dimY), m_dimZ(dimZ), m_is3D(dimZ > 1) {

    NS_LOG_FUNCTION(this << dimX << dimY << dimZ);

    m_mpiHelper = CreateObject<MpiResearchHelper>();
}

TorusScenario::~TorusScenario() {
    NS_LOG_FUNCTION(this);
}

void TorusScenario::RunSimulation(Time duration) {
    NS_LOG_FUNCTION(this << duration);

    NS_LOG_INFO("Starting Torus Simulation with dimensions: "
        << m_dimX << "x" << m_dimY << (m_is3D ? "x" + std::to_string(m_dimZ) : ""));

    // Create the torus topology
    if (m_is3D) {
        Create3DTorusTopology(m_dimX, m_dimY, m_dimZ);
    }
    else {
        Create2DTorusTopology(m_dimX, m_dimY);
    }

    // Install network stack
    InternetStackHelper stack;
    stack.Install(m_allNodes);

    // Setup IP addressing
    SetupIpAddressing();

    // Setup routing
    SetupRouting();

    // Install MPI applications
    InstallMpiApplications();

    // Schedule collective operations
    ScheduleCollectiveOperations();

    // Run simulation
    NS_LOG_INFO("Running simulation for " << duration.GetSeconds() << " seconds");
    Simulator::Stop(duration);
    Simulator::Run();

    // Collect results
    CollectResults();

    Simulator::Destroy();
}

void TorusScenario::Create2DTorusTopology(uint32_t dimX, uint32_t dimY) {
    NS_LOG_FUNCTION(this << dimX << dimY);

    uint32_t totalNodes = dimX * dimY;
    NS_LOG_INFO("Creating 2D Torus topology: " << dimX << "x" << dimY
        << " = " << totalNodes << " nodes");

    // Create all nodes
    m_allNodes.Create(totalNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    // Connect nodes in 2D torus with wrap-around
    for (uint32_t y = 0; y < dimY; ++y) {
        for (uint32_t x = 0; x < dimX; ++x) {
            uint32_t currentNode = y * dimX + x;

            // Connect to right neighbor (with wrap-around)
            uint32_t rightX = (x + 1) % dimX;
            uint32_t rightNode = y * dimX + rightX;

            if (currentNode < rightNode) { // Avoid duplicate connections
                NetDeviceContainer link = p2p.Install(m_allNodes.Get(currentNode),
                    m_allNodes.Get(rightNode));
                m_allDevices.Add(link);
                m_links.push_back(link);

                NS_LOG_DEBUG("Connected node " << currentNode << " to right node " << rightNode);
            }

            // Connect to bottom neighbor (with wrap-around)
            uint32_t bottomY = (y + 1) % dimY;
            uint32_t bottomNode = bottomY * dimX + x;

            if (currentNode < bottomNode) { // Avoid duplicate connections
                NetDeviceContainer link = p2p.Install(m_allNodes.Get(currentNode),
                    m_allNodes.Get(bottomNode));
                m_allDevices.Add(link);
                m_links.push_back(link);

                NS_LOG_DEBUG("Connected node " << currentNode << " to bottom node " << bottomNode);
            }
        }
    }

    NS_LOG_INFO("Created 2D torus with " << m_links.size() << " links");
}

void TorusScenario::Create3DTorusTopology(uint32_t dimX, uint32_t dimY, uint32_t dimZ) {
    NS_LOG_FUNCTION(this << dimX << dimY << dimZ);

    uint32_t totalNodes = dimX * dimY * dimZ;
    NS_LOG_INFO("Creating 3D Torus topology: " << dimX << "x" << dimY << "x" << dimZ
        << " = " << totalNodes << " nodes");

    // Create all nodes
    m_allNodes.Create(totalNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    // Connect nodes in 3D torus with wrap-around
    for (uint32_t z = 0; z < dimZ; ++z) {
        for (uint32_t y = 0; y < dimY; ++y) {
            for (uint32_t x = 0; x < dimX; ++x) {
                uint32_t currentNode = z * dimX * dimY + y * dimX + x;

                // Connect to X+ neighbor (with wrap-around)
                uint32_t rightX = (x + 1) % dimX;
                uint32_t rightNode = z * dimX * dimY + y * dimX + rightX;

                if (currentNode < rightNode) {
                    NetDeviceContainer link = p2p.Install(m_allNodes.Get(currentNode),
                        m_allNodes.Get(rightNode));
                    m_allDevices.Add(link);
                    m_links.push_back(link);
                }

                // Connect to Y+ neighbor (with wrap-around)
                uint32_t bottomY = (y + 1) % dimY;
                uint32_t bottomNode = z * dimX * dimY + bottomY * dimX + x;

                if (currentNode < bottomNode) {
                    NetDeviceContainer link = p2p.Install(m_allNodes.Get(currentNode),
                        m_allNodes.Get(bottomNode));
                    m_allDevices.Add(link);
                    m_links.push_back(link);
                }

                // Connect to Z+ neighbor (with wrap-around)
                uint32_t frontZ = (z + 1) % dimZ;
                uint32_t frontNode = frontZ * dimX * dimY + y * dimX + x;

                if (currentNode < frontNode) {
                    NetDeviceContainer link = p2p.Install(m_allNodes.Get(currentNode),
                        m_allNodes.Get(frontNode));
                    m_allDevices.Add(link);
                    m_links.push_back(link);
                }
            }
        }
    }

    NS_LOG_INFO("Created 3D torus with " << m_links.size() << " links");
}

void TorusScenario::SetupIpAddressing() {
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Setting up IP addressing for torus topology");

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");

    for (uint32_t i = 0; i < m_links.size(); ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(m_links[i]);
        address.NewNetwork();
    }

    NS_LOG_INFO("Assigned IP addresses to " << m_links.size() << " links");
}

void TorusScenario::SetupRouting() {
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Setting up routing for torus topology");

    // Use global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Routing tables populated");
}

void TorusScenario::InstallMpiApplications() {
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Installing MPI applications on torus nodes");

    // Configure MPI helper for torus topology
    if (m_is3D) {
        m_mpiHelper->SetNetworkTopology(TORUS_3D);
    }
    else {
        m_mpiHelper->SetNetworkTopology(TORUS_2D);
    }

    m_mpiHelper->SetWorldSize(m_allNodes.GetN());
    m_mpiHelper->SetBaseComputationDelay(MicroSeconds(50));
    m_mpiHelper->SetBaseCommunicationDelay(MicroSeconds(5));
    m_mpiHelper->EnableDetailedLogging(true);

    // Install on all nodes
    m_mpiApps = m_mpiHelper->Install(m_allNodes);

    NS_LOG_INFO("Installed MPI applications on " << m_mpiApps.GetN() << " nodes");
}

void TorusScenario::ScheduleCollectiveOperations() {
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Scheduling collective operations for torus topology");

    Time startTime = Seconds(1.0);

    // Schedule operations that benefit from torus topology
    // Broadcast operations from different locations
    std::vector<uint32_t> rootPositions;

    if (m_is3D) {
        // 3D center and corners
        rootPositions.push_back((m_dimZ / 2) * m_dimX * m_dimY + (m_dimY / 2) * m_dimX + (m_dimX / 2)); // Center
        rootPositions.push_back(0); // Corner (0,0,0)
        rootPositions.push_back(m_dimX * m_dimY * m_dimZ - 1); // Opposite corner
    }
    else {
        // 2D center and corners
        rootPositions.push_back((m_dimY / 2) * m_dimX + (m_dimX / 2)); // Center
        rootPositions.push_back(0); // Corner (0,0)
        rootPositions.push_back(m_dimX * m_dimY - 1); // Opposite corner
    }

    for (uint32_t i = 0; i < rootPositions.size(); ++i) {
        Time operationTime = startTime + Seconds(i * 2.0);
        uint32_t dataSize = 2048 * (1 << i); // 2KB, 4KB, 8KB

        m_mpiHelper->ScheduleBroadcast(m_mpiApps, rootPositions[i], dataSize, operationTime);

        NS_LOG_INFO("Scheduled broadcast from root " << rootPositions[i]
            << " with size " << dataSize << " at " << operationTime.GetSeconds() << "s");
    }

    // Allreduce operations with different data sizes
    for (uint32_t i = 0; i < 4; ++i) {
        Time operationTime = startTime + Seconds(7 + i * 1.5);
        uint32_t dataSize = 1024 * (1 << i); // 1KB, 2KB, 4KB, 8KB

        m_mpiHelper->ScheduleAllreduce(m_mpiApps, dataSize, operationTime);

        NS_LOG_INFO("Scheduled allreduce with size " << dataSize
            << " at " << operationTime.GetSeconds() << "s");
    }

    // Topology-aware operations
    for (uint32_t i = 0; i < 2; ++i) {
        Time operationTime = startTime + Seconds(14 + i * 2.0);
        uint32_t rootRank = i * (m_allNodes.GetN() / 2);
        uint32_t dataSize = 8192; // 8KB

        // Schedule topology-aware broadcast
        for (uint32_t j = 0; j < m_mpiApps.GetN(); ++j) {
            Ptr<MpiResearchApplication> app = m_mpiApps.Get(j)->GetObject<MpiResearchApplication>();
            if (app) {
                Simulator::Schedule(operationTime, &MpiResearchApplication::SimulateTopologyAwareBroadcast,
                    app, rootRank, dataSize);
            }
        }

        NS_LOG_INFO("Scheduled topology-aware broadcast from root " << rootRank
            << " at " << operationTime.GetSeconds() << "s");
    }

    NS_LOG_INFO("Scheduled " << (rootPositions.size() + 4 + 2) << " collective operations");
}

void TorusScenario::CollectResults() {
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Collecting torus simulation results");

    // Generate performance report
    std::string filename = m_is3D ?
        "torus3d_performance.csv" : "torus2d_performance.csv";
    m_mpiHelper->GeneratePerformanceReport(m_mpiApps, filename);

    // Collect summary statistics
    m_mpiHelper->CollectPerformanceMetrics(m_mpiApps);

    // Log topology-specific metrics
    NS_LOG_INFO("Torus Topology: " << m_dimX << "x" << m_dimY
        << (m_is3D ? "x" + std::to_string(m_dimZ) : ""));
    NS_LOG_INFO("Total nodes: " << m_allNodes.GetN());
    NS_LOG_INFO("Total links: " << m_links.size());

    NS_LOG_INFO("Torus simulation results collection completed");
}

int main(int argc, char* argv[]) {
    LogComponentEnable("TorusScenario", LOG_LEVEL_INFO);
    LogComponentEnable("MpiResearchApplication", LOG_LEVEL_INFO);

    uint32_t dimX = 4, dimY = 4, dimZ = 1;
    bool threeD = false;

    CommandLine cmd;
    cmd.AddValue("x", "X dimension", dimX);
    cmd.AddValue("y", "Y dimension", dimY);
    cmd.AddValue("z", "Z dimension (0 for 2D)", dimZ);
    cmd.AddValue("3d", "Enable 3D torus", threeD);
    cmd.Parse(argc, argv);

    if (threeD && dimZ == 1) {
        dimZ = 2; // Default 3D size
    }

    NS_LOG_INFO("Starting Torus simulation with dimensions: "
        << dimX << "x" << dimY << (dimZ > 1 ? "x" + std::to_string(dimZ) : ""));

    TorusScenario scenario(dimX, dimY, dimZ);
    scenario.RunSimulation(Seconds(20.0));

    NS_LOG_INFO("Torus simulation completed successfully");

    return 0;
}