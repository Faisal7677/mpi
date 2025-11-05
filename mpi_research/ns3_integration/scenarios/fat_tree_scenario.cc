#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mpi-research-application.h"
#include "ns3/mpi-research-helper.h"
#include <iostream>
#include <vector>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FatTreeScenario");

/**
 * Fat Tree Network Scenario for MPI Research
 *
 * Implements a k-ary fat tree topology commonly used in HPC clusters
 * with multiple levels of network hierarchy.
 */
class FatTreeScenario {
public:
    FatTreeScenario(uint32_t k);
    ~FatTreeScenario();

    void RunSimulation(Time duration = Seconds(10.0));
    void ConfigureFatTree(uint32_t k);
    void InstallMpiApplications();
    void ScheduleCollectiveOperations();
    void CollectResults();

private:
    void CreateFatTreeTopology(uint32_t k);
    void SetupIpAddressing();
    void SetupRouting();
    void ConnectNodes(uint32_t node1, uint32_t node2, DataRate dataRate, Time delay);

    uint32_t m_k;
    NodeContainer m_coreSwitches;
    NodeContainer m_aggregationSwitches;
    NodeContainer m_edgeSwitches;
    NodeContainer m_computeNodes;
    NodeContainer m_allNodes;

    NetDeviceContainer m_allDevices;
    std::vector<NetDeviceContainer> m_links;

    ApplicationContainer m_mpiApps;
    Ptr<MpiResearchHelper> m_mpiHelper;

    // Topology parameters
    uint32_t m_pods;
    uint32_t m_cores;
    uint32_t m_aggregationPerPod;
    uint32_t m_edgePerPod;
    uint32_t m_computePerEdge;
};

FatTreeScenario::FatTreeScenario(uint32_t k)
    : m_k(k) {

    NS_LOG_FUNCTION(this << k);

    m_mpiHelper = CreateObject<MpiResearchHelper>();
    m_pods = k;
    m_cores = (k / 2) * (k / 2);
    m_aggregationPerPod = k / 2;
    m_edgePerPod = k / 2;
    m_computePerEdge = k / 2;
}

FatTreeScenario::~FatTreeScenario() {
    NS_LOG_FUNCTION(this);
}

