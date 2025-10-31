/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-mac-scheduler-ofdma-dppa.h"

#include "nr-mac-scheduler-ue-info-dppa.h"

#include <ns3/log.h>

#include <fstream>

#include "httplib.h"
#include "json.hpp"

#include "simulation-data-logger.h"

#include <chrono>

#include "nr-fh-control.h"

bool LOG_ENABLE = true;

static const int MCS_SWITCH_INTERVAL = 10;
static int mcs_counter = 0;

#include <random>
static std::random_device rd_mcs;
static std::mt19937 gen_mcs(rd_mcs());
static std::uniform_real_distribution<> dist_mcs_prob(0.0, 1.0);
static std::uniform_real_distribution<> dist_steps_prob(1, 5);
static std::uniform_real_distribution<> dist_steps_prob28(1, 1);
static std::uniform_real_distribution<> dist_steps_prob20(1, 1);
int MCS_UPDATE_INTERVAL = 1;

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("NrMacSchedulerOfdmaDPPA");
NS_OBJECT_ENSURE_REGISTERED(NrMacSchedulerOfdmaDPPA);

std::ofstream timingFile;
bool enable_xApp = true; // if true, it connects to the xApp REST server
// std::vector<std::shared_ptr<NrMacSchedulerUeInfo>> NrMacSchedulerOfdmaDPPA::m_allUEs;
std::vector<std::pair<UePtr, uint32_t>> NrMacSchedulerOfdmaDPPA::m_allUEs;

// Historial de estados (máximo 2)
std::vector<int> state_history;

struct Action {
        int v;
        std::vector<int> wq;
        std::vector<int> wg;
    };

#include <unistd.h>
std::string csv_filename;
std::ofstream file;
int count = 0;  // Contador de líneas escritas en el CSV
void initialize_csv() {
    static bool initialized = false;
    if (!initialized) {
        std::ostringstream filename;
        filename << "dataset_" << getpid() << ".csv";
        csv_filename = filename.str();
        // std::ofstream file(csv_filename);
        file.open(csv_filename);
        if (file.is_open()) {
            // Escribir header
            file << "mcs_1,mcs_2,mcs_3,mcs_4,avg_buffer_1,avg_buffer_2,avg_buffer_3,avg_buffer_4,"
                 << "v,w_q,w_g,reward_thput,reward_thput_resources,"
                 << "next_mcs_1,next_mcs_2,next_mcs_3,next_mcs_4,next_avg_buffer_1,next_avg_buffer_2,next_avg_buffer_3,next_avg_buffer_4\n";
            // file.close();
            // std::cout << "CSV file initialized: " << csv_filename << std::endl;
        }
        initialized = true;
    }
}

// Función para escribir la fila
void save_to_csv(const std::vector<int>& state, const Action& action, float reward_thput, float reward) {
    if (!state_history.empty()) {
        
        // std::ofstream file(csv_filename, std::ios::app);

        // Imprimir state0
        for (int val : state_history)
            file << val << ",";

        file << action.v << ",";

        for (size_t i = 0; i < action.wq.size(); ++i)
            file << action.wq[i] << ",";
        for (size_t i = 0; i < action.wg.size(); ++i)
            file << action.wg[i] << ",";

        // Recompensas (con 2 decimales)
        file << std::fixed << std::setprecision(2) << reward_thput << "," << reward << ",";

        // Imprimir state1
        // for (int val : state)
        //     file << val << ",";
        std::copy(state.begin(), state.end() - 1, std::ostream_iterator<int>(file, ","));
        file << state.back();  // Último elemento sin coma

        file << "\n";
        count++;
        if (count % 100 == 0) {
            file.flush();  // fuerza escritura cada x líneas
        }
        // file.close();
    }
    state_history = state;
}

// Crear un generador de números aleatorios
std::random_device rd;
std::mt19937 gen(rd());
// Definir distribuciones para cada parámetro
std::uniform_int_distribution<> dist_v(0, 10);
std::uniform_int_distribution<> dist_wq(0, 3);
std::uniform_int_distribution<> dist_wg(0, 3);

TypeId
NrMacSchedulerOfdmaDPPA::GetTypeId()
{
    static TypeId tid = 
        TypeId("ns3::NrMacSchedulerOfdmaDPPA")
            .SetParent<NrMacSchedulerOfdma>()
            .AddConstructor<NrMacSchedulerOfdmaDPPA>()
            .AddAttribute(
                "DppV",
                "Value of the weight value to consider the objective function",
                DoubleValue(0),
                MakeDoubleAccessor(&NrMacSchedulerOfdmaDPPA::m_v_lyapunov),
                MakeDoubleChecker<double>(0))
            .AddAttribute(
                "DppWeightQ",
                "Value of the weight value to consider the queue Q",
                DoubleValue(1),
                MakeDoubleAccessor(&NrMacSchedulerOfdmaDPPA::m_weight_q),
                MakeDoubleChecker<double>(0))
            .AddAttribute(
                "DppWeightG",
                "Value of the weight value to consider the queue G",
                DoubleValue(1),
                MakeDoubleAccessor(&NrMacSchedulerOfdmaDPPA::m_weight_g),
                MakeDoubleChecker<double>(0))
            .AddAttribute(
                "EnableVirtualQueue",
                "Enable the throughput virtual queue",
                BooleanValue(true),
                MakeBooleanAccessor(&NrMacSchedulerOfdmaDPPA::m_enableVirtualQueue),
                MakeBooleanChecker())
            .AddAttribute(
                "ReconfigPeriod",
                "The time interval in seconds between consecutive adjustments of the scheduler's configuration.",
                DoubleValue(2),
                MakeDoubleAccessor(&NrMacSchedulerOfdmaDPPA::m_reconfig_period),
                MakeDoubleChecker<double>(0.1))
            .AddAttribute(
                "RestServerPort",
                "The port number used to connect to the REST server.",
                UintegerValue(8080),
                MakeUintegerAccessor(&NrMacSchedulerOfdmaDPPA::m_rest_server_port),
                MakeUintegerChecker<uint16_t>(1024, 65535));

    return tid;
}

