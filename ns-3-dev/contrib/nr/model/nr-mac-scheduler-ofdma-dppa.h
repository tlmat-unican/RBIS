/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "nr-mac-scheduler-ofdma.h"

#include "ns3-drl-interface.h"

#include "ns3/node-container.h"

#include "ns3/mobility-module.h"

namespace ns3
{

/**
 * \ingroup scheduler
 * \brief Assign frequencies in a round-robin fashion
 *
 * Each UE will receive a proportional number of frequencies, with a fixed
 * number of symbols depending on the requirements of each beam. With \f$n\f$ UE,
 * each one will receive:
 *
 * \f$ freq_{i} = \frac{totFreq}{n} \f$
 *
 * If \f$ n > totFreq \f$, then there will be UEs which will not have any
 * resource assigned. The class does not remember the UEs which did not get
 * any resource in the previous slot, so this opens the door to a possible
 * starvation.
 *
 * \see NrMacSchedulerUeInfoDPP
 */
class NrMacSchedulerOfdmaDPPA : public NrMacSchedulerOfdma
{
  public:

    /**
     * \brief GetTypeId
     * \return The TypeId of the class
     */
    static TypeId GetTypeId();

    /**
     * \brief NrMacSchedulerTdmaDPP constructor
     */
    NrMacSchedulerOfdmaDPPA();

    /**
     * \brief Set the value of attribute "v_lyapunov"
     * \param v
     */
    //void SetVlyapunov(double v);

    /**
     * \brief Get the value of attribute "v_lyapunov"
     * @return
     */
    //double GetVlyapunov() const;

  protected:
    /**
     * \brief Create an UE representation of the type NrMacSchedulerUeInfoDPP
     * \param params parameters
     * \return NrMacSchedulerUeInfoDPP instance
     */
    std::shared_ptr<NrMacSchedulerUeInfo> CreateUeRepresentation(
        const NrMacCschedSapProvider::CschedUeConfigReqParameters& params) const override;

    /**
     * \brief Return the comparison function to sort DL UE according to the scheduler policy
     * \return a pointer to NrMacSchedulerUeInfoDPP::CompareUeWeightsDl
     */
    std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq& lhs,
                       const NrMacSchedulerNs3::UePtrAndBufferReq& rhs)>
    GetUeCompareDlFn() const override;

    /**
     * \brief Return the comparison function to sort UL UE according to the scheduler policy
     * \return a pointer to NrMacSchedulerUeInfoDPP::CompareUeWeightsUl
     */
    std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq& lhs,
                       const NrMacSchedulerNs3::UePtrAndBufferReq& rhs)>
    GetUeCompareUlFn() const override;

    /**
     * \brief Update the UE representation after a symbol (DL) has been assigned to it
     * \param ue UE to which a symbol has been assigned
     * \param assigned the amount of resources assigned
     * \param totAssigned the total amount of resources assigned in the slot
     *
     * Update DL metrics by calling NrMacSchedulerUeInfoDPP::UpdateDlMetric
     */
    void AssignedDlResources(const UePtrAndBufferReq& ue,
                             const FTResources& assigned,
                             const FTResources& totAssigned) const override;

    /**
     * \brief Update the UE representation after a symbol (DL) has been assigned to it
     * \param ue UE to which a symbol has been assigned
     * \param assigned the amount of resources assigned
     * \param totAssigned the total amount of resources assigned in the slot
     *
     * Update DL metrics by calling NrMacSchedulerUeInfoDPP::UpdateUlMetric
     */
    void AssignedUlResources(const UePtrAndBufferReq& ue,
                             const FTResources& assigned,
                             const FTResources& totAssigned) const override;

    // RR is a simple scheduler: it doesn't do anything in the next
    // inherited calls.
    void NotAssignedDlResources(const UePtrAndBufferReq& ue,
                                const FTResources& notAssigned,
                                const FTResources& totalAssigned) const override;

    void NotAssignedUlResources(const UePtrAndBufferReq& ue,
                                const FTResources& notAssigned,
                                const FTResources& totalAssigned) const override
    {
    }

    void BeforeDlSched(const UePtrAndBufferReq& ue,
                       const FTResources& assignableInIteration) const override;

    void BeforeUlSched(const UePtrAndBufferReq& ue,
                       const FTResources& assignableInIteration) const override
    {
    }

    BeamSymbolMap AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const override;

    void saveQueuesState(const std::shared_ptr<NrMacSchedulerUeInfo>& ue, uint32_t rlc_queue) const;
    void saveRBGallocation(const std::shared_ptr<NrMacSchedulerUeInfo>& ue) const;

    void ScheduleRestRequest() const;
    void MakeRestRequest() const;

    Vector getPosition(int rnti) const;

  private:
    static void AllocateCurrentResourceToUe(std::shared_ptr<NrMacSchedulerUeInfo> currentUe,
                                            const uint32_t& currentRbg,
                                            const uint32_t beamSym,
                                            FTResources& assignedResources,
                                            std::vector<bool>& availableRbgs);
    bool ShouldScheduleUeBasedOnFronthaul(
        const std::vector<UePtrAndBufferReq>::iterator& schedInfoIt,
        uint32_t resourcesAssignable) const;
    bool AttemptAllocationOfCurrentResourceToUe(
        std::vector<UePtrAndBufferReq>::iterator schedInfoIt,
        std::set<uint32_t>& remainingRbgSet,
        const uint32_t beamSym,
        FTResources& assignedResources,
        std::vector<bool>& availableRbgs) const;
    bool AdvanceToNextUeToSchedule(std::vector<UePtrAndBufferReq>::iterator& schedInfoIt,
                                   const std::vector<UePtrAndBufferReq>::iterator end,
                                   uint32_t resourcesAssignable) const;
    void DeallocateResourcesDueToFronthaulConstraint(const std::vector<UePtrAndBufferReq>& ueVector,
                                                     const uint32_t& beamSym,
                                                     FTResources& assignedResources,
                                                     std::vector<bool>& availableRbgs) const;
    static void DeallocateCurrentResourceFromUe(std::shared_ptr<NrMacSchedulerUeInfo> currentUe,
                                                const uint32_t& currentRbg,
                                                const uint32_t beamSym,
                                                FTResources& assignedResources,
                                                std::vector<bool>& availableRbgs);

    double m_v_lyapunov; // V config parameter for Lyaponuv drif-plus-penalty
    double m_weight_q; // Weight given to Q
    double m_weight_g; // Weight given to G
    bool m_enableVirtualQueue; // Enable virtual queue
    double m_reconfig_period; // Reconfiguration period in seconds
    uint16_t m_rest_server_port; // Port number used to connect to the REST server
    // Reference to the singleton Ns3DrlInterface instance
    ns3::Ns3DrlInterface& m_drl_itf;
    NodeContainer m_ueNodes = NodeContainer::GetGlobal(); // NODE cuando drlInterface sea singleton podría pasar el número de nodes por el escenario

    static std::vector<std::pair<UePtr, uint32_t>> m_allUEs; //!< All UEs in the system
};

} // namespace ns3
