/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "dsr-header.h"
#include "dsr-virtual-queue-disc.h"

#define FAST_LANE 0
#define SLOW_LANE 1
#define NORMAL_LANE 2

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DsrVirtualQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (DsrVirtualQueueDisc);

TypeId DsrVirtualQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DsrVirtualQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("DsrRouting")
    .AddConstructor<DsrVirtualQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc.",
                   QueueSizeValue (QueueSize ("1000p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
  ;
  return tid;
}

DsrVirtualQueueDisc::DsrVirtualQueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES, QueueSizeUnit::PACKETS)
{
  NS_LOG_FUNCTION (this);
}

DsrVirtualQueueDisc::~DsrVirtualQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}


bool
DsrVirtualQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
  uint32_t lane = EnqueueClassify (item);
  if (GetInternalQueue(lane)->GetCurrentSize ().GetValue() >= LinesSize[lane])
  {
    DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
  }
  bool retval = GetInternalQueue (lane)->Enqueue (item);
  return retval;
}

Ptr<QueueDiscItem>
DsrVirtualQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;
  uint32_t prio = Classify ();
  if (prio == 88)
  {
    return 0;
  }
  if (item = GetInternalQueue (prio)->Dequeue ())
    {
      NS_LOG_LOGIC ("Popped from band " << prio << ": " << item);
      NS_LOG_LOGIC ("Number packets band " << prio << ": " << GetInternalQueue (prio)->GetNPackets ());
      // std::cout << "++++++ Current Queue length: " << GetInternalQueue (prio)->GetNPackets () << " at band: " << item <<  std::endl;
      /**
       * \todo Count the number of time-out drops in each queue by compare the packet delay budget and its sojourn time
      */
      return item;
    }
  NS_LOG_LOGIC ("Queue empty");
  return item;
}

