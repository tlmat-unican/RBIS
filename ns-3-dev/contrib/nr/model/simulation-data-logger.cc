#include "simulation-data-logger.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SimulationDataLogger");
NS_OBJECT_ENSURE_REGISTERED (SimulationDataLogger);

// Initialize the singleton instance to nullptr
Ptr<SimulationDataLogger> SimulationDataLogger::m_instance = nullptr;
std::vector<SimulationDataLogger::QueueEntry> SimulationDataLogger::m_queueQEntries;
std::vector<SimulationDataLogger::QueueEntry> SimulationDataLogger::m_queueGEntries;
std::vector<SimulationDataLogger::RBGAllocationEntry> SimulationDataLogger::m_rbgAllocationEntries;

// Variables globales para testing de MCS
double SimulationDataLogger::g_mcs_test_value = 20.0;  // Inicializar con valor válido
double SimulationDataLogger::g_last_mcs_change_time = 0.0;

TypeId
SimulationDataLogger::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SimulationDataLogger")
    .SetParent<Object> ()
    .SetGroupName ("Nr")
    .AddConstructor<SimulationDataLogger> ()
  ;
  return tid;
}

SimulationDataLogger::SimulationDataLogger ()
{
  NS_LOG_FUNCTION (this);
  
  // Schedule the SaveToFiles method to be called at the end of the simulation
  // Simulator::ScheduleDestroy (&SimulationDataLogger::SaveToFiles, this);
}

SimulationDataLogger::~SimulationDataLogger ()
{
  NS_LOG_FUNCTION (this);
}

Ptr<SimulationDataLogger>
SimulationDataLogger::GetInstance ()
{
  NS_LOG_FUNCTION_NOARGS ();
  
  // Create the instance if it doesn't exist
  if (m_instance == nullptr)
    {
      m_instance = CreateObject<SimulationDataLogger> ();
    }
  
  return m_instance;
}

void
SimulationDataLogger::LogQueueQ (uint32_t ueId, uint32_t queueSize, Time time)
{ 
  // Create and add the entry
  QueueEntry entry;
  entry.ueId = ueId;
  entry.queueSize = queueSize;
  entry.time = time;
  
  m_queueQEntries.push_back (entry);

  if (ueId == 1 && m_queueQEntries.size() > 1000) {
    // If the queue size exceeds 1000, save to file and clear the vector
    SaveToFiles();
    m_queueQEntries.clear();
    m_queueGEntries.clear();
  }
}

void
SimulationDataLogger::LogQueueG (uint32_t ueId, uint64_t queueSize, Time time)
{
  // Create and add the entry
  QueueEntry entry;
  entry.ueId = ueId;
  entry.queueSize = queueSize;
  entry.time = time;
  
  m_queueGEntries.push_back (entry);

  // if (ueId == 1 && m_queueGEntries.size() > 1000) {
  //   // If the queue size exceeds 1000, save to file and clear the vector
  //   SaveToFiles();
  //   m_queueGEntries.clear();
  // }
}

void 
SimulationDataLogger::LogRBGAllocation (uint32_t ueId, uint32_t resources, uint8_t mcs, Time time)
{
  RBGAllocationEntry entry;
  entry.ueId = ueId;
  entry.resources = resources;
  entry.mcs = mcs;
  entry.time = time;
  
  m_rbgAllocationEntries.push_back(entry);
}

void
SimulationDataLogger::SaveToFiles ()
{
  std::string qFilePath = "queue_q.txt"; 
  std::string gFilePath = "queue_g.txt";

  // Save Q queue data
  std::ofstream qFile(qFilePath, std::ios::app);
  std::ifstream checkQfile(qFilePath, std::ios::ate | std::ios::binary);
  bool writeHeader = !checkQfile.good() || checkQfile.tellg() == 0;
  checkQfile.close();

  if (qFile.is_open ())
  {
    if (writeHeader)
    {
      qFile << "time\tue\tq" << std::endl;
    }
    
    for (const auto &entry : m_queueQEntries)
      {
          qFile << entry.time.GetSeconds () << "\t" << entry.ueId <<  "\t" << entry.queueSize << std::endl;
      }
    
    qFile.close ();
    NS_LOG_INFO ("Saved Q queue data to " << qFilePath);
  }
  else
  {
    NS_LOG_WARN ("Failed to open file for writing: " << qFilePath);
  }

  // Save G queue data
  std::ofstream gFile(gFilePath, std::ios::app);
  std::ifstream checkGfile(gFilePath, std::ios::ate | std::ios::binary);
  writeHeader = !checkGfile.good() || checkGfile.tellg() == 0;
  checkGfile.close();
  
  if (gFile.is_open ())
  {
    if (writeHeader)
    {
      gFile << "time\tue\tg" << std::endl;
    }
    
    for (const auto &entry : m_queueGEntries)
      {
          gFile << entry.time.GetSeconds () << "\t" << entry.ueId <<  "\t" << entry.queueSize << std::endl;
      }
    
    gFile.close ();
    NS_LOG_INFO ("Saved G queue data to " << gFilePath);
  }
  else
  {
    NS_LOG_WARN ("Failed to open file for writing: " << gFilePath);
  }

  // CAMBIAR: Usar un solo archivo alpha.txt sin distinguir scheduler
  std::string alphaFilePath = "alpha.txt";
  std::ofstream alphaFile(alphaFilePath, std::ios::app);
  
  // Comprobar si necesita header
  std::ifstream checkAlphaFile(alphaFilePath, std::ios::ate | std::ios::binary);
  writeHeader = !checkAlphaFile.good() || checkAlphaFile.tellg() == 0;
  checkAlphaFile.close();
  
  if (alphaFile.is_open())
  {
    if (writeHeader)
    {
      alphaFile << "time\tue\tresources\tmcs" << std::endl;
    }
    
    // Escribir todas las entradas RBG en un solo archivo
    for (const auto& entry : m_rbgAllocationEntries)
    {
      alphaFile << entry.time.ToDouble(Time::MS) << "\t" 
                << entry.ueId << "\t" 
                << entry.resources << "\t" 
                << (int)entry.mcs << std::endl;
    }
    
    alphaFile.close();
    NS_LOG_INFO ("RBG allocation data saved to " << alphaFilePath);
  }
  else
  {
    NS_LOG_WARN ("Failed to open file for writing: " << alphaFilePath);
  }
}

} // namespace ns3