void FatTreeScenario::RunSimulation(Time duration) {
    NS_LOG_FUNCTION(this << duration);

    NS_LOG_INFO("Starting Fat Tree Simulation with k=" << m_k);
    NS_LOG_INFO("Expected nodes: " << (m_pods * m_edgePerPod * m_computePerEdge +
        m_pods * m_aggregationPerPod + m_cores));

    // Create the fat tree topology
    CreateFatTreeTopology(m_k);

    // Install network stack
    InternetStackHelper stack;
    stack.Install(m_allNodes);

    // Setup IP addressing
    SetupIpAddressing();

    // Setup routing (simplified - would use custom routing for fat tree)
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

void FatTreeScenario::CreateFatTreeTopology(uint32_t k) {
    NS_LOG_FUNCTION(this << k);

    NS_LOG_INFO("Creating Fat Tree topology with k=" << k);

    // Calculate node counts
    uint32_t totalComputeNodes = m_pods * m_edgePerPod * m_computePerEdge;
    uint32_t totalEdgeSwitches = m_pods * m_edgePerPod;
    uint32_t totalAggregationSwitches = m_pods * m_aggregationPerPod;
    uint32_t totalCoreSwitches = m_cores;

    NS_LOG_INFO("Node counts - Compute: " << totalComputeNodes
        << ", Edge: " << totalEdgeSwitches
        << ", Aggregation: " << totalAggregationSwitches
        << ", Core: " << totalCoreSwitches);

    // Create nodes
    m_computeNodes.Create(totalComputeNodes);
    m_edgeSwitches.Create(totalEdgeSwitches);
    m_aggregationSwitches.Create(totalAggregationSwitches);
    m_coreSwitches.Create(totalCoreSwitches);

    // Combine all nodes
    m_allNodes.Add(m_computeNodes);
    m_allNodes.Add(m_edgeSwitches);
    m_allNodes.Add(m_aggregationSwitches);
    m_allNodes.Add(m_coreSwitches);

    NS_LOG_INFO("Created " << m_allNodes.GetN() << " total nodes");

    // Create point-to-point helper
    PointToPointHelper p2p;

    // Connect compute nodes to edge switches
    NS_LOG_INFO("Connecting compute nodes to edge switches");
    for (uint32_t pod = 0; pod < m_pods; ++pod) {
        for (uint32_t edge = 0; edge < m_edgePerPod; ++edge) {
            uint32_t edgeIndex = pod * m_edgePerPod + edge;
            Ptr<Node> edgeSwitch = m_edgeSwitches.Get(edgeIndex);

            for (uint32_t compute = 0; compute < m_computePerEdge; ++compute) {
                uint32_t computeIndex = pod * m_edgePerPod * m_computePerEdge +
                    edge * m_computePerEdge + compute;
                Ptr<Node> computeNode = m_computeNodes.Get(computeIndex);

                // Connect compute node to edge switch
                p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
                p2p.SetChannelAttribute("Delay", TimeValue(MicroSeconds(1)));

                NetDeviceContainer link = p2p.Install(computeNode, edgeSwitch);
                m_allDevices.Add(link);
                m_links.push_back(link);

                NS_LOG_DEBUG("Connected compute " << computeIndex << " to edge " << edgeIndex);
            }
        }
    }

    // Connect edge switches to aggregation switches within pods
    NS_LOG_INFO("Connecting edge to aggregation switches");
    for (uint32_t pod = 0; pod < m_pods; ++pod) {
        for (uint32_t edge = 0; edge < m_edgePerPod; ++edge) {
            uint32_t edgeIndex = pod * m_edgePerPod + edge;
            Ptr<Node> edgeSwitch = m_edgeSwitches.Get(edgeIndex);

            for (uint32_t agg = 0; agg < m_aggregationPerPod; ++agg) {
                uint32_t aggIndex = pod * m_aggregationPerPod + agg;
                Ptr<Node> aggSwitch = m_aggregationSwitches.Get(aggIndex);

                // Connect edge to aggregation switch
                p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("40Gbps")));
                p2p.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

                NetDeviceContainer link = p2p.Install(edgeSwitch, aggSwitch);
                m_allDevices.Add(link);
                m_links.push_back(link);

                NS_LOG_DEBUG("Connected edge " << edgeIndex << " to aggregation " << aggIndex);
            }
        }
    }

    // Connect aggregation switches to core switches
    NS_LOG_INFO("Connecting aggregation to core switches");
    for (uint32_t pod = 0; pod < m_pods; ++pod) {
        for (uint32_t agg = 0; agg < m_aggregationPerPod; ++agg) {
            uint32_t aggIndex = pod * m_aggregationPerPod + agg;
            Ptr<Node> aggSwitch = m_aggregationSwitches.Get(aggIndex);

            for (uint32_t coreGroup = 0; coreGroup < m_k / 2; ++coreGroup) {
                uint32_t coreIndex = agg * (m_k / 2) + coreGroup;
                if (coreIndex < m_cores) {
                    Ptr<Node> coreSwitch = m_coreSwitches.Get(coreIndex);

                    // Connect aggregation to core switch
                    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("40Gbps")));
                    p2p.SetChannelAttribute("Delay", TimeValue(MicroSeconds(5)));

                    NetDeviceContainer link = p2p.Install(aggSwitch, coreSwitch);
                    m_allDevices.Add(link);
                    m_links.push_back(link);

                    NS_LOG_DEBUG("Connected aggregation " << aggIndex << " to core " << coreIndex);
                }
            }
        }
    }
}

void FatTreeScenario::SetupIpAddressing() {
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Setting up IP addressing");

    Ipv4AddressHelper address;

    // Assign IP addresses to compute-edge links
    address.SetBase("10.1.0.0", "255.255.255.0");
    uint32_t interfaceCount = 0;

    for (uint32_t pod = 0; pod < m_pods; ++pod) {
        for (uint32_t edge = 0; edge < m_edgePerPod; ++edge) {
            for (uint32_t compute = 0; compute < m_computePerEdge; ++compute) {
                uint32_t linkIndex = pod * m_edgePerPod * m_computePerEdge +
                    edge * m_computePerEdge + compute;
                if (linkIndex < m_links.size()) {
                    Ipv4InterfaceContainer interfaces = address.Assign(m_links[linkIndex]);
                    address.NewNetwork();
                    interfaceCount++;
                }
            }
        }
    }

    NS_LOG_INFO("Assigned IP addresses to " << interfaceCount << " interfaces");
}

