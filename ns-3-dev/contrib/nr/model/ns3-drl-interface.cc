#include "ns3-drl-interface.h"
#include <algorithm>
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

namespace ns3
{

    Ns3DrlInterface::Ns3DrlInterface() = default; // Constructor vacío

    // Initialize the static instance pointer
    Ns3DrlInterface* Ns3DrlInterface::m_instance = nullptr;

    // Get the singleton instance
    Ns3DrlInterface& Ns3DrlInterface::GetInstance()
    {
        if (m_instance == nullptr)
        {
            m_instance = new Ns3DrlInterface();
        }
        return *m_instance;
    }

    void Ns3DrlInterface::Reset()
    {
        // std::cout << "-------- Resetting Ns3DrlInterface state." << std::endl;
        m_mcs.clear();
        m_q.clear();
        m_count.clear();
        m_prev_position.clear();
        m_velocity.clear();
        m_distance.clear();
        m_closeToObs.clear();
        // m_thput.clear();
        // std::cout << Simulator::Now().GetMilliSeconds() << "\t| m_thput reset with -1 values" << std::endl;
        for (auto& [rnti, thput] : m_thput) {
            thput = -1.0;  // Establecer valor de debug
        }
        m_rbs.clear();
        m_count_reward.clear();
        m_reward = 0;
        m_reward_thput = 0; // only for dataset
    }

    void Ns3DrlInterface::UpdateState(int rnti, double mcs, double q, double g, double gfbr, Vector pos, double timeSlot, double t_last_update)
    {    
        if (m_count.find(rnti) == m_count.end()) {
            m_count[rnti] = 0; // Initialize counter in the first iteration
        }

        m_mcs[rnti] = (m_mcs[rnti] * m_count[rnti] + mcs) / (m_count[rnti] + 1);
        m_mcs[rnti] = std::round(m_mcs[rnti]); // Round to the nearest integer
        // m_mcs[rnti] = mcs;
        // std::cout << "!!!! UE" << rnti << " -> recibo mcs " << mcs << ", actualizo m_mcs a " << m_mcs[rnti] << std::endl;
        m_q[rnti] = (m_q[rnti] * m_count[rnti] + q) / (m_count[rnti] + 1);
        m_g[rnti] = (m_g[rnti] * m_count[rnti] + g) / (m_count[rnti] + 1);
        m_gfbr[rnti] = (m_gfbr[rnti] * m_count[rnti] + gfbr) / (m_count[rnti] + 1);
        Vector currentPos = m_position[rnti];
        // currentPos.x = (currentPos.x * m_count[rnti] + pos.x) / (m_count[rnti] + 1);
        // currentPos.y = (currentPos.y * m_count[rnti] + pos.y) / (m_count[rnti] + 1);
        // currentPos.z = (currentPos.z * m_count[rnti] + pos.z) / (m_count[rnti] + 1);
        currentPos.x = pos.x; // Update position directly
        currentPos.y = pos.y; // Update position directly
        currentPos.z = pos.z; // Update position directly
        m_position[rnti] = currentPos;

        // Calculate velocity
        if (m_prev_position.find(rnti) == m_prev_position.end()) { // Check if its the first iteration
            m_prev_position[rnti] = currentPos;
        }
        // FIXME
        Vector velocity(0,0,0);
        if (t_last_update == m_t_last_update[rnti]) {
            std::cout << "!!!!!!!!!!!!!! t_last_update == m_t_last_update Using previous velocity for UE" << rnti << std::endl;
            // If the last update time is the same, we can use the previous velocity
            velocity = m_velocity[rnti];
        } else {
            velocity.x = (std::round((pos.x - m_prev_position[rnti].x)/timeSlot) == -0) ? 0 : std::round((pos.x - m_prev_position[rnti].x)/(t_last_update-m_t_last_update[rnti]));
            velocity.y = (std::round((pos.y - m_prev_position[rnti].y)/timeSlot) == -0) ? 0 : std::round((pos.y - m_prev_position[rnti].y)/(t_last_update-m_t_last_update[rnti]));
            velocity.z = (std::round((pos.z - m_prev_position[rnti].z)/timeSlot) == -0) ? 0 : std::round((pos.z - m_prev_position[rnti].z)/(t_last_update-m_t_last_update[rnti]));
        }
        // Calculate average velocity
        m_velocity[rnti].x = (m_velocity[rnti].x * m_count[rnti] + velocity.x) / (m_count[rnti] + 1);
        m_velocity[rnti].y = (m_velocity[rnti].y * m_count[rnti] + velocity.y) / (m_count[rnti] + 1);
        m_velocity[rnti].z = (m_velocity[rnti].z * m_count[rnti] + velocity.z) / (m_count[rnti] + 1);
        // std::cout << Simulator::Now().GetSeconds() << " ---- UE" << rnti << " -> pos = " << pos << ", m_prev_position = " << m_prev_position[rnti] << ", velocity = " << velocity << std::endl;
        // TODO cambiar esto por el cálculo de la velocidad real
        // m_velocity[0] = {-1.0, 0.0, 0.0};
        // m_velocity[1] = {-1.0, 0.0, 0.0};

        // Calculate distance
        // m_distance[rnti] += std::round(std::sqrt(std::pow(pos.x - m_prev_position[rnti].x, 2) + std::pow(pos.y - m_prev_position[rnti].y, 2) + std::pow(pos.z - m_prev_position[rnti].z, 2)));
        m_distance[rnti] += std::sqrt(m_prev_position[rnti].x * m_prev_position[rnti].x + m_prev_position[rnti].y * m_prev_position[rnti].y + m_prev_position[rnti].z * m_prev_position[rnti].z) -
                           std::sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);