NrMacSchedulerOfdmaDPPA::NrMacSchedulerOfdmaDPPA()
    : NrMacSchedulerOfdma(),
      m_drl_itf(ns3::Ns3DrlInterface::GetInstance())
{
    NS_LOG_FUNCTION(this);

    timingFile.open("client_timestamps.csv", std::ios::out);
    timingFile << "t_request_us,t_response_us\n";

    // if (enable_xApp){
    // initialize_csv();
    Simulator::Schedule(NanoSeconds(1), &NrMacSchedulerOfdmaDPPA::ScheduleRestRequest, this);
    // }
}

std::shared_ptr<NrMacSchedulerUeInfo>
NrMacSchedulerOfdmaDPPA::CreateUeRepresentation(
    const NrMacCschedSapProvider::CschedUeConfigReqParameters& params) const
{
    // NrMacSchedulerUeInfoDPPA::SetDppWeights (m_v_lyapunov, m_weight_q, m_weight_g);

    NS_LOG_FUNCTION(this);
    return std::make_shared<NrMacSchedulerUeInfoDPPA>(
        params.m_rnti,
        params.m_beamId,
        std::bind(&NrMacSchedulerOfdmaDPPA::GetNumRbPerRbg, this));
}

void
NrMacSchedulerOfdmaDPPA::AllocateCurrentResourceToUe(std::shared_ptr<NrMacSchedulerUeInfo> currentUe,
                                                 const uint32_t& currentRbg,
                                                 const uint32_t beamSym,
                                                 FTResources& assignedResources,
                                                 std::vector<bool>& availableRbgs)
{
    // Assign 1 RBG for each available symbols for the beam,
    // and then update the count of available resources
    auto& assignedRbgs = currentUe->m_dlRBG;
    auto existingRbgs = assignedRbgs.size();
    assignedRbgs.resize(assignedRbgs.size() + beamSym);
    std::fill(assignedRbgs.begin() + existingRbgs, assignedRbgs.end(), currentRbg);
    assignedResources.m_rbg++; // We increment one RBG

    auto& assignedSymbols = currentUe->m_dlSym;
    auto existingSymbols = assignedSymbols.size();
    assignedSymbols.resize(assignedSymbols.size() + beamSym);
    std::iota(assignedSymbols.begin() + existingSymbols, assignedSymbols.end(), 0);
    assignedResources.m_sym = beamSym; // We keep beams per symbol fixed, since it depends on beam

    availableRbgs.at(currentRbg) = false; // Mark RBG as occupied
}

bool
NrMacSchedulerOfdmaDPPA::AttemptAllocationOfCurrentResourceToUe(
    std::vector<UePtrAndBufferReq>::iterator schedInfoIt,
    std::set<uint32_t>& remainingRbgSet,
    const uint32_t beamSym,
    FTResources& assignedResources,
    std::vector<bool>& availableRbgs) const
{
    auto currentUe = schedInfoIt->first;

    uint32_t currentRbgPos = std::numeric_limits<uint32_t>::max();

    // Use wideband information in case there is no sub-band feedback yet
    if (currentUe->m_dlSbMcsInfo.empty() ||
        m_mcsCsiSource == NrMacSchedulerUeInfo::McsCsiSource::WIDEBAND_MCS)
    {
        currentRbgPos = *remainingRbgSet.begin();
    }
    else
    {
        // Find the best resource for UE among the available ones
        int maxCqi = 0;
        for (auto resourcePos : remainingRbgSet)
        {
            const auto resourceSb = currentUe->m_rbgToSb.at(resourcePos);
            if (currentUe->m_dlSbMcsInfo.at(resourceSb).cqi > maxCqi)
            {
                currentRbgPos = resourcePos;
                maxCqi = currentUe->m_dlSbMcsInfo.at(resourceSb).cqi;
            }
        }

        // Do not schedule RBGs that are lower than 4 CQI than maximum
        if (!currentUe->m_dlRBG.empty())
        {
            const auto bestCqi =
                currentUe->m_dlSbMcsInfo.at(currentUe->m_rbgToSb.at(currentUe->m_dlRBG.at(0))).cqi;
            if (maxCqi < bestCqi - 4)
            {
                return false;
            }
        }

        // Do not schedule RBGs with sub-band CQI equals to zero
        if (currentRbgPos == std::numeric_limits<uint32_t>::max())
        {
            return false;
        }
    }

    AllocateCurrentResourceToUe(currentUe,
                                currentRbgPos,
                                beamSym,
                                assignedResources,
                                availableRbgs);
    // Save previous tbSize to check if we need to undo this allocation because of a bad
    // MCS
    const auto previousTbSize = currentUe->m_dlTbSize;

    // std::cout << Simulator::Now().GetMilliSeconds() << " | Allocated RBG "
    //           << currentRbgPos << " to UE " << currentUe->m_rnti
    //           << " with beam symbols: " << beamSym << std::endl;

    AssignedDlResources(*schedInfoIt, FTResources(beamSym, beamSym), assignedResources);

    // Check if the allocated RBG had a bad MCS and lowered our overall tbsize
    const auto currentTbSize = currentUe->m_dlTbSize;
    
    // if () {
    //     std::cout << "currentTbSize " << currentTbSize << " previousTbSize " << previousTbSize << std::endl;
    // }
    
    if (currentTbSize < previousTbSize * 0.99 && currentUe->GetDlMcs() > 0)
    {
        // std::cout << "We do not understand why the tbSize has decreased after allocation" << std::endl;
        // std::exit(-1);
        // Undo allocation
        DeallocateCurrentResourceFromUe(currentUe,
                                        currentRbgPos,
                                        beamSym,
                                        assignedResources,
                                        availableRbgs);

        // Update UE stats to go back to previous state
        AssignedDlResources(*schedInfoIt, FTResources(beamSym, beamSym), assignedResources);
        return false; // Unsuccessful allocation
    }
    remainingRbgSet.erase(currentRbgPos);
    return true; // Successful allocation
}