void FatTreeScenario::SetupRouting() {
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Setting up routing");

    // Use global routing for simplicity
    // In a real implementation, we would use custom routing for fat tree
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Routing tables populated");
}

void FatTreeScenario::InstallMpiApplications() {
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Installing MPI applications on compute nodes");

    // Configure MPI helper
    m_mpiHelper->SetNetworkTopology(FAT_TREE);
    m_mpiHelper->SetWorldSize(m_computeNodes.GetN());
    m_mpiHelper->SetBaseComputationDelay(MicroSeconds(100));
    m_mpiHelper->SetBaseCommunicationDelay(MicroSeconds(10));
    m_mpiHelper->EnableDetailedLogging(true);

    // Install on compute nodes only
    m_mpiApps = m_mpiHelper->Install(m_computeNodes);

    NS_LOG_INFO("Installed MPI applications on " << m_mpiApps.GetN() << " compute nodes");
}

void FatTreeScenario::ScheduleCollectiveOperations() {
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Scheduling collective operations");

    // Schedule various collective operations
    Time startTime = Seconds(1.0);

    // Broadcast operations from different roots
    for (uint32_t i = 0; i < 5; ++i) {
        Time operationTime = startTime + Seconds(i * 1.5);
        uint32_t rootRank = i * (m_computeNodes.GetN() / 5);
        uint32_t dataSize = 1024 * (1 << i); // 1KB, 2KB, 4KB, 8KB, 16KB

        m_mpiHelper->ScheduleBroadcast(m_mpiApps, rootRank, dataSize, operationTime);

        NS_LOG_INFO("Scheduled broadcast from root " << rootRank
            << " with size " << dataSize << " at " << operationTime.GetSeconds() << "s");
    }

    // Allreduce operations
    for (uint32_t i = 0; i < 3; ++i) {
        Time operationTime = startTime + Seconds(7 + i * 2.0);
        uint32_t dataSize = 2048 * (1 << i); // 2KB, 4KB, 8KB

        m_mpiHelper->ScheduleAllreduce(m_mpiApps, dataSize, operationTime);

        NS_LOG_INFO("Scheduled allreduce with size " << dataSize
            << " at " << operationTime.GetSeconds() << "s");
    }

    // Barrier operation
    m_mpiHelper->ScheduleBarrier(m_mpiApps, startTime + Seconds(14.0));

    NS_LOG_INFO("Scheduled " << 5 + 3 + 1 << " collective operations");
}

void FatTreeScenario::CollectResults() {
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Collecting simulation results");

    // Generate performance report
    m_mpiHelper->GeneratePerformanceReport(m_mpiApps, "fat_tree_performance.csv");

    // Collect and log summary statistics
    m_mpiHelper->CollectPerformanceMetrics(m_mpiApps);

    NS_LOG_INFO("Results collection completed");
}

int main(int argc, char* argv[]) {
    LogComponentEnable("FatTreeScenario", LOG_LEVEL_INFO);
    LogComponentEnable("MpiResearchApplication", LOG_LEVEL_INFO);
    LogComponentEnable("MpiResearchHelper", LOG_LEVEL_INFO);

    uint32_t k = 4; // Default k-ary fat tree

    CommandLine cmd;
    cmd.AddValue("k", "Fat tree k parameter (even number)", k);
    cmd.Parse(argc, argv);

    // Validate k parameter
    if (k % 2 != 0) {
        std::cerr << "Error: k must be an even number" << std::endl;
        return 1;
    }

    NS_LOG_INFO("Starting Fat Tree simulation with k=" << k);

    FatTreeScenario scenario(k);
    scenario.RunSimulation(Seconds(20.0));

    NS_LOG_INFO("Fat Tree simulation completed successfully");

    return 0;
}