Ptr<const QueueDiscItem>
DsrVirtualQueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Peek ()) != 0)
        {
          NS_LOG_LOGIC ("Peeked from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
DsrVirtualQueueDisc::CheckConfig (void)
{
  // std::cout << "queue line Number = " << GetNInternalQueues () << std::endl;
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("DsrVirtualQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () != 0)
    {
      NS_LOG_ERROR ("DsrVirtualQueueDisc needs no packet filter");
      return false;
    }
  
  if (GetNInternalQueues () == 0)
    {
      // create 3 DropTail queues with GetLimit() packets each
      ObjectFactory factory;
      factory.SetTypeId ("ns3::DropTailQueue<QueueDiscItem>");
      factory.Set ("MaxSize", QueueSizeValue (GetMaxSize ()));
      AddInternalQueue (factory.Create<InternalQueue> ());
      AddInternalQueue (factory.Create<InternalQueue> ());
      AddInternalQueue (factory.Create<InternalQueue> ());
    }

  if (GetNInternalQueues () != 3)
    {
      NS_LOG_ERROR ("DsrVirtualQueueDisc needs 3 internal queues");
      return false;
    }

  if (GetInternalQueue (0)-> GetMaxSize ().GetUnit () != QueueSizeUnit::PACKETS ||
      GetInternalQueue (1)-> GetMaxSize ().GetUnit () != QueueSizeUnit::PACKETS ||
      GetInternalQueue (2)-> GetMaxSize ().GetUnit () != QueueSizeUnit::PACKETS)
    {
      NS_LOG_ERROR ("PfifoFastQueueDisc needs 3 internal queues operating in packet mode");
      return false;
    }

  for (uint8_t i = 0; i < 2; i++)
    {
      if (GetInternalQueue (i)->GetMaxSize () < GetMaxSize ())
        {
          NS_LOG_ERROR ("The capacity of some internal queue(s) is less than the queue disc capacity");
          return false;
        }
    }
  // std::cout << "The number of Internal Queues = "<< GetNInternalQueues () << std::endl;
  return true;
}

void
DsrVirtualQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
}

uint32_t
DsrVirtualQueueDisc::Classify ()
{
  if (currentFastWeight > 0)
    {
      if (!GetInternalQueue (0)->IsEmpty ())
        {
          currentFastWeight--;
          return 0;
        }
      else
        {
          currentFastWeight = 0;
        }
    }
  if (currentSlowWeight > 0)
    {
      if (!GetInternalQueue (1)->IsEmpty ())
        {
          currentSlowWeight--;
          return 1;
        }
      else
        {
          currentSlowWeight = 0;
        }
    }
  if (currentNormalWeight > 0)
    {
      if (!GetInternalQueue (2)->IsEmpty ())
        {
          currentNormalWeight--;
          return 2;
        }
      else
        {
          currentNormalWeight = 0;
        }
    }
  currentFastWeight = m_fastWeight;
  currentSlowWeight = m_slowWeight;
  
   if (currentFastWeight > 0)
    {
      if (!GetInternalQueue (0)->IsEmpty ())
        {
          currentFastWeight--;
          return 0;
        }
      else
        {
          m_remainWeight = currentFastWeight;
          currentFastWeight = 0;
        }
    }
  if (currentSlowWeight > 0)
    {
      if (!GetInternalQueue (1)->IsEmpty ())
        {
          currentSlowWeight--;
          return 1;
        }
      else
        {
          m_remainWeight += currentSlowWeight;
          currentSlowWeight = 0;
        }
    }
  currentNormalWeight = m_remainWeight;
  if (currentNormalWeight > 0)
    {
      if (!GetInternalQueue (2)->IsEmpty ())
        {
          currentNormalWeight--;
          return 2;
        }
      else
        {
          currentNormalWeight = 0;
          m_remainWeight = 0;
        }
    }
  
  /**
   * \todo Write TD-AQM algorithm here
  */


  return 88;
}   

uint32_t
DsrVirtualQueueDisc::EnqueueClassify (Ptr<QueueDiscItem> item)
{
  DsrHeader header;

  if (item->GetPacket()->PeekHeader (header) == 0) // Q: slow/loss of ACK may also lead to network congestion
  {
    std::cout << "Empty header" << std::endl;
    return NORMAL_LANE;
  }

  uint8_t priority = header.GetPriority ();

  switch (priority)
  {
  case 0x00:
    return FAST_LANE;
  case 0x01:
    return SLOW_LANE;
  default:
    return NORMAL_LANE;
  }
}

/**
 * \brief Update queue length estimation by TD
 * \param m_queueLength Real queue length of previous time period
 * \param m_estQlOld Estimated queue length of the previous time period
 * \param m_estQlNew Estimated queue length of the current time period
 * \return Updated m_estQlNew
*/
uint32_t 
// DsrVirtualQueueDisc::QueueLengthUpdate (uint32_t m_queueLength, uint32_t m_estQlOld, uint32_t m_estQlNew)
DsrVirtualQueueDisc::QueueLengthUpdate (void)
{
  double eta = 0.5; // adjusting param
  double tempQl;
  for (int i=0; i<3; i++)
  {
    tempQl = m_estQlNew[i] + eta * (m_queueLength[i] - m_estQlOld[i]);
    m_estQlNew[i] = (uint32_t) tempQl;
  }
  m_estQlOld = m_estQlNew;
  return m_estQlNew;
}

/**
 * \brief Update drop probability estimation
 * \param m_OptDrop Optimal drop probability of previous time period
 * \param m_estDropOld Estimated drop probability of the previous time period
 * \param m_estDropNew Estimated drop probability of the current time period
*/
double 
// DsrVirtualQueueDisc::DropProbUpdate (double m_estDropNew, uint32_t m_OptDrop, uint32_t m_estDropOld, uint32_t m_delayRef, uint32_t m_queueLength, uint32_t m_gamma
//                                               uint32_t m_estQlNew, uint32_t m_TODrop, uint32_t m_arrivals)
DsrVirtualQueueDisc::DropProbUpdate (void)
{
  double eta = 0.5;
  double tempDp;
  tempDp = DropProbEstimate (void);
  for (int i=0; i<3; i++)
  {
    m_estDropNew[i] = tempDp[i] + eta * (m_optDrop[i] - m_estDropOld[i]);
  }
  m_estQlOld = m_estQlNew;
  return m_estDropNew;
}



/**
 * \brief Find optimal drop probability for each queue by solving the optimization problem
 * \return Drop proability estimation
*/
double 
// DsrVirtualQueueDisc::DropProbEstimate (uint32_t m_estQlNew, uint32_t m_TODrop, uint32_t m_arrivals, uint32_t m_delayRef, uint32_t m_queueLength, uint32_t m_gamma)
DsrVirtualQueueDisc::DropProbEstimate (void)
{
  double alp = 0.5;
  double beta = 0.5;
  uint32_t linkRate; // Get link rate from p2pLink
  double W[3]={0,0,0}; // W matrix
  double b[3]={0,0,0}; //b matrix

  for (int i=0; i<3; i++)
  {
    double a1 = 1 / (m_delayRef[i] * linkRate[i]);
    double a2 = std::max(m_queueLength[i] - m_toDrop[i] - m_usedTokens[i], 0);
    double a3 = m_arrivals[i];
    double b1 = m_toDrop[i]/a3;
    double A0 = 1 - a1 * a2;
    double A1 = a1 * a3;
    double B0 = beta * b1;

    W[i] = -alp * A1 * beta * m_gamma[i];
    b[i] = (alp * A0 * beta + alp * A1 * B0) * m_gamma[i];
  }
  
  double x_drop = computMatrix (W, b); // Solve opt problem 
  for (int i=0; i<3; i++)
  {
    m_estDropNew[i] = 1 - x_drop[i];
  }

  return m_estDropNew;
}

/**
 * \brief Estimate queue length of the next time period
 * \param m_toDrop number of time-out drops in the previous time period
 * \param m_usedTokens number of used tokens in the previous time period
 * \param m_arrivals number of arrived packets in the previous time period
 * \param m_queuelength queue length of current time period
*/
uint32_t 
// DsrVirtualQueueDisc::QueueEstimate (uint32_t m_toDrop, uint32_t m_usedTokens, uint32_t m_arrivals, uint32_t m_queueLength)
DsrVirtualQueueDisc::QueueEstimate (void)
{
  uint32_t tempQl;
  for (int i=0; i<3; i++)
  {
    tempQl = std::max (m_queueLength[i] - m_usedTokens[i] - m_toDrop[i], 0) + m_arrivals[i];
    m_estQlNew[i] = tempQl;
  }
  return m_estQlNew;
}


} // namespace ns3