bool
NrMacSchedulerOfdmaDPPA::ShouldScheduleUeBasedOnFronthaul(
    const std::vector<UePtrAndBufferReq>::iterator& schedInfoIt,
    uint32_t resourcesAssignable) const
{
    GetFirst GetUe;
    uint32_t quantizationStep = resourcesAssignable;
    uint32_t maxAssignable = m_nrFhSchedSapProvider->GetMaxRegAssignable(
        GetBwpId(),
        GetUe(*schedInfoIt)->m_dlMcs,
        GetUe(*schedInfoIt)->m_rnti,
        GetUe(*schedInfoIt)->m_dlRank); // maxAssignable is in REGs
    // set a minimum of the maxAssignable equal to 5 RBGs
    maxAssignable = std::max(maxAssignable, 5 * resourcesAssignable);

    // the minimum allocation is one resource in freq, containing rbgAssignable
    // in time (REGs)
    return GetUe(*schedInfoIt)->m_dlRBG.size() + quantizationStep <= maxAssignable;
}

bool
NrMacSchedulerOfdmaDPPA::AdvanceToNextUeToSchedule(
    std::vector<UePtrAndBufferReq>::iterator& schedInfoIt,
    const std::vector<UePtrAndBufferReq>::iterator end,
    uint32_t resourcesAssignable) const
{
    // Skip UEs which already have enough resources to transmit
    while (schedInfoIt != end)
    {
        const uint32_t bufQueueSize = schedInfoIt->second;
        if (schedInfoIt->first->m_dlTbSize >= std::max(bufQueueSize, 10U))
        {
            std::advance(schedInfoIt, 1);
        }
        else
        {
            if (m_nrFhSchedSapProvider &&
                m_nrFhSchedSapProvider->GetFhControlMethod() ==
                    NrFhControl::FhControlMethod::OptimizeRBs &&
                !ShouldScheduleUeBasedOnFronthaul(schedInfoIt, resourcesAssignable))
            {
                std::advance(schedInfoIt, 1);
            }
            else
            {
                return true; // UE left to schedule
            }
        }
    }
    return false; // No UE left to schedule
}

void
NrMacSchedulerOfdmaDPPA::DeallocateCurrentResourceFromUe(
    std::shared_ptr<NrMacSchedulerUeInfo> currentUe,
    const uint32_t& currentRbg,
    const uint32_t beamSym,
    FTResources& assignedResources,
    std::vector<bool>& availableRbgs)
{
    auto& assignedRbgs = currentUe->m_dlRBG;
    auto& assignedSymbols = currentUe->m_dlSym;

    assignedRbgs.resize(assignedRbgs.size() - beamSym);
    assignedSymbols.resize(assignedSymbols.size() - beamSym);

    NS_ASSERT_MSG(assignedResources.m_rbg > 0,
                  "Should have more than 0 resources allocated before deallocating");
    assignedResources.m_rbg--; // We decrement the allocated RBGs
    // We zero symbols allocated in case number of RBGs reaches 0
    assignedResources.m_sym = assignedResources.m_rbg == 0 ? 0 : assignedResources.m_sym;
    availableRbgs.at(currentRbg) = true;
}

void
NrMacSchedulerOfdmaDPPA::DeallocateResourcesDueToFronthaulConstraint(
    const std::vector<UePtrAndBufferReq>& ueVector,
    const uint32_t& beamSym,
    FTResources& assignedResources,
    std::vector<bool>& availableRbgs) const
{
    GetFirst GetUe;
    std::vector<UePtrAndBufferReq> fhUeVector = ueVector;
    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(fhUeVector), std::end(fhUeVector), rng);
    for (auto schedInfoIt : fhUeVector)
    {
        const auto numAssignedResourcesToUe = GetUe(schedInfoIt)->m_dlRBG.size();
        if (numAssignedResourcesToUe > 0) // UEs with an actual allocation
        {
            if (DoesFhAllocationFit(GetBwpId(),
                                    GetUe(schedInfoIt)->GetDlMcs(),
                                    numAssignedResourcesToUe,
                                    GetUe(schedInfoIt)->m_dlRank) == 0)
            {
                // remove allocation if the UE does not fit in the available FH
                // capacity
                while (!schedInfoIt.first->m_dlRBG.empty())
                {
                    uint32_t resourceAssigned = schedInfoIt.first->m_dlRBG.back();
                    DeallocateCurrentResourceFromUe(schedInfoIt.first,
                                                    resourceAssigned,
                                                    beamSym,
                                                    assignedResources,
                                                    availableRbgs);
                }
            }
        }
    }
}

