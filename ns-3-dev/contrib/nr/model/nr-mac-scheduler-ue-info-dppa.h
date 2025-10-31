/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "nr-mac-scheduler-ns3.h"

namespace ns3
{

/**
 * \ingroup scheduler
 * \brief UE representation for a round-robin scheduler
 *
 * The UE representation does not store any additional information,
 * but provides a way for a RR scheduler to order the UE based on the assigned
 * RBG.
 *
 * \see CompareUeWeightsDl
 */
class NrMacSchedulerUeInfoDPPA : public NrMacSchedulerUeInfo
{
  public:
    /**
     * \brief NrMacSchedulerUeInfoDPPA constructor
     * \param rnti RNTI of the UE
     * @param beamId BeamId of the UE
     * \param fn A function that tells how many RB per RBG
     */
    NrMacSchedulerUeInfoDPPA(uint16_t rnti,
                            BeamId beamId,
                            const GetRbPerRbgFn& fn)
        : NrMacSchedulerUeInfo(rnti, beamId, fn)
    {
    }

    // static void SetDppWeights (double dppV, double dppWeightQ, double dppWeightG) {
    void SetDppWeights (double dppV, double dppWeightQ, double dppWeightG) {
      // m_v_lyapunov = dppV;
      // // m_weight_q = dppWeightQ;
      // // m_weight_g = dppWeightG;

      // Mapping for the Lyapunov parameter V
      // if (dppV == 9) m_v_lyapunov = 10;
      // else if (dppV == 10) m_v_lyapunov = 20;
      // // if (dppV == 10) m_v_lyapunov = 20;
      // // else if (dppV == 9) m_v_lyapunov = 10;
      // // else m_v_lyapunov = dppV; // Keep the original value if outside the range 9–10
      // std::cout << "m_v_lyapunov = " << m_v_lyapunov << std::endl;
      // m_v_lyapunov = dppV;

      // 1 UE:
      if (dppV == 0) {m_v_lyapunov = 0;}
      else if (dppV == 1) {m_v_lyapunov = 5;}
      else if (dppV == 2) {m_v_lyapunov = 9;}
      else if (dppV == 3) {m_v_lyapunov = 40;}
      else {m_v_lyapunov = dppV;} // Keep the original value if outside the range 0–4

      // 2 UEs:
      // m_v_lyapunov = dppV*2;

      // Mapping for the queue weight Q
      // if (dppWeightQ == 0) m_weight_q = 0;
      // else if (dppWeightQ == 1) m_weight_q = 1;
      // else if (dppWeightQ == 2) m_weight_q = 10;
      // else if (dppWeightQ == 3) m_weight_q = 100;
      // else m_weight_q = dppWeightQ; // Keep the original value if outside the range 0–3
      m_weight_q = 0;
      
      // Mapping for the queue weight G
      if (dppWeightG == 0) m_weight_g = 0.1;
      else if (dppWeightG == 1) m_weight_g = 1;
      else m_weight_g = dppWeightG; // Keep the original value if outside the range 0–3
      // m_weight_g = 1;
    }

    static double GetVLyapunov() {
      return m_v_lyapunov;
    }

    uint32_t GetId() { return m_rnti; }

    /**
     * \brief Reset DL scheduler info
     *
     * Zeroes the average throughput as well as the current throughput.
     *
     * It calls also NrMacSchedulerUeInfoQos::ResetDlSchedInfo.
     */
    void ResetDlSchedInfo() override
    {
        m_currTputDl = 0.0;
        NrMacSchedulerUeInfo::ResetDlSchedInfo();
    }
    
    /**
     * \brief Update metrics for downlink
     * \param totAssigned the resources assigned
     * \param timeSlot the time window
     * \param amc a pointer to the AMC
     *
     * To be defined
     * 
     */
    void UpdateDlTputVirtualQueue(double timeSlot);
    
    /**
     * \brief Update metrics for downlink
     * \param q the RLC queue occupancy
     * \param resources the resources available
     * \param amc a pointer to the AMC
     *
     * To be defined
     * 
     */
    void UpdateDlK(const uint32_t q, const NrMacSchedulerNs3::FTResources& resources, const Ptr<const NrAmc>& amc, const double timeSlot);

    /**
     * \brief comparison function object (i.e. an object that satisfies the
     * requirements of Compare) which returns ​true if the first argument is less
     * than (i.e. is ordered before) the second.
     * \param lue Left UE
     * \param rue Right UE
     * \return true if the assigned RBG of lue is less than the assigned RBG of rue
     *
     * The ordering is made by considering the RBG. An UE with 0 RBG will always
     * be the first (i.e., has an higher priority) in a RR scheduler. The objective
     * is to distribute evenly all the resources, in order to have the same RBG
     * number for all the UEs.
     */
    static bool CompareUeWeightsDl(const NrMacSchedulerNs3::UePtrAndBufferReq& lue,
                                   const NrMacSchedulerNs3::UePtrAndBufferReq& rue);

    /**
     * \brief comparison function object (i.e. an object that satisfies the
     * requirements of Compare) which returns ​true if the first argument is less
     * than (i.e. is ordered before) the second.
     * \param lue Left UE
     * \param rue Right UE
     * \return true if the assigned RBG of lue is less than the assigned RBG of rue
     *
     * The ordering is made by considering the RBG. An UE with 0 RBG will always
     * be the first (i.e., has an higher priority) in a RR scheduler. The objective
     * is to distribute evenly all the resources, in order to have the same RBG
     * number for all the UEs.
     */
    static bool CompareUeWeightsUl(const NrMacSchedulerNs3::UePtrAndBufferReq& lue,
                                   const NrMacSchedulerNs3::UePtrAndBufferReq& rue)
    {
        return (lue.first->m_ulRBG < rue.first->m_ulRBG);
    }

    double m_currTputDl{0.0};      //!< Current slot throughput in downlink
                                   //!< (can be a symbol or a RBG)
    double m_g{0.0}; //!<  Virtual queues
    double m_k{0.0}; //!<  Weight
    static double m_v_lyapunov; //!<  V config parameter for Lyaponuv drif-plus-penalty
    static double m_weight_q; //!<  Weight for queue Q
    static double m_weight_g; //!< Weight for queue G
    double m_gfbr{0.0}; //!<  Guaranteed bit rate for the UE
};
} // namespace ns3