        m_prev_position[rnti] = currentPos; // Actualizar la posición anterior

        // m_closeToObs[rnti] = isCloseToObs(rnti);
        // std::cout << "UE" << rnti << " -> closeToObs = " << m_closeToObs[rnti] << std::endl;

        m_count[rnti]++;

        m_t_last_update[rnti] = t_last_update;
    }

    // bool Ns3DrlInterface::isCloseToObs(int rnti) const
    // {
    //     // Obstáculo: círculo con centro en (180, 0) y radio 50m
    //     const Vector obstacleCenter(200.0, 0.0, 0.0);
    //     // TODO nuevos obstáculos 
    //     const double obstacleRadius = 50.0;
    //     const double predictionTime = 2.0; // Predecir 2 segundos hacia adelante

    //     // Obtener posición y velocidad actual del UE
    //     Vector currentPos = this->GetPos(rnti);
    //     Vector velocity = this->GetVelocity(rnti); // FIXME
        
    //     // Si no hay velocidad, solo verificar distancia actual
    //     double velocityMagnitude = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    //     if (velocityMagnitude < 0.1) { // UE prácticamente estático
    //         double currentDistance = std::sqrt(
    //             std::pow(currentPos.x - obstacleCenter.x, 2) + 
    //             std::pow(currentPos.y - obstacleCenter.y, 2)
    //         );
    //         return currentDistance <= obstacleRadius;
    //     }
        
    //     // Predecir trayectoria futura
    //     Vector futurePos;
    //     futurePos.x = currentPos.x + velocity.x * (predictionTime + (Simulator::Now().GetSeconds() - this->GetLastUpdateTime(rnti)));
    //     futurePos.y = currentPos.y + velocity.y * (predictionTime + (Simulator::Now().GetSeconds() - this->GetLastUpdateTime(rnti)));
    //     futurePos.z = currentPos.z; // Ignorar componente Z

    //     // std::cout << "UE" << rnti << " last time = " << this->GetLastUpdateTime(rnti) << std::endl;
    //     // std::cout << "UE" << rnti << " -> futurePos.y = " << currentPos.y << " + " << velocity.y << " * " << (predictionTime + (Simulator::Now().GetSeconds() - this->GetLastUpdateTime(rnti))) << " = " << futurePos.y << std::endl;

    //     // Verificar si la trayectoria intersecta con el círculo del obstáculo
    //     // Usar algoritmo de distancia punto-línea
        
    //     // Vector de la línea de movimiento
    //     Vector movement(futurePos.x - currentPos.x, futurePos.y - currentPos.y, 0.0);
        
    //     // Vector desde posición actual al centro del obstáculo
    //     Vector toObstacle(obstacleCenter.x - currentPos.x, obstacleCenter.y - currentPos.y, 0.0);
        
    //     // Calcular el punto más cercano en la línea de trayectoria al obstáculo
    //     double movementLengthSquared = movement.x * movement.x + movement.y * movement.y;
        
    //     if (movementLengthSquared < 1e-6) {
    //         // Movimiento insignificante, usar distancia actual
    //         double distance = std::sqrt(toObstacle.x * toObstacle.x + toObstacle.y * toObstacle.y);
    //         return distance <= obstacleRadius;
    //     }
        
    //     // Proyección del vector al obstáculo sobre el vector de movimiento
    //     double t = (toObstacle.x * movement.x + toObstacle.y * movement.y) / movementLengthSquared;
        
    //     // Limitar t al segmento [0,1] (solo la trayectoria hacia adelante)
    //     t = std::max(0.0, std::min(1.0, t));
        
    //     // Punto más cercano en la trayectoria
    //     Vector closestPoint;
    //     closestPoint.x = currentPos.x + t * movement.x;
    //     closestPoint.y = currentPos.y + t * movement.y;
        
    //     // Distancia del punto más cercano al centro del obstáculo
    //     double minDistance = std::sqrt(
    //         std::pow(closestPoint.x - obstacleCenter.x, 2) + 
    //         std::pow(closestPoint.y - obstacleCenter.y, 2)
    //     );
        
    //     // También verificar distancia actual y futura
    //     double currentDistance = std::sqrt(toObstacle.x * toObstacle.x + toObstacle.y * toObstacle.y);
    //     double futureDistance = std::sqrt(
    //         std::pow(futurePos.x - obstacleCenter.x, 2) + 
    //         std::pow(futurePos.y - obstacleCenter.y, 2)
    //     );
        
    //     // Margen de seguridad adicional para predicción
    //     const double safetyMargin = 10.0; // 10m de margen

    //     // std::cout << "UE" << rnti << " -> currentPos = " << currentPos << ", velocity = " << velocity << ", futurePos = " << futurePos << std::endl;
        
    //     return (minDistance <= (obstacleRadius + safetyMargin)) || 
    //            (currentDistance <= (obstacleRadius + safetyMargin)) || 
    //            (futureDistance <= (obstacleRadius + safetyMargin));
    // }

    bool Ns3DrlInterface::isCloseToObs(int rnti) const
{
    auto pos_it = m_position.find(rnti);
    auto vel_it = m_velocity.find(rnti);
    auto time_it = m_t_last_update.find(rnti);
    // auto pos_it = this->GetPos(rnti);
    // auto vel_it = this->GetVelocity(rnti);
    // auto time_it = this->GetLastUpdateTime(rnti);
    
    if (pos_it == m_position.end() || vel_it == m_velocity.end() || time_it == m_t_last_update.end())
    {
        // UE no inicializado, asumir que no está cerca de obstáculos
        return false;
    }

    // Lista de obstáculos circulares
    std::vector<CircularObstacle> circularObstacles = {
        // CircularObstacle(-1.0, 20.0, 0.0, 1.0),   // Obstáculo 1 antes
        // CircularObstacle(14.0, 20.0, 0.0, 1.0),   // Obstáculo 2 despues
        // CircularObstacle(-17, 20.0, 0.0, 1.0)   // Obstáculo 3 bien creo
    };
    
    // Lista de obstáculos rectangulares
    std::vector<RectangularObstacle> rectangularObstacles = {        
        RectangularObstacle(0.0, 21.0, 12.0, 2.0, 90*M_PI/180),       
        RectangularObstacle(11.5, 21.0, 14.0, 2.0, 60*M_PI/180),
        RectangularObstacle(-12.0, 21.0, 14.0, 2.0, -60*M_PI/180),
    };

    const double predictionTime = 1.0; // Predecir 2 segundos hacia adelante
    const double safetyMargin = 0.0;   // Margen de seguridad

    // Vector currentPos = this->GetPos(rnti);
    // Vector velocity = this->GetVelocity(rnti);
    Vector currentPos = pos_it->second;
    Vector velocity = vel_it->second;
    double lastUpdateTime = time_it->second;
    // std::cout << "currentPos = " << currentPos << ", velocity = " << velocity << ", lastUpdateTime = " << lastUpdateTime << std::endl;

    double velocityMagnitude = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

    Vector futurePos(0,0,0);
    // double deltaTime = predictionTime + (Simulator::Now().GetSeconds() - this->GetLastUpdateTime(rnti));
    double deltaTime = predictionTime + (Simulator::Now().GetSeconds() - lastUpdateTime);
    futurePos.x = currentPos.x + velocity.x * deltaTime;
    futurePos.y = currentPos.y + velocity.y * deltaTime;
    futurePos.z = currentPos.z;
    // std::cout << "UE" << rnti << " -> futurePos.x = " << futurePos.x << ", futurePos.y = " << futurePos.y << ", futurePos.z = " << futurePos.z << std::endl;

    // 1. Verificar obstáculos circulares
    for (const auto& obs : circularObstacles)
    {
        const Vector& obstacleCenter = obs.center;
        const double obstacleRadius = obs.radius;

        Vector toObstacle(obstacleCenter.x - currentPos.x, obstacleCenter.y - currentPos.y, 0.0);

        if (velocityMagnitude < 0.1)
        {
            double currentDistance = std::sqrt(toObstacle.x * toObstacle.x + toObstacle.y * toObstacle.y);
            if (currentDistance <= obstacleRadius + safetyMargin)
                return true;
            continue;
        }

        Vector movement(futurePos.x - currentPos.x, futurePos.y - currentPos.y, 0.0);
        double movementLengthSquared = movement.x * movement.x + movement.y * movement.y;

        double t = 0.0;
        if (movementLengthSquared >= 1e-6)
        {
            t = (toObstacle.x * movement.x + toObstacle.y * movement.y) / movementLengthSquared;
            t = std::max(0.0, std::min(1.0, t));
        }

        Vector closestPoint;
        closestPoint.x = currentPos.x + t * movement.x;
        closestPoint.y = currentPos.y + t * movement.y;

        double minDistance = std::sqrt(
            std::pow(closestPoint.x - obstacleCenter.x, 2) +
            std::pow(closestPoint.y - obstacleCenter.y, 2)
        );

        double currentDistance = std::sqrt(toObstacle.x * toObstacle.x + toObstacle.y * toObstacle.y);
        double futureDistance = std::sqrt(
            std::pow(futurePos.x - obstacleCenter.x, 2) +
            std::pow(futurePos.y - obstacleCenter.y, 2)
        );

        if (minDistance <= (obstacleRadius + safetyMargin) ||
            currentDistance <= (obstacleRadius + safetyMargin) ||
            futureDistance <= (obstacleRadius + safetyMargin))
        {
            return true;
        }
    }
    
    // 2. Verificar obstáculos rectangulares
    for (const auto& obstacle : rectangularObstacles)
    {
        try {
            // Crear una versión expandida del obstáculo con margen de seguridad
            RectangularObstacle expandedObstacle = obstacle;
            expandedObstacle.width += 2 * safetyMargin;
            expandedObstacle.height += 2 * safetyMargin;

            // 2.1. Verificar si la posición actual está dentro del obstáculo expandido
            if (IsPointInRotatedRectangle(currentPos, expandedObstacle))
            {
                return true;
            }

            // 2.2. Verificar si la posición futura está dentro del obstáculo expandido
            if (IsPointInRotatedRectangle(futurePos, expandedObstacle))
            {
                return true;
            }

            // 2.3. Si el UE está en movimiento, verificar si la trayectoria intersecta el rectángulo
            if (velocityMagnitude >= 0.1)
            {
                if (DoesLineIntersectRotatedRectangle(currentPos, futurePos, expandedObstacle))
                {
                    return true;
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "Caught an exception while checking for obstacles." << std::endl;
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    return false; // Ningún obstáculo cercano
}

    // uint32_t Ns3DrlInterface::RntiToUserId(uint32_t rnti) {
    //     return m_ueNum - rnti + 1; // Convert RNTI to user ID
    //     // return rnti;
    // }


    void Ns3DrlInterface::UpdateMetrics(int rnti, double thput, double rbs, double total_rbs)
    {
        // m_thput[rnti] = (m_thput[rnti] * m_count_reward[rnti] + thput) / (m_count_reward[rnti] + 1);
        // m_rbs[rnti] = (m_rbs[rnti] * m_count_reward[rnti] + rbs) / (m_count_reward[rnti] + 1);

        // int userId = GetUEsNumber() - rnti; // 2 - 0 = 2, 2 - 1 = 1
        // m_rbs[userId] = thput;
        m_rbs[rnti] += rbs;

        m_total_rbs = total_rbs;
        // m_count_reward[rnti]++;
    }

    void Ns3DrlInterface::UpdateMetricsThput(int rnti, double thput)
    {
        // m_thput[rnti] = (m_thput[rnti] * m_count_reward[rnti] + thput) / (m_count_reward[rnti] + 1);
        // m_count_reward[rnti]++;

        // TODO cambiar rnti por el índice correspondiente
        // int userId = GetUEsNumber() - rnti;
        // m_thput[userId] = thput;
        // std::cout << "++ UE" << userId << " updates thput = " << thput << std::endl;

        m_thput[rnti] = thput; // TODO
        // std::cout << Simulator::Now().GetMilliSeconds() << "\t| (En UpdateMetricsThput) UE" << rnti << " -> thput = " << thput/1e6 << " Mbps" << std::endl;
    }

    void Ns3DrlInterface::UpdateMetricsRbs(int rnti, double rbs, double total_rbs)
    {
        // m_rbs[rnti] = (m_rbs[rnti] * m_count_reward[rnti] + rbs) / (m_count_reward[rnti] + 1);
        m_rbs[rnti] += rbs;
        m_total_rbs = total_rbs;
        m_count_reward[rnti]++;
    }

    void Ns3DrlInterface::CalculateReward()
    {
        // std::cout << "m_total_rbs: " << m_total_rbs << std::endl;
        double reward_thput = 0;
        double reward_rbs = 0;
        double rbs_used = 0;
        double gfbr_aux = 0; // GFBR with safety margin
        double bonus = 0.3; // bonus for throughput reward
        for (int i = 0; i < GetUEsNumber(); i++)
        {
            if (m_gfbr[i] == 0){
                reward_thput += 1.0;
            } else{
                // reward_thput += std::min(1.0, 1.0 - (m_gfbr[i] - m_thput[i]) / m_gfbr[i]);
                gfbr_aux = m_gfbr[i]*1; // safety margin if necessary
                reward_thput += std::min(1.0, 1.0 - (gfbr_aux - m_thput[i]) / gfbr_aux);

            }
            // std::cout << Simulator::Now().GetMilliSeconds() << "\t| UE" << i << " -> thput = " << m_thput[i]/1e6 << " Mbps, gfbr = " << m_gfbr[i]/1e6 << " Mbps, reward_thput = " << reward_thput << std::endl;
        }
        reward_thput /= GetUEsNumber();

        if (reward_thput == 1){
            for (int i = 0; i < GetUEsNumber(); i++)
            {
                rbs_used += m_rbs[i];
            }
            // reward_rbs /= GetUEsNumber();
            // reward_rbs = (1 - rbs_used/m_total_rbs);
            reward_rbs = (1 - rbs_used/(m_total_rbs*1000)); // TODO Generalize for all numerologies
            // std::cout << "reward_rbs = " << reward_rbs << ", rbs_used = " << rbs_used << ", m_total_rbs = " << m_total_rbs*2000 << std::endl;
        }
        else
        {
            reward_rbs = 0;
            bonus = 0;
        }
        m_reward = reward_thput*0.2 + bonus + reward_rbs*0.5;
        m_reward_thput = reward_thput; // only for dataset
        // std::cout << "reward = 0.2*reward_thput + 0.8*reward_rbs = " << 0.2*reward_thput << " + " << 0.8*reward_rbs << " = " << m_reward << std::endl;
        
        // for (int i = 0; i < GetUEsNumber(); i++)
        // {
        //     std::cout << Simulator::Now().GetMilliSeconds() << "\t| UE" << i << " -> thput = " << m_thput[i]/1e6 << " Mbps, gfbr = " << m_gfbr[i]/1e6 << " Mbps, rbs = " << m_rbs[i]
        //     << ", mcs = " << GetMcs(i) << std::endl;
        // }
    }

    int Ns3DrlInterface::GetUEsNumber() const
    {
        return m_mcs.size();
    }

    int Ns3DrlInterface::GetStateSize() const
    {
        // 5 métricas para cada UE
        return 5*GetUEsNumber();
    }

    double Ns3DrlInterface::GetMcs(int rnti) const
    {
        auto it = m_mcs.find(rnti);
        return (it != m_mcs.end()) ? it->second : 0.0;
    }

    double Ns3DrlInterface::GetQ(int rnti) const
    {
        auto it = m_q.find(rnti);
        return (it != m_q.end()) ? it->second : 0.0;
    }

    double Ns3DrlInterface::GetG(int rnti) const
    {
        auto it = m_g.find(rnti);
        return (it != m_g.end()) ? it->second : 0.0;
    }

    double Ns3DrlInterface::GetGfbr(int rnti) const
    {
        auto it = m_gfbr.find(rnti);
        return (it != m_gfbr.end()) ? it->second : 0.0;
    }

    Vector Ns3DrlInterface::GetPos(int rnti) const
    {
        auto it = m_position.find(rnti);
        return (it != m_position.end()) ? it->second : Vector();
    }

    Vector Ns3DrlInterface::GetVelocity(int rnti) const
    {
        auto it = m_velocity.find(rnti);
        return (it != m_velocity.end()) ? it->second : Vector();
    }

    double Ns3DrlInterface::GetDistance(int rnti) const
    {
        auto it = m_distance.find(rnti);
        return (it != m_distance.end()) ? it->second : 0;
    }

    bool Ns3DrlInterface::GetCloseToObs(int rnti) const
    {
        // auto it = m_closeToObs.find(rnti);
        // return (it != m_closeToObs.end()) ? it->second : false;
        return isCloseToObs(rnti);
    }

    double Ns3DrlInterface::GetLastUpdateTime(int rnti) const
    {
        auto it = m_t_last_update.find(rnti);
        return (it != m_t_last_update.end()) ? it->second : 0.0;
    }

    float Ns3DrlInterface::GetReward() const
    {
        return m_reward;
    }

    float Ns3DrlInterface::GetRewardThput() const
    {
        return m_reward_thput;
    }

    std::ostream& operator<<(std::ostream& os, const Ns3DrlInterface& itf)
    {
        os << "MCS: ";
        for (const auto& [rnti, mcs] : itf.m_mcs)
        {
            os << "(" << rnti << ": " << mcs << ") ";
        }
        os << "\nQ: ";
        for (const auto& [rnti, q] : itf.m_q)
        {
            os << "(" << rnti << ": " << q << ") ";
        }
        os << "\nG: ";
        for (const auto& [rnti, g] : itf.m_g)
        {
            os << "(" << rnti << ": " << g << ") ";
        }
        os << "\nGFBR: ";
        for (const auto& [rnti, gfbr] : itf.m_gfbr)
        {
            os << "(" << rnti << ": " << gfbr << ") ";
        }
        os << "\nPosition: ";
        for (const auto& [rnti, pos] : itf.m_position)
        {
            os << "(" << rnti << ": " << pos << ") ";
        }
        os << "\nVelocity: ";
        for (const auto& [rnti, vel] : itf.m_velocity)
        {
            os << "(" << rnti << ": " << vel << ") ";
        }
        os << "\nDistance: ";
        for (const auto& [rnti, dist] : itf.m_distance)
        {
            os << "(" << rnti << ": " << dist << ") ";
        }

        os << "\nThput: ";
        for (const auto& [rnti, thput] : itf.m_thput)
        {
            os << "(" << rnti << ": " << thput << ") ";
        }
        os << "\nRBs: ";
        for (const auto& [rnti, rbs] : itf.m_rbs)
        {
            os << "(" << rnti << ": " << rbs << ") ";
        }

        os << "\nReward: " << itf.m_reward;
        return os;
    }

    // Funciones auxiliares para detección de obstáculos rectangulares
    bool Ns3DrlInterface::IsPointInRectangle(const Vector& point, 
                                            double minX, double maxX, 
                                            double minY, double maxY) const
    {
        return (point.x >= minX && point.x <= maxX && 
                point.y >= minY && point.y <= maxY);
    }

    bool Ns3DrlInterface::DoesLineIntersectRectangle(const Vector& start, const Vector& end,
                                                    double minX, double maxX,
                                                    double minY, double maxY) const
    {
        // Algoritmo de Liang-Barsky para intersección línea-rectángulo
        double dx = end.x - start.x;
        double dy = end.y - start.y;
        
        // Si la línea es un punto
        if (std::abs(dx) < 1e-6 && std::abs(dy) < 1e-6)
        {
            return IsPointInRectangle(start, minX, maxX, minY, maxY);
        }
        
        double tMin = 0.0;
        double tMax = 1.0;
        
        // Verificar intersección con cada lado del rectángulo
        if (!ClipLine(dx, minX - start.x, tMin, tMax)) return false; // Lado izquierdo
        if (!ClipLine(-dx, start.x - maxX, tMin, tMax)) return false; // Lado derecho
        if (!ClipLine(dy, minY - start.y, tMin, tMax)) return false; // Lado inferior
        if (!ClipLine(-dy, start.y - maxY, tMin, tMax)) return false; // Lado superior
        
        return tMin <= tMax;
    }

    bool Ns3DrlInterface::ClipLine(double denom, double num, double& tMin, double& tMax) const
    {
        if (std::abs(denom) < 1e-6)
        {
            // Línea paralela al lado del rectángulo
            return num <= 0;
        }
        
        double t = num / denom;
        
        if (denom > 0)
        {
            // Línea entra por este lado
            if (t > tMax) return false;
            if (t > tMin) tMin = t;
        }
        else
        {
            // Línea sale por este lado
            if (t < tMin) return false;
            if (t < tMax) tMax = t;
        }
        
        return true;
    }

    bool Ns3DrlInterface::DoLinesIntersect(const Vector& p1, const Vector& q1,
                                          const Vector& p2, const Vector& q2) const
    {
        auto orientation = [](const Vector& p, const Vector& q, const Vector& r) -> int {
            double val = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);
            if (std::abs(val) < 1e-6) return 0;  // Colineales
            return (val > 0) ? 1 : 2; // Sentido horario o antihorario
        };
        
        auto onSegment = [](const Vector& p, const Vector& q, const Vector& r) -> bool {
            return q.x <= std::max(p.x, r.x) && q.x >= std::min(p.x, r.x) &&
                   q.y <= std::max(p.y, r.y) && q.y >= std::min(p.y, r.y);
        };
        
        int o1 = orientation(p1, q1, p2);
        int o2 = orientation(p1, q1, q2);
        int o3 = orientation(p2, q2, p1);
        int o4 = orientation(p2, q2, q1);
        
        // Caso general
        if (o1 != o2 && o3 != o4) return true;
        
        // Casos especiales (puntos colineales)
        if (o1 == 0 && onSegment(p1, p2, q1)) return true;
        if (o2 == 0 && onSegment(p1, q2, q1)) return true;
        if (o3 == 0 && onSegment(p2, p1, q2)) return true;
        if (o4 == 0 && onSegment(p2, q1, q2)) return true;
        
        return false;
    }

    // Funciones auxiliares para detección de obstáculos circulares (opcionales, para consistencia)
    bool Ns3DrlInterface::IsPointInCircle(const Vector& point, const Vector& center, double radius) const
    {
        double distance = std::sqrt(std::pow(point.x - center.x, 2) + std::pow(point.y - center.y, 2));
        return distance <= radius;
    }

    bool Ns3DrlInterface::DoesLineIntersectCircle(const Vector& start, const Vector& end,
                                                 const Vector& center, double radius) const
    {
        // Vector del punto start al centro del círculo
        Vector toCenter(center.x - start.x, center.y - start.y, 0.0);
        
        // Vector del movimiento
        Vector movement(end.x - start.x, end.y - start.y, 0.0);
        double movementLengthSquared = movement.x * movement.x + movement.y * movement.y;

        double t = 0.0;
        if (movementLengthSquared >= 1e-6)
        {
            t = (toCenter.x * movement.x + toCenter.y * movement.y) / movementLengthSquared;
            t = std::max(0.0, std::min(1.0, t));
        }

        Vector closestPoint;
        closestPoint.x = start.x + t * movement.x;
        closestPoint.y = start.y + t * movement.y;

        double minDistance = std::sqrt(
            std::pow(closestPoint.x - center.x, 2) +
            std::pow(closestPoint.y - center.y, 2)
        );

        return minDistance <= radius;
    }

    // Funciones auxiliares para rectángulos con rotación
    
    Vector Ns3DrlInterface::RotatePoint(const Vector& point, double centerX, double centerY, double angle) const
    {
        // Trasladar al origen
        double x = point.x - centerX;
        double y = point.y - centerY;
        
        // Aplicar rotación
        double cosA = std::cos(angle);
        double sinA = std::sin(angle);
        
        Vector rotated;
        rotated.x = x * cosA - y * sinA + centerX;
        rotated.y = x * sinA + y * cosA + centerY;
        rotated.z = point.z;
        
        return rotated;
    }
    
    Vector Ns3DrlInterface::InverseRotatePoint(const Vector& point, double centerX, double centerY, double angle) const
    {
        // Rotación inversa es rotación con ángulo negativo
        return RotatePoint(point, centerX, centerY, -angle);
    }
    
    bool Ns3DrlInterface::IsPointInRotatedRectangle(const Vector& point, const RectangularObstacle& obstacle) const
    {
        // Si no hay rotación, usar método optimizado
        if (std::abs(obstacle.rotationAngle) < 1e-6)
        {
            double halfWidth = obstacle.width / 2.0;
            double halfHeight = obstacle.height / 2.0;
            return IsPointInRectangle(point, 
                                    obstacle.centerX - halfWidth, obstacle.centerX + halfWidth,
                                    obstacle.centerY - halfHeight, obstacle.centerY + halfHeight);
        }
        
        // Rotar el punto al sistema de coordenadas del rectángulo (rotación inversa)
        Vector localPoint = InverseRotatePoint(point, obstacle.centerX, obstacle.centerY, obstacle.rotationAngle);
        
        // Verificar si está dentro del rectángulo en su sistema local
        double halfWidth = obstacle.width / 2.0;
        double halfHeight = obstacle.height / 2.0;
        
        return (localPoint.x >= obstacle.centerX - halfWidth && 
                localPoint.x <= obstacle.centerX + halfWidth &&
                localPoint.y >= obstacle.centerY - halfHeight && 
                localPoint.y <= obstacle.centerY + halfHeight);
    }
    
    bool Ns3DrlInterface::DoesLineIntersectRotatedRectangle(const Vector& start, const Vector& end,
                                                           const RectangularObstacle& obstacle) const
    {
        // Si no hay rotación, usar método optimizado
        if (std::abs(obstacle.rotationAngle) < 1e-6)
        {
            double halfWidth = obstacle.width / 2.0;
            double halfHeight = obstacle.height / 2.0;
            return DoesLineIntersectRectangle(start, end,
                                            obstacle.centerX - halfWidth, obstacle.centerX + halfWidth,
                                            obstacle.centerY - halfHeight, obstacle.centerY + halfHeight);
        }
        
        // Rotar la línea al sistema de coordenadas del rectángulo
        Vector localStart = InverseRotatePoint(start, obstacle.centerX, obstacle.centerY, obstacle.rotationAngle);
        Vector localEnd = InverseRotatePoint(end, obstacle.centerX, obstacle.centerY, obstacle.rotationAngle);
        
        // Verificar intersección en el sistema local
        double halfWidth = obstacle.width / 2.0;
        double halfHeight = obstacle.height / 2.0;
        
        return DoesLineIntersectRectangle(localStart, localEnd,
                                        obstacle.centerX - halfWidth, obstacle.centerX + halfWidth,
                                        obstacle.centerY - halfHeight, obstacle.centerY + halfHeight);
    }

} // namespace ns3