void NrMacSchedulerOfdmaDPPA::saveQueuesState(const std::shared_ptr<NrMacSchedulerUeInfo>& ue, uint32_t rlc_queue) const{
    SimulationDataLogger::LogQueueG (ue->m_rnti, std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue)->m_g, Simulator::Now());
    SimulationDataLogger::LogQueueQ (ue->m_rnti, rlc_queue, Simulator::Now());
}

void NrMacSchedulerOfdmaDPPA::saveRBGallocation(const std::shared_ptr<NrMacSchedulerUeInfo>& ue) const{
    static std::ofstream outputFileAlpha("alpha.txt");
    static bool c = true;
    if(c){
        outputFileAlpha << "time\tue\tresources\tmcs\n";
    }
    c = false;
    outputFileAlpha << Simulator::Now().ToDouble (Time::MS) << "\t" << ue->m_rnti << "\t" << ue->m_dlRBG.size() << "\t" << (int)ue->m_dlMcs << "\n";
}

// bool aux = false;

NrMacSchedulerNs3::BeamSymbolMap
NrMacSchedulerOfdmaDPPA::AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const
{
    // aux = !aux;
    NS_LOG_DEBUG("-------------------- At time " << Simulator::Now());
    NS_LOG_FUNCTION(this);

    NS_LOG_DEBUG("# beams active flows: " << activeDl.size() << ", # sym: " << symAvail);
    // std::cout << "===== " << Simulator::Now().GetMilliSeconds() << " Called AssignDLRBG" << std::endl << std::endl;

    GetFirst GetBeamId;
    GetSecond GetUeVector;
    BeamSymbolMap symPerBeam = GetSymPerBeam(symAvail, activeDl);

    uint32_t total_resources = 0; // For DRL

    // Iterate through the different beams
    auto ctr = 0;
    for (const auto& el : activeDl)
    {
        NS_LOG_DEBUG(Simulator::Now() << " |  Beam " << ctr++);
        // Distribute the RBG evenly among UEs of the same beam
        uint32_t beamSym = symPerBeam.at(GetBeamId(el)); // Number of symbols available for assignment
        uint32_t rbgAssignable = 1 * beamSym;
        std::vector<UePtrAndBufferReq> ueVector; // Active UEs, i.e. with data to send
        FTResources assignedResources(0, 0); // Total number of resources assigned (RBGs, symbols)
        // const std::vector<uint8_t> dlNotchedRBGsMask = GetDlNotchedRbgMask();
        std::vector<bool> availableRbgs = GetDlBitmask();
        std::set<uint32_t> remainingRbgSet;
        for (size_t i = 0; i < availableRbgs.size(); i++)
        {
            if (availableRbgs.at(i))
            {
                remainingRbgSet.emplace(i);
            }
        }

        total_resources = rbgAssignable*GetNumRbPerRbg()*remainingRbgSet.size(); // For DRL
        // std::cout << "total_resources: " << total_resources << std::endl;

        NS_ASSERT(!remainingRbgSet.empty());

        // std::cout << Simulator::Now().GetMilliSeconds() << " |  New slot for beam " << GetBeamId(el) << " with " << beamSym
        //           << " symbols and " << remainingRbgSet.size() << " RBGs available" << std::endl;

        for (const auto& ue : GetUeVector(el))
        {
            ueVector.emplace_back(ue);
            BeforeDlSched(ueVector.back(), FTResources(beamSym, beamSym));
            if (m_allUEs.size() < m_drl_itf.GetUEsNumber())
            {
                // m_allUEs.push_back(std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue.first));
                m_allUEs.push_back(ue);
            }
            uint32_t avg_gfbr = 0.0, gfbr = 0.0;
            int i = 0;
            for (const auto& ueLcg : ue.first->m_dlLCG){
                std::vector<uint8_t> ueActiveLCs = ueLcg.second->GetActiveLCIds();
                for (const auto lcId : ueActiveLCs){
                    std::unique_ptr<NrMacSchedulerLC>& LCPtr = ueLcg.second->GetLC(lcId);
                    gfbr += unsigned(LCPtr->m_eRabGuaranteedBitrateDl);
                    i++;
                }
            }
            avg_gfbr = gfbr/i;
            // Update the state of the UE
            // TODO Update cuando el UE no está activo con thput = 0
            // std::cout << Simulator::Now().GetMilliSeconds() << " | Update state of UE " << ue.first->m_rnti << " with MCS = " << (int)ue.first->m_dlMcs << std::endl;
            m_drl_itf.UpdateState(ue.first->m_rnti-1,                                               // RNTI
                                (int)ue.first->m_dlMcs,                                       // MCS
                                ue.second,                                                          // Q
                                std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue.first)->m_g, // G
                                avg_gfbr,                                                           // GFBR
                                getPosition(ue.first->m_rnti),                                      // Position       
                                m_macSchedSapUser->GetSlotPeriod().GetSeconds(),                    // Time slot duration                        
                                Simulator::Now().GetSeconds());                                     // Last update time
        }

        // Llama al scheduler
        GetFirst GetUe;
        std::sort(ueVector.begin(), ueVector.end(), GetUeCompareDlFn()); // Sort UEs in ueVector as a function of K from smallest to largest
        // Here the ueVector is sorted by priority, but we need to check if V is sufficiently high that not resources must be allocated
        auto schedInfoIt = ueVector.begin(); // UE to whom all resources are assigned a priori

        // std::cout << Simulator::Now().GetMilliSeconds() << " | Prioritized UE: " << GetUe(*schedInfoIt)->m_rnti << std::endl;

        bool reapingResources = true;

        // for (auto& ue : ueVector)
        while (reapingResources)
        {
            // While there are resources to schedule
            while (!remainingRbgSet.empty())
            {
                // Keep track if resources are being allocated. If not, then stop.
                const auto prevRemaining = remainingRbgSet.size();

                if (m_activeDlAi)
                {
                    CallNotifyDlFn(ueVector);
                }
                // Sort UEs based on the selected scheduler policy (PF, RR, QoS, AI)
                // SortUeVector(&ueVector, std::bind(&NrMacSchedulerOfdma::GetUeCompareDlFn, this));

                // Select the first UE
                // auto schedInfoIt = ueVector.begin();
                bool lyapunov = -NrMacSchedulerUeInfoDPPA::GetVLyapunov() >= std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(GetUe(*schedInfoIt))->m_k;
                // bool lyapunov = aux ? true : false; // TEMPORAL
                // Advance schedInfoIt iterator to the next UE to schedule
                while (AdvanceToNextUeToSchedule(schedInfoIt, ueVector.end(), beamSym))
                {
                    // TODO this if only once (m_k is updated every slot) 
                    // std::cout << "UE " << GetUe(*schedInfoIt)->m_rnti << " | " << -NrMacSchedulerUeInfoDPPA::GetVLyapunov() << " >= " << std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(GetUe(*schedInfoIt))->m_k << std::endl;
                    // if (-NrMacSchedulerUeInfoDPPA::GetVLyapunov() >= std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(GetUe(*schedInfoIt))->m_k){ // The UE can receive resources
                    if (lyapunov){ // The UE can receive resources
                        NS_LOG_INFO("Priority to UE" << GetUe(*schedInfoIt)->m_rnti);
                        // std::cout << Simulator::Now().GetMilliSeconds() << " |  Priority to UE" << GetUe(*schedInfoIt)->m_rnti << std::endl;
                    }else{
                        NS_LOG_INFO("Discard UE" << GetUe(*schedInfoIt)->m_rnti);
                        // std::cout << Simulator::Now().GetMilliSeconds() << " | DISCARD UE" << GetUe(*schedInfoIt)->m_rnti << " porque " << -NrMacSchedulerUeInfoDPPA::GetVLyapunov() << " < " << std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(GetUe(*schedInfoIt))->m_k << std::endl;
                        break; // TODO Revisar
                    }
                    // Try to allocate the resource to the current UE
                    // If it fails, try again for the next UE
                    if (!AttemptAllocationOfCurrentResourceToUe(schedInfoIt,
                                                                remainingRbgSet,
                                                                beamSym,
                                                                assignedResources,
                                                                availableRbgs))
                    {
                        // std::cout << Simulator::Now().GetMilliSeconds() << " |  Failed allocation to UE" << GetUe(*schedInfoIt)->m_rnti << std::endl;
                        std::advance(schedInfoIt, 1); // Get the next UE
                        if (schedInfoIt == ueVector.end()) {
                            // std::cout << Simulator::Now().GetMilliSeconds() << " |  No more UEs to schedule, breaking" << std::endl;
                            reapingResources = false; // REVIEW
                            break;
                        }
                        // bool lyapunov = aux ? true : false; // TEMPORAL
                        lyapunov = -NrMacSchedulerUeInfoDPPA::GetVLyapunov() >= std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(GetUe(*schedInfoIt))->m_k;
                        continue;
                    }
                    // Update metrics
                    GetFirst GetUe;

                    // Update metrics for the unsuccessful UEs (who did not get any resource in this
                    // iteration)
                    for (auto& ue : ueVector)
                    {
                        if (GetUe(ue)->m_rnti != GetUe(*schedInfoIt)->m_rnti)
                        {
                            // std::cout << "(1) Llamo a NotAssignedDlResources con assignedResources.m_rbgs = " << assignedResources.m_rbg << std::endl;
                            NotAssignedDlResources(ue,
                                                   FTResources(beamSym, beamSym),
                                                   assignedResources); // FIXME Llama de 1 en 1
                        }
                    }
                    break; // Successful allocation
                }
                // No more UEs to allocate in the current beam
                if (prevRemaining == remainingRbgSet.size())
                {
                    break;
                }
            }

            // If we got here, we either allocated all resources (remainingRbgSet.empty()),
            // or the remaining RBGs do not improve TBS of UEs (prevRemaining ==
            // remainingRbgSet.size()).

            // Now we need to check if there is a UE with less than the minimal TBS.
            std::sort(ueVector.begin(), ueVector.end(), [](auto a, auto b) {
                GetFirst GetUe;
                return GetUe(a)->m_dlTbSize > GetUe(b)->m_dlTbSize;
            });

            // In case there is, reap its resources and redistribute to other UEs at same beam.
            if (!ueVector.empty() && ueVector.back().first->m_dlTbSize < 10)
            {
                auto& ue = ueVector.back();
                while (!ue.first->m_dlRBG.empty())
                {
                    auto reapedRbg = ue.first->m_dlRBG.back();
                    DeallocateCurrentResourceFromUe(ue.first,
                                                    reapedRbg,
                                                    beamSym,
                                                    assignedResources,
                                                    availableRbgs);
                    remainingRbgSet.emplace(reapedRbg);
                }
                // Update DL metrics
                AssignedDlResources(ue, FTResources(beamSym, beamSym), assignedResources);

                // After all resources were reaped, update statistics
                for (auto& uev : ueVector)
                {
                    // std::cout << "(3) Llamo a NotAssignedDlResources con assignedResources.m_rbgs = " << assignedResources.m_rbg << std::endl;
                    NotAssignedDlResources(uev, FTResources(beamSym, beamSym), assignedResources);
                }

                // Remove UE from allocation vector (it won't receive more resources in this round)
                ueVector.pop_back();
                continue;
            }
            reapingResources = false;
        }
        if (m_nrFhSchedSapProvider)
        {
            if (m_nrFhSchedSapProvider->GetFhControlMethod() ==
                NrFhControl::FhControlMethod::OptimizeMcs)
            {
                GetFirst GetUe;
                for (auto& schedInfoIt : GetUeVector(el)) // over all UEs with data
                {
                    if (!GetUe(schedInfoIt)->m_dlRBG.empty()) // UEs with an actual allocation
                    {
                        uint8_t maxMcsAssignable = m_nrFhSchedSapProvider->GetMaxMcsAssignable(
                            GetBwpId(),
                            GetUe(schedInfoIt)->m_dlRBG.size(),
                            GetUe(schedInfoIt)->m_rnti,
                            GetUe(schedInfoIt)->m_dlRank); // max MCS index assignable

                        NS_LOG_DEBUG("UE " << GetUe(schedInfoIt)->m_rnti
                                           << " MCS from sched: " << GetUe(schedInfoIt)->GetDlMcs()
                                           << " FH max MCS: " << maxMcsAssignable);

                        GetUe(schedInfoIt)->m_fhMaxMcsAssignable =
                            std::min(GetUe(schedInfoIt)->GetDlMcs(), maxMcsAssignable);
                    }
                }
            }

            if (GetFhControlMethod() == NrFhControl::FhControlMethod::Postponing ||
                GetFhControlMethod() == NrFhControl::FhControlMethod::OptimizeMcs ||
                GetFhControlMethod() == NrFhControl::FhControlMethod::OptimizeRBs)
            {
                DeallocateResourcesDueToFronthaulConstraint(ueVector,
                                                            beamSym,
                                                            assignedResources,
                                                            availableRbgs);
            }
        }
    }

    // TODO
    // - saveRBGallocation for each UE
    // - saveQueuesState for each UE
    // - Update the virtual queue for each UE
    // - Update the DRL interface with the metrics for each UE
    for (auto ue_buffer: m_allUEs){
        auto ue = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue_buffer.first);
        if(m_enableVirtualQueue)
        {
            double timeSlot = m_macSchedSapUser->GetSlotPeriod().GetNanoSeconds()/1e9;
            double aux = ue->m_g;
            ue->UpdateDlTputVirtualQueue(timeSlot);
            NS_LOG_INFO("Virtual queue UE" << ue->m_rnti << " updated " << aux << " -> " << ue->m_g);
        }
        if (LOG_ENABLE){
            saveRBGallocation(ue);
        }
        // std::cout << Simulator::Now().GetMilliSeconds() << "\t| Al UE" << ue->m_rnti << " " << ue->m_dlRBG.size() << " RBs (MCS de este slot =" << (int)ue->m_dlMcs << ")\n";
        // std::cout << "+ UE" << ue->m_rnti << "->m_g = " << ue_buffer.second << std::endl;
        // saveQueuesState(ue, ue_buffer.second); // FIXME
        m_drl_itf.UpdateMetricsRbs(ue->m_rnti-1, GetNumRbPerRbg()*ue->m_dlRBG.size(), GetNumRbPerRbg()*total_resources);
    }

    return symPerBeam;
}

