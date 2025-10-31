/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2022 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-mac-scheduler-ue-info-dppa.h"

#include <ns3/log.h>

#include <fstream>

#include <chrono>

#include <ctime>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrMacSchedulerUeInfoDPPA");

double NrMacSchedulerUeInfoDPPA::m_v_lyapunov = 0;
double NrMacSchedulerUeInfoDPPA::m_weight_q = 1;
double NrMacSchedulerUeInfoDPPA::m_weight_g = 1;

void
NrMacSchedulerUeInfoDPPA::UpdateDlTputVirtualQueue(double timeSlot)
{
    NS_LOG_FUNCTION(this);
    m_currTputDl = static_cast<double>(m_dlTbSize)*8/timeSlot;
    if (std::isnan(m_currTputDl))
    {
        m_currTputDl = 0.0;
    }
    uint32_t avg_gfbr = 0.0, gfbr = 0.0;
    int i = 0;
    for (const auto& ueLcg : m_dlLCG){
        std::vector<uint8_t> ueActiveLCs = ueLcg.second->GetActiveLCIds();
        for (const auto lcId : ueActiveLCs){
            std::unique_ptr<NrMacSchedulerLC>& LCPtr = ueLcg.second->GetLC(lcId);
            gfbr += unsigned(LCPtr->m_eRabGuaranteedBitrateDl);
            i++;
            NS_LOG_DEBUG("LC" << LCPtr->m_id << " - GFBR = " << unsigned(LCPtr->m_eRabGuaranteedBitrateDl));
        }
    }
    if (i == 0) {
        NS_LOG_DEBUG("No active LCs found for UE " << m_rnti);
        avg_gfbr = 0;
    } else {
        avg_gfbr = gfbr / i;
    }
    // If avg_gfbr is not zero, save the gfbr
    if(avg_gfbr != 0){
        m_gfbr = avg_gfbr;
    }
    // m_g = std::max(m_g + avg_gfbr - m_currTputDl, 0.0);
    m_g = std::max(m_g + m_gfbr - m_currTputDl, 0.0);
    NS_LOG_INFO("G_" << m_rnti << "(t+1) = max(G(t) + " << m_gfbr << " - " << m_currTputDl << ", 0) = " << m_g);
    // std::cout << Simulator::Now().GetMilliSeconds() << " | G_" << m_rnti << "(t+1) = max(G(t) + " << m_gfbr << " - " << m_currTputDl << ", 0) = " << m_g << std::endl;
    NS_LOG_DEBUG("m_currTputDl = " << m_currTputDl << ", G de rnti " << m_rnti << " actualizada a " << m_g);
}

void
NrMacSchedulerUeInfoDPPA::UpdateDlK(const uint32_t q, const NrMacSchedulerNs3::FTResources& resources, const Ptr<const NrAmc>& amc, const double timeSlot){
    //m_k = -(m_weight_q*q + m_weight_g*m_g) * amc->CalculateTbSize(m_dlMcs.at(0), resources.m_rbg*GetNumRbPerRbg());
    // m_k = -(m_weight_q*q*8 + m_weight_g*m_g/timeSlot)/1e3 * amc->CalculateTbSize(m_dlMcs.at(0), resources.m_rbg*GetNumRbPerRbg())*8/1e3/((resources.m_rbg*GetNumRbPerRbg())/12.0);
    m_k = -((m_weight_q*1e3)*q*8 + m_weight_g*m_g/timeSlot)/1e6 * amc->CalculateTbSize(m_dlMcs, 1, resources.m_rbg*GetNumRbPerRbg())*8/1e6/((resources.m_rbg*GetNumRbPerRbg())/12.0);
    // std::cout << Simulator::Now().GetMilliSeconds() <<  " | UE" << m_rnti << " | m_k = " << m_k << ", 1e3*m_weight_q*q*8/1e6 = " << 1e3*m_weight_q*q*8/1e6 << ", m_weight_g*m_g/timeSlot/1e6 = " << m_weight_g*m_g/timeSlot/1e6 << std::endl;
    // std::cout << Simulator::Now().GetMilliSeconds() << "\t| UE" << m_rnti
    // << ", m_k = " << "-((" << m_weight_q*1e3 << ")*" << q << "*8 + " << m_weight_g << "*" << m_g << "/" << timeSlot << ")/1e6 * "
    // << amc->CalculateTbSize(m_dlMcs, 1, resources.m_rbg*GetNumRbPerRbg())*8/1e6/((resources.m_rbg*GetNumRbPerRbg())/12.0) << " = "
    // << m_k << std::endl;
    // std::cout << "resources.m_rbg = " << resources.m_rbg << ", GetNumRbPerRbg() = " << GetNumRbPerRbg() << ", resources.m_rbg*GetNumRbPerRbg() = " << resources.m_rbg*GetNumRbPerRbg() << ", (resources.m_rbg*GetNumRbPerRbg())/12.0 = " <<  (resources.m_rbg*GetNumRbPerRbg())/12.0 << std::endl;
    // std::cout << Simulator::Now().GetMilliSeconds() << " | UE" << m_rnti
    //           << ", amc->CalculateTbSize(" << (int) m_dlMcs << ", 1, " << resources.m_rbg*GetNumRbPerRbg() << ") = "
    //           << amc->CalculateTbSize(m_dlMcs, 1, resources.m_rbg*GetNumRbPerRbg())
    //           << ", resources.m_rbg*GetNumRbPerRbg())/12 = " << (resources.m_rbg*GetNumRbPerRbg())/12 << std::endl;
}

bool NrMacSchedulerUeInfoDPPA::CompareUeWeightsDl(const NrMacSchedulerNs3::UePtrAndBufferReq& lue, const NrMacSchedulerNs3::UePtrAndBufferReq& rue)
{   
    auto l = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(lue.first);
    auto r = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDPPA>(rue.first);

    if (l->m_k == r->m_k){
        // if (l->m_g == r->m_g){
        //     //return (l->m_dlMcs.at(0) > r->m_dlMcs.at(0)); // Mayor MCS
        //     return NrMacSchedulerUeInfoMR::CompareUeWeightsDl(lue, rue);
        // } else {
        //     return (l->m_g < r->m_g); // Menor g
        // }

        //return NrMacSchedulerUeInfoMR::CompareUeWeightsDl(lue, rue);

        if (l->m_dlMcs == r->m_dlMcs){
            return (l->m_g > r->m_g); // Mayor g
        } else {
            return (l->m_dlMcs > r->m_dlMcs); // Mayor MCS
        }

    } else {
        return (l->m_k < r->m_k); // Menor k
    }
}

} // namespace ns3
