#ifndef SIMULATION_DATA_LOGGER_H
#define SIMULATION_DATA_LOGGER_H

#include "ns3/core-module.h"
#include <ns3/log.h>
#include <queue>
#include <map>
#include <utility>
#include <ns3/object.h>
#include <ns3/nstime.h>
#include <vector>
#include <string>
#include <fstream>

namespace ns3 {

/**
 * \ingroup nr
 *
 * \brief Simple class for logging queue data during simulation
 * 
 * This class collects queue data (Q and G) during simulation and
 * writes it to files at the end of the simulation.
 */
class SimulationDataLogger : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  
  /**
   * \brief Constructor
   */
  SimulationDataLogger ();
  
  /**
   * \brief Destructor
   */
  virtual ~SimulationDataLogger ();
  
  /**
   * \brief Get the singleton instance of the logger
   * \return the singleton instance
   */
  static Ptr<SimulationDataLogger> GetInstance ();
  
  /**
   * \brief Log a Q queue entry
   * 
   * \param ueId The ID of the UE
   * \param queueSize The size of the Q queue
   * \param time The time of the log entry
   */
  static void LogQueueQ (uint32_t ueId, uint32_t queueSize, Time time = Simulator::Now ());
  
  /**
   * \brief Log a G queue entry
   * 
   * \param ueId The ID of the UE
   * \param queueSize The size of the G queue
   * \param time The time of the log entry
   */
  static void LogQueueG (uint32_t ueId, uint64_t queueSize, Time time = Simulator::Now ());
  
  // AÑADIR: Función para logging de RBG allocation
  /**
   * \brief Log RBG allocation for any scheduler
   * 
   * \param ueId The ID of the UE (RNTI)
   * \param resources Number of RBGs allocated
   * \param mcs MCS value assigned
   * \param schedulerName Name of the scheduler (RR, DPPA, etc.)
   * \param time The time of the log entry
   */
  static void LogRBGAllocation (uint32_t ueId, uint32_t resources, uint8_t mcs, Time time = Simulator::Now ());

  /**
   * \brief Save queues data to files
   */
  static void SaveToFiles ();

public:
  // Struct to hold a queue entry
  struct QueueEntry
  {
    uint32_t ueId;
    uint64_t queueSize;
    Time time;
  };
  
  // AÑADIR: Struct para RBG allocation entries
  struct RBGAllocationEntry
  {
    uint32_t ueId;
    uint32_t resources;
    uint8_t mcs;
    std::string schedulerName;
    Time time;
  };
  
  // Vectors to store queue entries
  static std::vector<QueueEntry> m_queueQEntries;
  static std::vector<QueueEntry> m_queueGEntries;
  
  // AÑADIR: Vector para RBG allocation entries
  static std::vector<RBGAllocationEntry> m_rbgAllocationEntries;
  
  // The singleton instance
  static Ptr<SimulationDataLogger> m_instance;
  
  // Variable global para testing de MCS (alternativa más simple)
  static double g_mcs_test_value;
  static double g_last_mcs_change_time;
};

} // namespace ns3

#endif /* SIMULATION_DATA_LOGGER_H */