void
NrMacSchedulerOfdmaDPPA::BeforeDlSched(const UePtrAndBufferReq& ue,
                                      const FTResources& assignableInIteration) const
{
    NS_LOG_FUNCTION(this);
    auto uePtr = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue.first);

    // Update K
    // uePtr->m_k = -(ue.second + uePtr->m_g) * m_v_lyapunov * assignableInIteration.m_rbg*GetNumRbPerRbg();
    uePtr->UpdateDlK(ue.second, assignableInIteration, m_dlAmc, m_macSchedSapUser->GetSlotPeriod().GetNanoSeconds()/1e9);

    if (LOG_ENABLE) {saveQueuesState(ue.first, ue.second);}
    // uePtr->m_k = -(ue.second + uePtr->m_g) * m_dlAmc->CalculateTbSize(uePtr->m_dlMcs, 1, assignableInIteration.m_rbg*GetNumRbPerRbg());
    NS_LOG_INFO("CalculateTbSize con MCS de " << (int)uePtr->m_dlMcs << ", #RBs de " << assignableInIteration.m_rbg*GetNumRbPerRbg());
    NS_LOG_INFO("UE" << uePtr->m_rnti << "->m_k = -" 
            << m_dlAmc->CalculateTbSize(uePtr->m_dlMcs, 1, assignableInIteration.m_rbg*GetNumRbPerRbg()) 
            << " * (" << m_weight_q << " * " << ue.second << " + "
            << m_weight_g << " * " << uePtr->m_g << ") = " 
            << uePtr->m_k);
}

