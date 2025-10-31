// XR scenario with 3GPP traffic generator and buildings support

#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/boolean.h"
#include "ns3/config-store-module.h"
#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-enb-rrc.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nr-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/xr-traffic-mixer-helper.h"
#include <ns3/traffic-generator-ngmn-voip.h>
#include <ns3/traffic-generator-helper.h>

#include <vector>
#include <fstream>
#include <iomanip>
#include <random>

#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/itu-r-1411-los-propagation-loss-model.h"

// Buildings module includes
#include "ns3/buildings-helper.h"
#include "ns3/buildings-module.h"
#include "ns3/buildings-propagation-loss-model.h"
#include "ns3/hybrid-buildings-propagation-loss-model.h"

#include "httplib.h"

#include "ns3/simulation-data-logger.h"

bool LOG_ENABLE = true;

uint32_t ueNum;

/**
 * \file nr-sched-o-ran.cc
 * \ingroup examples
 * \brief Simple topology consisting of 1 GNB and various UEs.
 *  Can be configured with different 3GPP XR traffic generators (by using
 *  XR traffic mixer helper).
 *
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrSchedORAN");

ns3::Ns3DrlInterface& drlInterface = ns3::Ns3DrlInterface::GetInstance();

void connectToRestServer(int port, int ueNum, const std::string model, const std::string envType) {
    httplib::Client cli("http://localhost:" + std::to_string(port));

    // Create config JSON
    std::string json_str = R"({
        "num_ues": )" + std::to_string(ueNum) + R"(,
        "model": ")" + model + R"(",
        "env_type": ")" + envType + R"("
    })";

    std::cout << "JSON: " << json_str << std::endl;
    // Send JSON to the REST server
    auto res = cli.Post("/configure", json_str, "application/json");

    if (res && res->status == 200) {
        std::cout << "Response from server: " << res->body << std::endl;
    } else {
        std::cerr << "Error connecting to REST server: " << res.error() << std::endl;
    }
}

class PeriodicThroughputTracer {
public:
    PeriodicThroughputTracer(uint32_t ueId, double firstCallTime = 2.0, double intervalSeconds = 2.0)
        : m_ueId(ueId), m_interval(intervalSeconds), m_rxBytes(0), 
          outputFile_thput("thput_avg_window_id" + std::to_string(ueId) + ".csv") {
        
        // Escribir header del archivo
        outputFile_thput << "ue\tthput_mbps\trxBytes\ttime_s" << std::endl;
        
        Simulator::Schedule(Seconds(firstCallTime), &PeriodicThroughputTracer::UpdateThroughput, this);
    }
    
    ~PeriodicThroughputTracer() {
        if (outputFile_thput.is_open()) {
            outputFile_thput.close();
        }
    }
    
    void RxTrace(Ptr<const Packet> pkt, const Address& from) {
        m_rxBytes += pkt->GetSize();
    }
    
private:
    void UpdateThroughput() {
        // Calcular throughput en bits por segundo
        double throughputBps = (m_rxBytes * 8.0) / m_interval;
        double throughputMbps = throughputBps * 1e-6;
        
        // Escribir al archivo CSV
        outputFile_thput << m_ueId << "\t" 
                        << throughputMbps << "\t"
                        << m_rxBytes << "\t"
                        << Simulator::Now().GetSeconds() << std::endl;
        
        // Actualizar métricas DRL
        drlInterface.UpdateMetricsThput(m_ueId - 1, throughputBps);
        
        // std::cout << Simulator::Now().GetMilliSeconds() 
        //           << "\t| UE" << m_ueId 
        //           << " thput ventana = " << throughputMbps << " Mbps" << std::endl;
        
        // Reset contador
        m_rxBytes = 0;
        
        // Programar siguiente medición
        Simulator::Schedule(Seconds(m_interval), &PeriodicThroughputTracer::UpdateThroughput, this);
    }
    
    uint32_t m_ueId;
    double m_interval;
    uint64_t m_rxBytes;
    std::ofstream outputFile_thput;
};

class Tracer {
    public:
        double id;
        double time = 2.4;
        bool end = false;
        double rxBytes = 0;
        double rxBytes_total = 0;
        std::ofstream outputFile_thput;
        Tracer(double id) : outputFile_thput("thput_avg_window_id" + std::to_string(int(id)) + ".csv") {
            this->id = id;
            // outputFile_thput << "ue\tthput\trxBytes\tduration\n";
        }
        ~Tracer() {
            if (outputFile_thput.is_open()) {
                outputFile_thput.close();
            }
        }
        // Calcula el thput cada avg. window y lo escribe en outputFile_thput
        void rxTracer_avgWindow(Ptr<const Packet> pkt, const Address & from) {
            // std::cout << "Starting rxTracer_avgWindow for UE " << id << std::endl;
            if (Simulator::Now().GetSeconds() > time){
                // std::cout << "ue" << id << " thput=" << rxBytes*8.0/2*1e-6 << " time=" << time << " rxBytes=" << rxBytes << " Simulator::Now=" << Simulator::Now().GetSeconds() << std::endl;
                if (rxBytes >= 0) {
                outputFile_thput << id << "\t"
                                << rxBytes * 8.0 / 2 * 1e-6 << "\t"
                                << rxBytes << "\t"
                                << Simulator::Now().GetSeconds() << std::endl;
                                // std::cout << Simulator::Now().GetMilliSeconds() << "\t| UE" << id << " update thput=" << rxBytes * 8.0 / 2 * 1e-6 << "Mbps" << std::endl;
                                // drlInterface.UpdateMetricsThput(ueNum - id, rxBytes*8.0/2); // TODO ver si hace falta ueNum-id o id directamente
                                drlInterface.UpdateMetricsThput(id-1, rxBytes*8.0/2);
                                std::cout << Simulator::Now().GetMilliSeconds() << " | UE" << ueNum - id << " thput ventana =" << rxBytes * 8.0 / 2 * 1e-6 << "Mbps" << std::endl;
                } else if (rxBytes == -1) {
                    // // outputFile_thput << id << "\t"
                    // //             << "x\t"
                    // //             << "x\t"
                    // //             << Simulator::Now().GetSeconds() << std::endl;
                    std::cerr << "Error: rxBytes is invalid. id: " << id << ", rxBytes: " << rxBytes << std::endl;
                } else {
                    // std::cout << Simulator::Now().GetMilliSeconds() << "\t| UE" << id << " update thput= 0" << std::endl;
                    // drlInterface.UpdateMetricsThput(ueNum - id, 0);
                    drlInterface.UpdateMetricsThput(id-1, 0);
                    std::cout << Simulator::Now().GetMilliSeconds() << " | UE" << ueNum - id << " thput ventana =" << rxBytes * 8.0 / 2 * 1e-6 << "Mbps" << std::endl;
                    // // outputFile_thput << id << "\t"
                    // //             << "x\t"
                    // //             << "x\t"
                    // //             << Simulator::Now().GetSeconds() << std::endl;
                    std::cerr << "Error: rxBytes is invalid. id: " << id << ", rxBytes: " << rxBytes << std::endl;
                }
                time = floor(Simulator::Now().GetSeconds()) + 2;
                rxBytes = 0;
            }
            rxBytes += pkt->GetSize();
            // std::cout << Simulator::Now().GetSeconds() << " | ue" << id << " rxBytes=" << rxBytes << ", he sumado " << pkt->GetSize() << std::endl;

            // TODO
            // drlInterface.UpdateMetricsThput(ueNum - id, pkt->GetSize()*8/1e-3); // TODO
            // std::cout << "******* rnti" << ueNum - id << " thput=" << rxBytes*8/0.1/1e6 << "Mbps" << std::endl;

            rxBytes_total += pkt->GetSize();
            // std::cout << Simulator::Now().GetSeconds() << " | ue" << id << " rxBytes_total=" << rxBytes_total << ", he sumado " << pkt->GetSize() << std::endl;
        }

        void print_results(double t){
            // // outputFile_thput << "END" << std::endl;
            // // outputFile_thput << id << "\t"
            // //                  //<< rxBytes_total*8.0/(Simulator::Now().GetSeconds())* 1e-6 << "\t"
            // //                  << rxBytes_total*8.0/t* 1e-6 << "\t"
            // //                  << rxBytes_total << "\t"
            // //                  //<< Simulator::Now().GetSeconds() << std::endl;
            // //                  << t << std::endl;
            outputFile_thput.close();
        }
    };

std::ofstream outputFile_position("position.txt");
void positionTracer(NodeContainer ueNodes){
    for (uint32_t i = 0; i < ueNodes.GetN(); i++){
        // std::cout << "UE " << i << " position: " << ueNodes.Get(i)->GetObject<MobilityModel>()->GetPosition() << std::endl;
        try {
            Vector pos = ueNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
            outputFile_position << i+1 << "\t" << pos.x << "\t" << pos.y << "\t" << Simulator::Now().GetMilliSeconds() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error getting position for UE " << i << ": " << e.what() << std::endl;
        }
    }
}

std::ofstream outputFile_thput("thput_avg_window.txt");
long double rx_bytes_prev[4] = {0,0,0,0};
long double rxBytes, throughput;
void thput_avg_window(Ptr<ns3::FlowMonitor> monitor){
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
        rxBytes = i->second.rxBytes;
        throughput = (((rxBytes - rx_bytes_prev[i->first-1]) * 8.0) / 2) * 1e-6;
        // // outputFile_thput << i->first << "\t" << throughput << "\t" << rxBytes - rx_bytes_prev[i->first-1] << "\t" << rxBytes << "\t" << Simulator::Now().GetSeconds() << std::endl;
        rx_bytes_prev[i->first-1] = rxBytes;
    }
}

void
WriteBytesSent(Ptr<TrafficGenerator> trafficGenerator,
               uint64_t* previousBytesSent,
               uint64_t* previousWindowBytesSent,
               enum NrXrConfig NrXrConfig,
               std::ofstream* outFileTx)
{
    uint64_t totalBytesSent = trafficGenerator->GetTotalBytes();
    (*outFileTx) << "\n"
                 << Simulator::Now().GetMilliSeconds() << "\t" << *previousWindowBytesSent
                 << std::endl;
    (*outFileTx) << "\n"
                 << Simulator::Now().GetMilliSeconds() << "\t"
                 << totalBytesSent - *previousBytesSent << std::endl;

    *previousWindowBytesSent = totalBytesSent - *previousBytesSent;
    *previousBytesSent = totalBytesSent;
};

void
WriteBytesReceived(Ptr<PacketSink> packetSink, uint64_t* previousBytesReceived)
{
    uint64_t totalBytesReceived = packetSink->GetTotalRx();
    *previousBytesReceived = totalBytesReceived;
};

void
ConfigureXrApp(NodeContainer& ueContainer,
               uint32_t i,
               Ipv4InterfaceContainer& ueIpIface,
               enum NrXrConfig config,
               double appDataRate,
               double appMinDataRate,
               double appMaxDataRate,
               uint16_t appFps,
               uint16_t port,
               std::string transportProtocol,
               NodeContainer& remoteHostContainer,
               NetDeviceContainer& ueNetDev,
               Ptr<NrHelper> nrHelper,
               NrEpsBearer& bearer,
               Ptr<NrEpcTft> tft,
               bool isMx1,
               std::vector<Ptr<NrEpcTft>>& tfts,
               ApplicationContainer& serverApps,
               ApplicationContainer& clientApps,
               ApplicationContainer& pingApps)
{
    XrTrafficMixerHelper trafficMixerHelper;
    Ipv4Address ipAddress = ueIpIface.GetAddress(i, 0);
    trafficMixerHelper.ConfigureXr(config);
    auto it = XrPreconfig.find(config);

    std::vector<Address> addresses;
    std::vector<InetSocketAddress> localAddresses;
    for (size_t j = 0; j < it->second.size(); j++)
    {
        addresses.emplace_back(InetSocketAddress(ipAddress, port + j));
        // The sink will always listen to the specified ports
        localAddresses.emplace_back(Ipv4Address::GetAny(), port + j);
    }

    ApplicationContainer currentUeClientApps;
    currentUeClientApps.Add(
        trafficMixerHelper.Install(transportProtocol, addresses, remoteHostContainer.Get(0)));

    // Seed the ARP cache by pinging early in the simulation
    // This is a workaround until a static ARP capability is provided
    PingHelper ping(ipAddress);
    pingApps.Add(ping.Install(remoteHostContainer));

    Ptr<NetDevice> ueDevice = ueNetDev.Get(i);
    // Activate a dedicated bearer for the traffic type per node
    // nrHelper->ActivateDedicatedEpsBearer(ueDevice, bearer, tft);
    // Activate a dedicated bearer for the traffic type per node
    if (isMx1)
    {
        nrHelper->ActivateDedicatedEpsBearer(ueDevice, bearer, tft);
    }
    else
    {
        NS_ASSERT(tfts.size() >= currentUeClientApps.GetN());
        for (uint32_t j = 0; j < currentUeClientApps.GetN(); j++)
        {
            nrHelper->ActivateDedicatedEpsBearer(ueDevice, bearer, tfts[j]);
        }
    }

    for (uint32_t j = 0; j < currentUeClientApps.GetN(); j++)
    {
        PacketSinkHelper dlPacketSinkHelper(transportProtocol, localAddresses.at(j));
        Ptr<Application> packetSink = dlPacketSinkHelper.Install(ueContainer.Get(i)).Get(0);
        serverApps.Add(packetSink);
        Ptr<TrafficGenerator3gppGenericVideo> app =
            DynamicCast<TrafficGenerator3gppGenericVideo>(currentUeClientApps.Get(j));
        if (app)
        {
            app->SetAttribute("DataRate", DoubleValue(appDataRate));
            //app->SetAttribute("MinDataRate", DoubleValue(appMinDataRate));
            //app->SetAttribute("MaxDataRate", DoubleValue(appMaxDataRate));
            app->SetAttribute("Fps", UintegerValue(appFps));
        }
    }
    clientApps.Add(currentUeClientApps);
}

int
main(int argc, char* argv[])
{
    // set simulation time and mobility
    uint32_t appDuration = 1000;
    uint32_t appStartTimeMs = 400;
    uint16_t numerology = 0;
    uint16_t arUeNum = 0;
    uint16_t vrUeNum = 1;
    uint16_t cgUeNum = 0;
    uint16_t voiceUeNum = 0;
    double centralFrequency = 4e9;
    double bandwidth = 20e6;
    double txPower = 10; // 23; // 41, 49
    bool isMx1 = true;
    bool useUdp = true;
    double distance = 300;
    uint32_t rngRun = 0;
    double dppV = 0.0;
    bool enableVirtualQueue = true;
    double dppWeightG = 1;
    double dppWeightQ = 1;
    double reconfig_period = 1; // 2
    uint16_t rest_port = 8000;
    double dataRate = 5;
    double arDataRate = 5;  // Mbps
    double vrDataRate = 45; // Mbps
    double cgDataRate = 30; // Mbps
    double arMinDataRate = .1;  // Mbps
    double vrMinDataRate = .1; // Mbps
    double cgMinDataRate = .1; // Mbps
    double arMaxDataRate = 10;  // Mbps
    double vrMaxDataRate = 35; // Mbps
    double cgMaxDataRate = 25; // Mbps
    uint16_t arFps = 60;
    uint16_t vrFps = 120;
    uint16_t cgFps = 120;
    bool enableMobility = false;
    double speed = 2; // m/s
    double arGbrDl = 5e6;
    double vrGbrDl = 30e6;
    double cgGbrDl = 15e6;
    double voiceGbrDl = 0;
    std::string scenario = "UMa_Buildings"; // scenario
    // enum BandwidthPartInfo::Scenario scenarioEnum = BandwidthPartInfo::UMa;
    double hBS;          // base station antenna height in meters
    double hUT;          // user antenna height in meters
    std::string schedulerType = "DPPA"; // MAC scheduler type: RR, PF, MR, Qos, DPPA
    bool enableOfdma = true;
    bool logging = false;
    bool fixMcs = false;
    uint8_t fixMcsValue = 28;
    bool enableFading = true;
    bool enableShadowing = false;
    double rlc_buffer_size = 999999999;

    // Building parameters
    bool enableBuildings = false;
    double buildingX = 50.0;      // Building X coordinate
    double buildingY = 0.0;       // Building Y coordinate
    double buildingWidth = 20.0;  // Building width (m)
    double buildingLength = 20.0; // Building length (m)
    double buildingHeight = 100.0; // Building height (m)
    std::string mcsVector = "20,21,22,23,24";
    bool mcsAsUniformVar = false;
    uint8_t mcsMinValue = 15;
    std::string server_model = "PPO";
    double optuna_trial = -1;

    std::string remMode = "no";

    bool enable_xApp = true;

    std::string envType = "ISAC";

    CommandLine cmd(__FILE__);
    cmd.AddValue("remMode",
                 "What type of REM map to use: BeamShape, CoverageArea, UeCoverage."
                 "BeamShape shows beams that are configured in a user's script. "
                 "Coverage area is used to show worst-case SINR and best-case SNR maps "
                 "considering that at each point of the map the best beam is used "
                 "towards that point from the serving gNB and also of all the interfering"
                 "gNBs in the case of worst-case SINR."
                 "UeCoverage is similar to the previous, just that it is showing the "
                 "uplink coverage.",
                 remMode);
    cmd.AddValue("enableFading",
                 "If set to true it enables fading. Default value is true (fading enabled)",
                 enableFading);
    cmd.AddValue("enableShadowing",
                 "If set to true it enables shadowing. Default value is true (shadowing enabled)",
                 enableShadowing);
    cmd.AddValue("enableOfdma",
                 "If set to true it enables Ofdma scheduler. Default value is false (Tdma)",
                 enableOfdma);
    cmd.AddValue("schedulerType",
                 "PF: Proportional Fair (default), RR: Round-Robin, Qos",
                 schedulerType);
    cmd.AddValue("dppV", "Drift Plus Penalty V value", dppV);
    cmd.AddValue("enableVirtualQueue", "Enable the throughput virtual queue", enableVirtualQueue);
    cmd.AddValue("dppWeightQ", "Drift Plus Penalty weight to queue Q", dppWeightQ);
    cmd.AddValue("dppWeightG", "Drift Plus Penalty weight to queue G", dppWeightG);
    cmd.AddValue("reconfigPeriod", "The time interval between consecutive adjustments of the scheduler's configuration.", reconfig_period);
    cmd.AddValue("restPort", "The port number used to connect to the REST server.", rest_port);
    cmd.AddValue("dataRate", "The desired data rate in Mbps for the 3GPP generic video traffic generator", dataRate);
    cmd.AddValue("arDataRate", "The Datarate for AR UEs", arDataRate);
    cmd.AddValue("vrDataRate", "The Datarate for vR UEs", vrDataRate);
    cmd.AddValue("cgDataRate", "The Datarate for cg UEs", cgDataRate);
    cmd.AddValue("arMinDataRate", "The minimum Datarate for AR UEs", arMinDataRate);
    cmd.AddValue("vrMinDataRate", "The minimum Datarate for vR UEs", vrMinDataRate);
    cmd.AddValue("cgMinDataRate", "The minimum Datarate for cg UEs", cgMinDataRate);
    cmd.AddValue("arMaxDataRate", "The maximum Datarate for AR UEs", arMaxDataRate);
    cmd.AddValue("vrMaxDataRate", "The maximum Datarate for vR UEs", vrMaxDataRate);
    cmd.AddValue("cgMaxDataRate", "The maximum Datarate for cg UEs", cgMaxDataRate);
    cmd.AddValue("arFps", "The fps for AR UEs", arFps);
    cmd.AddValue("vrFps", "The fps for vR UEs", vrFps);
    cmd.AddValue("cgFps", "The fps for cg UEs", cgFps);
    cmd.AddValue("arGbrDl", "The GBR for VR UEs", arGbrDl);
    cmd.AddValue("vrGbrDl", "The GBR for VR UEs", vrGbrDl);
    cmd.AddValue("cgGbrDl", "The GBR for CG UEs", cgGbrDl);
    cmd.AddValue("voiceGbrDl", "The GBR for VOIP UEs", voiceGbrDl);
    cmd.AddValue("mobility", "Enable mobility", enableMobility);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.AddValue("arUeNum", "The number of AR UEs", arUeNum);
    cmd.AddValue("vrUeNum", "The number of VR UEs", vrUeNum);
    cmd.AddValue("cgUeNum", "The number of CG UEs", cgUeNum);
    cmd.AddValue("voiceUeNum", "The number of VOIP UEs", voiceUeNum);
    cmd.AddValue("numerology", "The numerology to be used.", numerology);
    cmd.AddValue("txPower", "Tx power to be configured to gNB", txPower);
    cmd.AddValue("frequency", "The system frequency", centralFrequency);
    cmd.AddValue("bandwidth", "The system bandwidth", bandwidth);
    cmd.AddValue("useUdp",
                 "if true, the NGMN applications will run over UDP connection, otherwise a TCP "
                 "connection will be used.",
                 useUdp);
    cmd.AddValue("distance",
                 "The radius of the disc (in meters) that the UEs will be distributed."
                 "Default value is 300m",
                 distance);
    cmd.AddValue("speed",
                 "The speed of the UEs in m/s. Default value is 1m/s (walking UT)",
                 speed);
    cmd.AddValue("isMx1",
                 "if true M SDFs will be mapped to 1 DRB, otherwise the mapping will "
                 "be 1x1, i.e., 1 SDF to 1 DRB.",
                 isMx1);
    cmd.AddValue("rngRun", "Rng run random number.", rngRun);
    cmd.AddValue("appDuration", "Duration of the application in milliseconds.", appDuration);
    cmd.AddValue("scenario",
                 "The scenario for the simulation. Choose among 'RMa', 'UMa', 'UMi-StreetCanyon', "
                 "'InH-OfficeMixed', 'InH-OfficeOpen'.",
                 scenario);
    cmd.AddValue("fixMcs", "If set to true, the MCS will be fixed to 28 (maximum)", fixMcs);
    cmd.AddValue("rlcBufferSize",
        "The RLC buffer size in bytes. Default value is 999999999",
        rlc_buffer_size);
    cmd.AddValue("fixMcsValue", "The fixed MCS value", fixMcsValue);
    cmd.AddValue("mcsVector", "List of MCSs.", mcsVector);
    cmd.AddValue("mcsAsUniformVar", "When enabled the MCS is modelled as a uniform variable between mcsMinValue and fixMcsValue." "Default value is false", mcsAsUniformVar);
    cmd.AddValue("mcsMinValue", "The minimum MCS value when mcsAsUniformVr is enabled.", mcsMinValue);
    cmd.AddValue("server_model", "Server model to be used: PPO, Random, Static", server_model);
    cmd.AddValue("optunaTrial", "The trial number to be used in the Optuna server.", optuna_trial);

    // Building parameters
    cmd.AddValue("enableBuildings", "Enable buildings in the scenario", enableBuildings);
    cmd.AddValue("buildingX", "Building X coordinate", buildingX);
    cmd.AddValue("buildingY", "Building Y coordinate", buildingY);
    cmd.AddValue("buildingWidth", "Building width in meters", buildingWidth);
    cmd.AddValue("buildingLength", "Building length in meters", buildingLength);
    cmd.AddValue("buildingHeight", "Building height in meters", buildingHeight);
    cmd.AddValue("envType", "Environment type", envType);

    cmd.Parse(argc, argv);

    ueNum = arUeNum + vrUeNum + cgUeNum + voiceUeNum;

    // NS_ABORT_MSG_IF(appDuration < 1000, "The appDuration should be at least 1000ms.");
    //NS_ABORT_MSG_IF(!vrUeNum && !arUeNum && !cgUeNum, "Activate at least one type of XR traffic by configuring the number of XR users");

    // enable logging or not
    if (logging)
    {
        //LogLevel logLevel1 =
        //    (LogLevel)(LOG_PREFIX_FUNC | LOG_PREFIX_TIME | LOG_PREFIX_NODE | LOG_LEVEL_INFO);
        //LogComponentEnable("NrMacSchedulerNs3", logLevel1);
        //LogComponentEnable("NrMacSchedulerTdma", logLevel1);
        //LogComponentEnable("NrMacSchedulerNs3", LOG_LEVEL_WARN);
        //LogComponentEnable("NrMacSchedulerOfdma", LOG_LEVEL_DEBUG);
        //LogComponentEnable("NrMacSchedulerOfdmaDPP", LOG_LEVEL_DEBUG);
        LogComponentEnable("NrMacSchedulerOfdmaDPPA", LOG_LEVEL_INFO);
        LogComponentEnable("NrMacSchedulerUeInfoDPPA", LOG_LEVEL_DEBUG);
    }
    // LogComponentEnable("NrRadioEnvironmentMapHelper", LOG_LEVEL_ALL);

    uint32_t simTimeMs = appStartTimeMs + appDuration + 2000;

    // Set simulation run number
    SeedManager::SetRun(rngRun);

    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(rlc_buffer_size));
    Config::SetDefault("ns3::NrGnbRrc::EpsBearerToRlcMapping",
                       EnumValue(useUdp ? NrGnbRrc::RLC_UM_ALWAYS : NrGnbRrc::RLC_AM_ALWAYS));
    
    // Config::SetDefault("ns3::NrRlcUm::EnablePdcpDiscarding", BooleanValue(true));
    // // Config::SetDefault("ns3::NrRlcUm::DiscardTimerMs", UintegerValue(50));
    // // Config::SetDefault("ns3::NrRlcUm::ReorderingTimer", TimeValue(MilliSeconds(reorderingTimerMs)));

    // Configure DPPA scheduler parameters only if DPPA is selected
    if (schedulerType == "DPPA")
    {
        Config::SetDefault("ns3::NrMacSchedulerOfdmaDPPA::DppV", DoubleValue(dppV));
        Config::SetDefault("ns3::NrMacSchedulerOfdmaDPPA::EnableVirtualQueue", BooleanValue(enableVirtualQueue));
        Config::SetDefault("ns3::NrMacSchedulerOfdmaDPPA::DppWeightQ", DoubleValue(dppWeightQ)); // Peso de Q en DPPA
        Config::SetDefault("ns3::NrMacSchedulerOfdmaDPPA::DppWeightG", DoubleValue(dppWeightG)); // Peso de G en DPP
        Config::SetDefault("ns3::NrMacSchedulerOfdmaDPPA::ReconfigPeriod", DoubleValue(reconfig_period)); // Periodo de reconfiguración en segundos
        Config::SetDefault("ns3::NrMacSchedulerOfdmaDPPA::RestServerPort", UintegerValue(rest_port)); // Puerto usado para conectarse al servidor REST
    }
    //Config::SetDefault("ns3::NrGnbPhy::PowerAllocationType", EnumValue(NrSpectrumValueHelper::UNIFORM_POWER_ALLOCATION_BW));
    //Config::SetDefault("ns3::NrUePhy::PowerAllocationType", EnumValue(NrSpectrumValueHelper::UNIFORM_POWER_ALLOCATION_USED));
    if (fixMcs){
        Config::SetDefault("ns3::NrMacSchedulerNs3::FixedMcsDl", BooleanValue(true)); // MCS fixed (206 bytes con 53 RBs)
        Config::SetDefault("ns3::NrMacSchedulerNs3::StartingMcsDl", UintegerValue(fixMcsValue)); // MCS starting index. [0, 28]
        // // Config::SetDefault("ns3::NrMacSchedulerNs3::McsVector", StringValue(mcsVector)); // List of MCSs. For now it overrides the fixMcsValue
        // // Config::SetDefault("ns3::NrMacSchedulerNs3::McsAsUniformVar", BooleanValue(mcsAsUniformVar)); // When enabled the MCS is modelled as a uniform variable between mcsMinValue and fixMcsValue
        // // Config::SetDefault("ns3::NrMacSchedulerNs3::McsMinValue", UintegerValue(mcsMinValue)); // The minimum MCS value when mcsAsUniformVr is enabled
    }

    // set mobile device and base station antenna heights in meters, according to the chosen scenario
    if (scenario == "RMa")
    {
        hBS = 35;
        hUT = 1.5;
        // scenarioEnum = BandwidthPartInfo::RMa;
    }
    else if (scenario == "UMa")
    {
        hBS = 25;
        hUT = 1.5;
        // scenarioEnum = BandwidthPartInfo::UMa;
    }
    else if (scenario == "UMi-StreetCanyon")
    {
        hBS = 10;
        hUT = 1.5;
        // scenarioEnum = BandwidthPartInfo::UMi_StreetCanyon;
    }
    else if (scenario == "InH-OfficeMixed")
    {
        hBS = 3;
        hUT = 1;
        // scenarioEnum = BandwidthPartInfo::InH_OfficeMixed;
    }
    else if (scenario == "InH-OfficeOpen")
    {
        hBS = 3;
        hUT = 1;
        // scenarioEnum = BandwidthPartInfo::InH_OfficeOpen;
    }
    else if (scenario == "UMa_LoS")
    {
        hBS = 25;
        hUT = 1.5;
        // scenarioEnum = BandwidthPartInfo::UMa_LoS;
    }
    else if (scenario == "InH_OfficeOpen_LoS")
    {
        hBS = 3;
        hUT = 1;
        // scenarioEnum = BandwidthPartInfo::InH_OfficeOpen_LoS;
    }
    else if (scenario == "Custom")
    {
        hBS = 25;
        hUT = 1.5;
        // scenarioEnum = BandwidthPartInfo::Custom;
    }
    else if (scenario == "UMa_Buildings")
    {
        // hBS = 1.5; // 25
        hBS = 25;
        hUT = 1.5;
        // scenarioEnum = BandwidthPartInfo::UMa_Buildings;
        enableBuildings = true;
    }
    else if (scenario == "UMi_Buildings")
    {
        hBS = 10;
        hUT = 1.5;
        // scenarioEnum = BandwidthPartInfo::UMi_Buildings;
        enableBuildings = true;
    }
    else if (scenario == "Custom")
    {
        // hBS = 1.5; // 25
        hBS = 5;
        hUT = 1.5;
        // scenarioEnum = BandwidthPartInfo::UMa_Buildings;
        enableBuildings = true;
    }
    else
    {
        NS_ABORT_MSG("Scenario not supported. Choose among 'RMa', 'UMa', 'UMi-StreetCanyon', "
                     "'InH-OfficeMixed', and 'InH-OfficeOpen'.");
    }

    // Create base stations and mobile terminals
    NodeContainer gNbNodes;
    NodeContainer ueNodes;
    MobilityHelper mobility;
    gNbNodes.Create(1);
    ueNodes.Create(arUeNum + vrUeNum + cgUeNum + voiceUeNum);

    // Position the base stations
    Ptr<ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator>();
    gnbPositionAlloc->Add(Vector(0.0, 0.0, hBS));
    MobilityHelper gnbmobility;
    gnbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbmobility.SetPositionAllocator(gnbPositionAlloc);
    gnbmobility.Install(gNbNodes);

    // Position the UEs
    MobilityHelper uemobility;
    std::mt19937 gen(0);
    // std::uniform_real_distribution<> dis(-distance, distance);
    std::uniform_real_distribution<> dis(-5, 5);
    if (enableMobility)
    {
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        {
            positionAlloc->Add(Vector(dis(gen), 25, 0.0));
        }
        uemobility.SetPositionAllocator(positionAlloc);
        uemobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                // "Bounds", RectangleValue(Rectangle(-distance, distance, -distance, distance)),
                                "Bounds", RectangleValue(Rectangle(-22, 22, 18, 28)),
                                "Distance", DoubleValue(5.0),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(1) + "]"),
                                "Direction", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.283184]"));
                                // "Direction", StringValue("ns3::UniformRandomVariable[Constant=0.0]"));
        // uemobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        uemobility.Install(ueNodes);

        // // // uemobility.SetMobilityModel("ns3::WaypointMobilityModel");
        // // // uemobility.Install(ueNodes);

        // // // // Añadir waypoints a cada UE
        // // // double pos_1, pos_2, time;
        // // // for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        // // // {
        // // //     if (i == 0) {pos_1 = 25; pos_2 = 10; time=16;}
        // // //     if (i == 1) {pos_1 = 20; pos_2 = -5; time=36;}//pos_1 = -10; pos_2 = -5;}
        // // //     if (i == 2) {pos_1 = -20; pos_2 = -15;}

        // // //     Ptr<WaypointMobilityModel> waypointMobility = ueNodes.Get(i)->GetObject<WaypointMobilityModel>();
        // // //     for (double j = 0; j <= simTimeMs/1000; j += time)
        // // //     {
        // // //         // waypointMobility->AddWaypoint(Waypoint(Seconds(j), Vector(pos_1, pos_1, 0)));
        // // //         // waypointMobility->AddWaypoint(Waypoint(Seconds(j+20.0), Vector(-pos_1, pos_1, 0)));
        // // //         // waypointMobility->AddWaypoint(Waypoint(Seconds(j+25.0), Vector(-pos_1, pos_2, 0)));
        // // //         // waypointMobility->AddWaypoint(Waypoint(Seconds(j+45.0), Vector(pos_1, pos_2, 0)));
        // // //         if (i == 0) {waypointMobility->AddWaypoint(Waypoint(Seconds(j), Vector(-9, 26, 0)));
        // // //         waypointMobility->AddWaypoint(Waypoint(Seconds(j+8), Vector(6, 26, 0)));}
        // // //         if (i == 1) {waypointMobility->AddWaypoint(Waypoint(Seconds(j), Vector(-9, 22, 0)));
        // // //         waypointMobility->AddWaypoint(Waypoint(Seconds(j+18), Vector(25, 22, 0)));}
        // // //     }
        // // // }

        // Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        // for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        // {
        //     // Posición inicial aleatoria dentro del área, más hacia la parte superior (y > 0)
        //     positionAlloc->Add(Vector((i+1)*10, 10, 0.0));
        // }
        // uemobility.SetPositionAllocator(positionAlloc);

        // uemobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        // uemobility.Install(ueNodes);


        // for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        // {
        //     Ptr<MobilityModel> mob = ueNodes.Get(i)->GetObject<MobilityModel>();
        //     // Velocidad: hacia abajo en el eje Y (negativa)
        //     mob->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0.0, -speed, 0.0));

        //     std::cout << "UE " << i << " position: "
        //             << mob->GetPosition()
        //             << ", velocity: " << mob->GetVelocity()
        //             << std::endl;
        // }

        // for (uint32_t i = 0; i < ueNodes.GetN(); i++)
        // {
        //     std::cout << "UE " << i << " position: "
        //     << ueNodes.Get(i)->GetObject<MobilityModel>()->GetPosition()
        //     << ", velocity: " << ueNodes.Get(i)->GetObject<MobilityModel>()->GetVelocity()
        //     << std::endl;
        // }
    } else {
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        {
            positionAlloc->Add(Vector(dis(gen), dis(gen), hUT)); // Distribute UEs in a line
        }
        uemobility.SetPositionAllocator(positionAlloc);
        uemobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        uemobility.Install(ueNodes);
    }

    // Create and configure buildings if enabled
    if (enableBuildings)
    {
        uint32_t numOfBuildings = 3;
        uint32_t apartmentsX = 20; // Number of apartments in X direction
        uint32_t nFloors = 4;

        Ptr<GridBuildingAllocator> gridBuildingAllocator;
        gridBuildingAllocator = CreateObject<GridBuildingAllocator>();
        gridBuildingAllocator->SetAttribute("GridWidth", UintegerValue(numOfBuildings));
        // gridBuildingAllocator->SetAttribute("LengthX", DoubleValue(2 * apartmentsX));
        // gridBuildingAllocator->SetAttribute("LengthY", DoubleValue(60));
        gridBuildingAllocator->SetAttribute("LengthX", DoubleValue(2));
        gridBuildingAllocator->SetAttribute("LengthY", DoubleValue(2));
        gridBuildingAllocator->SetAttribute("DeltaX", DoubleValue(7));
        gridBuildingAllocator->SetAttribute("DeltaY", DoubleValue(10));
        // gridBuildingAllocator->SetAttribute("Height", DoubleValue(3 * nFloors));
        gridBuildingAllocator->SetAttribute("Height", DoubleValue(10));
        // gridBuildingAllocator->SetBuildingAttribute("NRoomsX", UintegerValue(apartmentsX));
        // gridBuildingAllocator->SetBuildingAttribute("NRoomsY", UintegerValue(2));
        // gridBuildingAllocator->SetBuildingAttribute("NFloors", UintegerValue(nFloors));
        gridBuildingAllocator->SetBuildingAttribute("NRoomsX", UintegerValue(0));
        gridBuildingAllocator->SetBuildingAttribute("NRoomsY", UintegerValue(0));
        gridBuildingAllocator->SetBuildingAttribute("NFloors", UintegerValue(0));
        gridBuildingAllocator->SetAttribute("MinX", DoubleValue(-10));
        gridBuildingAllocator->SetAttribute("MinY", DoubleValue(15));
        gridBuildingAllocator->Create(numOfBuildings);
        // gridBuildingAllocator->SetBuildingAttribute("ExternalWallsType", EnumValue(0));

        BuildingsHelper::Install(gNbNodes);
        BuildingsHelper::Install(ueNodes);
        std::cout << "Buildings created and BuildingsHelper installed." << std::endl;
    }

    /*
     * Create various NodeContainer(s) for the different traffic types.
     * In ueArContainer, ueVrContainer, ueCgContainer, we will put
     * AR, VR, CG UEs, respectively.*/
    NodeContainer ueArContainer;
    NodeContainer ueVrContainer;
    NodeContainer ueCgContainer;
    NodeContainer ueVoiceContainer;

    for (auto j = 0; j < arUeNum; ++j)
    {
        Ptr<Node> ue = ueNodes.Get(j);
        ueArContainer.Add(ue);
    }
    for (auto j = arUeNum; j < arUeNum + vrUeNum; ++j)
    {
        Ptr<Node> ue = ueNodes.Get(j);
        ueVrContainer.Add(ue);
    }
    for (auto j = arUeNum + vrUeNum; j < arUeNum + vrUeNum + cgUeNum; ++j)
    {
        Ptr<Node> ue = ueNodes.Get(j);
        ueCgContainer.Add(ue);
    }
    for (auto j = arUeNum + vrUeNum + cgUeNum; j < arUeNum + vrUeNum + cgUeNum + voiceUeNum; ++j)
    {
        Ptr<Node> ue = ueNodes.Get(j);
        ueVoiceContainer.Add(ue);
    }

    // setup the nr simulation
    Ptr<NrChannelHelper> nrChannelHelper = CreateObject<NrChannelHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    // simple band configuration and initialize
    BandwidthPartInfoPtrVector allBwps;
    // ////////////// Modelo 3GPP //////////////
    // bool enableFading = true;
    // auto bandMask = NrChannelHelper::INIT_PROPAGATION | NrChannelHelper::INIT_CHANNEL;
    uint8_t bandMask = NrChannelHelper::INIT_PROPAGATION;
    if (enableFading)
    {
        bandMask |= NrChannelHelper::INIT_FADING;
    }

    // bool enableShadowing = false;
    // nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(enableShadowing));

    // nrHelper->SetAttribute("PathlossModel", StringValue ("ns3::HybridBuildingsPropagationLossModel")); // try it, but 200 to 2600 MHz!

    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1; // in this example we have a single band, and that band is composed of a single component carrier
    // CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, 1);
    /* Create the configuration for the CcBwpHelper. SimpleOperationBandConf creates
    * a single BWP per CC and a single BWP in CC.
    *
    * Hence, the configured spectrum is:
    *
    * |---------------Band---------------|
    * |---------------CC-----------------|
    * |---------------BWP----------------|
    */
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency,
                                                bandwidth,
                                                numCcPerBand);
                                                // scenarioEnum);
    
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    // Set the spectrum channel
    if (enableBuildings){
        nrChannelHelper->ConfigureFactories("UMa", "Buildings", "ThreeGpp");
        nrChannelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(enableShadowing));
    } else {
        // nrChannelHelper->ConfigureFactories("UMa", "LOS", "ThreeGpp");
        // // nrChannelHelper->ConfigureFactories("UMa", "LOS", "TwoRay");
        nrChannelHelper->ConfigurePropagationFactory(FriisPropagationLossModel::GetTypeId());
    }
    
    // Set shadowing and update period
    // // nrChannelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(enableShadowing));
    nrChannelHelper->AssignChannelsToBands({band}, bandMask);

    // Initialize channel and pathloss, plus other things inside band.
    // Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(0)));
    // Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(20)));
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(0)));
    // nrHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    // nrChannelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(100)));
    // nrChannelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(true));

    // // Config::SetDefault("ns3::ThreeGppChannelModel::BuildingPenetrationLossesEnabled", BooleanValue(false));

    // Values from cttc-nr-fh-xr.cc
    // nrHelper->SetPhasedArraySpectrumPropagationLossModelTypeId(
    //         DistanceBasedThreeGppSpectrumPropagationLossModel::GetTypeId());
    // // nrChannelHelper->SetPhasedArraySpectrumPropagationLossModelAttribute("MaxDistance",
    // //                                                                 DoubleValue(2 * 200));
    // // nrChannelHelper->SetChannelConditionModelAttribute("LinkO2iConditionToAntennaHeight",
    // //                                             BooleanValue(false));
    // // nrChannelHelper->SetChannelConditionModelAttribute("O2iThreshold", DoubleValue(0));
    // // nrChannelHelper->SetChannelConditionModelAttribute("O2iLowLossThreshold",
    // //                                             DoubleValue(1.0));
    // In case of DistanceBasedThreeGppSpectrumPropagationLossModel, the creation of the channel
    // must be manually done, as the channel helper does not support the creation of this specific
    // model.
    ObjectFactory distanceBasedChannelFactory;
    distanceBasedChannelFactory.SetTypeId(
        DistanceBasedThreeGppSpectrumPropagationLossModel::GetTypeId());
    distanceBasedChannelFactory.Set("MaxDistance", DoubleValue(2 * 200));
    // Note: BuildingsChannelConditionModel doesn't support these ThreeGpp-specific attributes
    // Only set these if using ThreeGppChannelConditionModel
    // nrChannelHelper->SetChannelConditionModelAttribute(
    //     "LinkO2iConditionToAntennaHeight",
    //     BooleanValue(false));
    // nrChannelHelper->SetChannelConditionModelAttribute("O2iThreshold", DoubleValue(0));
    // nrChannelHelper->SetChannelConditionModelAttribute("O2iLowLossThreshold",
    //                                                     DoubleValue(1.0));

    // nrHelper->InitializeOperationBand(&band);
    // nrHelper->InitializeOperationBand(&band, bandMask);
    // BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});
    // allBwps = CcBwpCreator::GetAllBwps({band});
    // BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});
    allBwps = CcBwpCreator::GetAllBwps({band});
    // /////////////////////////////////////////

    //////////// Modelo no 3GPP ////////////
    // uint8_t bwpId = 1;
    // // std::vector<std::reference_wrapper<std::unique_ptr<BandwidthPartInfo> > > allBwps;
    // // BandwidthPartInfoPtrVector allBwps;
    // std::unique_ptr<BandwidthPartInfo> bwpi (new BandwidthPartInfo (bwpId, centralFrequency, bandwidth));
    // auto spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();
    // auto propagationLoss = CreateObject<FriisPropagationLossModel> ();
    // // auto propagationLoss = CreateObject<ns3::ThreeGppUmaPropagationLossModel> ();
    // propagationLoss->SetAttributeFailSafe ("Frequency", DoubleValue (centralFrequency));
    // spectrumChannel->AddPropagationLossModel (propagationLoss);
    // bwpi->m_channel = spectrumChannel;
    // allBwps.push_back(bwpi);
    ////////////////////////////////////////

    nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(txPower));
    nrHelper->SetGnbPhyAttribute("Numerology", UintegerValue(numerology));
    nrHelper->SetGnbPhyAttribute("NoiseFigure", DoubleValue(5));
    nrHelper->SetUePhyAttribute("TxPower", DoubleValue(7)); // 23
    nrHelper->SetUePhyAttribute("NoiseFigure", DoubleValue(7));

    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    // nrHelper->SetGnbAntennaAttribute("AntennaElement",
    //                                  PointerValue(CreateObject<ThreeGppAntennaModel>()));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                        PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetGnbAntennaAttribute("AntennaHorizontalSpacing", DoubleValue(0.5));
    nrHelper->SetGnbAntennaAttribute("AntennaVerticalSpacing", DoubleValue(0.8));
    nrHelper->SetGnbAntennaAttribute("DowntiltAngle", DoubleValue(0 * M_PI / 180.0));
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));
                                    // PointerValue(CreateObject<ThreeGppAntennaModel>()));

    // Set up MAC scheduler
    std::stringstream scheduler;
    std::string subType;
    subType = enableOfdma == false ? "Tdma" : "Ofdma";
    scheduler << "ns3::NrMacScheduler" << subType << schedulerType;
    std::cout << "Scheduler: " << scheduler.str() << std::endl;
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName(scheduler.str()));


    // Beamforming method
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);

    // idealBeamformingHelper->SetBeamformingMethod(
    //             QuasiOmniDirectPathBeamforming::GetTypeId());

    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);
    epcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));

    // Initialize nrHelper
    nrHelper->Initialize();

    NetDeviceContainer gNbNetDev = nrHelper->InstallGnbDevice(gNbNodes, allBwps);
    NetDeviceContainer ueArNetDev = nrHelper->InstallUeDevice(ueArContainer, allBwps);
    NetDeviceContainer ueVrNetDev = nrHelper->InstallUeDevice(ueVrContainer, allBwps);
    NetDeviceContainer ueCgNetDev = nrHelper->InstallUeDevice(ueCgContainer, allBwps);
    NetDeviceContainer ueVoiceNetDev = nrHelper->InstallUeDevice(ueVoiceContainer, allBwps);

    int64_t randomStream = 1;
    randomStream += nrHelper->AssignStreams(gNbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueArNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueVrNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueCgNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueVoiceNetDev, randomStream);

    for (auto it = gNbNetDev.Begin(); it != gNbNetDev.End(); ++it)
    {
        DynamicCast<NrGnbNetDevice>(*it)->UpdateConfig();
    }
    for (auto it = ueArNetDev.Begin(); it != ueArNetDev.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }
    for (auto it = ueVrNetDev.Begin(); it != ueVrNetDev.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }
    for (auto it = ueCgNetDev.Begin(); it != ueCgNetDev.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }
    for (auto it = ueVoiceNetDev.Begin(); it != ueVoiceNetDev.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }

    // create the internet and install the IP stack on the UEs
    // get SGW/PGW and create a single RemoteHost
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // connect a remoteHost to pgw. Setup routing too
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1000));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.000)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueArIpIface;
    Ipv4InterfaceContainer ueVrIpIface;
    Ipv4InterfaceContainer ueCgIpIface;
    Ipv4InterfaceContainer ueVoiceIpIface;

    ueArIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueArNetDev));
    ueVrIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueVrNetDev));
    ueCgIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueCgNetDev));
    ueVoiceIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueVoiceNetDev));

    // Set the default gateway for the UEs
    for (uint32_t j = 0; j < ueNodes.GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(j)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // attach UEs to the closest gNB
    nrHelper->AttachToClosestGnb(ueArNetDev, gNbNetDev);
    nrHelper->AttachToClosestGnb(ueVrNetDev, gNbNetDev);
    nrHelper->AttachToClosestGnb(ueCgNetDev, gNbNetDev);
    nrHelper->AttachToClosestGnb(ueVoiceNetDev, gNbNetDev);

    // Prueba
    // nrHelper->SetSchedulerAttribute("CqiTimerThreshold", TimeValue(MilliSeconds(0)));

    // Install sink application
    ApplicationContainer serverApps;

    // configure the transport protocol to be used
    std::string transportProtocol;
    transportProtocol = useUdp == true ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory";
    uint16_t dlPortArStart = 1121; // AR has 3 flows
    uint16_t dlPortArStop = 1124;
    uint16_t dlPortVrStart = 1131;
    uint16_t dlPortCgStart = 1141;
    uint16_t dlPortVoiceStart = 1151;

    // The bearer that will carry AR traffic
    NrEpsBearer arBearer(NrEpsBearer::NGBR_LOW_LAT_EMBB);
    // GbrQosInformation qosUe1flow;
    // qosUe1flow.gbrDl = 5e6; // Downlink GBR
    // EpsBearer arBearer(EpsBearer::GBR_LIVE_UL_76, qosUe1flow);

    Ptr<NrEpcTft> arTft = Create<NrEpcTft>();
    NrEpcTft::PacketFilter dlpfAr;
    std::vector<Ptr<NrEpcTft>> arTfts;

    if (isMx1)
    {
        dlpfAr.localPortStart = dlPortArStart;
        dlpfAr.localPortEnd = dlPortArStop;
        arTft->Add(dlpfAr);
    }
    else
    {
        // create 3 xrTfts for 1x1 mapping
        for (uint32_t i = 0; i < 3; i++)
        {
            Ptr<NrEpcTft> tempTft = Create<NrEpcTft>();
            dlpfAr.localPortStart = dlPortArStart + i;
            dlpfAr.localPortEnd = dlPortArStart + i;
            tempTft->Add(dlpfAr);
            arTfts.emplace_back(tempTft);
        }
    }


    // The bearer that will carry VR traffic
    // EpsBearer vrBearer(EpsBearer::NGBR_LOW_LAT_EMBB);
    NrGbrQosInformation qosVrflow;
    qosVrflow.gbrDl = vrGbrDl; // Downlink GBR
    NrEpsBearer vrBearer(NrEpsBearer::NGBR_LOW_LAT_EMBB, qosVrflow);
    // NrEpsBearer vrBearer(NrEpsBearer::GBR_V2X, qosVrflow);

    Ptr<NrEpcTft> vrTft = Create<NrEpcTft>();
    NrEpcTft::PacketFilter dlpfVr;
    dlpfVr.localPortStart = dlPortVrStart;
    dlpfVr.localPortEnd = dlPortVrStart;
    vrTft->Add(dlpfVr);

    // The bearer that will carry CG traffic
    // EpsBearer cgBearer(EpsBearer::NGBR_LOW_LAT_EMBB);
    NrGbrQosInformation qosCgflow;
    qosCgflow.gbrDl = cgGbrDl; // Downlink GBR
    NrEpsBearer cgBearer(NrEpsBearer::GBR_GAMING, qosCgflow);

    Ptr<NrEpcTft> cgTft = Create<NrEpcTft>();
    NrEpcTft::PacketFilter dlpfCg;
    dlpfCg.localPortStart = dlPortCgStart;
    dlpfCg.localPortEnd = dlPortCgStart;
    cgTft->Add(dlpfCg);

    // The bearer that will carry Voice traffic
    NrGbrQosInformation qosVoiceflow;
    qosVoiceflow.gbrDl = voiceGbrDl; // Downlink GBR
    NrEpsBearer voiceBearer(NrEpsBearer::GBR_CONV_VOICE, qosVoiceflow);

    Ptr<NrEpcTft> voiceTft = Create<NrEpcTft>();
    NrEpcTft::PacketFilter dlpfVoice;
    dlpfVoice.localPortStart = dlPortVoiceStart;
    dlpfVoice.localPortEnd = dlPortVoiceStart;
    voiceTft->Add(dlpfVoice);

    // Install traffic generators
    // XR traffic generator
    ApplicationContainer clientApps;
    ApplicationContainer pingApps;

    std::ostringstream xrFileTag;

    for (uint32_t i = 0; i < ueArContainer.GetN(); ++i)
    {
        ConfigureXrApp(ueArContainer,
                       i,
                       ueArIpIface,
                       AR_M3,
                       arDataRate,
                       arMinDataRate,
                       arMaxDataRate,
                       arFps,
                       dlPortArStart,
                       transportProtocol,
                       remoteHostContainer,
                       ueArNetDev,
                       nrHelper,
                       arBearer,
                       arTft,
                       isMx1,
                       arTfts,
                       serverApps,
                       clientApps,
                       pingApps);
    }
    // TODO for VR and CG of 2 flows Tfts and isMx1 have to be set. Currently they are
    // hardcoded for 1 flow
    for (uint32_t i = 0; i < ueVrContainer.GetN(); ++i)
    {
        ConfigureXrApp(ueVrContainer,
                       i,
                       ueVrIpIface,
                       VR_DL1,
                       vrDataRate,
                       vrMinDataRate,
                       vrMaxDataRate,
                       vrFps,
                       dlPortVrStart,
                       transportProtocol,
                       remoteHostContainer,
                       ueVrNetDev,
                       nrHelper,
                       vrBearer,
                       vrTft,
                       true,
                       arTfts,
                       serverApps,
                       clientApps,
                       pingApps);
    }
    for (uint32_t i = 0; i < ueCgContainer.GetN(); ++i)
    {
        ConfigureXrApp(ueCgContainer,
                       i,
                       ueCgIpIface,
                       CG_DL1,
                       cgDataRate,
                       cgMinDataRate,
                       cgMaxDataRate,
                       cgFps,
                       dlPortCgStart,
                       transportProtocol,
                       remoteHostContainer,
                       ueCgNetDev,
                       nrHelper,
                       cgBearer,
                       cgTft,
                       true,
                       arTfts,
                       serverApps,
                       clientApps,
                       pingApps);
    }

    // Voice traffic generator
    //////////////////////////
    //uint32_t udpPacketSize = 3000;
    //uint32_t lambda = 1000;
    //UdpClientHelper dlClientVoiceflow;
    //dlClientVoiceflow.SetAttribute("RemotePort", UintegerValue(dlPortVoiceStart));
    //dlClientVoiceflow.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    //dlClientVoiceflow.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
    //dlClientVoiceflow.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambda)));

    for (uint32_t i = 0; i < ueVoiceContainer.GetN(); ++i)
    {
        //Ptr<NetDevice> ueDevice = ueVoiceNetDev.Get(i);
        Ipv4Address ipAddress = ueVoiceIpIface.GetAddress(i, 0);

        std::vector<Address> addresses;
        std::vector<InetSocketAddress> localAddresses;
        for (size_t j = 0; j < voiceUeNum; j++)
        {
            addresses.emplace_back(InetSocketAddress(ipAddress, dlPortVoiceStart + j));
            // The sink will always listen to the specified ports
            localAddresses.emplace_back(Ipv4Address::GetAny(), dlPortVoiceStart + j);
        }

        // The client, who is transmitting, is installed in the remote host,
        // with destination address set to the address of the UE
        //dlClientVoiceflow.SetAttribute("RemoteAddress", AddressValue(ipAddress));
        ApplicationContainer currentVoiceClientApps;
        //currentVoiceClientApps.Add(dlClientVoiceflow.Install(remoteHost));
        TrafficGeneratorHelper trafficGeneratorHelper(
            transportProtocol,
            InetSocketAddress(ueVoiceIpIface.GetAddress(i, 0), dlPortVoiceStart),
            TrafficGeneratorNgmnVoip::GetTypeId());
            // TrafficGeneratorNgmnVideo::GetTypeId());
        currentVoiceClientApps.Add(trafficGeneratorHelper.Install(remoteHost));

        // Seed the ARP cache by pinging early in the simulation
        // This is a workaround until a static ARP capability is provided
        PingHelper ping(ipAddress);
        pingApps.Add(ping.Install(remoteHostContainer));

        Ptr<NetDevice> ueDevice = ueVoiceNetDev.Get(i);
        // Activate a dedicated bearer for the traffic type per node
        // nrHelper->ActivateDedicatedEpsBearer(ueDevice, voiceBearer, voiceTft);
        // Activate a dedicated bearer for the traffic type per node
        if (isMx1)
        {
            nrHelper->ActivateDedicatedEpsBearer(ueDevice, voiceBearer, voiceTft);
        }
        else
        {
            NS_ASSERT(arTfts.size() >= currentVoiceClientApps.GetN());
            for (uint32_t j = 0; j < currentVoiceClientApps.GetN(); j++)
            {
                nrHelper->ActivateDedicatedEpsBearer(ueDevice, voiceBearer, arTfts[j]);
            }
        }

        for (uint32_t j = 0; j < currentVoiceClientApps.GetN(); j++)
        {
            PacketSinkHelper dlPacketSinkHelper(transportProtocol, localAddresses.at(j));
            Ptr<Application> packetSink = dlPacketSinkHelper.Install(ueVoiceContainer.Get(i)).Get(0);
            serverApps.Add(packetSink);
        }
        clientApps.Add(currentVoiceClientApps);
    }
    //////////////////////////

    pingApps.Start(MilliSeconds(100));
    pingApps.Stop(MilliSeconds(appStartTimeMs));

    // start server and client apps
    serverApps.Start(MilliSeconds(appStartTimeMs));
    clientApps.Start(MilliSeconds(appStartTimeMs));
    serverApps.Stop(MilliSeconds(simTimeMs));
    clientApps.Stop(MilliSeconds(appStartTimeMs + appDuration));

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(ueNodes);

    Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.0001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    // Rem parameters --xMin=-250 --xMax=250 --xRes=700 --yMin=-250 --yMax=250 --yRes=700
    double xMin = -50.0;
    double xMax = 50.0;
    uint16_t xRes = 200;
    double yMin = -50.0;
    double yMax = 50.0;
    uint16_t yRes = 200;
    double z = 1.5;
    uint16_t remBwpId = 0;
    // Radio Environment Map Generation for ccId 0
    Ptr<NrRadioEnvironmentMapHelper> remHelper = CreateObject<NrRadioEnvironmentMapHelper>();
    remHelper->SetMinX(xMin);
    remHelper->SetMaxX(xMax);
    remHelper->SetResX(xRes);
    remHelper->SetMinY(yMin);
    remHelper->SetMaxY(yMax);
    remHelper->SetResY(yRes);
    remHelper->SetZ(z);
    remHelper->SetSimTag("scheduler");

    // gNbNetDev.Get(0)
    //     ->GetObject<NrGnbNetDevice>()
    //     ->GetPhy(remBwpId)
    //     ->GetSpectrumPhy(0)
    //     ->GetBeamManager()
    //     // ->ChangeBeamformingVector(ueVrNetDev.Get(0));
    //     ->ChangeBeamformingVector(ueVoiceNetDev.Get(0));

    std::string typeOfRem = "DlRem";
    if (remMode == "BeamShape")
    {
        remHelper->SetRemMode(NrRadioEnvironmentMapHelper::BEAM_SHAPE);

        if (typeOfRem == "DlRem")
        {
            // remHelper->CreateRem(gNbNetDev, ueVrNetDev.Get(0), remBwpId);
            // remHelper->CreateRem(gNbNetDev, ueVoiceNetDev.Get(0), remBwpId);
            if (voiceUeNum > 0){
                remHelper->CreateRem(gNbNetDev, ueVoiceNetDev.Get(0), remBwpId);
            }
            else
            {
                remHelper->CreateRem(gNbNetDev, ueVrNetDev.Get(0), remBwpId);
            }
        }
        else if (typeOfRem == "UlRem")
        {
            remHelper->CreateRem(ueVrNetDev, gNbNetDev.Get(0), remBwpId);
        }
        else
        {
            NS_ABORT_MSG("typeOfRem not supported. "
                         "Choose among 'DlRem', 'UlRem'.");
        }
    }
    else if (remMode == "CoverageArea")
    {
        remHelper->SetRemMode(NrRadioEnvironmentMapHelper::COVERAGE_AREA);
        // remHelper->CreateRem(gNbNetDev, ueVrNetDev.Get(0), remBwpId); // FIXME
        if (voiceUeNum > 0){
            remHelper->CreateRem(gNbNetDev, ueVoiceNetDev.Get(0), remBwpId);
        }
        else
        {
            remHelper->CreateRem(gNbNetDev, ueVrNetDev.Get(0), remBwpId);
        }
    }
    else if (remMode == "UeCoverage")
    {
        remHelper->SetRemMode(NrRadioEnvironmentMapHelper::UE_COVERAGE);
        remHelper->CreateRem(ueVrNetDev, gNbNetDev.Get(0), remBwpId);
    }
    else
    {
        // NS_ABORT_MSG("Not supported remMode.");
    }

    if (LOG_ENABLE)
    {
        outputFile_position << "ue\tx\ty\ttime" << std::endl;
        for (double i=0; i<=simTimeMs; i+=1){
            Simulator::Schedule(MilliSeconds(400+i), &positionTracer, ueNodes);
        }
    }
    std::vector<std::unique_ptr<PeriodicThroughputTracer>> periodicTracers;
    // std::vector<std::unique_ptr<Tracer>> tracers; // Vector to hold the dynamically allocated Tracer instances
    for (int i=0; i<(arUeNum+vrUeNum+cgUeNum); i++){
        // auto tracer = std::make_unique<Tracer>(i+1); // Dynamically allocate a new Tracer instance
        // std::string rxCallbackPath = "/NodeList/" + std::to_string(i+1) + "/ApplicationList/0/$ns3::PacketSink/Rx"; // vr1 -> NodeList/4, vr2 -> NodeList/5, cg1 -> NodeList/6, cg2 -> NodeList/7
        // Config::ConnectWithoutContext(rxCallbackPath, MakeCallback(&Tracer::rxTracer_avgWindow, tracer.get()));
        // tracers.push_back(std::move(tracer)); // Store the unique_ptr in the vector to maintain ownership and ensure persistence
        // Primera llamada a los 2.4 segundos, luego cada 2 segundos
        auto tracer = std::make_unique<PeriodicThroughputTracer>(i + 1, 1.0, 1.0); // TODO revise
        std::string rxCallbackPath = "/NodeList/" + std::to_string(i + 1) + "/ApplicationList/0/$ns3::PacketSink/Rx";
        Config::ConnectWithoutContext(rxCallbackPath, MakeCallback(&PeriodicThroughputTracer::RxTrace, tracer.get()));
        periodicTracers.push_back(std::move(tracer));
    }

    if (schedulerType=="DPPA" and enable_xApp){
        connectToRestServer(rest_port, arUeNum + vrUeNum + cgUeNum + voiceUeNum, server_model, envType);
    }

    Simulator::Stop(MilliSeconds(simTimeMs));
    Simulator::Run();

    // // std::ofstream outputFile("res.txt");

    // // outputFile << "thput\trxBytes\tduration\n";

    // Print per-flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double averageFlowThroughput = 0.0;
    double averageFlowDelay = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::stringstream protoStream;
        protoStream << (uint16_t)t.protocol;
        if (t.protocol == 6)
        {
            protoStream.str("TCP");
        }
        if (t.protocol == 17)
        {
            protoStream.str("UDP");
        }

        Time txDuration = MilliSeconds(appDuration);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
                  << t.destinationAddress << ":" << t.destinationPort << ") proto "
                  << protoStream.str() << "\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  TxOffered:  "
                  << ((i->second.txBytes * 8.0) / txDuration.GetSeconds()) * 1e-6 << " Mbps\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";

        if (i->second.rxPackets > 0)
        {
            // Measure the duration of the flow from receiver's perspective
            Time rxDuration = i->second.timeLastRxPacket - i->second.timeFirstTxPacket;
            averageFlowThroughput += ((i->second.rxBytes * 8.0) / rxDuration.GetSeconds()) * 1e-6;
            averageFlowDelay += 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;

            //double throughput = ((i->second.rxBytes * 8.0) / rxDuration.GetSeconds()) * 1e-6;
            double throughput = ((i->second.rxBytes * 8.0) / simTimeMs) * 1e-3;
            double delay = 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;
            double jitter = 1000 * i->second.jitterSum.GetSeconds() / i->second.rxPackets;

            std::cout << "  Throughput: " << throughput << " Mbps\n";
            // std::cout << i->second.rxBytes * 8.0 << " / " << rxDuration.GetSeconds() << std::endl;
            // std::cout << "tf = " << i->second.timeLastRxPacket << " t0 = " << i->second.timeFirstTxPacket << std::endl;
            // double thput_prueba = ((i->second.rxBytes * 8.0) / txDuration.GetSeconds()) * 1e-6;
            // std::cout << "  Throughput*: " << thput_prueba << " Mbps\n";
            std::cout << "  Mean delay:  " << delay << " ms\n";
            std::cout << "  Mean jitter:  " << jitter << " ms\n";

            // // outputFile << throughput << "\t" << i->second.rxBytes << "\t" << rxDuration.GetSeconds() << std::endl;
        }
        else
        {
            std::cout << "  Throughput:  0 Mbps\n";
            std::cout << "  Mean delay:  0 ms\n";
            std::cout << "  Mean upt:  0  Mbps \n";
            std::cout << "  Mean jitter: 0 ms\n";

            // // outputFile << 0 << "\t" << 0 << "\t" << 0 << std::endl;
        }
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    }

    std::cout << "\n\n  Mean flow throughput: " << averageFlowThroughput / stats.size()
              << "Mbps \n";
    std::cout << "  Mean flow delay: " << averageFlowDelay / stats.size() << " ms\n";

    for (uint32_t i = 0; i < ueNodes.GetN(); i++)
    {
        std::cout << "UE " << i << " position: " << ueNodes.Get(i)->GetObject<MobilityModel>()->GetPosition() << std::endl;
    }

    // // if (optuna_trial >= 0) {
    // //     // std::string data = "{\"trial_num\": " + std::to_string(optuna_trial) + "}";
    // //     httplib::Client cli("http://localhost:8000");
    // //     auto res = cli.Post("/report_final_reward");
    // // }

    if (LOG_ENABLE) {SimulationDataLogger::SaveToFiles();}

    Simulator::Destroy();

    std::cout << "=== END OF SIMULATION ===" << std::endl;

    return 0;
}