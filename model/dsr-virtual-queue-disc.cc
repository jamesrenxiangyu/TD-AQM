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
  currentNormalWeight = m_normalWeight;
  
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


uint32_t DsrVirtualQueueDisc::QueueLengthUpdate (uint32_t m_queueLength, uint32_t m_estQlOld, uint32_t m_estQlNew, uint32_t m_usedTokens);
{
  double eta = 0.5; // adjusting param
  double tempQl[3] = {0.0, 0.0, 0.0};
  tempQl = m_estQlNew + eta * (m_queueLength - m_estQlOld);
  m_estQlNew = (int) tempQl;
  return m_estQlNew;
}


uint32_t DsrVirtualQueueDisc::DropProbUpdate ( double m_estDropOld, uint32_t m_OptDrop, uint32_t estQl, uint32_t m_TODrop, uint32_t m_arrivals);
{
  double eta = 0.5;
  double tempDp[3] = {0.0, 0.0, 0.0};
  double m_estDropNew = DropProbEstimate (uint32_t estQl, uint32_t m_TODrop, uint32_t m_arrivals);
  tempDp = m_estDropNew + eta * (m_optDrop - m_estDropOld);
  m_estDropNew = tempDp;
  return m_estDropNew;
}


double DropProbEstimate (uint32_t m_estQlNew, uint32_t m_TODrop, uint32_t m_arrivals, uint32_t m_delayRef, uint32_t m_queueLength, uint32_t m_gamma);
{
  /**
   * TD-AQM algorithm
  */
  double alp = 0.5;
  double beta = 0.5;
  uint32_t linkRate[3] = {1, 2, 3};
  double a1 = 1 / (m_delayRef * linkRate);
  double a2 = std::max(m_queueLength - m_TODrop - m_usedTokens, 0);
  double a3 = m_arrivals;
  double b1 = m_TODrop/a3;
  double A0 = 1 - a1 * a2;
  double A1 = a1 * a3;
  double B0 = beta * b1;

  double dropProb;
  double w = m_gamma.asDiagonal () * (alp * A1 * beta);
  double w = 1/W; // Inverse of the matrix
  double W = w.asDiagonal ();
  double b = alp * A0 * beta + alp * B0 * A1;

  m_estDropNew = 0.5 * W * b;
    



  
  
  return  m_estDropNew;
}


uint32_t QueueEstimate (uint32_t m_toDrop, uint32_t m_usedTokens, uint32_t m_arrivals, uint32_t m_queueLength);
{
  uint32_t m_estQl = std::max (m_queueLength - m_usedTokens - m_toDrop, 0) + m_arrivals;
  return m_estQl;
}


} // namespace ns3