void
NrMacSchedulerOfdmaDPPA::AssignedDlResources(const UePtrAndBufferReq& ue,
                                            [[maybe_unused]] const FTResources& assigned,
                                            [[maybe_unused]] const FTResources& totAssigned) const
{
    NS_LOG_FUNCTION(this);
    ue.first->UpdateDlMetric(); // Actualiza m_dlTbSize
}

void
NrMacSchedulerOfdmaDPPA::NotAssignedDlResources(
    const UePtrAndBufferReq& ue,
    [[maybe_unused]] const FTResources& assigned,
    [[maybe_unused]] const FTResources& totAssigned) const
{
    NS_LOG_FUNCTION(this);
    auto uePtr = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue.first);
    ue.first->UpdateDlMetric(); // Actualiza m_dlTbSize
}

void
NrMacSchedulerOfdmaDPPA::AssignedUlResources(const UePtrAndBufferReq& ue,
                                            [[maybe_unused]] const FTResources& assigned,
                                            [[maybe_unused]] const FTResources& totAssigned) const
{
    NS_LOG_FUNCTION(this);
    GetFirst GetUe;
    // GetUe(ue)->UpdateUlMetric(m_ulAmc);
    GetUe(ue)->UpdateUlMetric();
}

std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq& lhs,
                   const NrMacSchedulerNs3::UePtrAndBufferReq& rhs)>
NrMacSchedulerOfdmaDPPA::GetUeCompareDlFn() const
{
    return NrMacSchedulerUeInfoDPPA::CompareUeWeightsDl;
}

std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq& lhs,
                   const NrMacSchedulerNs3::UePtrAndBufferReq& rhs)>
NrMacSchedulerOfdmaDPPA::GetUeCompareUlFn() const
{
    return NrMacSchedulerUeInfoDPPA::CompareUeWeightsUl;
}

void
NrMacSchedulerOfdmaDPPA::ScheduleRestRequest() const
{
    Simulator::Schedule(Seconds(m_reconfig_period), &NrMacSchedulerOfdmaDPPA::MakeRestRequest, this);
}

using json = nlohmann::json;

void
NrMacSchedulerOfdmaDPPA::MakeRestRequest() const
{
    if (!enable_xApp){
        m_drl_itf.CalculateReward();
        // // Action action;
        // // action.v = dist_v(gen);
        // // action.wq = {dist_wq(gen)};
        // // action.wg = {dist_wg(gen)};
        for (auto ue_buffer : m_allUEs) {
            auto ue = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue_buffer.first);
            // ue->SetDppWeights(action.v, action.wq[0], action.wg[0]);
            ue->SetDppWeights(m_v_lyapunov, 0, 1);
        }
        // TODO: Implement this function to save the state and action to a CSV file
        // std::vector<int> state;
        // for(int i = 0; i < m_drl_itf.GetUEsNumber(); i++){
        //     state.push_back(m_drl_itf.GetMcs(i) < 22 ? 0 : (m_drl_itf.GetMcs(i) < 25 ? 1 : 2));
        //     state.push_back(m_drl_itf.GetQ(i) < 1.5e6 ? 0 : (m_drl_itf.GetQ(i) < 3e6 ? 1 : 2));
        //     state.push_back(m_drl_itf.GetG(i) < 1e7 ? 0 : (m_drl_itf.GetG(i) < 1e9 ? 1 : 2));
        // }
        // save_to_csv(state, action, m_drl_itf.GetRewardThput(), m_drl_itf.GetReward()); // FIXME rewards
        
        // TODO Cola G de cada UE a cero
        // for (auto ue_buffer : m_allUEs) {
        //     auto ue = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue_buffer.first);
        //     ue->m_g = 0;
        // }
        
        m_drl_itf.Reset();
        Simulator::Schedule(Seconds(m_reconfig_period), &NrMacSchedulerOfdmaDPPA::MakeRestRequest, this);
        return; // Salir si no hay xApp
    }
    
    json json_data;
    json_data["time"] = Simulator::Now().GetSeconds();
    // std::cout << "Time: " << json_data["time"] << std::endl;
    m_drl_itf.CalculateReward();
    json_data["reward"] = m_drl_itf.GetReward();
    json_data["reward_thput"] = m_drl_itf.GetRewardThput();
    
    static int mcs_step_counter = 0;
    mcs_step_counter++;

    //// Only with fixed MCS for testing ////
    // Every MCS_UPDATE_INTERVAL step, choose new MCS for each UE
    // if (mcs_step_counter >= MCS_UPDATE_INTERVAL) {
    //     for(int i = 0; i < m_drl_itf.GetUEsNumber(); i++){
    //         double random_prob = dist_mcs_prob(gen_mcs);
    //         if (random_prob < 0.15) {
    //             m_drl_itf.m_mcs_prueba[i] = 20;  // 15% low MCS
    //             MCS_UPDATE_INTERVAL = dist_steps_prob20(gen_mcs);
    //         } else {
    //             m_drl_itf.m_mcs_prueba[i] = 28;  // 85% high MCS
    //             MCS_UPDATE_INTERVAL = dist_steps_prob28(gen_mcs); // New random interval
    //         }
    //     }
    //     mcs_step_counter = 0; // Reset counter
    // }
    ///////////////////////////

    for(int i = 0; i < m_drl_itf.GetUEsNumber(); i++){
        json_data["state"]["mcs"].push_back(m_drl_itf.GetMcs(i) < 22 ? 0 : (m_drl_itf.GetMcs(i) < 25 ? 1 : 2));
        // json_data["state"]["mcs"].push_back(m_drl_itf.GetMcs(i) < 25 ? 1 : 2);
        // json_data["state"]["mcs"].push_back(m_drl_itf.GetMcs(i) < 25 ? 0 : 2);
        json_data["state"]["q"].push_back(m_drl_itf.GetQ(i) < 1e6 ? 0 : (m_drl_itf.GetQ(i) < 5e6 ? 1 : 2));
        json_data["state"]["g"].push_back(m_drl_itf.GetG(i) < 1e6 ? 0 : (m_drl_itf.GetG(i) < 1e7 ? 1 : 2));
        json_data["state"]["gfbr"].push_back((int) (m_drl_itf.GetGfbr(i)/1e6));
        json_data["state"]["closeToObstacle"].push_back(m_drl_itf.GetCloseToObs(i) ? 1 : 0); // NOTE: comment if fixed MCS

        //// Only with fixed MCS for testing ////
        // json_data["state"]["closeToObstacle"].clear(); // Comment json_data["state"]["closeToObstacle"].push_back() instead
        // Set closeToObstacle based on current MCS
        // if (m_drl_itf.m_mcs_prueba[i] == 20) {
        //     json_data["state"]["closeToObstacle"].push_back(1);
        // } else {
        //     json_data["state"]["closeToObstacle"].push_back(0);
        // }
        ///////////////////////////
    }
    
    mcs_counter++;

    // Reset G queues after sending the state to the xApp 
    // // for (auto ue_buffer : m_allUEs) {
    // //     auto ue = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue_buffer.first);
    // //     ue->m_g = 0;
    // // }

    // Verificar si el JSON está vacío
    if (json_data["state"].empty()) {
        std::cerr << "JSON data is empty. Not sending the request." << std::endl;
        return;
    }

    std::string json_str = json_data.dump();

    // Get timing for the request
    // auto now = std::chrono::high_resolution_clock::now();
    // auto epoch = now.time_since_epoch();
    // auto t1_us = std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();

    // HTTP POST using httplib
    // std::cout << Simulator::Now().GetMilliSeconds() << " | Sending REST request: " << json_str << std::endl;
    httplib::Client cli("http://localhost:" + std::to_string(m_rest_server_port));
    auto res = cli.Post("/infer_sched_config", json_str, "application/json");

    // Get timing for the response
    // now = std::chrono::high_resolution_clock::now();
    // epoch = now.time_since_epoch();
    // auto t2_us = std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();

    // timingFile << t1_us << "," << t2_us << std::endl;

    if (res) {
        // std::cout << "Response from server: " << res->body << std::endl;
        auto actions = nlohmann::json::parse(res->body);
        // std::cout << "SetDppWeights(" << actions["v"] << ", " << actions["wq"] << ", " << actions["wg"] << ")" << std::endl;
        // NrMacSchedulerUeInfoDPPA::SetDppWeights (actions["v"], actions["wq"], actions["wg"]);
        if (m_ueNodes.GetN() == 0) {
            std::cerr << "Error: there are no nodes in m_ueNodes." << std::endl;
        } else {
            // for (uint32_t i = 0; i < m_ueNodes.GetN(); i++)
            // std::cout << "UEs activos: " << m_drl_itf.GetUEsNumber() << std::endl;
            for (auto ue_buffer : m_allUEs)
            {
                auto ue = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue_buffer.first);
                if (ue == nullptr) {
                    std::cerr << "Error: UE con índice es nulo" << std::endl;
                    continue;
                }
                if (actions["wq"].is_array()) {
                    // std::cout << "actions es un array con " << actions["wq"].size() << " elementos" << std::endl;
                    // std::cout << "UE " << ue->GetId()-1 << ": "
                            //   << "position = " << ue->GetObject<MobilityModel>()->GetPosition() << ", "
                            // << "v = " << actions["v"] << ", "
                            // << "wq_" << ue->GetId()-1 << " = " << actions["wq"][ue->GetId()-1] << ", "
                            // << "wg_" << ue->GetId()-1 << " = " << actions["wg"][ue->GetId()-1] << std::endl;
                    ue->SetDppWeights (actions["v"], actions["wq"][ue->GetId()-1], actions["wg"][ue->GetId()-1]);
                } else {
                    // std::cout << "actions no es un array, se aplica a todos los UEs" << std::endl;
                    ue->SetDppWeights (actions["v"], actions["wq"], actions["wg"]);
                }
            }
        }
    } else {
        for (auto ue_buffer : m_allUEs) {
            auto ue = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(ue_buffer.first);
            ue->SetDppWeights(0,0,0); // Test
        }
        // NS_LOG_UNCOND("Error en la solicitud HTTP: " << res.error());
    }

    m_drl_itf.Reset();
    Simulator::Schedule(Seconds(m_reconfig_period), &NrMacSchedulerOfdmaDPPA::MakeRestRequest, this);
}

Vector
NrMacSchedulerOfdmaDPPA::getPosition(int rnti) const
{
    // rnti += 3;
    Vector pos_ue, vel_ue;
    if (m_ueNodes.GetN() == 0) {
        std::cerr << "Error: No hay nodos en m_ueNodes." << std::endl;
    } else {
        Ptr<Node> node = m_ueNodes.Get(rnti);
        if (node != nullptr) {
            // std::cout << "Nodo " << rnti << ": " << node->GetId() << std::endl;
            Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
            if (mobility != nullptr) {
                pos_ue = mobility->GetPosition();
            } else {
                std::cerr << "Error: Nodo " << rnti << " no tiene MobilityModel." << std::endl;
            }
        } else {
            std::cerr << "Error: Nodo " << rnti << " es nulo." << std::endl;
        }
    }
    return pos_ue;
}
    

} // namespace ns3
